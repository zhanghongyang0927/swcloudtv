///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include "src/RamsInterpreter.h"
#include "src/RamsUnitStore.h"

#include <rplayer/IPacketSink.h>

#include <inttypes.h>

namespace rplayer {

struct IStreamDecrypt;
struct IRamsChunkAllocator;

class Rams : public IPacketSink
{
public:
    Rams();
    ~Rams();

    // Call this to reset the Rams interpreter to its initial state.
    void reset();

    // Call this to split Stream data into Rams or TS, could be one or more packets.
    void put(const uint8_t *data, uint32_t size);

    void setMetaData(const StreamMetaData &);

    // Registration of a TS output that will receive the RAMS-decoded or forwarded transport stream.
    void setTsPacketOutput(IPacketSinkWithMetaData *packetOut);

    // Registration of a stream decrypt engine
    void registerStreamDecryptEngine(IStreamDecrypt *streamDecryptEngine);

    // Registration of a RAMS chunk allocator
    void registerRamsChunkAllocator(IRamsChunkAllocator *ramsChunkAllocator);

    // Set current real time in ms. The time may (and will) wrap around. This is no problem.
    // It should be continuous, however, meaning that any difference in the real time should
    // equal the difference in the time passed.
    // The origin of the absolute value does not matter.
    // A real-time thread can/will call this on regular basis.
    // If used, this method must be called immediately prior to each call to put() for time
    // management to properly operate.
    void setCurrentTime(uint16_t timeInMs);

private:
    Rams(const Rams &);
    Rams &operator=(const Rams &);

    enum SplitterState
    {
        STATE_TS          = 0x00,
        STATE_RAMS        = 0x01,
        STATE_OUT_OF_SYNC = 0x02
    };

    uint32_t m_packetByteCount;
    uint32_t m_ramsPacketLength;
    SplitterState m_splitterState;
    IPacketSinkWithMetaData *m_packetOut;

    RamsUnitStore m_ramsUnitStore;
    RamsInterpreter m_ramsInterpreter;
};

} // namespace
