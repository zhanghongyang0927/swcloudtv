/// \file HttpClient.h
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

#pragma once

#include <porting_layer/Socket.h>
#include <porting_layer/ResultCode.h>

#include <string>

#include <inttypes.h>

namespace ctvc {

class IHttpDataSink;
class IHttpDataSource;

/**A simple HTTP Client
 The HttpClient is composed of:
 - The actual client (HttpClient)
 - Classes that act as a data repository, each of which deriving from the HttpData class (HttpText for short text content, HttpFile for file I/O, HttpMap for key/value pairs, and HttpStream for streaming purposes)
 */
class HttpClient
{
public:
    static const ResultCode UNRECOGNIZED_PROTOCOL;     ///< Protocol (e.g. http://) not recognized
    static const ResultCode PROTOCOL_ERROR;            ///< Encountered some HTTP protocol violation
    static const ResultCode CONNECTION_CLOSED;         ///< Connection was closed by peer
    static const ResultCode EXCEEDED_MAX_REDIRECTIONS; ///< The maximum number of redirections have been exceeded

    /// Instantiate the HTTP client
    HttpClient();
    ~HttpClient();

    static const int HTTP_CLIENT_DEFAULT_TIMEOUT = 15000;

    /**
     Provides a basic authorization feature (Base64 encoded username and password)
     Pass two NULL pointers to switch back to no authentication
     @param[in] user username to use for authorization
     @param[in] user password to use for authorization
     */
    void set_basic_authorization_credentials(const char *user, const char *password);

    /**
     Set custom headers for request.

     Pass NULL, 0 to turn off custom headers.

     @code
     const char * hdrs[] =
     {
     "Connection", "keep-alive",
     "Accept", "text/html",
     "User-Agent", "Mozilla/5.0 (Windows NT 6.1; WOW64)",
     "Accept-Encoding", "gzip,deflate,sdch",
     "Accept-Language", "en-US,en;q=0.8",
     };

     http.set_basic_authorization_credentials("username", "password");
     http.set_custom_headers(hdrs, 5);
     @endcode

     @param[in] headers an array (size multiple of two) key-value pairs, must remain valid during the whole HTTP session
     @param[in] pairs number of key-value pairs
     */
    void set_custom_headers(const char * const *headers, uint32_t pairs);

    // High Level setup functions
    /** Execute a GET request on the URL
     Blocks until completion
     Unless there was an error, receive() MUST be called to complete the transaction and close the connection.
     @param[in] url : url on which to execute the request
     @param[in] timeout waiting timeout in ms
     @return ResultCode
     */
    ResultCode get(const char *url, int timeout = HTTP_CLIENT_DEFAULT_TIMEOUT); // Blocking

    /** Execute a GET request on the URL and receive all data.
     Blocks until completion.
     This would be the 'normal' use of the HttpClient
     However, the get() would block until all data has been received. For some purposes (mainly
     stream reception) it is more desirable to separate the connection and header parsing from
     the retrieval of the data.
     @param[in] url : url on which to execute the request
     @param[in] timeout waiting timeout in ms
     @return ResultCode
     */
    ResultCode get(const char *url, IHttpDataSink *data_sink, int timeout = HTTP_CLIENT_DEFAULT_TIMEOUT);

    /** Execute a POST request on the URL.
     Blocks until completion.
     Unless there was an error, receive() MUST be called to complete the transaction and close the connection.
     @param[in] url : url on which to execute the request
     @param[in] data_source : a IHttpDataSource instance that contains the data that will be posted
     @param[in] timeout waiting timeout in ms
     @return ResultCode
     */
    ResultCode post(const char *url, IHttpDataSource &data_source, int timeout = HTTP_CLIENT_DEFAULT_TIMEOUT); // Blocking

    /** Execute a PUT request on the URL
     Blocks until completion
     Unless there was an error, receive() MUST be called to complete the transaction and close the connection.
     @param[in] url : url on which to execute the request
     @param[in] data_source : a IHttpDataSource instance that contains the data that will be put
     @param[in] timeout waiting timeout in ms
     @return ResultCode
     */
    ResultCode put(const char *url, IHttpDataSource &data_source, int timeout = HTTP_CLIENT_DEFAULT_TIMEOUT); // Blocking

    /** Execute a DELETE request on the URL
     Blocks until completion
     Unless there was an error, receive() MUST be called to complete the transaction and close the connection.
     @param[in] url : url on which to execute the request
     @param[in] timeout waiting timeout in ms
     @return ResultCode
     */
    ResultCode del(const char *url, int timeout = HTTP_CLIENT_DEFAULT_TIMEOUT); // Blocking

    /** Receive any data from the connection that was setup by one of the get(), post(), put() or del() requests
     Blocks until completion
     @param[in] data_sink : pointer to an IHttpDataSink instance that will collect the data returned by the request, can be NULL
     @return ResultCode
     */
    ResultCode receive(IHttpDataSink *data_sink); // Execute reception

    /** Get last request's HTTP response code
     @return The HTTP response code of the last request
     */
    int get_response_code();

    /** Set the maximum number of automated redirections
     @param[in] i is the number of redirections.
     */
    void set_max_redirections(unsigned int i);

private:
    HttpClient(const HttpClient &);
    HttpClient &operator=(const HttpClient &);

    ResultCode connect(const char *url, const char *method, IHttpDataSource *data_source, int timeout); // Execute request
    ResultCode send_headers(const char *method, const std::string &path, const std::string &hostname, int port, const std::string &authorization, IHttpDataSource *data_source);
    ResultCode send_data(IHttpDataSource *data_source);
    ResultCode receive_headers(std::string &redirect_location/*out*/);
    ResultCode receive_chunked_data(IHttpDataSink *data_sink);
    ResultCode receive_data(uint32_t content_length, IHttpDataSink *data_sink);
    ResultCode recv();
    ResultCode send(const char *buf, uint32_t len = 0);

    void read_data(uint32_t n);
    ResultCode read_crlf();
    ResultCode find_line(uint32_t &line_length/*out*/);

    TcpSocket m_socket;

    int m_timeout;
    int m_response_code;
    bool m_is_chunked_data;
    uint32_t m_content_length;
    std::string m_data_type;

    std::string m_basic_authorization;
    const char * const *m_custom_headers;
    uint32_t m_num_custom_headers;
    int m_max_redirections;

    char * const m_rx_buf;
    char * const m_rx_buf_end;
    char *m_rx_data;
    uint32_t m_rx_data_len;
};

} // namespace
