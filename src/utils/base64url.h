///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <string>
#include <vector>

namespace ctvc {

std::string base64url_encode(unsigned char const *, unsigned int);
std::vector<unsigned char> base64url_decode(std::string const &);

} // namespace
