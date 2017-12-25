///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include "RamsUnitStore.h"

#include <vector>

#include <inttypes.h>

namespace rplayer
{

class RamsHeader;
class RamsOutput;
class StreamMetaData;

class RamsPacketHandler
{
public:
    RamsPacketHandler(RamsUnitStore &ramsUnitStore, RamsOutput &ramsOutput);
    ~RamsPacketHandler();

    // Payload can point to the beginning of the payload or to a fragment of it
    void processPayload(RamsHeader &ramsHeader, const uint8_t *payload, uint32_t size, bool end, const StreamMetaData &metaData);

private:
    enum State {
        INITIAL, PROCESSING_COMMANDS, FINISHED
    };
    State m_state;

    struct Label
    {
        uint16_t m_unitId;
        uint16_t m_byteCount;
    };
    std::vector<Label> m_labelList;
    uint32_t m_labelIndex;

    RamsUnitStore &m_ramsUnitStore;
    RamsOutput &m_ramsOutput;

    // This function is only called with payload that doesn't require
    // extra-processing, i.e. decryption
    void process(RamsHeader &ramsHeader, const uint8_t *payload, uint32_t size, bool end);
    void parseLabelData(uint8_t payloadUnitOffset, const uint8_t *data, uint8_t size);
    uint8_t mapPatchActionToBytes(uint8_t action);
};

}
