///
/// \file RfbtvMessage.h
///
/// \brief RFB-TV Message container
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <inttypes.h>

#include <string>
#include <vector>
#include <map>

namespace ctvc {

class RfbtvMessage
{
public:
    RfbtvMessage();
    ~RfbtvMessage();

    // Clear the entire message
    void clear();

    // Write fixed-sized integer primitives
    void write_uint8(uint8_t v);
    void write_uint16(uint16_t v);
    void write_uint32(uint32_t v);
    void write_uint64(uint64_t v);

    // Write raw binary data
    void write_raw(const uint8_t *data, uint32_t length);
    void write_raw(const std::vector<uint8_t> &data);

    // Write binary data preceded by a 32-bit size field
    void write_blob(const uint8_t *data, uint32_t length);
    void write_blob(const std::vector<uint8_t> &data);

    // Write a string preceded with a 16-bit length field
    void write_string(const char *str);
    void write_string(const std::string &str);

    // Write a key-value pair as two consecutive strings
    void write_key_value_pair(const char *key, const char *value);

    // Write a number of key-value pairs preceded by an 8-bit count field
    void write_key_value_pairs(const std::map<std::string, std::string> &key_value_pairs);

    // Read fixed-sized integer primitives
    uint8_t read_uint8();
    uint16_t read_uint16();
    uint32_t read_uint32();
    uint64_t read_uint64();

    // Read raw binary data
    std::string read_raw_as_string(uint32_t n);
    std::vector<uint8_t> read_raw_as_vector(uint32_t n);

    // Read binary data preceded by a 32-bit size field
    std::vector<uint8_t> read_blob();

    // Read a string preceded with a 16-bit length field
    std::string read_string();
    std::vector<uint8_t> read_string_as_vector();

    // Read a key-value list from the message
    // It first reads an 8-bit integer specifying the number of key value pairs and
    // subsequently reads all strings and returns them as a map of key value pairs.
    std::map<std::string, std::string> read_key_value_pairs();

    // Access to individual bytes; no bounds checking takes place, so 0 <= index < size()
    uint8_t &operator[](int index);

    // Access to size, raw data and bytes read until now
    uint32_t size() const;
    const uint8_t *data() const;
    uint32_t bytes_read() const;

    // Rewind the read pointer
    void rewind();

    // Discard all bytes read until now (implicitly rewinds the read pointer)
    void discard_bytes_read();

    // Underflow management
    // Underflow state will be reset by a call to clear(), rewind() or discard_bytes_read()
    // Remember that in underflow state a number of bytes may already have been read whereas others may not...
    bool has_data_underflow() const;

private:
    std::vector<uint8_t> m_message;
    uint32_t m_bytes_read;
    bool m_has_data_underflow;
};

} // namespace
