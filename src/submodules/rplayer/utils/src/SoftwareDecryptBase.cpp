///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include <rplayer/utils/SoftwareDecryptEngineBase.h>
#include <rplayer/utils/Aes.h>

using namespace rplayer;

class SoftwareDecryptEngineBase::Impl
{
public:
    Aes128 m_aes;
};

SoftwareDecryptEngineBase::SoftwareDecryptEngineBase() :
    m_impl(*new SoftwareDecryptEngineBase::Impl())
{
}

SoftwareDecryptEngineBase::~SoftwareDecryptEngineBase()
{
    delete &m_impl;
}

void SoftwareDecryptEngineBase::setKey(const uint8_t (&keyId)[16])
{
    m_impl.m_aes.setKey(keyId);
}

void SoftwareDecryptEngineBase::setInitializationVector(const uint8_t (&iv)[16])
{
    m_impl.m_aes.setIv(iv);
}

bool SoftwareDecryptEngineBase::decrypt(uint8_t *data, uint32_t size)
{
    return m_impl.m_aes.CTR_scramble(data, size);
}
