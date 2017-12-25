/// \file Version.cpp
/// \brief CloudTV Nano SDK version query.
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include <core/Version.h>
#include <utils/utils.h>

#include <string>

#define STR_(x) #x
#define STR(x) STR_(x)

std::string ctvc::get_sdk_version()
{
    return STR(CTVC_VERSION);
}
