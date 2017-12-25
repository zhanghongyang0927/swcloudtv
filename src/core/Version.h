///
/// \file Version.h
///
/// \brief CloudTV Nano SDK Northbound version query interface.
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <string>

#include <inttypes.h>

namespace ctvc {

/// \brief Get the version string of the CloudTV Nano SDK.
///
/// \result The current CloudTV Nano SDK version.
///
std::string get_sdk_version();

} // namespace
