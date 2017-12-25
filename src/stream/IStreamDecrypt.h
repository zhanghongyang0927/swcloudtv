///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <inttypes.h>

namespace ctvc {

struct IStream;

/// \brief This interface offers the functionality to decrypt a stream
/// with given key identifier and initialization vector.
struct IStreamDecrypt
{
    IStreamDecrypt() {}
    virtual ~IStreamDecrypt() {}

    /// \brief Set the stream return path.
    /// \param [in] stream_out The return path that should be used, or 0.
    ///
    /// The decrypted stream should be returned using the interface that is
    /// set here.
    /// The interface can be removed by setting a null pointer (and should be
    /// if the object receiving the stream is destroyed).
    /// If no output interface is set, the decrypted data may be dropped.
    virtual void set_stream_return_path(IStream *stream_out) = 0;

    /// \brief Set the key identifier to use for decryption.
    /// \param [in] key_id The key identifier.
    ///
    /// The license and key retrieval is left to the underlying DRM system.
    virtual void set_key_identifier(const uint8_t (&key_id)[16]) = 0;

    /// \brief Set the initialization vector to use for decryption.
    /// \param [in] iv The initialization vector.
    ///
    /// 8 byte initialization vectors can be emulated by setting byte 8-15 to 0.
    /// If no initialization vectors are used, this method doesn't need to be called.
    virtual void set_initialization_vector(const uint8_t (&iv)[16]) = 0;

    /// \brief Decrypt the stream using given key identifier and initialization vector.
    /// \param [in] data Pointer to the data to be decrypted.
    /// \param [in] length The number of bytes to decrypt.
    /// \result bool Returns true on success, false on failure.
    ///
    /// set_key_identifier() and set_initialization_vector() must/will have been called
    /// at least once if the DRM scheme requires such.
    /// Multiple calls to stream_data() will update the internal (stream-specific) state.
    /// set_key_identifier() and set_initialization_vector() may or may not be called
    /// between successive calls to stream_data(), as is defined by the stream. If called,
    /// this will signal a new decrypt state.
    /// The function returns true if decryption succeeded, and false if not.
    /// Possible errors could be: failure to set key identifier or initialization vector,
    /// uninitialized DRM system, absent or expired license and more.
    ///
    /// \note This method can (will) be called with \a data=0 and \a length=0 at regular
    ///       intervals (typically every 20 milliseconds. This is done to drive specific
    ///       crypto engines on specific clients.
    virtual bool stream_data(const uint8_t *data, uint32_t length) = 0;
};

} // namespace
