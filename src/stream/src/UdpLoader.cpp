/// \file UdpLoader.cpp
/// \brief Downloads a UDP stream
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include <stream/UdpLoader.h>

#include <porting_layer/Log.h>
#include <stream/IStream.h>
#include <utils/utils.h>

#include <string.h>

using namespace ctvc;

static const unsigned int UDP_DATA_BUFFER_SIZE = (256 * 1024);  // 256KB, see CTV-27938

UdpLoader::UdpLoader()
{
}

UdpLoader::~UdpLoader()
{
}

ResultCode UdpLoader::setup()
{
    m_socket.open();

    ResultCode ret = m_socket.set_receive_buffer_size(UDP_DATA_BUFFER_SIZE);
    if (ret.is_error()) {
        CTVC_LOG_ERROR("m_socket.set_receive_buffer_size() failed");
        return ret;
    }

    std::string dummy;
    std::string host;
    int port;
    url_split(m_uri, dummy, dummy, host, port, dummy);

    ret = m_socket.bind(host.c_str(), port);
    if (ret.is_error()) {
        CTVC_LOG_ERROR("m_socket.bind(%s,%d) failed", host.c_str(), port);
        return ret;
    }

    return ResultCode::SUCCESS;
}

void UdpLoader::teardown()
{
    m_socket.close();
}

bool UdpLoader::run()
{
    CTVC_LOG_DEBUG("");

    uint8_t buffer[UDP_DATA_BUFFER_SIZE];
    uint32_t recsize;
    ResultCode ret = m_socket.receive(buffer, UDP_DATA_BUFFER_SIZE, recsize);
    if (ret.is_error() || recsize == 0) {
        if (ret == Socket::THREAD_SHUTDOWN) {
            CTVC_LOG_DEBUG("Thread shutdown");
        } else if (ret.is_error()) {
            CTVC_LOG_ERROR("Receive error %s. url:%s", ret.get_description(), m_uri.c_str());
        } else {
            // Socket closed/timeout
            CTVC_LOG_DEBUG("Socket closed");
        }

        // Signal end-of-stream with result code.
        m_stream_sink->stream_error(ret);

        return true; // Exit thread
    } else {
        m_stream_sink->stream_data(buffer, recsize);
    }

    return false;
}
