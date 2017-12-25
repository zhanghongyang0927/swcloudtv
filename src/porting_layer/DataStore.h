///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include "ResultCode.h"

#include <inttypes.h>
#include <string>
#include <vector>

namespace ctvc {

/// \brief Class for (secure) storage of the data
///
/// Nano SDK uses this class to store data in the platform.
class DataStore
{
public:
    static const ResultCode INVALID_PARAMETER;     ///< The parameter is not valid \see ResultCode
    static const ResultCode COULD_NOT_OPEN_ITEM;   ///< The requested item cannot be opened \see ResultCode
    static const ResultCode READ_ERROR;            ///< Error during reading of the item \see ResultCode
    static const ResultCode WRITE_ERROR;           ///< Error during writing of the item \see ResultCode
    static const ResultCode COULD_NOT_REMOVE_ITEM; ///< The item could not be deleted \see ResultCode

    DataStore();
    ~DataStore();

    /// \brief Set base store path for get/set/delete_data.
    /// \param[in] path The path.
    void set_base_store_path(const char *path);

    /// \brief This is called to save persistent data.
    /// \param[in] id The id of the data.
    /// \param[in] data The data to be saved.
    /// \param[in] length The length of the data, may be 0.
    /// \result ResultCode
    /// \note The DataStore may implement mechanisms to prevent writing the same data multiple times.
    ResultCode set_data(const char *id, const uint8_t *data, uint32_t length);

    /// \brief This is called to save persistent data from an std::vector.
    /// \param[in] id The id of the  data.
    /// \param[in] data The data to be saved, may be empty.
    /// \result ResultCode
    /// \note The DataStore may implement mechanisms to prevent writing the same data multiple times.
    ResultCode set_data(const char *id, const std::vector<uint8_t> &data)
    {
        return set_data(id, &data[0], data.size());
    }

    /// \brief This is called to save persistent data from an std::string.
    /// \param[in] id The id of the data.
    /// \param[in] data The data to be saved, may be an empty string.
    /// \result ResultCode
    /// \note The DataStore may implement mechanisms to prevent writing the same data multiple times.
    ResultCode set_data(const char *id, const std::string &data)
    {
        return set_data(id, reinterpret_cast<const uint8_t *>(data.c_str()), data.size());
    }

    /// \brief This is called to get stored persistent data.
    /// \param[in] id The id of the data.
    /// \param[out] data Buffer for the data to be retrieved; if null, only the length will be returned.
    /// \param[in] size The size of data buffer (if data != null).
    /// \param[out] length The length of data that is or will be retrieved, may return 0 if the data is empty.
    /// \result ResultCode
    ResultCode get_data(const char *id, uint8_t *data, uint32_t size, uint32_t *length);

    /// \brief This is called to get stored persistent data into an std::vector.
    /// \param[in] id The id of the data.
    /// \param[out] data The data that is retrieved, may be empty.
    /// \result ResultCode
    ResultCode get_data(const char *id, std::vector<uint8_t> &data/*out*/);

    /// \brief This is called to get stored persistent data into an std::string
    /// \param[in] id The id of the data.
    /// \param[out] data The data that is retrieved, may be empty.
    /// \result ResultCode
    ResultCode get_data(const char *id, std::string &data/*out*/);

    /// \brief This is called to delete persistent data.
    /// \param[in] id The id of the data.
    /// \result ResultCode
    ResultCode delete_data(const char *id);

private:
    DataStore(const DataStore &);
    DataStore &operator=(const DataStore &);

    std::string m_base_store_path;
};

} // namespace
