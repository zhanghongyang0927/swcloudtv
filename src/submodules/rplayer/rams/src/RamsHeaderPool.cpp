///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include "RamsHeaderPool.h"
#include "RamsHeader.h"

using namespace rplayer;

RamsHeaderPool::RamsHeaderPool()
{
}

RamsHeaderPool::~RamsHeaderPool()
{
    while (!m_ramsHeaderPool.empty()) {
        delete m_ramsHeaderPool.back();
        m_ramsHeaderPool.pop_back();
    }
}

RamsHeader *RamsHeaderPool::getRamsHeader()
{
    RamsHeader *ramsHeader = 0;

    if (m_ramsHeaderPool.empty()) {
        ramsHeader = new RamsHeader();
    } else {
        ramsHeader = m_ramsHeaderPool.back();
        m_ramsHeaderPool.pop_back();
    }

    ramsHeader->addRef();

    return ramsHeader;
}

void RamsHeaderPool::releaseRamsHeader(RamsHeader &ramsHeader)
{
    if (ramsHeader.decRef()) {
        ramsHeader.reset();
        m_ramsHeaderPool.push_back(&ramsHeader);
    }
}
