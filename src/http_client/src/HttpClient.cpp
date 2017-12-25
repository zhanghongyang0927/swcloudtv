/// \file HttpClient.cpp
/* Copyright (C) 2012 mbed.org, MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 * and associated documentation files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge, publish, distribute,
 * sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include <http_client/HttpClient.h>
#include <http_client/IHttpData.h>

#include <porting_layer/Log.h>
#include <porting_layer/Thread.h>
#include <utils/utils.h>
#include <utils/base64.h>

#include <algorithm>

#include <string.h>
#include <stdio.h>
#include <stdint.h>

static const unsigned int CHUNK_SIZE = 4096;
static const unsigned int READ_BUF_SIZE = 4096;

using namespace ctvc;

const ResultCode HttpClient::UNRECOGNIZED_PROTOCOL("Protocol (e.g. http://) not recognized");
const ResultCode HttpClient::PROTOCOL_ERROR("Encountered some HTTP protocol violation");
const ResultCode HttpClient::CONNECTION_CLOSED("Connection was closed by peer");
const ResultCode HttpClient::EXCEEDED_MAX_REDIRECTIONS("The maximum number of redirections have been exceeded");

HttpClient::HttpClient() :
    m_socket(),
    m_timeout(0),
    m_response_code(0),
    m_is_chunked_data(false),
    m_content_length(0),
    m_num_custom_headers(0),
    m_max_redirections(10),
    m_rx_buf(new char[READ_BUF_SIZE]),
    m_rx_buf_end(m_rx_buf + READ_BUF_SIZE),
    m_rx_data(m_rx_buf),
    m_rx_data_len(0)
{
}

HttpClient::~HttpClient()
{
    delete[] m_rx_buf;
}

void HttpClient::set_basic_authorization_credentials(const char *user, const char *password)
{
    m_basic_authorization = "";
    if (user) {
        m_basic_authorization = user;
        m_basic_authorization += ":";
        if (password) {
            m_basic_authorization += password;
        }
    }
}

void HttpClient::set_custom_headers(const char * const *headers, uint32_t pairs)
{
    m_custom_headers = headers;
    m_num_custom_headers = pairs;
}

ResultCode HttpClient::get(const char *url, int timeout /*= HTTP_CLIENT_DEFAULT_TIMEOUT*/)
{
    return connect(url, "GET", NULL, timeout);
}

ResultCode HttpClient::get(const char *url, IHttpDataSink *data_sink, int timeout /*= HTTP_CLIENT_DEFAULT_TIMEOUT*/)
{
    ResultCode ret = get(url, timeout);
    if (ret.is_error()) {
        return ret;
    }
    return receive(data_sink);
}

ResultCode HttpClient::post(const char *url, IHttpDataSource &data_source, int timeout /*= HTTP_CLIENT_DEFAULT_TIMEOUT*/)
{
    return connect(url, "POST", &data_source, timeout);
}

ResultCode HttpClient::put(const char *url, IHttpDataSource &data_source, int timeout /*= HTTP_CLIENT_DEFAULT_TIMEOUT*/)
{
    return connect(url, "PUT", &data_source, timeout);
}

ResultCode HttpClient::del(const char *url, int timeout /*= HTTP_CLIENT_DEFAULT_TIMEOUT*/)
{
    return connect(url, "DELETE", NULL, timeout);
}

int HttpClient::get_response_code()
{
    return m_response_code;
}

void HttpClient::set_max_redirections(unsigned int i)
{
    m_max_redirections = i;
}

ResultCode HttpClient::connect(const char *url, const char *method, IHttpDataSource *data_source, int timeout)
{
    CTVC_LOG_INFO("connect(%s %s timeout=%d)", method, url, timeout);

    m_timeout = timeout;
    m_response_code = 0; // Invalidate code
    m_is_chunked_data = false;
    m_content_length = 0;
    m_data_type = "";

    std::string redirect_location;

    for (int n_redirections_left = m_max_redirections; n_redirections_left >= 0; --n_redirections_left) {
        CTVC_LOG_DEBUG("parse: [%s]", url);

        // First we need to parse the url (http[s]://host[:port][/[path]]) -- HttpS not supported (yet?)
        std::string protocol;
        std::string authorization;
        std::string hostname;
        int port;
        std::string path;
        url_split(url, protocol, authorization, hostname, port, path);

        CTVC_LOG_DEBUG("Scheme:%s, Host:%s, Port:%d, Path:%s", protocol.c_str(), hostname.c_str(), port, path.c_str());

        if (port < 0) { // If not specified in the URL
            // Assign default port according to the protocol
            // Protocol is not checked if the port is given since we'll always do HTTP here...
            if (protocol == "http") {
                port = 80;
            } else {
                return UNRECOGNIZED_PROTOCOL;
            }
        }

        // Empty received data
        m_rx_data = m_rx_buf;
        m_rx_data_len = 0;

        // Connect
        CTVC_LOG_DEBUG("Connecting socket to server");
        ResultCode ret = m_socket.connect(hostname.c_str(), port);
        if (ret.is_error()) {
            CTVC_LOG_ERROR("Unable to connect: %s", ret.get_description());
            m_socket.close();
            return ret;
        }

        // Send request line and headers
        ret = send_headers(method, path, hostname, port, authorization, data_source);
        if (ret.is_error()) {
            m_socket.close();
            return ret;
        }

        // Send data (if available)
        if (data_source) {
            ret = send_data(data_source);
            if (ret.is_error()) {
                m_socket.close();
                return ret;
            }
        }

        // Receive response
        CTVC_LOG_DEBUG("Receiving response");
        ret = receive_headers(redirect_location);
        if (ret.is_error()) {
            m_socket.close();
            return ret;
        }

        if (!redirect_location.empty()) {
            if (n_redirections_left <= 0) {
                CTVC_LOG_ERROR("Exceeded max number of redirections:%d", m_max_redirections);
                return EXCEEDED_MAX_REDIRECTIONS;
            }
            url = redirect_location.c_str();
            CTVC_LOG_INFO("Following redirect[%d] to [%s]", m_max_redirections - n_redirections_left + 1, url);
            m_socket.close();
        } else {
            break;
        }
    }

    CTVC_LOG_DEBUG("Done receiving response header");

    return ResultCode::SUCCESS;
}

ResultCode HttpClient::receive(IHttpDataSink *data_sink)
{
    if (data_sink) {
        data_sink->reset_write();
        data_sink->set_data_len(m_content_length);
        data_sink->set_is_chunked(m_is_chunked_data);
        data_sink->set_data_type(m_data_type.c_str());
    }

    // Receive data
    CTVC_LOG_DEBUG("Receiving data");
    ResultCode ret;
    if (m_is_chunked_data) {
        ret = receive_chunked_data(data_sink);
    } else {
        ret = receive_data(m_content_length, data_sink);
    }
    if (ret.is_error()) {
        m_socket.close();
        return ret;
    }

    m_socket.close();
    CTVC_LOG_DEBUG("Completed HTTP transaction");

    return ResultCode::SUCCESS;
}

ResultCode HttpClient::send_headers(const char *method, const std::string &path, const std::string &hostname, int port, const std::string &authorization, IHttpDataSource *data_source)
{
    std::string tmp;

    // Create request
    string_printf(tmp, "%s %s HTTP/1.1\r\nHost: %s:%d\r\n", method, path.c_str(), hostname.c_str(), port);

    // Create authorization
    if (!authorization.empty() || !m_basic_authorization.empty()) {
        std::string auth = base64_encode(!authorization.empty() ? authorization : m_basic_authorization);
        tmp += "Authorization: Basic ";
        tmp += auth;
        tmp += "\r\n";
        CTVC_LOG_DEBUG("Authorization (%s) => (%s)", m_basic_authorization.c_str(), auth.c_str());
    }

    // Create custom headers
    for (uint32_t i = 0; i < m_num_custom_headers; i++) {
        CTVC_LOG_DEBUG("hdr[%2u] %s: %s", i, m_custom_headers[2 * i], m_custom_headers[2 * i + 1]);
        tmp += m_custom_headers[2 * i];
        tmp += ": ";
        tmp += m_custom_headers[2 * i + 1];
        tmp += "\r\n";
    }

    // Create default headers
    if (data_source) {
        if (data_source->get_is_chunked()) {
            tmp += "Transfer-Encoding: chunked\r\n";
        } else {
            string_printf_append(tmp, "Content-Length: %u\r\n", data_source->get_data_len());
        }
        std::string type = data_source->get_data_type();
        if (!type.empty()) {
            string_printf_append(tmp, "Content-Type: %s\r\n", type.c_str());
        }
    }

    // Close headers
    tmp += "\r\n";

    CTVC_LOG_DEBUG("Sending request {%s}", tmp.c_str());

    return send(tmp.c_str(), tmp.size());
}

ResultCode HttpClient::send_data(IHttpDataSource *data_source)
{
    CTVC_LOG_DEBUG("Sending data");

    data_source->reset_read();

    char buf[CHUNK_SIZE];
    uint32_t transfer_len;

    if (data_source->get_is_chunked()) {
        while (true) {
            transfer_len = data_source->read(buf, sizeof(buf));

            // Write chunk header
            char chunk_header[16];
            snprintf(chunk_header, sizeof(chunk_header), "%X\r\n", transfer_len); // In hex encoding
            ResultCode ret = send(chunk_header);
            if (ret.is_error()) {
                return ret;
            }

            if (transfer_len != 0) {
                ResultCode ret = send(buf, transfer_len);
                if (ret.is_error()) {
                    return ret;
                }
            }

            ret = send("\r\n"); // Chunk-terminating CRLF
            if (ret.is_error()) {
                return ret;
            }

            if (transfer_len == 0) { // Done
                break;
            }
        }
    } else {
        for (uint32_t written_len = 0; written_len < data_source->get_data_len(); written_len += transfer_len) {
            transfer_len = data_source->read(buf, sizeof(buf));
            if (transfer_len == 0) {
                CTVC_LOG_ERROR("Premature termination of sent data");
                return PROTOCOL_ERROR; // We cannot send the data we say we would send, so it's a protocol error
            }

            ResultCode ret = send(buf, transfer_len);
            if (ret.is_error()) {
                return ret;
            }
        }
    }

    return ResultCode::SUCCESS;
}

void HttpClient::read_data(uint32_t n)
{
    if (n < m_rx_data_len) {
        m_rx_data += n;
        m_rx_data_len -= n;
    } else {
        m_rx_data = m_rx_buf;
        m_rx_data_len = 0;
    }
}

ResultCode HttpClient::read_crlf()
{
    // There must be at least 2 bytes in the buffer
    while (m_rx_data_len < 2) {
        ResultCode ret = recv();
        if (ret.is_error()) {
            return ret;
        }
    }

    // Check CRLF
    if ((m_rx_data[0] != '\r') || (m_rx_data[1] != '\n')) {
        CTVC_LOG_ERROR("Format error");
        return PROTOCOL_ERROR;
    }

    // Skip CRLF
    read_data(2);

    return ResultCode::SUCCESS;
}

ResultCode HttpClient::find_line(uint32_t &line_length/*out*/)
{
    for (uint32_t n = 0; ; n++) {
        while (m_rx_data_len < n + 2) { // Read at least enough for a CRLF
            ResultCode ret = recv();
            if (ret.is_error()) {
                return ret;
            }
        }

        if (m_rx_data[n] == '\r' && m_rx_data[n + 1] == '\n') {
            m_rx_data[n] = '\0'; // Terminate line to enable sscanf() by caller
            line_length = n + 2; // Include CRLF in line length
            return ResultCode::SUCCESS;
        }
    }
}

ResultCode HttpClient::receive_headers(std::string &redirect_location/*out*/)
{
    m_is_chunked_data = false;
    m_content_length = 0;
    m_data_type = "";
    redirect_location = "";

    uint32_t line_length = 0;
    ResultCode ret = find_line(line_length);
    if (ret.is_error()) {
        return ret;
    }

    CTVC_LOG_DEBUG("Received %u chars; Line: [%s], line_length=%u", m_rx_data_len, m_rx_data, line_length);

    // Parse HTTP response
    if (sscanf(m_rx_data, "HTTP/%*d.%*d %d %*[^\r\n]", &m_response_code) != 1) {
        // Cannot match string, error
        CTVC_LOG_ERROR("Not a correct HTTP answer: {%s}", m_rx_data);
        return PROTOCOL_ERROR;
    }

    read_data(line_length);

    if ((m_response_code < 200) || (m_response_code >= 400)) {
        CTVC_LOG_WARNING("Response code %d", m_response_code);
        return PROTOCOL_ERROR;
    }

    CTVC_LOG_DEBUG("Reading headers");

    // Now get headers
    while (true) {
        ret = find_line(line_length);
        if (ret.is_error()) {
            return ret;
        }

        CTVC_LOG_DEBUG("Received %u chars; Line: [%s], line_length=%u", m_rx_data_len, m_rx_data, line_length);

        if (line_length == 2) { // End of headers
            CTVC_LOG_DEBUG("Headers read");
            read_data(line_length);
            return ResultCode::SUCCESS;
        }

        char key[32];
        char value[121];

        key[31] = '\0';
        value[120] = '\0';

        int n = sscanf(m_rx_data, "%31[^:]: %120[^\r\n]", key, value);
        if (n == 2) {
            CTVC_LOG_DEBUG("Read header: %s: %s", key, value);
            if (!ctvc::strcasecmp(key, "Content-Length")) {
                sscanf(value, "%u", &m_content_length);
            } else if (!ctvc::strcasecmp(key, "Transfer-Encoding")) {
                if (!ctvc::strcasecmp(value, "Chunked")) {
                    m_is_chunked_data = true;
                }
            } else if (!ctvc::strcasecmp(key, "Content-Type")) {
                m_data_type = value;
            } else if (!ctvc::strcasecmp(key, "Location")) {
                redirect_location = value;
                return ResultCode::SUCCESS; // Exit to follow the redirect
            }
            read_data(line_length);
        } else {
            CTVC_LOG_ERROR("Could not parse header");
            return PROTOCOL_ERROR;
        }
    }
}

ResultCode HttpClient::receive_chunked_data(IHttpDataSink *data_sink)
{
    while (true) {
        // Read chunk header
        uint32_t line_length = 0;
        ResultCode ret = find_line(line_length);
        if (ret.is_error()) {
            return ret;
        }

        // Similar to sscanf or strtol(s, 0, 16) but much faster, which saves around 1% of overall CPU.
        uint32_t chunk_len = atox(m_rx_data);

        read_data(line_length);

        if (chunk_len == 0) {
            // Last chunk
            return ResultCode::SUCCESS;
        }

        ret = receive_data(chunk_len, data_sink);
        if (ret.is_error()) {
            return ret;
        }

        ret = read_crlf();
        if (ret.is_error()) {
            return ret;
        }
    }
}

ResultCode HttpClient::receive_data(uint32_t content_length, IHttpDataSink *data_sink)
{
    while (content_length > 0) {
        uint32_t n = std::min(m_rx_data_len, content_length);

        if (data_sink) {
            data_sink->write(m_rx_data, n);
        }

        read_data(n);

        content_length -= n;

        if (content_length > 0) {
            ResultCode ret = recv();
            if (ret.is_error()) {
                return ret;
            }
        }
    }

    return ResultCode::SUCCESS;
}

ResultCode HttpClient::recv()
{
    // Move the data from the end of the buffer it there is not enough left to read a minimal amount of data
    // (This part can only be unit tested with the current tests if m_rx_buf size == 120)
    if (m_rx_data != m_rx_buf) {
        if (m_rx_buf_end - m_rx_data < 16 || m_rx_data_len < 2 || m_rx_data + m_rx_data_len == m_rx_buf_end) {
            memmove(m_rx_buf, m_rx_data, m_rx_data_len);
            m_rx_data = m_rx_buf;
        }
    }

    if (m_rx_data + m_rx_data_len >= m_rx_buf_end) {
        CTVC_LOG_ERROR("Received data too big for buffer (%ld)", static_cast<long>(m_rx_buf_end - m_rx_buf));
        return PROTOCOL_ERROR;
    }

    uint32_t read_len = 0;
    ResultCode ret = m_socket.receive(reinterpret_cast<uint8_t *>(m_rx_data + m_rx_data_len), m_rx_buf_end - (m_rx_data + m_rx_data_len), read_len); // TODO: (CNP-2069) Make timeout operational
    m_rx_data_len += read_len;
    if (ret.is_ok() && read_len == 0) {
        CTVC_LOG_WARNING("Connection was closed by server");
        return CONNECTION_CLOSED;
    } else if (ret == Socket::THREAD_SHUTDOWN) {
        CTVC_LOG_INFO("Connection to be closed by us");
    } else if (ret.is_error()) {
        CTVC_LOG_ERROR("Connection error: %s", ret.get_description());
    }

    return ret;
}

ResultCode HttpClient::send(const char *buf, uint32_t len)
{
    if (len == 0) {
        len = strlen(buf);
    }

    CTVC_LOG_DEBUG("Sending %u bytes", len);

    ResultCode ret = m_socket.send(reinterpret_cast<const uint8_t *>(buf), len); // TODO: (CNP-2069) Make timeout operational
    if (ret.is_error()) {
        CTVC_LOG_ERROR("Connection error: %s", ret.get_description());
    }
    return ret;
}
