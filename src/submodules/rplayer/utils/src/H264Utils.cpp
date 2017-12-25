///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include <rplayer/utils/H264Utils.h>
#include <rplayer/utils/Logger.h>

#include <stddef.h>

using namespace rplayer;

inline uint32_t read_rbsp_uint32(const void *p)
{
    const uint8_t *_p = reinterpret_cast<const uint8_t*>(p);
    return (_p[0] << 24) | (_p[1] << 16) | (_p[2] << 8) | _p[3];
}

uint32_t countLeadingZeros(uint32_t pattern)
{
#ifdef __GNUC__
    return __builtin_clz(pattern);
#else
    uint32_t leadingZerosCount = 0;
    uint32_t mask = 1 << 31;
    while (!(pattern & mask) && mask) {
        leadingZerosCount++;
        mask = mask >> 1;
    }
    return leadingZerosCount;
#endif
}

//Annex B: Encoding

unsigned h264e_annex_b_escape(unsigned prefixLength, unsigned nal_ref_idc, unsigned nal_unit_type, const uint32_t *__restrict rbsp, unsigned bytes, void *__restrict escaped)
{
    const uint32_t *__restrict s = rbsp;
    unsigned i = 0, zeros = 0;
    uint32_t data = *s;
    uint8_t *d = (uint8_t*)escaped;

    /* Is prefix 4 bytes? */
    prefixLength = prefixLength > 3;

    /* Put NAL header. */
    *(reinterpret_cast<uint32_t*>(d)) = 0x010000 << (prefixLength << 3);
    d += 3 + prefixLength;
    *d++ = nal_ref_idc << 5 | nal_unit_type;

    while (i < bytes) {
        const uint8_t byte = (uint8_t)data;

        /* Escape 00 00 0x, where x <= 3. */
        if (zeros == 2 && byte <= 0x03) {
            *d++ = 0x03;
            zeros = 0;
        }

        /* Put byte. */
        *d++ = byte;

        /* Count zeros. */
        zeros += !byte;
        zeros *= !byte;

        /* Next byte. */
        ++i;
        if (!(i & 3)) {
            ++s;
            data = *s;
        } else {
            data >>= 8;
        }
    }

    return d - (uint8_t*)escaped;
}

//Annex B: Decoding

unsigned int h264d_annex_b_length(const uint8_t *__restrict escaped, unsigned bytes)
{
    const uint8_t *__restrict s = escaped, *e = escaped + bytes;
    uint32_t data = 0xffffffff;

    if (bytes <= 3) {
        RPLAYER_LOG_ERROR("incomplete NAL Unit header");
        return 0;
    }

    /* Skip first NAL Unit header. */
    s += 4;

    while (s < e) {
        data |= (uint32_t)*s;

        if ((data & 0x00ffffff) == 0x00000001) {
            s -= (2 + (data == 0x00000001));
            return (unsigned)(s - escaped);
        }

        data <<= 8;
        ++s;
    }

    return bytes;
}

unsigned h264d_annex_b_header(unsigned *prefixLength, unsigned *nal_ref_idc, unsigned *nal_unit_type, const uint8_t *__restrict escaped, unsigned bytes)
{
    uint32_t data;

    if (bytes < 4) {
        RPLAYER_LOG_ERROR("incomplete NAL Unit header");
        return 0;
    }

    data = read_rbsp_uint32(escaped);

    /* Parse startcode and NAL Unit header. */
    if (data == 0x00000001) {
        if (bytes < 5) {
            RPLAYER_LOG_ERROR("incomplete NAL Unit header");
            return 0;
        }

        *prefixLength = 4;
        *nal_ref_idc = (escaped[4] & 0x60) >> 5;
        *nal_unit_type = (escaped[4] & 0x1f);
    } else if ((data & 0xffffff00) == 0x00000100) {
        *prefixLength = 3;
        *nal_ref_idc = (data & 0x60) >> 5;
        *nal_unit_type = (data & 0x1f);
    } else {
        RPLAYER_LOG_ERROR("invalid or no NAL Unit startcode");
        return 0;
    }

    return *prefixLength + 1;
}

unsigned h264d_annex_b_unescape(const uint8_t *escaped, unsigned bytes, uint32_t *rbsp)
{
    /* Parse header and progress pointer to start of escaped RBSP. */
    unsigned i = 0, zeros = 0;
    const uint8_t *s = escaped, *e = escaped + bytes;
    uint32_t *d = rbsp;
    uint32_t data = 0;

    if (reinterpret_cast<size_t>(rbsp) & 3) {
        RPLAYER_LOG_ERROR("rbsp must be 32-bit aligned");
        return 0;
    }

    i = 0;
    while (s < e) {
        const uint32_t byte = (uint32_t)*s;
        ++s;

        /* 0x0000xx? */
        if (zeros == 2 && byte <= 0x03) {
            if (byte == 0x03) {
                zeros = 0;
                continue; /* skip */
            } else {
                /* 0x000000, 0x000001 or 0x000002 shall not occur at any byte-aligned position. */
                RPLAYER_LOG_ERROR("invalid 0x0000xx sequence in NAL Unit");
                return 0;
            }
        }

        /* Put byte. */
        data |= byte;

        /* Count zeros. */
        zeros += !byte;
        zeros *= !byte;

        /* Next byte. */
        ++i;
        if (!(i & 3)) {
            data = ((data & 0xFF000000) >> 24) | ((data & 0x00FF0000) >> 8) | ((data & 0x0000FF00) << 8) | ((data & 0x000000FF) << 24);
            *d = data;
            ++d;
            data = 0;
        } else {
            data <<= 8;
        }
    }

    zeros = i & 3;

    /* Store remaining data. */
    if (zeros) {
        data <<= ((3 - zeros) << 3);
        data = ((data & 0xFF000000) >> 24) | ((data & 0x00FF0000) >> 8) | ((data & 0x0000FF00) << 8) | ((data & 0x000000FF) << 24);
        *d = data;
    }
    return i;
}
