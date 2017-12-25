///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <vector>

namespace rplayer
{

class RamsHeader;

class RamsHeaderPool
{
public:
    RamsHeaderPool();
    ~RamsHeaderPool();

    // Returns a reference to a free RamsHeader object
    RamsHeader *getRamsHeader();

    // Release a RamsHeader object to be used later
    void releaseRamsHeader(RamsHeader &ramsHeader);

private:
    std::vector<RamsHeader *> m_ramsHeaderPool;
};

}
