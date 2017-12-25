///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <inttypes.h>

namespace rplayer {

//
// This interface offers the functionality to decrypt a stream
// with given key identifier and initialization vector.
//
struct IDecryptEngine
{
    IDecryptEngine() {}
    virtual ~IDecryptEngine() {}

    // Announce a key identifier that (probably) will be used soon to  decrypt
    // a stream. Such announcement enables the DRM system to check and fetch
    // the corresponding license and obtain the key.
    // Each key used with setKeyIdentifier() will have been announced at least
    // once by announceKeyIdentifier().
    // However, there is no guaranteed minimum time between the call to
    // announceKeyIdentifier() and setKeyIdentifier().
    virtual void announceKeyIdentifier(const uint8_t (&keyId)[16]) = 0;

    // Set the key identifier to use for decryption.
    // The key and license retrieval is left to the underlying DRM system.
    virtual void setKeyIdentifier(const uint8_t (&keyId)[16]) = 0;

    // Set the initialization vector to use for decryption.
    // 8 byte initialization vectors can be emulated by setting byte 8-15 to 0.
    virtual void setInitializationVector(const uint8_t (&iv)[16]) = 0;

    // Decrypt the stream in-place using given key identifier and initialization vector.
    // setKeyIdentifier() and setInitializationVector() must have been called at least once.
    // Multiple calls to decrypt() will update the internal (stream-specific) state.
    // setKeyIdentifier() and setInitializationVector() may or may not be called between
    // successive calls to decrypt(), as is defined by the stream.
    // The function returns true if decryption succeeded, and false if not.
    // Possible errors could be: failure to set key identifier or initialization vector,
    // uninitialized DRM system, absent or expired license and more, but to rplayer the only
    // interesting information is whether the call succeeded or not.
    virtual bool decrypt(uint8_t *data, uint32_t size) = 0;
};

//
// The IDecryptEngineFactory is registered with rplayer, bound to a specific DRM ID so it can
// call appropriate methods to create stream decryption instances when needed.
//
struct IDecryptEngineFactory
{
    IDecryptEngineFactory() {}
    virtual ~IDecryptEngineFactory() {}

    virtual const uint8_t *getDrmSystemId() = 0; // Must return the address of a 16-byte GUID.

    virtual IDecryptEngine *createDecryptEngine() = 0;
    virtual void destroyDecryptEngine(IDecryptEngine *) = 0;
};

} // namespace rplayer
