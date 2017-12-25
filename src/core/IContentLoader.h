///
/// \file IContentLoader.h
///
/// \brief CloudTV Nano SDK static content loader
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <porting_layer/ResultCode.h>

#include <string>
#include <vector>
#include <inttypes.h>

namespace ctvc {

/// \brief Interface used to fetch static content such images
struct IContentLoader
{
    IContentLoader() {}
    virtual ~IContentLoader() {}

    /// \brief Class where the result of the request will the returned
    struct IContentResult
    {
        static const ResultCode REQUEST_ERROR; ///<! The URL is malformed or the protocol is not supported
        static const ResultCode SERVER_ERROR;  ///<! The server returned an error or the connection could not be established
        static const ResultCode CANCELED_REQUEST; ///<! The request was canceled before it could be issued
        static const ResultCode UNKNOWN_ERROR; ///<! Any other error

        /// \brief Wait until the result of loading operation is available
        ///
        /// This call shall block until the operation has finished and it shall not be called twice.
        virtual ResultCode wait_for_result() = 0;
    };

    /// \brief Request to download an asset from the given URL and store it in the passed buffer
    ///
    /// This function will be called multiple times to issue parallel requests (if supported by the implementation).
    /// A synchronous implementation might decide to block the call until the resource has been downloaded. An
    /// asynchronous approach could immediately return after having passed the request to a pool of threads. Once
    /// all requests are posted via this function, the caller will wait for the results using IContentResult::wait_for_result().
    ///
    /// This function won't be used before calling start() or after calling stop().
    ///
    /// \param[in] url URL where the resource can be reached. Only HTTP is supported at this moment.
    /// \param[in] buffer Reference to the buffer where to download the requested asset. Nano SDK guarantees
    /// that it will be valid until release_content_result is called
    /// \return A pointer to the object where the result of operation is returned.
    virtual IContentResult *load_content(const std::string &url, std::vector<uint8_t> &buffer) = 0;

    /// \brief Releases the object that was allocated by load_content
    ///
    /// \param[in] content_result Pointer to the object that contained the result of a certain request.
    virtual void release_content_result(IContentResult *content_result) = 0;
};

} // namespace
