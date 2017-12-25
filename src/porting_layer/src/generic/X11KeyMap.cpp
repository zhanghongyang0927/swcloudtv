///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include <porting_layer/X11KeyMap.h>

using namespace ctvc;

X11KeyMap::X11KeyMap()
{
}

X11KeyMap::~X11KeyMap()
{
}

void X11KeyMap::add_mapping(int in_key, X11KeyCode out_key)
{
    m_keymap[in_key] = out_key;
}

void X11KeyMap::add_mapping(const KeyMap map[], unsigned int n_entries)
{
    for (unsigned int i = 0; i < n_entries; i++) {
        add_mapping(map[i].from_key, map[i].to_key);
    }
}

X11KeyCode X11KeyMap::translate(int native_key) const
{
    if (m_keymap.size() == 0) {
        return static_cast<X11KeyCode>(native_key);
    }

    std::map<int, X11KeyCode>::const_iterator it = m_keymap.find(native_key);
    if (it != m_keymap.end()) {
        return it->second;
    } else {
        return X11_INVALID;
    }
}
