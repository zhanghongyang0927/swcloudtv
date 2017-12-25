///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include "IFillerFrameCreator.h"
#include "Frame.h"

namespace rplayer {

class MpegAudioFillerFrameCreator: public IFillerFrameCreator
{
public:
    MpegAudioFillerFrameCreator(StreamType streamType) :
        m_streamType(streamType)
    {
    }

    StreamType getStreamType()
    {
        return m_streamType;
    }

    void processIncomingFrame(Frame *frame);
    Frame *create();

private:
    StreamType m_streamType;
    Frame m_silentAudioFrame;
};

}
