///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include "KeyFilter.h"

#include <utils/utils.h>

#include <porting_layer/AutoLock.h>
#include <porting_layer/Log.h>

#include <stddef.h>

using namespace ctvc;

// The max number of keys that can be in a range.
// This is a sanity check number.
static const int MAX_KEYS_IN_RANGE = 250;

// Or-able flags for the key filter as returned by find_filter_for_key()
static const int HANDLE_LOCALLY = (1 << 0);
static const int HANDLE_REMOTELY = (1 << 1);

KeyFilter::KeyFilter()
{
}

KeyFilter::~KeyFilter()
{
}

void KeyFilter::clear()
{
    AutoLock lck(m_mutex);

    m_key_filter_map.clear();
}

void KeyFilter::parse_lists(const std::string &local_keys, const std::string &remote_keys)
{
    AutoLock lck(m_mutex);

    // We need to parse one of the lists twice.
    // Keys that are mentioned in one of the lists should change their mapping.
    // Others should be left unmodified.
    // However, keys can be mentioned in both lists and should then map to both
    // local and remote handling.
    // Therefore, we parse both lists as if the keys are mentioned only there
    // (and reset their membership of any other list). After that, we parse the
    // first list again but add membership, we don't overwrite.
    parse_list(local_keys, false, true); // Set the mode of any keys in local_keys to handle locally (clearing any previous modes)
    parse_list(remote_keys, true, true); // Set the mode of any keys in remote_keys to handle remotely (clearing any previous modes)
    parse_list(local_keys, false, false); // Add the mode of any keys in local_keys to handle locally (adding to any previous modes)
}

void KeyFilter::find_filter_for_key(X11KeyCode x11_key, bool &client_must_handle_key_code/*out*/, bool &server_must_handle_key_code/*out*/)
{
    AutoLock lck(m_mutex);

    std::map<X11KeyCode, int>::iterator it = m_key_filter_map.find(x11_key);
    if (it != m_key_filter_map.end()) {
        client_must_handle_key_code = (it->second & HANDLE_LOCALLY) != 0;
        server_must_handle_key_code = (it->second & HANDLE_REMOTELY) != 0;
        return;
    }

    // By default only the server handles the key
    client_must_handle_key_code = false;
    server_must_handle_key_code = true;
}

void KeyFilter::parse_list(const std::string &list, bool is_remote_list, bool overwrite)
{
    std::vector<std::string> keys = parse_character_separated_list(list, ',');

    int flag = is_remote_list ? HANDLE_REMOTELY : HANDLE_LOCALLY;

    for (size_t i = 0; i < keys.size(); i++) {
        // Check if it is a range
        size_t range_delim = keys[i].find('-', 0);
        if (range_delim != std::string::npos && range_delim > 0) {
            std::vector<std::string> keys_range = parse_character_separated_list(keys[i], '-');
            if (keys_range.size() == 2) {
                int first_value = strtol(&keys_range[0][0], 0, 16);
                int last_value = strtol(&keys_range[1][0], 0, 16);
                if (first_value < last_value && MAX_KEYS_IN_RANGE > (last_value - first_value)) {
                    for (int i = first_value; i <= last_value; i++) {
                        X11KeyCode key_code(static_cast<X11KeyCode>(i));
                        if (overwrite) {
                            m_key_filter_map[key_code] = flag;
                        } else {
                            m_key_filter_map[key_code] |= flag;
                        }
                    }
                } else {
                    CTVC_LOG_ERROR("Range error first_value(%d) > last_value(%d) or MAX_KEYS_IN_RANGE(%d) > (last_value - first_value)", first_value, last_value, MAX_KEYS_IN_RANGE);
                }
            } else {
                CTVC_LOG_ERROR("Range error");
            }
        } else {
            X11KeyCode key_code(static_cast<X11KeyCode>(strtol(&keys[i][0], 0, 16)));
            if (overwrite) {
                m_key_filter_map[key_code] = flag;
            } else {
                m_key_filter_map[key_code] |= flag;
            }
        }
    }
}
