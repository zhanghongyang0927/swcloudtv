///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <porting_layer/Mutex.h>
#include <porting_layer/X11KeyMap.h>

#include <string>
#include <map>

#include <inttypes.h>

namespace ctvc {

// This class manages a list of key filters for a session.
// It is protected by a mutex so can be accessed in a thread-safe way.
class KeyFilter
{
public:
    KeyFilter();
    ~KeyFilter();

    void clear();
    void parse_lists(const std::string &local_keys, const std::string &remote_keys);
    void find_filter_for_key(X11KeyCode x11_key, bool &client_must_handle_key_code/*out*/, bool &server_must_handle_key_code/*out*/);

private:
    KeyFilter(const KeyFilter &);
    KeyFilter &operator=(const KeyFilter &);

    void parse_list(const std::string &list, bool is_remote_list, bool overwrite);

    std::map<X11KeyCode, int> m_key_filter_map;
    Mutex m_mutex;
};

} // namespace
