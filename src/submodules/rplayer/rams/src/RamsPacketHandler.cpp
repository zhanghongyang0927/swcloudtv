///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include "RamsPacketHandler.h"
#include "RamsHeader.h"
#include "RamsInterpreter.h"
#include "RamsUnit.h"

#include <rplayer/ts/src/common.h>

#include <rplayer/utils/BitReader.h>
#include <rplayer/utils/Logger.h>

#include <vector>
#include <list>
#include <map>

#include <stdio.h>
#include <string.h>
#include <assert.h>

using namespace rplayer;
using namespace std;

static const uint8_t PATCH_ACTION_TO_BYTE_COUNT[] = { 0, 1, 2, 3, 4, 6, 8, 16 };

RamsPacketHandler::RamsPacketHandler(RamsUnitStore &ramsUnitStore, RamsOutput &ramsOutput) :
    m_state(INITIAL),
    m_labelIndex(0),
    m_ramsUnitStore(ramsUnitStore),
    m_ramsOutput(ramsOutput)
{
}

RamsPacketHandler::~RamsPacketHandler()
{
    if (!m_labelList.empty()) {
        RPLAYER_LOG_WARNING("Unexpected non-empty RAMS label list");
        m_labelList.clear();
    }
}

void RamsPacketHandler::processPayload(RamsHeader &ramsHeader, const uint8_t *payload, uint32_t size, bool end, const StreamMetaData &metaData)
{
    // There is no label command; according to the spec we shall passthrough the payload
    if (!ramsHeader.hasLabelCommand() && size > 0) {
        // Set new (or same) metadata for the output; the packets it directly passes will
        // associate with this metadata.
        m_ramsOutput.setMetaData(metaData);

        m_ramsOutput.put(payload, size);
    }

    // If there are commands, we try to process them
    if (m_state != FINISHED) {
        process(ramsHeader, payload, size, end);
    }

    // There are no more commands and we're not waiting for encrypted data to come back
    if (end) {
        m_labelList.clear();
        m_labelIndex = 0;
        m_state = INITIAL;
    }
}

void RamsPacketHandler::process(RamsHeader &ramsHeader, const uint8_t *payload, uint32_t size, bool end)
{
    if (m_state == INITIAL) {
        ramsHeader.firstCommand();
        m_state = PROCESSING_COMMANDS;
    }

    bool isLabelPresent = false;
    RamsHeader::Command command;
    while (ramsHeader.getNextCommand(command)) {
        switch (command.m_code) {
        case RamsInterpreter::COMMAND_RESET:
            // Handled by the RamsInterpreter
            break;

        case RamsInterpreter::COMMAND_LABEL:
            if (isLabelPresent) {
                RPLAYER_LOG_ERROR("Multiple LABEL commands in same RAMS packet, ignoring.");
                break;
            }
            isLabelPresent = true;

            if (m_labelList.empty()) {
                parseLabelData(ramsHeader.getPayloadUnitOffset(), command.m_data, command.m_length);
                m_labelIndex = 0;
            }

            if (m_labelList.size() == 1 && m_labelList[0].m_byteCount == 0) {
                if (size > 0) {
                    RamsUnit *ramsUnit = m_ramsUnitStore.getOrAllocateUnit(m_labelList[0].m_unitId);

                    if (ramsUnit) {
                        ramsUnit->addBytes(payload, size);
                    } else {
                        RPLAYER_LOG_ERROR("Unable to create RAMS unit (m_labelList[0].m_unitId=%d)", m_labelList[0].m_unitId);
                    }
                }

                // We can't continue further with the commands because more payload is required
                if (!end) {
                    ramsHeader.revertCommand(command);
                    return;
                } else {
                    break;
                }
            }

            while (m_labelIndex < m_labelList.size() && size > 0) {
                Label &label = m_labelList[m_labelIndex];
                uint32_t numBytes = label.m_byteCount;

                if (size < numBytes) {
                    numBytes = size;
                }

                RamsUnit *ramsUnit = m_ramsUnitStore.getOrAllocateUnit(label.m_unitId);

                if (ramsUnit) {
                    ramsUnit->addBytes(payload, numBytes);
                } else {
                    RPLAYER_LOG_ERROR("Unable to create RAMS unit (label.m_unitId=%d)", label.m_unitId);
                }

                size -= numBytes;
                payload += numBytes;
                label.m_byteCount -= numBytes;

                if (label.m_byteCount == 0) {
                    ++m_labelIndex;
                }
            }

            // More data is required to complete this command and it's not a fragmented TS
            if (m_labelIndex < m_labelList.size() && !end) {
                ramsHeader.revertCommand(command);
                return;
            }
            break;

        case RamsInterpreter::COMMAND_DELETE: {
            // Field unitId is 12-bits, so every 3 bytes we have 2 unit IDs
            uint8_t numIds = static_cast<uint8_t>(static_cast<uint16_t>(command.m_length) * 2 / 3);
            BitReader packets(command.m_data, command.m_length, 0);

            for (uint8_t n = 0; n < numIds; n++) {
                uint16_t unitId = packets.read(12);
                m_ramsUnitStore.deleteUnit(unitId);
            }
            break;
        }

        case RamsInterpreter::COMMAND_KEY_INFO:
            // Handled by the RamsInterpreter
            break;

        case RamsInterpreter::COMMAND_OUTPUT: {
            uint16_t scheduledTime = ramsHeader.getClockReference();

            const uint8_t *commandData = command.m_data;
            const uint8_t *commandEnd = command.m_data + command.m_length;
            while (commandData < commandEnd) {
                RamsOutput::OutputAction outputAction;

                if (commandData + 2 > commandEnd) {
                    RPLAYER_LOG_ERROR("RAMS OUTPUT command underflow");
                    break;
                }

                bool patchFlag = commandData[0] & 0x80;
                bool clockDeltaFlag = commandData[0] & 0x40;

                outputAction.m_unitId = (commandData[0] & 0x0F) << 8 | commandData[1];

                commandData += 2;

                if (clockDeltaFlag) {
                    if (commandData + 2 > commandEnd) {
                        RPLAYER_LOG_ERROR("RAMS OUTPUT command underflow");
                        break;
                    }

                    scheduledTime += (commandData[0] << 8) | commandData[1];
                    commandData += 2;
                }

                outputAction.m_clock = scheduledTime;

                if (patchFlag) {
                    if (commandData + 1 > commandEnd) {
                        RPLAYER_LOG_ERROR("RAMS OUTPUT command underflow");
                        break;
                    }

                    uint32_t patchByteIndex = 0;
                    uint8_t patchLength = commandData[0];

                    commandData += 1;

                    const uint8_t *patchEnd = commandData + patchLength;
                    if (patchEnd > commandEnd) {
                        RPLAYER_LOG_ERROR("RAMS OUTPUT command underflow");
                        break;
                    }

                    while (commandData < patchEnd) {
                        if (commandData + 2 > patchEnd) {
                            RPLAYER_LOG_ERROR("RAMS OUTPUT patch command underflow");
                            break;
                        }
                        RamsOutput::PatchAction patchAction;
                        uint8_t action = commandData[1] & 0x0F;

                        patchByteIndex += commandData[0] << 4 | (commandData[1] & 0xF0) >> 4;
                        patchAction.m_offset = patchByteIndex;
                        patchAction.m_byteCount = mapPatchActionToBytes(action);

                        commandData += 2;
                        if (commandData + patchAction.m_byteCount > patchEnd) {
                            RPLAYER_LOG_ERROR("RAMS OUTPUT patch command underflow");
                            break;
                        }

                        memcpy(patchAction.m_patch, commandData, patchAction.m_byteCount);

                        outputAction.m_patchList.push_back(patchAction);

                        commandData += patchAction.m_byteCount;
                    }
                }

                // Fill in the metadata for this output action; this assumes that a clear TS is
                // always output.
                // TODO: If decryption failed or is impossible (or undesired) or if the output
                // is no TS at all, we should change this entry into the appropriate value.
                outputAction.m_metaData = StreamMetaData(StreamMetaData::CLEAR_TS, ramsHeader.getPayloadId());

                // If an output is scheduled NOW, we shouln't add it to the list but output it immediately
                // (Because it might be deleted right after)
                // This also saves some administration in the RamsOutput object.
                if (scheduledTime == ramsHeader.getClockReference()) {
                    m_ramsOutput.outputUnit(outputAction);
                } else {
                    m_ramsOutput.addOutputAction(outputAction);
                }
            }
            break;
        }

        default:
            RPLAYER_LOG_ERROR("Unrecognized RAMS command (commandCode=%d)", command.m_code);
            break;
        }
    }

    // If we get here, we're done processing all commands
    m_state = FINISHED;
}

void RamsPacketHandler::parseLabelData(uint8_t payloadUnitOffset, const uint8_t *data, uint8_t size)
{
    uint8_t numIds = size >> 1;
    for (uint8_t n = 0; n < numIds; n++) {
        uint8_t count = (data[0] & 0xF0) >> 4;
        uint16_t unitId = (data[0] & 0x0F) << 8 | data[1];

        // If the current unitId is equal to the previous one, we increase the count
        if (m_labelList.size() > 0 && unitId == m_labelList.back().m_unitId) {
            m_labelList.back().m_byteCount += count * TS_PACKET_SIZE;
        } else {
            Label label;
            label.m_unitId = unitId;
            label.m_byteCount = count * TS_PACKET_SIZE;

            m_labelList.push_back(label);
        }

        data += 2;
    }

    if (payloadUnitOffset > 0 && m_labelList.size() > 0 && m_labelList[0].m_byteCount >= payloadUnitOffset) {
        m_labelList[0].m_byteCount -= payloadUnitOffset;
    }
}

uint8_t RamsPacketHandler::mapPatchActionToBytes(uint8_t action)
{
    if (action < sizeof(PATCH_ACTION_TO_BYTE_COUNT) / sizeof(PATCH_ACTION_TO_BYTE_COUNT[0])) {
        return PATCH_ACTION_TO_BYTE_COUNT[action];
    } else {
        RPLAYER_LOG_ERROR("Action value out of range (action=%d)", action);
    }

    return 0;
}
