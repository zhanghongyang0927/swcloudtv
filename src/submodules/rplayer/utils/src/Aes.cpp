///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

//
// This code was based upon the code found in https://github.com/kokke/tiny-AES128-C
// License: public domain, http://unlicense.org/
//
// This is an implementation of the AES128 algorithm, specifically ECB, CBC and CTR mode.
// The implementation is verified against the test vectors in:
// National Institute of Standards and Technology Special Publication 800-38A 2001 ED
// ECB-AES128
//

#include <rplayer/utils/Aes.h>

#include <algorithm>

#include <stdint.h>
#include <assert.h>
#include <string.h>

using namespace rplayer;


static const uint8_t s_sbox[256] = {
    //0     1    2      3     4    5     6     7      8    9     A      B    C     D     E     F
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
};

static const uint8_t s_rsbox[256] = {
    0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
    0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
    0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
    0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
    0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
    0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
    0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
    0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02, 0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
    0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
    0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
    0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89, 0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
    0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
    0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
    0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
    0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
    0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d
};

// The round constant word array, s_rcon[i], contains the values given by
// x to th e power (i-1) being powers of x (x is denoted as {02}) in the field GF(2^8)
// Note that i starts at 1, not 0).
static const uint8_t s_rcon[255] = {
    0x8d, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36, 0x6c, 0xd8, 0xab, 0x4d, 0x9a,
    0x2f, 0x5e, 0xbc, 0x63, 0xc6, 0x97, 0x35, 0x6a, 0xd4, 0xb3, 0x7d, 0xfa, 0xef, 0xc5, 0x91, 0x39,
    0x72, 0xe4, 0xd3, 0xbd, 0x61, 0xc2, 0x9f, 0x25, 0x4a, 0x94, 0x33, 0x66, 0xcc, 0x83, 0x1d, 0x3a,
    0x74, 0xe8, 0xcb, 0x8d, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36, 0x6c, 0xd8,
    0xab, 0x4d, 0x9a, 0x2f, 0x5e, 0xbc, 0x63, 0xc6, 0x97, 0x35, 0x6a, 0xd4, 0xb3, 0x7d, 0xfa, 0xef,
    0xc5, 0x91, 0x39, 0x72, 0xe4, 0xd3, 0xbd, 0x61, 0xc2, 0x9f, 0x25, 0x4a, 0x94, 0x33, 0x66, 0xcc,
    0x83, 0x1d, 0x3a, 0x74, 0xe8, 0xcb, 0x8d, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b,
    0x36, 0x6c, 0xd8, 0xab, 0x4d, 0x9a, 0x2f, 0x5e, 0xbc, 0x63, 0xc6, 0x97, 0x35, 0x6a, 0xd4, 0xb3,
    0x7d, 0xfa, 0xef, 0xc5, 0x91, 0x39, 0x72, 0xe4, 0xd3, 0xbd, 0x61, 0xc2, 0x9f, 0x25, 0x4a, 0x94,
    0x33, 0x66, 0xcc, 0x83, 0x1d, 0x3a, 0x74, 0xe8, 0xcb, 0x8d, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20,
    0x40, 0x80, 0x1b, 0x36, 0x6c, 0xd8, 0xab, 0x4d, 0x9a, 0x2f, 0x5e, 0xbc, 0x63, 0xc6, 0x97, 0x35,
    0x6a, 0xd4, 0xb3, 0x7d, 0xfa, 0xef, 0xc5, 0x91, 0x39, 0x72, 0xe4, 0xd3, 0xbd, 0x61, 0xc2, 0x9f,
    0x25, 0x4a, 0x94, 0x33, 0x66, 0xcc, 0x83, 0x1d, 0x3a, 0x74, 0xe8, 0xcb, 0x8d, 0x01, 0x02, 0x04,
    0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36, 0x6c, 0xd8, 0xab, 0x4d, 0x9a, 0x2f, 0x5e, 0xbc, 0x63,
    0xc6, 0x97, 0x35, 0x6a, 0xd4, 0xb3, 0x7d, 0xfa, 0xef, 0xc5, 0x91, 0x39, 0x72, 0xe4, 0xd3, 0xbd,
    0x61, 0xc2, 0x9f, 0x25, 0x4a, 0x94, 0x33, 0x66, 0xcc, 0x83, 0x1d, 0x3a, 0x74, 0xe8, 0xcb
};


// This function produces 4(N_ROUNDS+1) round keys. The round keys are used in each round to decrypt the states.
void Aes128::setKey(const uint8_t *key)
{
    uint8_t tempa[4]; // Used for the column/row operations

    // The first round key is the key itself.
    for (int i = 0; i < KEYLEN; ++i) {
        m_roundKey[i] = key[i];
    }

    // All other round keys are found from the previous round keys.
    for (int i = N_WORDS_IN_KEY; (i < (4 * (N_ROUNDS + 1))); ++i) {
        tempa[0] = m_roundKey[(i - 1) * 4 + 0];
        tempa[1] = m_roundKey[(i - 1) * 4 + 1];
        tempa[2] = m_roundKey[(i - 1) * 4 + 2];
        tempa[3] = m_roundKey[(i - 1) * 4 + 3];

        if (i % N_WORDS_IN_KEY == 0) {
            // This function rotates the 4 bytes in a word to the left once.
            // [a0,a1,a2,a3] becomes [a1,a2,a3,a0]

            // Function RotWord()
            {
                uint8_t k = tempa[0];
                tempa[0] = tempa[1];
                tempa[1] = tempa[2];
                tempa[2] = tempa[3];
                tempa[3] = k;
            }

            // SubWord() is a function that takes a four-byte input word and
            // applies the S-box to each of the four bytes to produce an output word.

            // Function Subword()
            {
                tempa[0] = s_sbox[tempa[0]];
                tempa[1] = s_sbox[tempa[1]];
                tempa[2] = s_sbox[tempa[2]];
                tempa[3] = s_sbox[tempa[3]];
            }

            tempa[0] = tempa[0] ^ s_rcon[i / N_WORDS_IN_KEY];
        } else if (N_WORDS_IN_KEY > 6 && i % N_WORDS_IN_KEY == 4) {
            // Function Subword()
            {
                tempa[0] = s_sbox[tempa[0]];
                tempa[1] = s_sbox[tempa[1]];
                tempa[2] = s_sbox[tempa[2]];
                tempa[3] = s_sbox[tempa[3]];
            }
        }

        m_roundKey[i * 4 + 0] = m_roundKey[(i - N_WORDS_IN_KEY) * 4 + 0] ^ tempa[0];
        m_roundKey[i * 4 + 1] = m_roundKey[(i - N_WORDS_IN_KEY) * 4 + 1] ^ tempa[1];
        m_roundKey[i * 4 + 2] = m_roundKey[(i - N_WORDS_IN_KEY) * 4 + 2] ^ tempa[2];
        m_roundKey[i * 4 + 3] = m_roundKey[(i - N_WORDS_IN_KEY) * 4 + 3] ^ tempa[3];
    }

    m_isKeySet = true;
}

// This function adds the round key to state.
// The round key is added to the state by an XOR function.
void Aes128::AddRoundKey(uint8_t *state, uint8_t round)
{
    for (int i = 0; i < KEYLEN; ++i) {
        state[i] ^= m_roundKey[round * KEYLEN + i];
    }
}

// The SubBytes Function Substitutes the values in the
// state matrix with values in an S-box.
void Aes128::SubBytes(uint8_t *state)
{
    for (int i = 0; i < KEYLEN; ++i) {
        state[i] = s_sbox[state[i]];
    }
}

// The ShiftRows() function shifts the rows in the state to the left.
// Each row is shifted with different offset.
// Offset = Row number. So the first row is not shifted.
static void ShiftRows(uint8_t *state)
{
    // Rotate first row 1 columns to left
    uint8_t temp = state[1];
    state[1] = state[5];
    state[5] = state[9];
    state[9] = state[13];
    state[13] = temp;

    // Rotate second row 2 columns to left
    temp = state[2];
    state[2] = state[10];
    state[10] = temp;

    temp = state[6];
    state[6] = state[14];
    state[14] = temp;

    // Rotate third row 3 columns to left
    temp = state[3];
    state[3] = state[15];
    state[15] = state[11];
    state[11] = state[7];
    state[7] = temp;
}

static uint8_t xtime(uint8_t x)
{
    return ((x << 1) ^ (((x >> 7) & 1) * 0x1b));
}

// MixColumns function mixes the columns of the state matrix
static void MixColumns(uint8_t *state)
{
    for (int i = 0; i < 4; ++i) {
        uint8_t t = state[i * 4 + 0];
        uint8_t Tmp = state[i * 4 + 0] ^ state[i * 4 + 1] ^ state[i * 4 + 2] ^ state[i * 4 + 3];
        uint8_t Tm = state[i * 4 + 0] ^ state[i * 4 + 1];
        Tm = xtime(Tm);
        state[i * 4 + 0] ^= Tm ^ Tmp;
        Tm = state[i * 4 + 1] ^ state[i * 4 + 2];
        Tm = xtime(Tm);
        state[i * 4 + 1] ^= Tm ^ Tmp;
        Tm = state[i * 4 + 2] ^ state[i * 4 + 3];
        Tm = xtime(Tm);
        state[i * 4 + 2] ^= Tm ^ Tmp;
        Tm = state[i * 4 + 3] ^ t;
        Tm = xtime(Tm);
        state[i * 4 + 3] ^= Tm ^ Tmp;
    }
}

// Multiply is used to multiply numbers in the field GF(2^8)
static uint8_t Multiply(uint8_t x, uint8_t y)
{
    return (((y & 1) * x) ^
        ((y>>1 & 1) * xtime(x)) ^
        ((y>>2 & 1) * xtime(xtime(x))) ^
        ((y>>3 & 1) * xtime(xtime(xtime(x)))) ^
        ((y>>4 & 1) * xtime(xtime(xtime(xtime(x))))));
}

// MixColumns function mixes the columns of the state matrix.
// The method used to multiply may be difficult to understand for the inexperienced.
// Please use the references to gain more information.
static void InvMixColumns(uint8_t *state)
{
    for (int i = 0; i < 4; ++i) {
        uint8_t a = state[i * 4 + 0];
        uint8_t b = state[i * 4 + 1];
        uint8_t c = state[i * 4 + 2];
        uint8_t d = state[i * 4 + 3];

        state[i * 4 + 0] = Multiply(a, 0x0e) ^ Multiply(b, 0x0b) ^ Multiply(c, 0x0d) ^ Multiply(d, 0x09);
        state[i * 4 + 1] = Multiply(a, 0x09) ^ Multiply(b, 0x0e) ^ Multiply(c, 0x0b) ^ Multiply(d, 0x0d);
        state[i * 4 + 2] = Multiply(a, 0x0d) ^ Multiply(b, 0x09) ^ Multiply(c, 0x0e) ^ Multiply(d, 0x0b);
        state[i * 4 + 3] = Multiply(a, 0x0b) ^ Multiply(b, 0x0d) ^ Multiply(c, 0x09) ^ Multiply(d, 0x0e);
    }
}

// The SubBytes Function Substitutes the values in the
// state matrix with values in an S-box.
void Aes128::InvSubBytes(uint8_t *state)
{
    for (int i = 0; i < KEYLEN; ++i) {
        state[i] = s_rsbox[state[i]];
    }
}

static void InvShiftRows(uint8_t *state)
{
    // Rotate first row 1 columns to right
    uint8_t temp = state[13];
    state[13] = state[9];
    state[9] = state[5];
    state[5] = state[1];
    state[1] = temp;

    // Rotate second row 2 columns to right
    temp = state[2];
    state[2] = state[10];
    state[10] = temp;

    temp = state[6];
    state[6] = state[14];
    state[14] = temp;

    // Rotate third row 3 columns to right
    temp = state[3];
    state[3] = state[7];
    state[7] = state[11];
    state[11] = state[15];
    state[15] = temp;
}

// Cipher is the main function that encrypts the PlainText.
void Aes128::ECB_encrypt_block(uint8_t *state)
{
    // Add the First round key to the state before starting the rounds.
    AddRoundKey(state, 0);

    // There will be N_ROUNDS rounds.
    // The first N_ROUNDS-1 rounds are identical.
    // These N_ROUNDS-1 rounds are executed in the loop below.
    for (uint8_t round = 1; round < N_ROUNDS; ++round) {
        SubBytes(state);
        ShiftRows(state);
        MixColumns(state);
        AddRoundKey(state, round);
    }

    // The last round is given below.
    // The MixColumns function is not here in the last round.
    SubBytes(state);
    ShiftRows(state);
    AddRoundKey(state, N_ROUNDS);
}

void Aes128::ECB_decrypt_block(uint8_t *state)
{
    // Add the First round key to the state before starting the rounds.
    AddRoundKey(state, N_ROUNDS);

    // There will be N_ROUNDS rounds.
    // The first N_ROUNDS-1 rounds are identical.
    // These N_ROUNDS-1 rounds are executed in the loop below.
    for (uint8_t round = N_ROUNDS - 1; round > 0; round--) {
        InvShiftRows(state);
        InvSubBytes(state);
        AddRoundKey(state, round);
        InvMixColumns(state);
    }

    // The last round is given below.
    // The MixColumns function is not here in the last round.
    InvShiftRows(state);
    InvSubBytes(state);
    AddRoundKey(state, 0);
}

void Aes128::XorWithIv(uint8_t *buf, const uint8_t *iv)
{
    for (int i = 0; i < KEYLEN; ++i) {
        buf[i] ^= iv[i];
    }
}

void Aes128::CBC_encrypt_buffer(uint8_t *inOut, uint32_t length, const uint8_t *iv)
{
    assert(iv);
    assert(length % KEYLEN == 0);

    for (uint32_t i = 0; i < length / KEYLEN; ++i)
    {
        XorWithIv(inOut, iv);
        ECB_encrypt_block(inOut);
        iv = inOut;
        inOut += KEYLEN;
    }
}

void Aes128::CBC_decrypt_buffer(uint8_t *inOut, uint32_t length, const uint8_t *iv)
{
    assert(iv);
    assert(length % KEYLEN == 0);

    // TODO: Optimize if ever needed
    uint8_t tmp1[KEYLEN];
    uint8_t tmp2[KEYLEN];
    memcpy(tmp1, iv, KEYLEN);
    for (uint32_t i = 0; i < length / KEYLEN; ++i)
    {
        memcpy(tmp2, inOut, KEYLEN);
        ECB_decrypt_block(inOut);
        XorWithIv(inOut, tmp1);
        memcpy(tmp1, tmp2, KEYLEN);
        inOut += KEYLEN;
    }
}

void Aes128::setIv(const uint8_t *iv)
{
    memcpy(m_iv, iv, sizeof(m_iv));
    m_bytesDone = 0;
    m_isIvSet = true;
}

bool Aes128::CTR_scramble(uint8_t *inOut, uint32_t length)
{
    if (!(m_isKeySet && m_isIvSet)) {
        return false;
    }

    while (length > 0) {
        // Create an encrypted block from the initialization vector if necessary
        if (m_bytesDone == 0) {
            memcpy(m_block, m_iv, sizeof(m_block));
            ECB_encrypt_block(m_block);
        }

        // Compute number of bytes to be processed
        uint32_t n = std::min(length, static_cast<uint32_t>(KEYLEN - m_bytesDone));

        // (De)scramble the data
        for (uint32_t i = 0; i < n; i++) {
            *inOut++ ^= m_block[m_bytesDone++];
        }
        length -= n;

        // If an entire block is done, increment the CTR part of the initialization vector
        if (m_bytesDone == KEYLEN) {
            m_bytesDone = 0;
            for (int i = KEYLEN; --i >= 8;) {
                m_iv[i]++;
                if (m_iv[i] != 0) {
                    break; // No carry
                }
            }
        }
    }

    return true;
}
