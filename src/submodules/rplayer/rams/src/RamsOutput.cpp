///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include "RamsOutput.h"
#include "RamsUnit.h"
#include "RamsUnitStore.h"

#include <rplayer/utils/Logger.h>

using namespace rplayer;

RamsOutput::RamsOutput(RamsUnitStore &ramsUnitStore) :
    m_ramsUnitStore(ramsUnitStore),
    m_packetOut(0)
{
}

RamsOutput::~RamsOutput()
{
}

void RamsOutput::setTsPacketOutput(IPacketSinkWithMetaData *packetOut)
{
    m_packetOut = packetOut;
}

void RamsOutput::put(const uint8_t *data, uint32_t size)
{
    if (m_packetOut) {
        m_packetOut->put(data, size);
    }
}

void RamsOutput::setMetaData(const StreamMetaData &metaData)
{
    if (m_packetOut) {
        m_packetOut->setMetaData(metaData);
    }
}

void RamsOutput::reset()
{
    m_outputActionList.clear();
}

void RamsOutput::deleteSucceedingActions(uint16_t clock)
{
    // Find and remove all output actions that are scheduled later than or equal to the scheduled time (clock value) of given output action
    // Starting from the end saves us traversing unneeded entries.
    if (!m_outputActionList.empty()) {
        uint16_t firstClock = m_outputActionList.front().m_clock;
        while (!m_outputActionList.empty() && m_outputActionList.back().m_clock - firstClock >= clock - firstClock) {
            m_outputActionList.pop_back();
        }
    }
}

void RamsOutput::addOutputAction(const OutputAction &outputAction)
{
    // Append the new action
    // It should be later than all others, but this is taken care by a call to deleteSucceedingActions()
    m_outputActionList.push_back(outputAction);
}

void RamsOutput::outputAllUnitsUntil(uint16_t currentClock)
{
    // Output and remove all elements that are scheduled up to the current clock
    for (std::list<OutputAction>::iterator it = m_outputActionList.begin(); it != m_outputActionList.end(); it = m_outputActionList.erase(it)) {
        if (static_cast<int16_t>(it->m_clock - currentClock) > 0) { // This is not fully correct since it doesn't allow scheduling more than half the range ahead. We'll need the previous clock to allow that.
            break;
        }

        outputUnit(*it);
    }
}

void RamsOutput::outputUnit(const OutputAction &outputAction)
{
    RamsUnit *ramsUnit = m_ramsUnitStore.getUnit(outputAction.m_unitId);
    if (!ramsUnit) {
        RPLAYER_LOG_WARNING("RAMS unit not found (id=%d)", outputAction.m_unitId);
        return;
    }

    for (std::vector<PatchAction>::const_iterator patch = outputAction.m_patchList.begin(); patch != outputAction.m_patchList.end(); ++patch) {
        ramsUnit->applyPatch(patch->m_offset, patch->m_patch, patch->m_byteCount);
    }

    if (m_packetOut) {
        bool isFirst = true;
        const uint8_t *data = 0;
        uint32_t size = 0;

        m_packetOut->setMetaData(outputAction.m_metaData);

        while ((data = ramsUnit->getDataSegment(isFirst, size)) != 0) {
            isFirst = false;
            m_packetOut->put(data, size);
        }
    }
}
