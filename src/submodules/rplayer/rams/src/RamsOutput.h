///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <rplayer/IPacketSink.h>
#include <rplayer/StreamMetaData.h>

#include <vector>
#include <list>

#include <inttypes.h>

namespace rplayer
{

class RamsUnitStore;

class RamsOutput : public IPacketSink
{
public:
    RamsOutput(RamsUnitStore &ramsUnitStore);
    ~RamsOutput();

    // Set the packet output
    void setTsPacketOutput(IPacketSinkWithMetaData *packetOut);

    // To immediately forward data
    void put(const uint8_t *data, uint32_t size);

    // Sets the metadata for all following calls until set again
    void setMetaData(const StreamMetaData &metaData);

    // Reset all scheduled output
    void reset();

    struct PatchAction
    {
        uint8_t m_patch[16];
        uint8_t m_byteCount;
        uint32_t m_offset;
    };

    struct OutputAction
    {
        uint16_t m_unitId;
        uint16_t m_clock;
        std::vector<PatchAction> m_patchList;
        StreamMetaData m_metaData;
    };

    void deleteSucceedingActions(uint16_t clock);
    void addOutputAction(const OutputAction &outputAction);
    void outputUnit(const OutputAction &outputAction);
    void outputAllUnitsUntil(uint16_t currentClock);

private:
    RamsOutput(const RamsOutput &);
    RamsOutput &operator=(const RamsOutput &);

    RamsUnitStore &m_ramsUnitStore;
    IPacketSinkWithMetaData *m_packetOut;
    std::list<OutputAction> m_outputActionList;
};

}
