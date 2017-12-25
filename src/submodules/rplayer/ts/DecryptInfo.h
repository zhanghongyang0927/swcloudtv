///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <inttypes.h>

namespace rplayer {

//
// This class contains decryption context information.
//
struct DecryptInfo
{
    // The constructor initializes all members to 0.
    // Copy and assignment operators are trivial.
    DecryptInfo();

    // AES-128 uses 128 bit (16 byte) keys
    uint8_t m_keyIdentifier[16];
    // Initialization vectors can be passed as 8 or 16 byte entities.
    // Internally, IVs are always 16 bytes; if passed as 8 byte values,
    // the LSBs (m_initializationVector[8..15]) are defined to be 0.
    uint8_t m_initializationVector[16];
    // The byte offset within a PES packet at which the next AU starts.
    // If 0, it concerns the next PES packet; if non-0, it concerns an AU within a PES packet
    uint32_t m_auByteOffset;
};

} // namespace rplayer
