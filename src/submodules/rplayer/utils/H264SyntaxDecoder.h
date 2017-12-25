///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <rplayer/utils/BitReader.h>

#include <inttypes.h>
#include <vector>

namespace rplayer {

class H264SyntaxDecoder: public BitReader
{
public:
    explicit H264SyntaxDecoder(const uint8_t *data, uint32_t size, uint32_t bitIndex);
    explicit H264SyntaxDecoder(const std::vector<uint8_t> &data);

    void setData(const uint8_t *data, uint32_t size, uint32_t bitIndex);

    bool hasError() const
    {
        return m_isError;
    }
    void clearErrorFlag()
    {
        m_isError = false;
    }

    uint32_t u(uint32_t n);
    uint32_t ue();
    int32_t se();

    void u_skip(uint32_t n);
    void ue_skip();
    void se_skip();

private:
    bool m_isError;

    uint32_t codeNum();
};

} // namespace rplayer
