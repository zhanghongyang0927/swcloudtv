///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <rplayer/IDecryptEngine.h>

namespace rplayer {

class SoftwareDecryptEngineBase : public IDecryptEngine
{
public:
    SoftwareDecryptEngineBase();
    virtual ~SoftwareDecryptEngineBase();

    // A key identifier is announced; the default implementation is empty
    virtual void announceKeyIdentifier(const uint8_t (&)[16])
    {
    }

    // A new key identifier is set; the underlying system must retrieve the
    // corresponding key and call setKey() in order to apply this key.
    virtual void setKeyIdentifier(const uint8_t (&keyId)[16]) = 0;

    // Implemented by this class
    void setInitializationVector(const uint8_t (&iv)[16]);
    bool decrypt(uint8_t *data, uint32_t size);

protected:
    // Called by the derived implementation in response to a call to setKeyIdentifier().
    virtual void setKey(const uint8_t (&keyId)[16]);

private:
    class Impl;
    Impl &m_impl;
};

} // namespace rplayer
