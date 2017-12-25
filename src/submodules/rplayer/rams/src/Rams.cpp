///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include "RamsInterpreter.h"

#include <rplayer/ts/src/common.h>

#include <rplayer/StreamMetaData.h>
#include <rplayer/rams/Rams.h>
#include <rplayer/utils/BitReader.h>
#include <rplayer/utils/Logger.h>

#include <algorithm>

#include <assert.h>
#include <string.h>
#include <stdint.h>

using namespace rplayer;

static const uint8_t RAMS_SYNC_BYTE1 = 0x52;
static const uint8_t RAMS_SYNC_BYTE2 = 0x9A;

Rams::Rams() :
    m_packetByteCount(0),
    m_ramsPacketLength(0),
    m_splitterState(STATE_OUT_OF_SYNC),
    m_packetOut(0),
    m_ramsInterpreter(m_ramsUnitStore)
{
}

Rams::~Rams()
{
}

void Rams::reset()
{
    m_packetByteCount = 0;
    m_ramsPacketLength = 0;
    m_splitterState = STATE_OUT_OF_SYNC;

    m_ramsInterpreter.reset(); // (m_ramsUnitStore is reset() in here)
}

void Rams::setTsPacketOutput(IPacketSinkWithMetaData *packetOut)
{
    m_packetOut = packetOut;
    m_ramsInterpreter.setTsPacketOutput(packetOut);
}

void Rams::registerStreamDecryptEngine(IStreamDecrypt *streamDecryptEngine)
{
    m_ramsInterpreter.setStreamDecryptEngine(streamDecryptEngine);
}

void Rams::registerRamsChunkAllocator(IRamsChunkAllocator *ramsChunkAllocator)
{
    m_ramsUnitStore.registerRamsChunkAllocator(ramsChunkAllocator);
}

void Rams::setCurrentTime(uint16_t timeInMs)
{
    m_ramsInterpreter.setCurrentTime(timeInMs);
}

void Rams::put(const uint8_t *data, uint32_t size)
{
    const uint8_t *end = data + size;
    const uint8_t *packetStart = data;
    bool hasRamsSync = false;

    while (data < end) {
        // Start each iteration by trying to parse a new packet, either TS or RAMS
        switch (m_splitterState) {
        case STATE_OUT_OF_SYNC:
            while (data < end) { // Optimization, saves re-evaluating the switch
                if (data[0] == TS_SYNC_BYTE) {
                    m_splitterState = STATE_TS;
                    packetStart = data;
                    m_packetByteCount = 0;
                    // When switching to TS we signal that in the metadata so we don't have to do that each TS packet
                    if (m_packetOut) {
                        m_packetOut->setMetaData(StreamMetaData(StreamMetaData::CLEAR_TS));
                    }
                    break;
                } else if (data[0] == RAMS_SYNC_BYTE1) { // Second sync byte will be checked from within the RAMS state
                    m_splitterState = STATE_RAMS;
                    packetStart = data;
                    m_packetByteCount = 0;
                    m_ramsPacketLength = 0;
                    break;
                } else {
                    data++;
                }
            }
            break;

        case STATE_TS:
            while (data < end) { // Optimization, saves re-evaluating the switch
                // Packet sync is expected when m_packetByteCount == 0
                assert(m_packetByteCount < TS_PACKET_SIZE);
                if (m_packetByteCount == 0 && data[0] != TS_SYNC_BYTE) {
                    // Out of sync
                    // Might be the start of a RAMS, though
                    // Output all TS packet data up to now
                    if (m_packetOut && data > packetStart) {
                        m_packetOut->put(packetStart, data - packetStart);
                    }
                    m_splitterState = STATE_OUT_OF_SYNC;
                    break;
                }

                // Still in sync
                if (data - m_packetByteCount + TS_PACKET_SIZE >= end) {
                    // We reached the end of this data chunk
                    m_packetByteCount += end - data;
                    assert(m_packetByteCount <= TS_PACKET_SIZE);
                    if (m_packetByteCount == TS_PACKET_SIZE) {
                        m_packetByteCount = 0;
                    }
                    // Output all TS packet data up to now
                    if (m_packetOut) {
                        m_packetOut->put(packetStart, end - packetStart);
                    }
                    data = end;
                    break;
                }

                // In sync and not at end
                data -= m_packetByteCount;
                data += TS_PACKET_SIZE;
                m_packetByteCount = 0;
            }
            break;

        case STATE_RAMS:
            switch (m_packetByteCount) {
            case 0: // Packet sync is expected when m_packetByteCount == 0
                if (data[0] != RAMS_SYNC_BYTE1) {
                    // Out of sync
                    // Might be the start of a TS, though
                    m_splitterState = STATE_OUT_OF_SYNC;
                    break;
                } else {
                    // Put packet start at current position and 'read' sync byte 1
                    packetStart = data;
                    m_ramsPacketLength = 0;
                    m_packetByteCount++;
                    data++;
                    hasRamsSync = true;
                }
                if (data >= end) {
                    break;
                }
                // Fall through

            case 1: // We expect the second sync byte here
                if (data[0] != RAMS_SYNC_BYTE2) {
                    // Out of sync
                    m_splitterState = STATE_OUT_OF_SYNC;
                    break;
                } else {
                    m_packetByteCount++;
                    data++;
                }
                if (data >= end) {
                    break;
                }
                // Fall through

            case 2: // First length field
                m_ramsPacketLength = data[0] << 8;
                m_packetByteCount++;
                data++;
                if (data >= end) {
                    break;
                }
                // Fall through

            case 3: // Second length field
                m_ramsPacketLength += data[0];
                m_packetByteCount++;
                data++;
                if (data >= end) {
                    break;
                }
                // Fall through

            default: // All other data
                // Still in sync, 'read' as much data from the packet as possible
                uint32_t n = std::min(4 + m_ramsPacketLength - m_packetByteCount, static_cast<uint32_t>(end - data));
                m_packetByteCount += n;
                data += n;
                break;
            }

            if (m_splitterState != STATE_RAMS) {
                break;
            }

            if (m_packetByteCount >= 4 + m_ramsPacketLength) {
                // We have a complete RAMS packet so output all RAMS packet data up to now
                assert(m_packetByteCount == 4 + m_ramsPacketLength);
                m_ramsInterpreter.parse(packetStart, data - packetStart, hasRamsSync, true);
                packetStart = data;
                m_packetByteCount = 0;
                m_ramsPacketLength = 0;
            } else if (4 + m_ramsPacketLength - m_packetByteCount >= static_cast<uint32_t>(end - data)) { // This should always be true; we could put an assert here...
                // We reached the end of this data chunk
                m_packetByteCount += end - data;
                assert(m_packetByteCount <= 4 + m_ramsPacketLength);
                if (m_packetByteCount == 4 + m_ramsPacketLength) {
                    m_packetByteCount = 0;
                }
                // Output all TS packet data up to now
                m_ramsInterpreter.parse(packetStart, end - packetStart, hasRamsSync, m_packetByteCount == 0);
                // TODO: (CNP-1913) Currently, this /may/ output a single call in case of a single SYNC_BYTE1 not followed by a SYNC_BYTE2 if the data ends here.
                // (This happens when the first byte after a RAMS packet happens to be a RAMS SYNC_BYTE1 but the other bytes are non-RAMS; This, of course, is wrong
                // input, but we'll have erroneously passed this byte already to m_ramsInterpreter. We may fix this by only calling m_ramsInterpreter *after* having
                // parsed the RAMS header, and leave this parsing out of m_ramsInterpreter because that's duplicate code. Of course we have to pass the RAMS packet
                // length then.)

                data = end;
            }
            break;
        }
    }
}
