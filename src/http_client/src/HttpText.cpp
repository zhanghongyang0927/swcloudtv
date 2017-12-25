/// \file HttpText.cpp
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

#include "HttpText.h"

#include <algorithm>

#include <string.h>
#include <stdint.h>

using namespace ctvc;

HttpText::HttpText() :
    m_pos(0),
    m_is_chunked(false),
    m_data_type("text/plain")
{
}

HttpText::HttpText(const char *str) :
    m_string(str),
    m_pos(0),
    m_is_chunked(false),
    m_data_type("text/plain")
{
}

void HttpText::clear()
{
    m_string = "";
    m_pos = 0;
}

void HttpText::set_data(const char *s)
{
    m_string = s;
}

void HttpText::append_data(const char *s)
{
    m_string += s;
}

const std::string &HttpText::get_data() const
{
    return m_string;
}

uint32_t HttpText::get_data_len() const
{
    return m_string.size();
}

void HttpText::set_is_chunked(bool is_chunked)
{
    m_is_chunked = is_chunked;
}

bool HttpText::get_is_chunked() const
{
    return m_is_chunked;
}

void HttpText::set_data_type(const char *type)
{
    m_data_type = type;
}

std::string HttpText::get_data_type() const
{
    return m_data_type;
}

void HttpText::reset_read()
{
    m_pos = 0;
}

uint32_t HttpText::read(char *buf, uint32_t len)
{
    uint32_t n_read = std::min(len, static_cast<uint32_t>(m_string.size()) - m_pos);
    memcpy(buf, m_string.c_str() + m_pos, n_read);
    m_pos += n_read;
    return n_read;
}

void HttpText::reset_write()
{
    clear();
}

void HttpText::write(const char *buf, uint32_t len)
{
    m_string.append(buf, len);
}
