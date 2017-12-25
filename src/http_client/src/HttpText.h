/// \file HttpText.h
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

#include <http_client/IHttpData.h>

#include <string>

namespace ctvc {

/** A data endpoint to store text
 */
class HttpText : public IHttpDataSink, public IHttpDataSource
{
public:
    /** Create an HttpText instance for input
     */
    HttpText();

    /** Create an HttpText instance for output
     * @param[in] str String to be transmitted
     */
    HttpText(const char *str);

    void clear();

    void set_data(const char *s);
    void append_data(const char *s);

    const std::string &get_data() const;
    uint32_t get_data_len() const;

    void set_is_chunked(bool is_chunked);
    bool get_is_chunked() const;

    void set_data_type(const char *type);
    std::string get_data_type() const;

protected:
    // IHttpDataSink
    virtual void reset_read();
    virtual uint32_t read(char *buf, uint32_t len);

    // IHttpDataSource
    virtual void reset_write();
    virtual void write(const char *buf, uint32_t len);

private:
    std::string m_string;
    uint32_t m_pos;
    bool m_is_chunked;
    std::string m_data_type;
};

} // namespace
