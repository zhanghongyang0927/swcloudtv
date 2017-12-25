///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <inttypes.h>

namespace ctvc {

/// \brief Generic time and time stamp interface
///
/// The implementation is done as part of the porting layer. The inline coding is done
/// to reduce code size, increase speed and reduce memory usage.
class TimeStamp
{
public:
    /// \brief Construct a new TimeStamp object.
    TimeStamp() :
        m_time(0),
        m_flags(0)
    {
    }

    /// \brief Construct a new TimeStamp object with initial value.
    /// \param [in] rhs Initial value.
    TimeStamp(const TimeStamp &rhs) :
        m_time(rhs.m_time),
        m_flags(rhs.m_flags)
    {
    }

    /// \brief Assigns a new value to the TimeStamp, replacing its current contents.
    /// \param [in] rhs New value.
    /// \return Reference to this object.
    TimeStamp &operator=(const TimeStamp &rhs)
    {
        m_time = rhs.m_time;
        m_flags = rhs.m_flags;
        return *this;
    }

    ~TimeStamp()
    {
    }

    /// \brief Check if the stored time value is valid.
    /// \retval true if valid, false otherwise.
    bool is_valid() const
    {
        return (m_flags & IS_VALID) != 0;
    }

    /// \brief Check if the stored time value is absolute.
    /// \retval true if absolute, false otherwise.
    bool is_absolute() const
    {
        return is_valid() && ((m_flags & IS_ABSOLUTE) != 0);
    }

    /// \brief Check if the stored time value is relative.
    /// \retval true if relative, false otherwise.
    bool is_relative() const
    {
        return is_valid() && ((m_flags & IS_ABSOLUTE) == 0);
    }

    /// \brief Check if the stored time value can be compared to the other.
    /// \param [in] rhs Reference to object being compared.
    /// \retval true if so, false otherwise.
    /// If true, both time stamps are valid and of the same type.
    bool is_comparable(const TimeStamp &rhs) const
    {
        return is_valid() && (m_flags == rhs.m_flags);
    }

    /// \brief Make the time stamp invalid.
    void invalidate()
    {
        m_flags &= ~IS_VALID;
    }

    /// \brief Get the time value in microseconds.
    /// \return Time stamp value in microseconds.
    /// May return absolute or relative time, depending on is_absolute()
    /// \note Assumes the time stamp is valid
    int64_t get_as_microseconds() const
    {
        return m_time;
    }

    /// \brief Get the time value in milliseconds.
    /// \return Time stamp value in milliseconds.
    /// May return absolute or relative time, depending on is_absolute()
    /// \note Assumes the time stamp is valid
    int64_t get_as_milliseconds() const
    {
        return m_time / 1000;
    }

    /// \brief Get the time value in seconds.
    /// \return Time stamp value in seconds.
    /// May return absolute or relative time, depending on is_absolute()
    /// \note Assumes the time stamp is valid
    int64_t get_as_seconds() const
    {
        return m_time / 1000000;
    }

    /// \brief Sample the current time
    static TimeStamp now();

    /// \brief Return a relative time of 0
    static TimeStamp zero()
    {
        TimeStamp t;
        t.m_flags = IS_VALID;
        t.m_time = 0;
        return t;
    }

    /// \brief Comparison operators. Assumes is_comparable() to be true.
    ///@{
    bool operator==(const TimeStamp &rhs) const;
    bool operator>=(const TimeStamp &rhs) const;
    bool operator<=(const TimeStamp &rhs) const;
    bool operator!=(const TimeStamp &rhs) const;
    bool operator<(const TimeStamp &rhs) const;
    bool operator>(const TimeStamp &rhs) const;
    ///@}

    /// \brief Arithmetic operators.
    ///@{
    TimeStamp &operator+=(const TimeStamp &rhs);
    TimeStamp &operator-=(const TimeStamp &rhs);

    TimeStamp operator+(const TimeStamp &rhs) const
    {
        TimeStamp tmp(*this);
        tmp += rhs;
        return tmp;
    }
    TimeStamp operator-(const TimeStamp &rhs) const
    {
        TimeStamp tmp(*this);
        tmp -= rhs;
        return tmp;
    }
    ///@}

    /// \brief Add microseconds to a TimeStamp.
    /// \note Assumes the time stamp is valid
    /// \param [in] delta_in_us Time in microseconds.
    /// \returns TimeStamp reference to this.
    /// \note This alters the current object value.
    TimeStamp &add_microseconds(int64_t delta_in_us)
    {
        m_time += delta_in_us;
        return *this;
    }

    /// \brief Add milliseconds to a TimeStamp.
    /// \note Assumes the time stamp is valid
    /// \param [in] delta_in_ms Time in milliseconds.
    /// \returns TimeStamp reference to this.
    /// \note This alters the current object value.
    TimeStamp &add_milliseconds(int64_t delta_in_ms)
    {
        m_time += delta_in_ms * 1000;
        return *this;
    }

    /// \brief Add seconds to a TimeStamp.
    /// \note Assumes the time stamp is valid.
    /// \param [in] delta_in_s Time in seconds.
    /// \returns TimeStamp reference to this.
    /// \note This alters the current object value.
    TimeStamp &add_seconds(int32_t delta_in_s)
    {
        m_time += static_cast<int64_t>(delta_in_s) * 1000000;
        return *this;
    }

private:
    static const uint8_t IS_VALID = (1 << 0);
    static const uint8_t IS_ABSOLUTE = (1 << 1);

    int64_t m_time;
    uint8_t m_flags;
};

} // namespace
