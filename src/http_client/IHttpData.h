/// \file IHttpData.h
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

#include <string>

#include <inttypes.h>

namespace ctvc {

///This is a simple interface for HTTP data storage (impl examples are Key/Value Pairs, File, etc...)
class IHttpDataSource
{
public:
    virtual ~IHttpDataSource()
    {
    }

    /** Reset stream to its beginning
     * Called by the HttpClient on each new request
     */
    virtual void reset_read()
    {
    }

    /** Read a piece of data to be transmitted
     * @param[out] buf Pointer to the buffer on which to copy the data
     * @param[in] len Length of the buffer
     * @returns The actual copied data length
     */
    virtual uint32_t read(char *buf, uint32_t len) = 0;

    /** Get MIME type
     * @param[out] type Internet media type from Content-Type header
     */
    virtual std::string get_data_type() const = 0;

    /** Determine whether the HTTP client should chunk the data
     *  Used for Transfer-Encoding header
     */
    virtual bool get_is_chunked() const = 0;

    /** If the data is not chunked, get its size
     *  Used for Content-Length header
     */
    virtual uint32_t get_data_len() const = 0;
};

///This is a simple interface for HTTP data storage (impl examples are Key/Value Pairs, File, etc...)
class IHttpDataSink
{
public:
    virtual ~IHttpDataSink()
    {
    }

    /** Reset stream to its beginning
     * Called by the HttpClient on each new request
     */
    virtual void reset_write()
    {
    }

    /** Write a piece of data transmitted by the server
     * @param buf Pointer to the buffer from which to copy the data
     * @param len Length of the buffer
     */
    virtual void write(const char *buf, uint32_t len) = 0;

    /** Set MIME type
     * @param type Internet media type from Content-Type header
     */
    virtual void set_data_type(const char * /*type*/)
    {
    }

    /** Determine whether the data is chunked
     *  Recovered from Transfer-Encoding header
     */
    virtual void set_is_chunked(bool /*is_chunked*/)
    {
    }

    /** If the data is not chunked, set its size
     * From Content-Length header
     */
    virtual void set_data_len(uint32_t /*len*/)
    {
    }
};

} // namespace
