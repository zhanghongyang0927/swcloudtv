///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include "IStream.h"

#include <porting_layer/ResultCode.h>
#include <porting_layer/Mutex.h>

#include <string>
#include <vector>

#include <stdio.h>
#include <inttypes.h>

namespace ctvc {

class Socket;

/// \brief Helper class to forward a stream to a specified destination.
class StreamForwarder : public IStream
{
public:
    static const ResultCode INVALID_URL; ///< An invalid URL was passed to open()
    static const ResultCode CANNOT_CREATE_FILE; ///< Cannot create file

    StreamForwarder();
    virtual ~StreamForwarder();

    /// \brief Open a socket to forward a stream to.
    ///
    /// \param[in] url Url like "udp://127.0.0.1:9990" or "file:///home/test/grab.ts"
    ///
    /// \result error code.
    ResultCode open(const std::string &url);

    /// \brief Close the socket.
    void close();

    // Implements IStream
    virtual void stream_data(const uint8_t *data, uint32_t length);
    virtual void stream_error(ResultCode result);

private:
    Socket *m_socket;
    FILE *m_file;
    Mutex m_mutex;
    std::vector<uint8_t> m_buffer;
};

} // namespace
