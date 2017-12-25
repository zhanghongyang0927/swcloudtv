///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <rplayer/ts/TsCommon.h>

namespace rplayer {

class Frame;

struct IFillerFrameCreator
{
    virtual ~IFillerFrameCreator() {}

    virtual StreamType getStreamType() = 0;

    virtual void processIncomingFrame(Frame *frame) = 0;
    virtual Frame *create() = 0;
};

} // namespace
