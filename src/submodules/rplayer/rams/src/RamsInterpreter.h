///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include "RamsClock.h"
#include "RamsHeaderPool.h"
#include "RamsOutput.h"
#include "RamsPacketHandler.h"
#include "RamsUnitStore.h"

#include <rplayer/IStreamDecrypt.h>
#include <rplayer/IPacketSink.h>

#include <inttypes.h>
#include <vector>
#include <list>

namespace rplayer
{
struct IStreamDecrypt;

class RamsInterpreter : public IPacketSink
{
public:
    RamsInterpreter(RamsUnitStore &ramsUnitStore);
    ~RamsInterpreter();

    // Reset the RamsInterpreter to its initial state
    void reset();

    // Main method to parse RAMS commands
    // This method doesn't assume that the packet is complete, but "data" can't
    // contain two RAMS packets.
    void parse(const uint8_t *data, uint32_t size, bool startFlag, bool endFlag);

    void setStreamDecryptEngine(IStreamDecrypt *streamDecryptEngine);

    void setTsPacketOutput(IPacketSinkWithMetaData *packetOut);

    void put(const uint8_t *data, uint32_t size);

    void setCurrentTime(uint16_t timeInMs);

    static const uint8_t COMMAND_RESET    = 0;
    static const uint8_t COMMAND_LABEL    = 1;
    static const uint8_t COMMAND_DELETE   = 2;
    static const uint8_t COMMAND_KEY_INFO = 3;
    static const uint8_t COMMAND_OUTPUT   = 4;

private: // Members
    static const int MAX_NUM_PACKET_HANDLERS = 16;

    enum ParserState
    {
        STATE_PARSING_HEADER,
        STATE_PARSING_PAYLOAD,
        STATE_PARSING_COMPLETE
    };

    bool m_isKeyInfoSet;

    // Variables to keep the state of the current packet
    ParserState m_parserState;
    RamsHeader *m_currentRamsHeader;

    std::list<RamsHeader *> m_ramsHeaderDecryptionList;

    IStreamDecrypt *m_streamDecryptEngine;

    RamsPacketHandler *m_ramsPacketHandlerArray[MAX_NUM_PACKET_HANDLERS];
    RamsPacketHandler *m_currentRamsPacketHandler;

    RamsUnitStore &m_ramsUnitStore;
    RamsHeaderPool m_ramsHeaderPool;
    RamsOutput m_ramsOutput;
    RamsClock m_ramsClock;

    void resetCurrentRamsParsingState();
    void cleanupStreamDecryption();
};

}
