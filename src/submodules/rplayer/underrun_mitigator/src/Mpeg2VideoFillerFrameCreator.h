///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include "IFillerFrameCreator.h"
#include <stdlib.h>

#include <rplayer/utils/BitWriter.h>
#include <rplayer/utils/BitReader.h>

namespace rplayer {

class Mpeg2VideoFillerFrameCreator : public IFillerFrameCreator
{
public:
    Mpeg2VideoFillerFrameCreator();
    ~Mpeg2VideoFillerFrameCreator();
    StreamType getStreamType()
    {
        return STREAM_TYPE_MPEG2_VIDEO;
    }

    void processIncomingFrame(Frame *frame);
    Frame *create();

private:
    Mpeg2VideoFillerFrameCreator(const Mpeg2VideoFillerFrameCreator&);
    Mpeg2VideoFillerFrameCreator &operator=(const Mpeg2VideoFillerFrameCreator &);

    void parseSequenceHeader(const uint8_t *data, size_t size);
    void patchTemporalReference(uint8_t *pData);
    void encodeFillerPictureHeader(BitWriter &out);
    void encodeFillerSlice(BitWriter &out, int mbY, int mbW);

    bool m_isValidSequenceHeader;
    bool m_isValidPictureHeader;

    int m_nextTemporalReference;
    int m_horizontal_size_value;
    int m_vertical_size_value;
};

} // namespace
