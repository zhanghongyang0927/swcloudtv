///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include "RamsInterpreter.h"
#include "RamsHeader.h"
#include "RamsPacketHandler.h"

#include <rplayer/utils/BitReader.h>
#include <rplayer/utils/Logger.h>

#include <assert.h>
#include <string.h>

using namespace rplayer;
using namespace std;

//static const uint8_t PAYLOAD_TYPE_RA_TS     = 0;
static const uint8_t PAYLOAD_TYPE_RA_ECB_TS = 1;

RamsInterpreter::RamsInterpreter(RamsUnitStore &ramsUnitStore) :
    m_isKeyInfoSet(false),
    m_parserState(STATE_PARSING_HEADER),
    m_currentRamsHeader(0),
    m_streamDecryptEngine(0),
    m_currentRamsPacketHandler(0),
    m_ramsUnitStore(ramsUnitStore),
    m_ramsOutput(ramsUnitStore),
    m_ramsClock(m_ramsOutput)
{
    for (int i = 0; i < MAX_NUM_PACKET_HANDLERS; i++) {
        m_ramsPacketHandlerArray[i] = 0;
    }
}

RamsInterpreter::~RamsInterpreter()
{
    if (m_streamDecryptEngine) {
        m_streamDecryptEngine->setStreamReturnPath(0);
    }

    for (int i = 0; i < MAX_NUM_PACKET_HANDLERS; i++) {
        if (m_ramsPacketHandlerArray[i]) {
            delete m_ramsPacketHandlerArray[i];
        }
    }

    if (!m_ramsHeaderDecryptionList.empty()) {
        RPLAYER_LOG_WARNING("Unexpected non-empty RAMS header decryption list");
    }

    reset();
}

void RamsInterpreter::reset()
{
    while (!m_ramsHeaderDecryptionList.empty()) {
        RamsHeader *ramsHeader = m_ramsHeaderDecryptionList.front();
        m_ramsHeaderDecryptionList.pop_front();
        m_ramsHeaderPool.releaseRamsHeader(*ramsHeader);
    }

    m_isKeyInfoSet = false;
    resetCurrentRamsParsingState();

    m_ramsUnitStore.reset();
    m_ramsOutput.reset();
    m_ramsClock.reset();
}

void RamsInterpreter::resetCurrentRamsParsingState()
{
    if (m_currentRamsHeader) {
        m_ramsHeaderPool.releaseRamsHeader(*m_currentRamsHeader);
    }
    m_currentRamsHeader = 0;
    m_currentRamsPacketHandler = 0;
    m_parserState = STATE_PARSING_HEADER;
}

void RamsInterpreter::parse(const uint8_t *data, uint32_t size, bool startFlag, bool endFlag)
{
    if (startFlag) {
        resetCurrentRamsParsingState();
        m_currentRamsHeader = m_ramsHeaderPool.getRamsHeader();
    }

    if (m_parserState == STATE_PARSING_HEADER && m_currentRamsHeader->addBytes(data, size)) {
        // Start reading the first command
        m_currentRamsHeader->firstCommand();

        m_parserState = STATE_PARSING_PAYLOAD;

        // Find and process all commands that are relevant at this stage
        RamsHeader::Command command;
        bool isFirstCommand = true;
        bool isResetAsLastCommand = false;
        while (m_currentRamsHeader->getNextCommand(command)) {
            isResetAsLastCommand = false; // May have been set to true by an earlier RESET, but any command following it means that that RESET command was not the last command in the list.

            switch (command.m_code) {
            case COMMAND_KEY_INFO:
                if (command.m_length != 32) {
                    RPLAYER_LOG_WARNING("Illegal KEY_INFO command length: %u", command.m_length);
                    break;
                }

                if (m_streamDecryptEngine) {
                    uint8_t keyId[16];
                    uint8_t iv[16];

                    memcpy(keyId, command.m_data, sizeof(keyId));
                    memcpy(iv, command.m_data + 16, sizeof(iv));

                    m_streamDecryptEngine->setKeyIdentifier(keyId);
                    m_streamDecryptEngine->setInitializationVector(iv);

                    m_isKeyInfoSet = true; // This marks not only the presence of valid KEY_INFO but also successfully passing it to the stream decrypt engine.
                }
                break;

            case COMMAND_LABEL:
                m_currentRamsHeader->setLabelCommand();
                break;

            case COMMAND_RESET:
                if (isFirstCommand) {
                    // If the RESET is the first of the commands, we reset the clock immediately.
                    // This means that any clock value will be seen as an initial clock.
                    // (On the contrary, if the RESET occurs as the last of the commands, it should
                    // cause the *next* packet to assume an initial clock.)
                    m_ramsClock.reset();
                    // Also immediately reset the unit store and the output so we won't output anything when we synchronize the clock in the code below.
                    m_ramsUnitStore.reset();
                    m_ramsOutput.reset();
                }
                isResetAsLastCommand = true; // This will be marked last unless another command follows.
                break;

            case COMMAND_OUTPUT: {
                // If an OUTPUT command is present, it will replace all resident output actions
                // that were already scheduled at the given time or later.
                // We only need to process the first OUTPUT entry, if present.
                if (command.m_length < 2) {
                    break;
                }

                uint16_t scheduledTime = m_currentRamsHeader->getClockReference();

                bool clockDeltaFlag = command.m_data[0] & 0x40;
                if (clockDeltaFlag) {
                    if (command.m_length < 4) {
                        RPLAYER_LOG_ERROR("RAMS OUTPUT command underflow");
                        break;
                    }

                    scheduledTime += (command.m_data[2] << 8) | command.m_data[3];
                }

                // Upon an OUTPUT command, all succeeding scheduled outputs need to be deleted.
                // This should be done only for the first action in the list.
                m_ramsOutput.deleteSucceedingActions(scheduledTime);
            }
                break;

            default:
                break;
            }

            isFirstCommand = false;
        }

        // Synchronize the RAMS clock.
        // This will also output all units that are scheduled up to this time.
        m_ramsClock.synchronizeClock(m_currentRamsHeader->getClockReference());

        if (isResetAsLastCommand) {
            m_currentRamsHeader->setResetAsLastCommand(); // Keep this flag in the header for later use.
        }

        uint8_t payloadId = m_currentRamsHeader->getPayloadId();
        if (m_ramsPacketHandlerArray[payloadId] == 0) {
            m_ramsPacketHandlerArray[payloadId] = new RamsPacketHandler(m_ramsUnitStore, m_ramsOutput);
        }

        m_currentRamsPacketHandler = m_ramsPacketHandlerArray[payloadId];
        assert(m_currentRamsPacketHandler);

        // This packet can be decrypted, so we add it into the list
        if (m_currentRamsHeader->getPayloadType() == PAYLOAD_TYPE_RA_ECB_TS && m_currentRamsHeader->getPayloadLength() > 0 && m_isKeyInfoSet) {
            m_currentRamsHeader->addRef();
            m_ramsHeaderDecryptionList.push_back(m_currentRamsHeader);
        }
    }

    // At this point "data" points to the payload
    if (m_parserState == STATE_PARSING_PAYLOAD) {
        assert(m_currentRamsHeader);

        if (endFlag) {
            m_currentRamsHeader->setEndFlag();
        }

        if (m_currentRamsHeader->getPayloadType() == PAYLOAD_TYPE_RA_ECB_TS) {
            // We should try to decrypt any encrypted payload; if this fails for whatever reason, it is discarded.
            if (size > 0 && m_currentRamsHeader->getPayloadLength() > 0 && m_isKeyInfoSet) {
                m_currentRamsHeader->addReceivedBytesCount(size);
                assert(m_streamDecryptEngine); // m_isKeyInfoSet being true implies this.
                if (!m_streamDecryptEngine->streamData(data, size)) {
                    RPLAYER_LOG_ERROR("Decryption failed (size=%d)", size);

                    // The decryptor didn't accept the bytes.
                    // So, now we're out of sync. We need to call m_currentRamsPacketHandler->processPayload() with 'end' set to true
                    // in order to clean up the header list, but we don't know whether there's still data left for it in the decrypt
                    // queue since this may be the Nth decrypt call belonging to the same RAMS packet. There's no reliable way to know
                    // for sure what arriving data to match with what header. Also, it's unlikely that m_streamDecryptEngine->streamData()
                    // fails now but will succeed later. [[An exception being a temporary out-of-memory situation when feeding the decrypt
                    // engine with lots of data at once. This needs to be solved in a bigger context, however.]]
                    // So all in all, if this happens now, we consider this error to be fatal. Therefore we clean-up everything, making sure
                    // that we'll not have dangling headers or other resources.
                    cleanupStreamDecryption();
                }
            }
        } else {
            // We process the regular payload
            assert(m_currentRamsPacketHandler);
            m_currentRamsPacketHandler->processPayload(*m_currentRamsHeader, data, size, endFlag, StreamMetaData(StreamMetaData::CLEAR_TS, m_currentRamsHeader->getPayloadId()));
        }
    }

    if (endFlag) {
        assert(m_currentRamsHeader);

        if (m_currentRamsHeader->hasResetAsLastCommand()) {
            // If the RESET is the last of the commands, we reset the clock after having
            // processed the RAMS packet and after having done any actions with respect to
            // the current RAMS clock (i.c. checking and executing any scheduled OUTPUT commands).
            // This means that the *next* clock value will be seen as an initial clock.
            m_ramsClock.reset();
            // We also reset the unit store and the output here.
            m_ramsUnitStore.reset();
            m_ramsOutput.reset();
        }

        m_parserState = STATE_PARSING_COMPLETE; // Not needed, strictly speaking, but true.
    }
}

void RamsInterpreter::setTsPacketOutput(IPacketSinkWithMetaData *packetOut)
{
    m_ramsOutput.setTsPacketOutput(packetOut);
}

void RamsInterpreter::cleanupStreamDecryption()
{
    // With a new decrypt engine or after a cleanup, any previously registered key info must be considered lost.
    m_isKeyInfoSet = false;

    // If there are any transactions left, we also must clean up all pending packets.
    while (!m_ramsHeaderDecryptionList.empty()) {
        RamsHeader *ramsHeader = m_ramsHeaderDecryptionList.front();
        if (ramsHeader->isComplete()) {
            assert(m_ramsPacketHandlerArray[ramsHeader->getPayloadId()]);
            m_ramsPacketHandlerArray[ramsHeader->getPayloadId()]->processPayload(*ramsHeader, 0, 0, true, StreamMetaData(StreamMetaData::UNDEFINED, ramsHeader->getPayloadId()));
        }
        m_ramsHeaderDecryptionList.pop_front();
        m_ramsHeaderPool.releaseRamsHeader(*ramsHeader);
    }

    // The data of any further calls to RamsInterpreter::put() (by pending decrypted data in the stream decrypt engine) will be discarded
    // until new key info is set and new encrypted data arrives.
    // The process might get out-of-sync if this happens before all data from the decrypt engine is flushed. Let's hope that that won't happen.
}

void RamsInterpreter::setStreamDecryptEngine(IStreamDecrypt *streamDecryptEngine)
{
    if (streamDecryptEngine == m_streamDecryptEngine) {
        return; // Do nothing if nothing changes...
    }

    cleanupStreamDecryption();

    m_streamDecryptEngine = streamDecryptEngine;

    if (m_streamDecryptEngine) {
        m_streamDecryptEngine->setStreamReturnPath(this);
    }
}

void RamsInterpreter::put(const uint8_t *data, uint32_t size)
{
    while (m_ramsHeaderDecryptionList.size() > 0 && size > 0) {
        RamsHeader *ramsHeader = m_ramsHeaderDecryptionList.front();
        uint32_t bytes = ramsHeader->getReceivedBytesCount() - ramsHeader->getDecryptedBytesCount();

        if (size < bytes) {
            bytes = size;
        }

        ramsHeader->addDecryptedBytesCount(bytes);

        bool end = ramsHeader->isComplete();
        assert(m_ramsPacketHandlerArray[ramsHeader->getPayloadId()]);
        m_ramsPacketHandlerArray[ramsHeader->getPayloadId()]->processPayload(*ramsHeader, data, bytes, end, StreamMetaData(StreamMetaData::CLEAR_TS, ramsHeader->getPayloadId()));

        data += bytes;
        size -= bytes;

        if (end) {
            m_ramsHeaderDecryptionList.pop_front();
            m_ramsHeaderPool.releaseRamsHeader(*ramsHeader);
        } else if (bytes == 0) {
            RPLAYER_LOG_ERROR("Unexpected decrypted data received: size=%u, ramsHeader->getReceivedBytesCount()=%u, ramsHeader->getDecryptedBytesCount()=%u", size, ramsHeader->getReceivedBytesCount(), ramsHeader->getDecryptedBytesCount());
            break;
        }
    }
}

void RamsInterpreter::setCurrentTime(uint16_t timeInMs)
{
    m_ramsClock.setCurrentTime(timeInMs);
}
