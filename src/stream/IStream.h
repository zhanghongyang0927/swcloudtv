///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <porting_layer/ResultCode.h>

#include <inttypes.h>

namespace ctvc {

/// \brief Abstract interface class for passing stream data.
struct IStream
{
    IStream() {}
    virtual ~IStream() {}

    /// \brief Callback to send the stream buffer back to the caller.
    /// \param[in] data Stream data.
    /// \param[in] length Length of the stream data.
    ///
    /// \result void
    virtual void stream_data(const uint8_t *data, uint32_t length) = 0;

    /// \brief Callback to indicate an error downloading the stream.
    /// \param[in] result Result code indicating any problem. If ResultCode::SUCCESS,
    ///            the stream is terminated without an error and no further calls to
    ///            stream_data() are expected
    ///
    /// \result void
    virtual void stream_error(ResultCode result) = 0;
};

} // namespace
