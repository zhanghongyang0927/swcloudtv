///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <vector>
#include <string>

namespace ctvc {

std::vector<unsigned char> base16_decode(std::string const &encoded_string);

} // namespace
