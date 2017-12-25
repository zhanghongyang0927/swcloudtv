/// \file StreamForwarder.cpp
/// \brief Forwards a stream
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include <stream/StreamForwarder.h>

#include <porting_layer/Socket.h>
#include <porting_layer/Log.h>
#include <porting_layer/AutoLock.h>
#include <utils/utils.h>

#include <stdio.h>

using namespace ctvc;

const ResultCode StreamForwarder::INVALID_URL("Invalid URL");
const ResultCode StreamForwarder::CANNOT_CREATE_FILE("Cannot create file");

const uint32_t BUFFERING_THRESHOLD = 7 * 188; // Buffer at least 7 TS packets before forwarding.

StreamForwarder::StreamForwarder() :
    m_socket(0),
    m_file(0)
{
}

StreamForwarder::~StreamForwarder()
{
    close();
}

ResultCode StreamForwarder::open(const std::string &url)
{
    CTVC_LOG_DEBUG("url:%s", url.c_str());

    close();

    std::string proto, authorization, host, path;
    int port;
    url_split(url, proto, authorization, host, port, path);

    bool is_file = proto.compare("file") == 0;

    if (proto.empty() || (!is_file && (host.empty() || port <= 0))) {
        CTVC_LOG_ERROR("One or more illegal parameters");
        return INVALID_URL;
    }

    Socket *socket = 0;
    FILE *file = 0;
    if (is_file) {
        file = fopen(path.c_str(), "wb");
        if (!file) {
            CTVC_LOG_ERROR("Cannot create file:%s", path.c_str());
            return CANNOT_CREATE_FILE;
        }
    } else if (proto.compare("udp") == 0) {
        socket = new UdpSocket();
    } else {
        socket = new TcpSocket();
    }

    if (socket) {
        ResultCode ret = socket->connect(host.c_str(), port);
        if (ret.is_error()) {
            delete socket;
            return ret;
        }
    }

    AutoLock lck(m_mutex);
    m_socket = socket;
    m_file = file;

    return ResultCode::SUCCESS;
}

void StreamForwarder::close()
{
    AutoLock lck(m_mutex);

    // Flush any outstanding data
    stream_data(0, 0);

    if (m_socket) {
        m_socket->close();
        delete m_socket;
        m_socket = 0;
    }
    if (m_file) {
        fclose(m_file);
        m_file = 0;
    }
}

void StreamForwarder::stream_error(ResultCode result)
{
    CTVC_LOG_DEBUG("Error (%s) dropped", result.get_description());
}

void StreamForwarder::stream_data(const uint8_t *data, uint32_t length)
{
    AutoLock lck(m_mutex);

    if (m_file) {
        if (length > 0) {
            fwrite(data, 1, length, m_file);
        } else {
            fflush(m_file);
        }
    }
    if (m_socket) {
        if (m_buffer.empty() && length >= BUFFERING_THRESHOLD) {
            // Send right away if nothing is buffered and we have enough data to immediately send.
            m_socket->send(data, length);
        } else if (length > 0) {
            // Buffer the data
            m_buffer.insert(m_buffer.end(), data, data + length);
        }

        if ((length == 0 && !m_buffer.empty()) || m_buffer.size() >= BUFFERING_THRESHOLD) {
            // Flush buffer if triggered by length == 0 or if we have enough data to send.
            m_socket->send(&m_buffer[0], m_buffer.size());
            m_buffer.clear();
        }
    }
}
