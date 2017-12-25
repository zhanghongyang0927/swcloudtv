///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include <porting_layer/ResultCode.h>

#include <vector>
#include <map>

#include <assert.h>
#include <stddef.h>

using namespace ctvc;

// Special codes
const ResultCode ResultCode::SUCCESS(OK_CODE);
const ResultCode ResultCode::UNINITIALIZED(UNINITIALIZED_CODE);

ResultCode::ResultCode(const char *text)
{
    static std::map<std::string, int> s_error_map;

    if (get_vector().empty()) {
        // This implies that the following are always OK_CODE = 0 and UNINITIALIZED_CODE = 1
        get_vector().push_back("OK");
        s_error_map[get_vector().back()] = OK_CODE;
        get_vector().push_back("Non-initialized code");
        s_error_map[get_vector().back()] = UNINITIALIZED_CODE;
    }

    // If the error string already exists, assign the existing error code
    std::map<std::string, int>::const_iterator it = s_error_map.find(text);
    if (it != s_error_map.end()) {
        m_code = it->second;
        assert(get_vector()[m_code] == text);
        return;
    }

    // Assign a new error code
    std::string str(text);
    get_vector().push_back(str);
    m_code = get_vector().size() - 1;
    s_error_map[get_vector().back()] = m_code;
}

const char *ResultCode::get_description() const
{
    assert(static_cast<size_t>(m_code) < get_vector().size());
    return get_vector()[m_code].c_str();
}
