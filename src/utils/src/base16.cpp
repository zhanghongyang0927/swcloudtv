///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include <utils/base16.h>

#include <porting_layer/Log.h>

#include <ctype.h>
#include <assert.h>
#include <stddef.h>

namespace {

int hex_digit(char c)
{
    static char hex_digits[] = "0123456789ABCDEF";
    char upcased_c = toupper(c);
    for (size_t i = 0; i < sizeof(hex_digits) / sizeof(*hex_digits); ++i) {
        if (upcased_c == hex_digits[i]) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

} // namespace

std::vector<unsigned char> ctvc::base16_decode(std::string const &encoded_string)
{
    std::vector<unsigned char> ret;

    if ((encoded_string.size() % 2) != 0) {
        goto error;
    }

    for (size_t pos = 0; pos < encoded_string.size(); pos += 2) {
        int high_nib = hex_digit(encoded_string[pos]);
        int low_nib = hex_digit(encoded_string[pos + 1]);

        if ((high_nib < 0) || (low_nib < 0)) {
            goto error;
        }
        assert(high_nib < 16);
        assert(low_nib < 16);
        ret.push_back((high_nib << 4) | low_nib);
    }
    return ret;

error:
    CTVC_LOG_ERROR("invalid base16 string: '%s'", encoded_string.c_str());
    return std::vector<unsigned char>();
}
