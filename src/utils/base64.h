///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <string>
#include <vector>

namespace ctvc {

std::string base64_encode(const std::string &);
std::string base64_encode(unsigned char const *, unsigned int);
std::vector<unsigned char> base64_decode(std::string const &);

} // namespace
