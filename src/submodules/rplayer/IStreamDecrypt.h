///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <inttypes.h>

namespace rplayer {

struct IPacketSink;

/// \brief This interface offers the functionality to decrypt a stream
/// with given key identifier and initialization vector.
struct IStreamDecrypt
{
    IStreamDecrypt() {}
    virtual ~IStreamDecrypt() {}

    // Set the stream return path
    // The decrypted stream should be returned using the interface that is
    // set here.
    // The interface can be removed by setting a null pointer (and should be
    // if the object receiving the stream is destroyed).
    // If no output interface is set, the decrypted data may be dropped.
    virtual void setStreamReturnPath(IPacketSink *streamOut) = 0;

    // Set the key identifier to use for decryption.
    // The license and key retrieval is left to the underlying DRM system.
    virtual void setKeyIdentifier(const uint8_t (&keyId)[16]) = 0;

    // Set the initialization vector to use for decryption.
    // 8 byte initialization vectors can be emulated by setting byte 8-15 to 0.
    // If no initialization vectors are used, this method doesn't need to be called.
    virtual void setInitializationVector(const uint8_t (&iv)[16]) = 0;

    // Decrypt the stream using given key identifier and initialization vector.
    // set_key_identifier() and set_initialization_vector() must/will have been called
    // at least once if the DRM scheme requires such.
    // Multiple calls to stream_data() will update the internal (stream-specific) state.
    // set_key_identifier() and set_initialization_vector() may or may not be called
    // between successive calls to stream_data(), as is defined by the stream. If called,
    // this will signal a new decrypt state.
    // The function returns true if decryption succeeded, and false if not.
    // Possible errors could be: failure to set key identifier or initialization vector,
    // uninitialized DRM system, absent or expired license and more.
    virtual bool streamData(const uint8_t *data, uint32_t length) = 0;
};

} // namespace
