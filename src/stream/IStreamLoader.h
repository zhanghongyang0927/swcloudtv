///
/// \file IStreamLoader.h
///
/// \brief CloudTV Nano SDK Stream loader interface.
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <porting_layer/ResultCode.h>

#include <inttypes.h>
#include <string>

namespace ctvc {

struct IStream;

/// \brief Generic source for content streams i.e. various sources of streaming video.
///
/// Subclass the IStreamLoader to implement a stream loader that can resolve a media URI
/// and hand over a stream for further processing.
struct IStreamLoader
{
public:
    IStreamLoader()
    {
    }
    virtual ~IStreamLoader()
    {
    }

    /// \brief Called when streaming content should be opened (setup).
    /// \param [in] uri URI to open.
    /// \param [in] stream_sink Reference to stream sink to receive the data from the opened stream.
    /// \result ResultCode
    virtual ResultCode open_stream(const std::string &uri, IStream &stream_sink) = 0;

    /// \brief Called when the library wishes to stop the content.
    virtual void close_stream() = 0;
};

} // namespace
