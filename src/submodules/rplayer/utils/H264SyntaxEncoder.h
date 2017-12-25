///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <rplayer/utils/BitWriter.h>

#include <inttypes.h>
#include <vector>

namespace rplayer {

class H264SyntaxEncoder: public BitWriter
{
public:

    explicit H264SyntaxEncoder(uint8_t *data, uint32_t size);
    void setData(uint8_t *data, uint32_t size);

    bool hasError() const
    {
        return m_isError;
    }
    void clearErrorFlag()
    {
        m_isError = false;
    }

    void u(uint32_t value, unsigned int size);
    void ue(uint16_t value);
    void se(int16_t value);

private:
    uint32_t m_bufferSize;
    bool m_isError;

    bool isSpaceFor(uint32_t bits);
};

} // namespace rplayer
