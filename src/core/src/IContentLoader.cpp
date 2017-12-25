///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include <core/IContentLoader.h>

const ctvc::ResultCode ctvc::IContentLoader::IContentResult::REQUEST_ERROR("Content could not be downloaded. Request error.");
const ctvc::ResultCode ctvc::IContentLoader::IContentResult::SERVER_ERROR("Content could not be downloaded. Server error.");
const ctvc::ResultCode ctvc::IContentLoader::IContentResult::CANCELED_REQUEST("Operation was cancelled before starting the request.");
const ctvc::ResultCode ctvc::IContentLoader::IContentResult::UNKNOWN_ERROR("Content could not be downloaded. Unknown error.");
