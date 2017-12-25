/// \file utils.cpp
/// \brief Collection of utilities.
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include <utils/utils.h>

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <assert.h>
#include <ctype.h>

#include <string>

using namespace ctvc;

void ctvc::url_split(const std::string &url, std::string &proto/*out*/, std::string &authorization/*out*/, std::string &hostname/*out*/, int &port/*out*/, std::string &path/*out*/)
{
    proto = "";
    authorization = "";
    hostname = "";
    path = "";
    port = -1;

    // parse protocol
    const char *begin = url.c_str();
    const char *p = strchr(begin, ':');
    if (p) {
        proto.assign(begin, p);
        p++; // skip ':'
        if (*p == '/') {
            p++;
        }
        if (*p == '/') {
            p++;
        }
    } else {
        // no protocol means plain filename
        path = url;
        return;
    }

    // separate path from hostname
    const char *end_of_hostname = strchr(p, '/');
    if (!end_of_hostname) {
        end_of_hostname = strchr(p, '?');
    }
    if (end_of_hostname) {
        path.assign(end_of_hostname);
    } else {
        end_of_hostname = begin + url.size();
    }

    if (p == end_of_hostname) {
        return;
    }

    // the rest is hostname, use that to parse auth/port
    // authorization (user[:pass]@hostname)
    const char *at = strchr(p, '@');
    if (at && at < end_of_hostname) {
        authorization.assign(p, at);
        p = at + 1; // skip '@'
    }

    const char *brk = strchr(p, ']');
    const char *col = strchr(p, ':');
    if (*p == '[' && brk && brk < end_of_hostname) {
        // [host]:port
        hostname.assign(p + 1, brk);
        if (brk[1] == ':') {
            port = atoi(brk + 2);
        }
    } else if (col && col < end_of_hostname) {
        hostname.assign(p, col);
        port = atoi(col + 1);
    } else {
        hostname.assign(p, end_of_hostname);
    }
}

static char to_hex(char code)
{
    static const char *hex = "0123456789ABCDEF";
    return hex[code & 15];
}

std::string ctvc::url_encode(const char *src)
{
    std::string dst;

    while (*src) {
        if (isalnum(*src) || *src == '-' || *src == '_' || *src == '.' || *src == '~') {
            dst += *src;
        } else if (*src == ' ') {
            dst += '+';
        } else {
            dst += '%';
            dst += to_hex(*src >> 4);
            dst += to_hex(*src & 15);
        }
        src++;
    }

    return dst;
}

std::string ctvc::xmlencode(const char *src)
{
    std::string dst;

    while (*src) {
        switch (*src) {
        case '&':
            dst += "&amp;";
            break;
        case '\"':
            dst += "&quot;";
            break;
        case '\'':
            dst += "&apos;";
            break;
        case '<':
            dst += "&lt;";
            break;
        case '>':
            dst += "&gt;";
            break;
        default:
            dst += *src;
            break;
        }
        src++;
    }

    return dst;
}

// According to vsnprintf documentation, the va_list is modified when it's
// called so "ap1" cannot be reused. From C++11 onwards it is possible to use
// va_copy to duplicate ap1 and use it in the second vsnprintf. However, this
// is not possible with C++03, so passing "ap2" is a workaround.
static void append_print(std::string &s, const char *fmt, va_list ap1, va_list ap2)
{
    char buf[1024];

    size_t n = vsnprintf(buf, sizeof(buf), fmt, ap1);
    if (n < sizeof(buf)) {
        s.append(buf, n);
        return;
    }

    char *buf2 = new char[n + 1];
    size_t n2 = vsnprintf(buf2, n + 1, fmt, ap2);
    assert(n2 <= n);

    s.append(buf2, n);

    delete[] buf2;
}

void ctvc::string_printf(std::string &s, const char *fmt, ...)
{
    va_list ap1;
    va_list ap2;

    s = "";

    va_start(ap1, fmt);
    va_start(ap2, fmt);
    append_print(s, fmt, ap1, ap2);
    va_end(ap1);
    va_end(ap2);
}

void ctvc::string_printf_append(std::string &s, const char *fmt, ...)
{
    va_list ap1;
    va_list ap2;

    va_start(ap1, fmt);
    va_start(ap2, fmt);
    append_print(s, fmt, ap1, ap2);
    va_end(ap1);
    va_end(ap2);
}

std::string ctvc::uint64_to_string(uint64_t value)
{
    std::string return_string;

    uint32_t value_h = value / 1000000000U;
    uint32_t value_l = value % 1000000000U;
    if (value_h > 0) {
        string_printf(return_string, "%u%09u", value_h, value_l);
    } else {
        string_printf(return_string, "%u", value_l);
    }

    return return_string;
}

std::string ctvc::hex_dump(const void *data, size_t size)
{
    std::string dump;
    const size_t N = 16;
    const char *addr_format = size <= 0x10000ULL ? "%04X:" : size <= 0x1000000ULL ? "%06X:" : "%08X:";

    if (!data && size) {
        return std::string("(null)");
    }

    for (size_t i = 0; i < size; i += N) {
        const uint8_t *p = reinterpret_cast<const uint8_t *>(data) + i;

        string_printf_append(dump, addr_format, i);
        for (size_t j = 0; j < N; j++) {
            if (j + i < size) {
                string_printf_append(dump, " %02X", p[j]);
            } else {
                dump += "   ";
            }
        }
        dump += " |";
        for (size_t j = 0; j < N; j++) {
            if (j + i < size) {
                string_printf_append(dump, "%c", isprint(p[j]) ? p[j] : '.');
            } else {
                dump += " ";
            }
        }
        dump += "|\n";
    }

    return dump;
}

std::vector<std::string> ctvc::parse_character_separated_list(const std::string &list, char sep)
{
    std::vector<std::string> values;

    size_t start = 0;
    size_t end = 0;
    while ((end = list.find(sep, start)) != std::string::npos) {
        values.push_back(list.substr(start, end - start));
        start = end + 1;
    }
    values.push_back(list.substr(start));
    return values;
}

void ctvc::parse_guid_formatted_string(const std::string &string, uint8_t (&id)[16]/*out*/)
{
    // Initialize to all-0 in case we have an illegal or unrecognized format
    memset(id, 0, sizeof(id));

    const char *p = string.c_str();
    // Convert 32 nibbles
    for (size_t i = 0; i < 2 * sizeof(id);) {
        char c = *p++;
        if (c == '\0') {
            break;
        }
        if (c == '-') {
            continue;
        }
        int value;
        if (c >= '0' && c <= '9') {
            value = c - '0';
        } else if (c >= 'A' && c <= 'F') {
            value = c - 'A' + 10;
        } else if (c >= 'a' && c <= 'f') {
            value = c - 'a' + 10;
        } else {
            break;
        }
        if ((i & 1) == 0) {
            id[i >> 1] = value << 4;
        } else {
            id[i >> 1] |= value;
        }
        i++;
    }
}

std::string ctvc::id_to_guid_string(const uint8_t (&id)[16])
{
    // Convert 16 bytes into a 36-character string plus null terminator.
    char tmp[37], *s = tmp;
    for (unsigned i = 0; i < sizeof(id); i++) {
        // Add a dash where applicable
        if (i == 4 || i == 6 || i == 8 || i == 10) {
            *s++ = '-';
        }
        *s++ = to_hex(id[i] >> 4);
        *s++ = to_hex(id[i] & 15);
    }
    *s++ = '\0';
    assert(s == tmp + sizeof(tmp));

    // Convert to std::string upon return (constructing the string on-the-fly may lead to many string allocations).
    return tmp;
}

int ctvc::strcasecmp(const char *s1, const char *s2)
{
    if (!s1 || !s2) {
        return s1 ? 1 : s2 ? -1 : 0;
    }

    while (true) {
        int c = tolower(static_cast<unsigned char>(*s1)) - tolower(static_cast<unsigned char>(*s2));
        if (c) {
            return c;
        }
        if (*s1 == '\0') {
            return 0;
        }
        s1++;
        s2++;
    }
}

int ctvc::strncasecmp(const char *s1, const char *s2, size_t n)
{
    if (!s1 || !s2) {
        return s1 ? 1 : s2 ? -1 : 0;
    }

    while (n > 0) {
        int c = tolower(static_cast<unsigned char>(*s1)) - tolower(static_cast<unsigned char>(*s2));
        if (c) {
            return c;
        }
        if (*s1 == '\0') {
            return 0;
        }
        s1++;
        s2++;
        n--;
    }

    return 0;
}

uint32_t ctvc::atox(const char *s)
{
    uint32_t value = 0;
    while (isspace(*s)) {
        s++;
    }
    while (isxdigit(*s)) {
        char c = *s++;
        value <<= 4;
        value += (c >= 'a') ? c - 'a' + 10 : (c >= 'A') ? c - 'A' + 10 : c - '0';
    }
    return value;
}
