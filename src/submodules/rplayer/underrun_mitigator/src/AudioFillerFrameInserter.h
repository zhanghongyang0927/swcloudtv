///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include "UnderrunAlgorithmBase.h"
#include "Frame.h"

namespace rplayer {

struct IFillerFrameCreator;

class AudioFillerFrameInserter: public UnderrunAlgorithmBase
{
public:
    AudioFillerFrameInserter(StreamBuffer &source, const UnderrunAlgorithmParams &params, ICallback &callback);
    ~AudioFillerFrameInserter();

protected:
    // Get (and possibly modify) next frame from input (if present) or create new frame (if necessary and possible).
    Frame *getNextFrame(TimeStamp pcr);

private:
    AudioFillerFrameInserter(const AudioFillerFrameInserter &);
    AudioFillerFrameInserter &operator=(const AudioFillerFrameInserter &);

    void processNewFrame(Frame *frame);
    Frame *generateFillerFrame();

    Frame m_lastAudioFrame;
    uint32_t m_repeatCount;

    TimeStamp m_delay;

    IFillerFrameCreator *m_fillerFrameCreator;
};

} // namespace
