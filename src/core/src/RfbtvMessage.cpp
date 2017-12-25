///
/// \file RfbtvMessage.cpp
///
/// \brief RFB-TV Message container
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include "RfbtvMessage.h"

#include <string.h>
#include <assert.h>

using namespace ctvc;

RfbtvMessage::RfbtvMessage() :
    m_bytes_read(0),
    m_has_data_underflow(false)
{
}

RfbtvMessage::~RfbtvMessage()
{
}

void RfbtvMessage::clear()
{
    m_message.clear();
    m_bytes_read = 0;
    m_has_data_underflow = false;
}

uint32_t RfbtvMessage::size() const
{
    return m_message.size();
}

const uint8_t *RfbtvMessage::data() const
{
    return &m_message[0];
}

uint32_t RfbtvMessage::bytes_read() const
{
    return m_bytes_read;
}

void RfbtvMessage::rewind()
{
    m_bytes_read = 0;
    m_has_data_underflow = false;
}

void RfbtvMessage::discard_bytes_read()
{
    m_message.erase(m_message.begin(), m_message.begin() + m_bytes_read);
    m_bytes_read = 0;
    m_has_data_underflow = false;
}

uint8_t &RfbtvMessage::operator[](int index)
{
    return m_message[index];
}

void RfbtvMessage::write_uint8(uint8_t v)
{
    m_message.push_back(v);
}

void RfbtvMessage::write_uint16(uint16_t v)
{
    m_message.push_back(v >> 8);
    m_message.push_back(v);
}

void RfbtvMessage::write_uint32(uint32_t v)
{
    m_message.push_back(v >> 24);
    m_message.push_back(v >> 16);
    m_message.push_back(v >> 8);
    m_message.push_back(v);
}

void RfbtvMessage::write_uint64(uint64_t v)
{
    m_message.push_back(v >> 56);
    m_message.push_back(v >> 48);
    m_message.push_back(v >> 40);
    m_message.push_back(v >> 32);
    m_message.push_back(v >> 24);
    m_message.push_back(v >> 16);
    m_message.push_back(v >> 8);
    m_message.push_back(v);
}

void RfbtvMessage::write_raw(const uint8_t *data, uint32_t length)
{
    assert(data || length == 0); // With zero-sized data, a null pointer is allowed
    m_message.insert(m_message.end(), data, data + length);
}

void RfbtvMessage::write_raw(const std::vector<uint8_t> &data)
{
    m_message.insert(m_message.end(), data.begin(), data.end());
}

void RfbtvMessage::write_blob(const uint8_t *data, uint32_t length)
{
    assert(data || length == 0); // With zero-sized data, a null pointer is allowed
    write_uint32(length);
    write_raw(data, length);
}

void RfbtvMessage::write_blob(const std::vector<uint8_t> &data)
{
    write_uint32(data.size());
    write_raw(data);
}

void RfbtvMessage::write_string(const char *str)
{
    assert(str);
    uint16_t length = strlen(str);
    write_uint16(length);

    const uint8_t *data = reinterpret_cast<const uint8_t *>(str);
    write_raw(data, length);
}

void RfbtvMessage::write_string(const std::string &str)
{
    write_uint16(str.size());
    write_raw(reinterpret_cast<const uint8_t *>(str.data()), str.size());
}

void RfbtvMessage::write_key_value_pair(const char *key, const char *value)
{
    assert(key);
    assert(value);
    write_string(key);
    write_string(value);
}

void RfbtvMessage::write_key_value_pairs(const std::map<std::string, std::string> &key_value_pairs)
{
    write_uint8(key_value_pairs.size());

    for (std::map<std::string, std::string>::const_iterator i = key_value_pairs.begin(); i != key_value_pairs.end(); ++i) {
        write_string(i->first);
        write_string(i->second);
    }
}

uint8_t RfbtvMessage::read_uint8()
{
    if (m_bytes_read + 1 > m_message.size()) {
        m_has_data_underflow = true;
        return 0;
    }

    return m_message[m_bytes_read++];
}

uint16_t RfbtvMessage::read_uint16()
{
    if (m_bytes_read + 2 > m_message.size()) {
        m_has_data_underflow = true;
        return 0;
    }

    uint16_t value = m_message[m_bytes_read] << 8;
    value |= m_message[m_bytes_read + 1];
    m_bytes_read += 2;
    return value;
}

uint32_t RfbtvMessage::read_uint32()
{
    if (m_bytes_read + 4 > m_message.size()) {
        m_has_data_underflow = true;
        return 0;
    }

    uint32_t value = m_message[m_bytes_read] << 24;
    value |= m_message[m_bytes_read + 1] << 16;
    value |= m_message[m_bytes_read + 2] << 8;
    value |= m_message[m_bytes_read + 3];
    m_bytes_read += 4;
    return value;
}

uint64_t RfbtvMessage::read_uint64()
{
    if (m_bytes_read + 8 > m_message.size()) {
        m_has_data_underflow = true;
        return 0;
    }

    uint64_t value = static_cast<uint64_t>(m_message[m_bytes_read]) << 56;
    value |= static_cast<uint64_t>(m_message[m_bytes_read + 1]) << 48;
    value |= static_cast<uint64_t>(m_message[m_bytes_read + 2]) << 40;
    value |= static_cast<uint64_t>(m_message[m_bytes_read + 3]) << 32;
    value |= static_cast<uint64_t>(m_message[m_bytes_read + 4]) << 24;
    value |= static_cast<uint64_t>(m_message[m_bytes_read + 5]) << 16;
    value |= static_cast<uint64_t>(m_message[m_bytes_read + 6]) << 8;
    value |= static_cast<uint64_t>(m_message[m_bytes_read + 7]);
    m_bytes_read += 8;
    return value;
}

std::string RfbtvMessage::read_raw_as_string(uint32_t length)
{
    if (m_bytes_read + length > m_message.size()) {
        m_has_data_underflow = true;
        return std::string();
    }

    std::string str(reinterpret_cast<const char *>(&m_message[m_bytes_read]), length);
    m_bytes_read += length;

    return str;
}

std::vector<uint8_t> RfbtvMessage::read_raw_as_vector(uint32_t length)
{
    if (m_bytes_read + length > m_message.size()) {
        m_has_data_underflow = true;
        return std::vector<uint8_t>();
    }

    std::vector<uint8_t> data(&m_message[m_bytes_read], &m_message[m_bytes_read + length]);
    m_bytes_read += length;

    return data;
}

std::vector<uint8_t> RfbtvMessage::read_blob()
{
    uint32_t length = read_uint32();
    return read_raw_as_vector(length);
}

std::string RfbtvMessage::read_string()
{
    uint16_t length = read_uint16();
    return read_raw_as_string(length);
}

std::vector<uint8_t> RfbtvMessage::read_string_as_vector()
{
    uint16_t length = read_uint16();
    return read_raw_as_vector(length);
}

std::map<std::string, std::string> RfbtvMessage::read_key_value_pairs()
{
    std::map<std::string, std::string> map;

    uint8_t nr_pairs = read_uint8();
    if (m_has_data_underflow) {
        return map;
    }

    for (uint8_t i = 0; i < nr_pairs; i++) {
        std::string key = read_string();
        if (m_has_data_underflow) {
            break;
        }
        map[key] = read_string();
        if (m_has_data_underflow) {
            break;
        }
    }

    return map;
}

bool RfbtvMessage::has_data_underflow() const
{
    return m_has_data_underflow;
}
