///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include <porting_layer/TimeStamp.h>
#include <porting_layer/Log.h>

#include <time.h>

using namespace ctvc;

TimeStamp TimeStamp::now()
{
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t)) {
        CTVC_LOG_ERROR("Can't obtain time");
    }

    TimeStamp tmp;

    tmp.m_time = static_cast<uint64_t>(t.tv_sec) * 1000000ULL + t.tv_nsec / 1000U;
    tmp.m_flags = IS_VALID | IS_ABSOLUTE;

    return tmp;
}
