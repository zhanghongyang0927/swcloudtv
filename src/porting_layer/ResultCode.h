///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <vector>
#include <string>

#include <assert.h>

namespace ctvc
{

/// \brief Generic class to return the result of the methods.
///
/// The design of this class has the following features:
///     - Minimum overhead when the objects are copied (e.g. when returning an object)
///     - The object can be compared
///     - Type check during compiling time
///     - Access to a textual description of the result (\see get_description())
///     - Each defined ResultCode has an unique code number (\see get_code())
class ResultCode
{
public:
    static const ResultCode SUCCESS; ///< Operation succeeded

    /// \brief Default constructor for non-initialized code
    ResultCode() :
        m_code(UNINITIALIZED_CODE)
    {
    }

// The copy constructor and (empty) destructor are explicitly disabled.
// Since they are trivial, we don't really need to specify them.
// If they are specified, however, the compiler does not make use of the
// move constructor (not defined in C++03 so we can't define that here).
// The move constructor is needed to make object code using ResultCode
// efficient, i.e. returning the class by value as a single integer.
// This significantly impacts code speed and size with respect to the
// use of ResultCode. The overall code size is affected by 1.3-1.9%.
// If this code is enabled, ResultCode objects are passed on stack and
// use additional indirections, making it less efficient in both space
// and time.
//    /// \brief Copy constructor
//    /// \param[in] rhs Original object to be copied
//    ResultCode(const ResultCode &rhs) :
//        m_code(rhs.m_code)
//    {
//        assert(rhs.m_code != UNINITIALIZED_CODE);
//    }
//
//    ~ResultCode()
//    {
//    }

    /// \private
    // This constructor is for internal use only, i.e it is intended only for CloudTV Nano SDK.
    explicit ResultCode(const char *text);

    /// \brief Return the unique code number of the result
    /// \return Unique code number of the result
    int get_code() const
    {
        return m_code;
    }

    /// \brief Return a textual description of the result
    /// \return Pointer to the array that contains the textual description of the result
    const char *get_description() const;

    /// \brief Comparison operator
    /// \param[in] rhs Operand on the right hand side of "==" sign
    /// \return True, if both ResultCode objects are the same. False otherwise.
    bool operator==(const ResultCode &rhs) const
    {
        return m_code == rhs.m_code;
    }

    /// \brief Comparison operator
    /// \param[in] rhs Operand on the right hand side of "!=" sign
    /// \return True, if both ResultCode objects are different. False otherwise.
    bool operator!=(const ResultCode &rhs) const
    {
        return m_code != rhs.m_code;
    }

    /// \brief Assignment operator
    /// \param[in] rhs Operand on the right hand side of "=" sign
    /// \return Reference to this
    ResultCode &operator=(const ResultCode &rhs)
    {
        assert(rhs.m_code != UNINITIALIZED_CODE);
        m_code = rhs.m_code;
        return *this;
    }

    /// \brief Return true if the object is ResultCode::SUCCESS
    /// \return True is the object is ResultCode::SUCCESS. False otherwise.
    bool is_ok() const
    {
        return m_code == OK_CODE;
    }

    /// \brief Return true if the object is not ResultCode::SUCCESS
    /// \return True is the object is not ResultCode::SUCCESS. False otherwise.
    bool is_error() const
    {
        return m_code != OK_CODE;
    }

    /// \brief Combine this result code with the right hand side code.
    ///        If this code is_ok(), the right hand side is taken.
    ///        If this code is_error(), the code is not changed.
    /// \param[in] rhs Operand on the right hand side of "|=" sign
    /// \return Reference to this
    ResultCode &operator|=(const ResultCode &rhs)
    {
        if (is_ok()) {
            m_code = rhs.m_code;
        }
        return *this;
    }

private:
    static const ResultCode UNINITIALIZED;
    static const int OK_CODE = 0;
    static const int UNINITIALIZED_CODE = 1;

    int m_code;

    static std::vector<std::string> &get_vector() {
        static std::vector<std::string> s_error_messages;
        return s_error_messages;
    }

public:
    // Special constructor for specific codes
    /// \privatesection
    explicit ResultCode(int code) :
        m_code(code)
    {
    }
};

} // namespace
