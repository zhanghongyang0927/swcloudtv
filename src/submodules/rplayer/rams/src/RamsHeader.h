///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <vector>

#include <inttypes.h>

namespace rplayer
{

class RamsHeader
{
public:
    RamsHeader();
    ~RamsHeader() { }

    // Add bytes into the object until a valid RamsHeader is found. It updates
    // the input pointer and its size, and returns true when the header is
    // available
    bool addBytes(const uint8_t *&data /*in/out*/, uint32_t &size /*in/out*/);

    uint8_t getPayloadId() const;
    uint8_t getPayloadType() const;
    uint16_t getClockReference() const;
    uint8_t getNumOfCommands() const;
    uint8_t getPayloadUnitOffset() const;
    uint16_t getPayloadLength() const;

    // Access to the commands
    struct Command
    {
        uint8_t m_code;
        uint8_t m_length;
        const uint8_t *m_data;
    };

    // Reset the command pointer
    void firstCommand();
    // Get the next command. Returns true if available.
    bool getNextCommand(Command &command/*out*/);
    // Undo the last call to getNextCommand()
    // This requires the command to be valid
    // Can only be called right after a successful call to getNextCommand()
    void revertCommand(const Command &command);

    // Count of bytes for the payload
    uint32_t getReceivedBytesCount() const;
    void addReceivedBytesCount(int32_t bytes);

    // Bytes that have been decrypted
    uint32_t getDecryptedBytesCount() const;
    void addDecryptedBytesCount(int32_t bytes);

    void setLabelCommand();
    bool hasLabelCommand() const;

    void setResetAsLastCommand();
    bool hasResetAsLastCommand() const;

    void setEndFlag();

    // There are still bytes to be decrypted
    bool isComplete() const;

    // Refcount management
    void addRef();
    bool decRef(); // Returns true if refcount == 0

private:
    friend class RamsHeaderPool;

    // This vector stores a copy of the header. Its size is 7 + 1023
    std::vector<uint8_t> m_ramsHeaderBuf;

    // Total size of the RAMS header, i.e. field header_length + 7
    uint16_t m_ramsHeaderLength;

    // Id of the payload
    uint8_t m_payloadId;

    // Payload type
    uint8_t m_payloadType;

    uint16_t m_clockReference;
    uint8_t m_numOfCommands;
    uint8_t m_payloadUnitOffset;
    uint16_t m_payloadLength;

    // This is only used when decrypting data
    uint32_t m_receivedBytesCount;
    uint32_t m_processedBytesCount;

    bool m_hasLabelCommand;
    bool m_hasResetAsLastCommand;
    uint8_t m_currentCommandIndex;
    uint16_t m_currentCommandOffset;

    // The endFlag has been received for this packet
    bool m_endFlag;

    unsigned m_refCount;

    void reset();
};

}
