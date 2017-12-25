///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include <string>
#include <assert.h>

using namespace std;

string &string::assign(const char *s, size_t n)
{
    assert(s || n == 0);
    assert(s != m_data || m_data == 0);
    if (n >= m_allocated_size) {
        delete[] m_data;
        m_data = new char[n + 1];
        m_allocated_size = n + 1;
    }
    memcpy(m_data, s, n);
    m_size = n;
    m_data[m_size] = '\0';
    return *this;
}

string &string::append(const char *s, size_t n)
{
    assert(s || n == 0);
    assert(s != m_data || m_data == 0);
    if (m_size + n >= m_allocated_size) {
        char *p = new char[m_size + n + 1];
        memcpy(p, m_data, m_size);
        delete[] m_data;
        m_data = p;
        m_allocated_size = m_size + n + 1;
    }
    memcpy(m_data + m_size, s, n);
    m_size += n;
    m_data[m_size] = '\0';
    return *this;
}

size_t string::find(const string &s, size_t pos) const
{
    for (size_t i = pos; i + s.m_size <= m_size; i++) {
        bool found = true;
        for (size_t j = 0; j < s.m_size; j++) {
            if (s.m_data[j] != m_data[i + j]) {
                found = false;
                break;
            }
        }
        if (found) {
            return i;
        }
    }
    return npos;
}

int string::compare(const string &s) const
{
    for (size_t i = 0; i < m_size && i < s.m_size; i++) {
        int diff = m_data[i] - s.m_data[i];
        if (diff != 0) {
            return diff;
        }
    }
    if (m_size == s.m_size) {
        return 0;
    }
    if (m_size < s.m_size) {
        return -1;
    } else {
        return 1;
    }
}

void string::resize(size_t count, char c)
{
    if (count == 0) {
        delete[] m_data;
        m_data = 0;
        m_size = 0;
        m_allocated_size = 0;
        return;
    }

    if (count >= m_allocated_size) {
        char *p = new char[count + 1];
        memcpy(p, m_data, m_size);
        delete[] m_data;
        m_data = p;
        m_allocated_size = count + 1;
    }

    if (count > m_size) {
        memset(m_data + m_size, c, count - m_size);
    }

    m_size = count;
    m_data[m_size] = '\0';
}
