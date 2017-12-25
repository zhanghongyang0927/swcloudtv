///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

namespace rplayer {

enum StreamType {
    STREAM_TYPE_UNKNOWN,
    STREAM_TYPE_MPEG1_AUDIO,
    STREAM_TYPE_MPEG2_AUDIO,
    STREAM_TYPE_AAC_AUDIO,
    STREAM_TYPE_AC3_AUDIO,
    STREAM_TYPE_MPEG2_VIDEO,
    STREAM_TYPE_H264_VIDEO
};

} // namespace rplayer
