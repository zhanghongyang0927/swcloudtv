///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include "IFillerFrameCreator.h"
#include "Frame.h"

namespace rplayer {

class AacFillerFrameCreator: public IFillerFrameCreator
{
public:
    StreamType getStreamType()
    {
        return STREAM_TYPE_AAC_AUDIO;
    }

    void processIncomingFrame(Frame *frame);
    Frame *create();

private:
    Frame m_silentAudioFrame;
};

}
