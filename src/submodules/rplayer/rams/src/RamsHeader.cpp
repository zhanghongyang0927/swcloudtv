///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include "RamsHeader.h"

#include <rplayer/utils/Logger.h>

#include <algorithm>

#include <assert.h>
#include <stdio.h>
#include <stdint.h>

using namespace rplayer;
using namespace std;

static const int MAX_RAMS_HEADER_SIZE = 7 + 1023;

RamsHeader::RamsHeader() :
    m_ramsHeaderLength(0),
    m_payloadId(0),
    m_payloadType(0),
    m_clockReference(0),
    m_numOfCommands(0),
    m_payloadUnitOffset(0),
    m_payloadLength(0),
    m_receivedBytesCount(0),
    m_processedBytesCount(0),
    m_hasLabelCommand(false),
    m_hasResetAsLastCommand(false),
    m_currentCommandIndex(0),
    m_currentCommandOffset(0),
    m_endFlag(false),
    m_refCount(0)
{
    m_ramsHeaderBuf.reserve(MAX_RAMS_HEADER_SIZE);
}

bool RamsHeader::addBytes(const uint8_t *&data /*in/out*/, uint32_t &size /*in/out*/)
{
    if (m_ramsHeaderLength > 0 && m_ramsHeaderBuf.size() == m_ramsHeaderLength) {
        return true;
    }

    // We haven't parsed yet the field header_length
    if (m_ramsHeaderLength == 0) {
        assert(m_ramsHeaderBuf.size() < 7);
        uint32_t bytes = 7 - m_ramsHeaderBuf.size();
        bytes = std::min(bytes, size);

        m_ramsHeaderBuf.insert(m_ramsHeaderBuf.end(), data, data + bytes);
        data += bytes;
        size -= bytes;

        if (m_ramsHeaderBuf.size() == 7) {
            m_ramsHeaderLength = (((m_ramsHeaderBuf[5] & 0x03) << 8) | m_ramsHeaderBuf[6]) + 7;
        } else {
            return false;
        }
    }

    if (m_ramsHeaderBuf.size() < m_ramsHeaderLength) {
        uint32_t bytes = m_ramsHeaderLength - m_ramsHeaderBuf.size();
        bytes = std::min(bytes, size);

        m_ramsHeaderBuf.insert(m_ramsHeaderBuf.end(), data, data + bytes);
        data += bytes;
        size -= bytes;

        if (m_ramsHeaderBuf.size() < m_ramsHeaderLength) {
            return false;
        }
    }

    assert(m_ramsHeaderBuf.size() == m_ramsHeaderLength);

    // RAMS header length should be at least 12, to incorporate all required fields.
    // If less, the packet is incorrect.
    if (m_ramsHeaderBuf.size() < 12) {
        RPLAYER_LOG_ERROR("RAMS header too small (%u)", static_cast<uint16_t>(m_ramsHeaderBuf.size()));
        return true; // Keep the defaults
    }

    m_payloadId = (m_ramsHeaderBuf[10] & 0xF0) >> 4;
    m_payloadType = m_ramsHeaderBuf[10] & 0x0F;
    m_clockReference = m_ramsHeaderBuf[8] << 8 | m_ramsHeaderBuf[9];
    m_numOfCommands = m_ramsHeaderBuf[11];
    m_payloadUnitOffset = m_ramsHeaderBuf[7];

    m_payloadLength = ((m_ramsHeaderBuf[2] << 8) | m_ramsHeaderBuf[3]) + 4; // entire RAMS packet
    m_payloadLength -= std::min(m_payloadLength, m_ramsHeaderLength); // minus RAMS header; RAMS packet length should be large enough, but cap to 0 if it isn't

    return true;
}

uint8_t RamsHeader::getPayloadId() const
{
    return m_payloadId;
}

uint8_t RamsHeader::getPayloadType() const
{
    return m_payloadType;
}

uint16_t RamsHeader::getClockReference() const
{
    return m_clockReference;
}

uint8_t RamsHeader::getNumOfCommands() const
{
    return m_numOfCommands;
}

uint8_t RamsHeader::getPayloadUnitOffset() const
{
    return m_payloadUnitOffset;
}

uint16_t RamsHeader::getPayloadLength() const
{
    return m_payloadLength;
}

void RamsHeader::firstCommand()
{
    // Reset the command pointer
    m_currentCommandIndex = 0;
    m_currentCommandOffset = 12; // Fixed offset, the commands, if present, start at the 12th byte of the RAMS header.
}

bool RamsHeader::getNextCommand(Command &command/*out*/)
{
    // Return if we're out of commands
    if (m_currentCommandIndex >= m_numOfCommands) {
        return false;
    }

    // Return if there's not enough data for the next command (this is a protocol error)
    if (m_currentCommandOffset + 2U > m_ramsHeaderBuf.size()) {
        RPLAYER_LOG_ERROR("RAMS header too small (%u) for the number of commands given (%u)", static_cast<uint16_t>(m_ramsHeaderBuf.size()), m_numOfCommands);
        return false;
    }

    // Get the next command.
    command.m_code = m_ramsHeaderBuf[m_currentCommandOffset + 0];
    command.m_length = m_ramsHeaderBuf[m_currentCommandOffset + 1];
    command.m_data = &m_ramsHeaderBuf[m_currentCommandOffset + 2];

    // Update command pointers
    m_currentCommandIndex++;
    m_currentCommandOffset += 2 + command.m_length;

    // If the command doesn't fit, return failure (this is a protocol error)
    if (m_currentCommandOffset > m_ramsHeaderBuf.size()) {
        RPLAYER_LOG_ERROR("RAMS header too small (%u) for the command size given (%u)", static_cast<uint16_t>(m_ramsHeaderBuf.size()), m_currentCommandOffset);
        return false;
    }

    // Success
    return true;
}

void RamsHeader::revertCommand(const Command &command)
{
    // Undo the last call to getNextCommand()
    // This requires the command to be valid
    // Can only be called right after a successful call to getNextCommand()
    m_currentCommandIndex--;
    m_currentCommandOffset -= 2 + command.m_length;
}

uint32_t RamsHeader::getReceivedBytesCount() const
{
    return m_receivedBytesCount;
}

void RamsHeader::addReceivedBytesCount(int32_t bytes)
{
    m_receivedBytesCount += bytes;
}

uint32_t RamsHeader::getDecryptedBytesCount() const
{
    return m_processedBytesCount;
}

void RamsHeader::addDecryptedBytesCount(int32_t bytes)
{
    m_processedBytesCount += bytes;
}

void RamsHeader::setLabelCommand()
{
    m_hasLabelCommand = true;
}

bool RamsHeader::hasLabelCommand() const
{
    return m_hasLabelCommand;
}

void RamsHeader::setResetAsLastCommand()
{
    m_hasResetAsLastCommand = true;
}

bool RamsHeader::hasResetAsLastCommand() const
{
    return m_hasResetAsLastCommand;
}

void RamsHeader::setEndFlag()
{
    m_endFlag = true;
}

bool RamsHeader::isComplete() const
{
    return m_endFlag && (m_receivedBytesCount == m_processedBytesCount);
}

void RamsHeader::reset()
{
    m_ramsHeaderBuf.clear();

    m_ramsHeaderLength = 0;
    m_payloadId = 0;
    m_payloadType = 0;
    m_clockReference = 0;
    m_numOfCommands = 0;
    m_payloadUnitOffset = 0;
    m_payloadLength = 0;
    m_receivedBytesCount = 0;
    m_processedBytesCount = 0;
    m_hasLabelCommand = false;
    m_hasResetAsLastCommand = false;
    m_currentCommandIndex = 0;
    m_currentCommandOffset = 0;
    m_endFlag = false;
}

void RamsHeader::addRef()
{
    m_refCount++;
}

bool RamsHeader::decRef()
{
    assert(m_refCount > 0);
    return --m_refCount == 0;
}
