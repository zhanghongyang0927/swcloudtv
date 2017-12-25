///
/// \file OptionalValue.h
///
/// \brief CloudTV Nano SDK optional value template.
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <inttypes.h>

namespace ctvc
{

template<typename TYPE> class OptionalValue
{
public:
    OptionalValue() :
        m_is_set(false)
    {
    }

    OptionalValue(const OptionalValue &rhs) :
        m_value(rhs.m_value),
        m_is_set(rhs.m_is_set)
    {
    }

    OptionalValue &operator=(const OptionalValue &rhs)
    {
        if (this != &rhs) {
            m_value = rhs.m_value;
            m_is_set = rhs.m_is_set;
        }

        return *this;
    }

    OptionalValue &operator=(const TYPE &rhs)
    {
        set(rhs);

        return *this;
    }

    void set(TYPE value)
    {
        m_value = value;
        m_is_set = true;
    }

    TYPE get() const
    {
        return m_value;
    }

    bool is_set() const
    {
        return m_is_set;
    }

    void reset()
    {
        m_is_set = false;
    }

private:
    TYPE m_value;
    bool m_is_set;
};

} // namespace
