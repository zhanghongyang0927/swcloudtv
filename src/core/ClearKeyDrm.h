///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include "ICdmSession.h"

#include <stream/IStreamDecrypt.h>

#include <rplayer/utils/Aes.h>

class ClearKeyCdmSession : public ctvc::ICdmSession, public ctvc::IStreamDecrypt
{
public:
    ClearKeyCdmSession();
    ~ClearKeyCdmSession();

    // Implementation of ICdmSession
    ctvc::IStreamDecrypt *get_stream_decrypt_engine();

    void setup(const std::string &session_type, const std::map<std::string, std::string> &init_data, ICallback &callback);
    void terminate(ICallback &callback);

    // Implementation of IStreamDecrypt
    void set_stream_return_path(ctvc::IStream *stream_out);

    void set_key_identifier(const uint8_t (&key_id)[16]);
    void set_initialization_vector(const uint8_t (&iv)[16]);

    bool stream_data(const uint8_t *data, uint32_t length);

private:
    ctvc::IStream *m_stream_out;

    rplayer::Aes128 m_aes;

    uint8_t m_spare_bytes[16];
    uint32_t m_spare_count;

    uint8_t m_key_id[16];
    uint8_t m_key_value[16];
};

class ClearKeyCdmSessionFactory : public ctvc::ICdmSessionFactory
{
public:
    ClearKeyCdmSessionFactory();
    ~ClearKeyCdmSessionFactory();

    void get_drm_system_id(uint8_t (&id)[16]/*out*/);

    ctvc::ICdmSession *create();
    void destroy(ctvc::ICdmSession *p);
};
