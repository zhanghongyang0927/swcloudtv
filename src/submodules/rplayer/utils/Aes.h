///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <stdint.h>

namespace rplayer {

class Aes128
{
public:
    Aes128() :
        m_bytesDone(0),
        m_isKeySet(false),
        m_isIvSet(false)
    {
    }
    ~Aes128() {}

    // Set the key to encrypt or decrypt.
    // This MUST be called before any of the other methods.
    void setKey(const uint8_t *key);

    // Set the initialization vector (IV) for AES-CTR scrambling.
    void setIv(const uint8_t *iv);

    // AES-ECB encryption. Encrypt/decrypt a block of 16 bytes.
    // You need to set the key before the first call.
    void ECB_encrypt_block(uint8_t *inOut);
    void ECB_decrypt_block(uint8_t *inOut);

    // AES-CBC (Cipher Block Chaining) encyption. Encrypt/decrypt multiple blocks.
    // You need to set the key before the first call.
    void CBC_encrypt_buffer(uint8_t *inOut, uint32_t length, const uint8_t *iv);
    void CBC_decrypt_buffer(uint8_t *inOut, uint32_t length, const uint8_t *iv);

    // AES-CTR (Counter) encryption. Here, encryption and decryption are symmetrical.
    // You need to set the Initialization Vector (IV) and key before the first call.
    // The IV will update after each full block of data.
    // After partial blocks, the remaining of the block will be used next call.
    // Returns true if succeeded, false if failed because the key or IV was not set.
    bool CTR_scramble(uint8_t *inOut, uint32_t length);

private:
    void AddRoundKey(uint8_t *state, uint8_t round);

    static void SubBytes(uint8_t *state);
    static void InvSubBytes(uint8_t *state);
    static void XorWithIv(uint8_t *buf, const uint8_t *iv);

    // Key length in bytes [128 bit]
    static const int KEYLEN = 16;
    // The number of rounds in AES Cipher.
    static const int N_ROUNDS = 10;
    // The number of 32 bit words in a key.
    static const int N_WORDS_IN_KEY = KEYLEN / sizeof(uint32_t);

    uint8_t m_roundKey[(N_ROUNDS + 1) * KEYLEN];
    uint8_t m_iv[KEYLEN];
    uint8_t m_block[KEYLEN];
    uint8_t m_bytesDone;
    bool m_isKeySet;
    bool m_isIvSet;
};

} // namespace rplayer
