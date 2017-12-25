///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include <core/ClearKeyDrm.h>

#include <stream/IStream.h>
#include <utils/base64.h>
#include <utils/utils.h>
#include <porting_layer/Log.h>

#include <string.h>

using namespace ctvc;

// Clear key GUID 1077EFEC-C0B2-4D02-ACE3-3C1E52E2FB4B
static const uint8_t CLEAR_KEY_GUID[16] = { 0x10, 0x77, 0xEF, 0xEC, 0xC0, 0xB2, 0x4D, 0x02, 0xAC, 0xE3, 0x3C, 0x1E, 0x52, 0xE2, 0xFB, 0x4B };

ClearKeyCdmSession::ClearKeyCdmSession() :
    m_stream_out(0),
    m_spare_count(0)
{
    memset(m_spare_bytes, 0, sizeof(m_spare_bytes));
    memset(m_key_id, 0, sizeof(m_key_id));
    memset(m_key_value, 0, sizeof(m_key_value));
}

ClearKeyCdmSession::~ClearKeyCdmSession()
{
}

IStreamDecrypt *ClearKeyCdmSession::get_stream_decrypt_engine()
{
    // We're both CDM session and decrypt engine
    return this;
}

void ClearKeyCdmSession::setup(const std::string &/*session_type*/, const std::map<std::string, std::string> &init_data, ICallback &callback)
{
    // Clear out all persistent data
    m_spare_count = 0;
    memset(m_spare_bytes, 0, sizeof(m_spare_bytes));
    memset(m_key_id, 0, sizeof(m_key_id));
    memset(m_key_value, 0, sizeof(m_key_value));

    std::vector<uint8_t> key_id;
    std::vector<uint8_t> key_value;
    std::map<std::string, std::string> response;

    std::map<std::string, std::string>::const_iterator it = init_data.find("key_id");
    if (it != init_data.end()) {
        key_id = base64_decode(it->second);
    } else {
        CTVC_LOG_ERROR("key_id not found in init_data");
        callback.setup_result(ICdmSession::SETUP_DRM_SYSTEM_ERROR, response);
        return;
    }

    it = init_data.find("key_value");
    if (it != init_data.end()) {
        key_value = base64_decode(it->second);
    } else {
        CTVC_LOG_ERROR("key_value not found in init_data");
        callback.setup_result(ICdmSession::SETUP_DRM_SYSTEM_ERROR, response);
        return;
    }

    if (key_id.size() != sizeof(m_key_id) || key_value.size() != sizeof(m_key_value)) {
        CTVC_LOG_ERROR("Unexpected key_id or key_value size in init_data: %lu/%lu", static_cast<unsigned long>(key_id.size()), static_cast<unsigned long>(key_value.size()));
        callback.setup_result(ICdmSession::SETUP_DRM_SYSTEM_ERROR, response);
        return;
    }

    // All data ok, so now set the key
    memcpy(m_key_id, &key_id[0], sizeof(m_key_id));
    memcpy(m_key_value, &key_value[0], sizeof(m_key_value));

    // Simple implementation does not require asynchronous handling, so pass the result immediately.
    callback.setup_result(SETUP_OK, response);
}

void ClearKeyCdmSession::terminate(ICallback &callback)
{
    std::map<std::string, std::string> stop_data;
    // Simple implementation does not require asynchronous handling, so pass the result immediately.
    callback.terminate_result(stop_data);
}

void ClearKeyCdmSession::set_stream_return_path(IStream *stream_out)
{
    m_stream_out = stream_out;
}

void ClearKeyCdmSession::set_key_identifier(const uint8_t (&key_id)[16])
{
    // We could get away not comparing anything and immediately calling m_aes.setKey()
    // when setup() is called, but this way we check the proper flow of events.
    if (memcmp(key_id, m_key_id, sizeof(m_key_id)) != 0) {
        CTVC_LOG_ERROR("Given key ID not found (%s)", id_to_guid_string(key_id).c_str());
        return;
    }

    m_aes.setKey(m_key_value);
}

void ClearKeyCdmSession::set_initialization_vector(const uint8_t (&/*iv*/)[16])
{
    // We do ECB so no IV is used
}

bool ClearKeyCdmSession::stream_data(const uint8_t *data, uint32_t length)
{
    uint32_t total = m_spare_count + length;
    uint32_t n_bytes_to_decrypt = total & ~15;
    uint32_t new_spare_count = total & 15;

    if (n_bytes_to_decrypt > 0) {
        if (m_stream_out) {
            // Because the data is decrypted in-place, we need to copy all data anyway,
            // irrespective of whether we had any spare data left or not.
            uint8_t *out_data = new uint8_t[n_bytes_to_decrypt];
            memcpy(out_data, m_spare_bytes, m_spare_count);
            memcpy(out_data + m_spare_count, data, length - new_spare_count);

            // Do the decryption in-place.
            for (uint32_t i = 0; i < n_bytes_to_decrypt; i += 16) {
                m_aes.ECB_decrypt_block(out_data + i);
            }

            // Send it out
            m_stream_out->stream_data(out_data, n_bytes_to_decrypt);

            // Done with our temporary buffer.
            delete [] out_data;
        }

        // Store the left-over data (any non-multiple of 16 bytes) as spare for next time.
        memcpy(m_spare_bytes, data + length - new_spare_count, new_spare_count);
        m_spare_count = new_spare_count;
    } else {
        // Not enough to decrypt a block, so accumulate all data as spare.
        memcpy(m_spare_bytes + m_spare_count, data, length);
        m_spare_count = total;
    }

    return true;
}

ClearKeyCdmSessionFactory::ClearKeyCdmSessionFactory()
{
}

ClearKeyCdmSessionFactory::~ClearKeyCdmSessionFactory()
{
}

void ClearKeyCdmSessionFactory::get_drm_system_id(uint8_t (&id)[16]/*out*/)
{
    memcpy(id, CLEAR_KEY_GUID, sizeof(id));
}

ICdmSession *ClearKeyCdmSessionFactory::create()
{
    return new ClearKeyCdmSession();
}

void ClearKeyCdmSessionFactory::destroy(ICdmSession *p)
{
    delete p;
}
