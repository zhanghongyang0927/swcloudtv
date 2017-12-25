///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include <porting_layer/DataStore.h>
#include <porting_layer/Log.h>
#include <porting_layer/FileSystem.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>

#define PROTECT_REWRITE_OF_EQUAL_DATA 1 // Set to nonzero if we want to protect the storage to rewrite the data if it didn't change (e.g. to prevent flash wear-out)

using namespace ctvc;

const ResultCode DataStore::INVALID_PARAMETER("Invalid parameter");
const ResultCode DataStore::COULD_NOT_OPEN_ITEM("Could not open item");
const ResultCode DataStore::READ_ERROR("Read error");
const ResultCode DataStore::WRITE_ERROR("Write error");
const ResultCode DataStore::COULD_NOT_REMOVE_ITEM("Could not remove item");

DataStore::DataStore()
{
}

DataStore::~DataStore()
{
}

void DataStore::set_base_store_path(const char *path)
{
    CTVC_LOG_DEBUG("%s", path);

    m_base_store_path = path ? path : "";


    if (!m_base_store_path.empty() && m_base_store_path[m_base_store_path.length() - 1] != FILE_SEPARATOR) {
        m_base_store_path += FILE_SEPARATOR;
    }

    CTVC_LOG_INFO("DataStore::set_base_store_path('%s')", m_base_store_path.c_str());
}

ResultCode DataStore::set_data(const char *id, const uint8_t *data, uint32_t length)
{
    if (!id || (!data && length > 0)) {
        return INVALID_PARAMETER;
    }

    std::string store_path = m_base_store_path + id;

#if PROTECT_REWRITE_OF_EQUAL_DATA
    // Check the existence and size of the data
    uint32_t existing_length;
    ResultCode ret = get_data(id, 0, 0, &existing_length);
    if (ret.is_ok() && existing_length == length) {
        // If the file exists and the length matches, check the contents
        std::vector<uint8_t> existing_data;
        ret = get_data(id, existing_data);
        if (ret.is_ok() && existing_data.size() == length && memcmp(data, &existing_data[0], length) == 0) {
            CTVC_LOG_INFO("Data of %s not rewritten", id);
            return ResultCode::SUCCESS;
        }
    }
#endif

    FILE *fp = fopen(store_path.c_str(), "wb");
    if (!fp) {
        CTVC_LOG_ERROR("Could not open file: %s (errno:%d)", store_path.c_str(), errno);
        return COULD_NOT_OPEN_ITEM;
    }

    size_t write_size = length > 0 ? fwrite(data, sizeof(uint8_t), length, fp) : 0;

    int res = fclose(fp);
    if (res < 0) {
        CTVC_LOG_DEBUG("No data written to file: %s (errno:%d)", store_path.c_str(), errno);
        return WRITE_ERROR;
    } else if (write_size != length) {
        CTVC_LOG_DEBUG("Write error while writing to file: %s (written %u of %u, errno:%d)", store_path.c_str(), static_cast<uint32_t>(write_size), length, errno);
        return WRITE_ERROR;
    }

    CTVC_LOG_DEBUG("Written %u bytes to file: %s", static_cast<uint32_t>(write_size), store_path.c_str());

    return ResultCode::SUCCESS;
}

ResultCode DataStore::get_data(const char *id, uint8_t *data, uint32_t size, uint32_t *length)
{
    if (!id || !length) {
        return INVALID_PARAMETER;
    }

    *length = 0;

    std::string store_path = m_base_store_path + id;

    FILE *fp = fopen(store_path.c_str(), "rb");
    if (!fp) {
        CTVC_LOG_INFO("Could not open file: %s (errno:%d)", store_path.c_str(), errno);
        return COULD_NOT_OPEN_ITEM;
    }

    if (!data) { // Obtain the length of the data but don't read
        fseek(fp, 0, SEEK_END);
        *length = ftell(fp);
        fclose(fp);
        return ResultCode::SUCCESS;
    }

    size_t read_size = fread(data, 1, size, fp);
    fclose(fp);

    if (read_size != size) {
        CTVC_LOG_DEBUG("Read error while reading from file: %s (read %u of %u, errno:%d)", store_path.c_str(), static_cast<uint32_t>(read_size), size, errno);
        return READ_ERROR;
    }

    *length = read_size;

    CTVC_LOG_DEBUG("Read %u bytes from file: %s", static_cast<uint32_t>(read_size), store_path.c_str());

    return ResultCode::SUCCESS;
}

ResultCode DataStore::get_data(const char *id, std::vector<uint8_t> &data)
{
    if (!id) {
        return INVALID_PARAMETER;
    }

    std::string store_path = m_base_store_path + id;

    FILE *fp = fopen(store_path.c_str(), "rb");
    if (!fp) {
        CTVC_LOG_INFO("Could not open file: %s (errno:%d)", store_path.c_str(), errno);
        return COULD_NOT_OPEN_ITEM;
    }

    // Obtain the length of the data
    fseek(fp, 0, SEEK_END);
    uint32_t length = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    data.resize(length); // Resize so we can store all data.
    size_t read_size = fread(&data[0], 1, length, fp);
    fclose(fp);

    if (read_size != length) {
        CTVC_LOG_DEBUG("Read error while reading from file: %s (read %u of %u, errno:%d)", store_path.c_str(), static_cast<uint32_t>(read_size), length, errno);
        return READ_ERROR;
    }

    CTVC_LOG_DEBUG("Read %u bytes from file: %s", length, store_path.c_str());

    return ResultCode::SUCCESS;
}

ResultCode DataStore::get_data(const char *id, std::string &data)
{
    std::vector<uint8_t> tmp;
    ResultCode ret = get_data(id, tmp);
    if (ret.is_error()) {
        return ret;
    }

    data.assign(reinterpret_cast<const char *>(&tmp[0]), tmp.size());

    return ResultCode::SUCCESS;
}

ResultCode DataStore::delete_data(const char *id)
{
    if (!id) {
        return INVALID_PARAMETER;
    }

    std::string store_path = m_base_store_path + id;
    int res = remove(store_path.c_str());
    if (res != 0) {
        CTVC_LOG_ERROR("Could not remove file: %s (errno:%d)", store_path.c_str(), errno);
        return COULD_NOT_REMOVE_ITEM;
    }

    CTVC_LOG_DEBUG("Removed file: %s", store_path.c_str());

    return ResultCode::SUCCESS;
}
