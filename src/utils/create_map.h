//
// This template was obtained from the public domain on www.stackoverflow.com
//
// Usage:
// std::map mymap = create_map<int, int >(1,2)(3,4)(5,6);
//
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <map>

namespace ctvc {

template<typename T, typename U>
class create_map
{
public:
    create_map(const T &key, const U &val)
    {
        m_map[key] = val;
    }

    create_map<T, U> &operator()(const T &key, const U &val)
    {
        m_map[key] = val;
        return *this;
    }

    operator std::map<T, U>()
    {
        return m_map;
    }

private:
    std::map<T, U> m_map;
};

} // namespace
