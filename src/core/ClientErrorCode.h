///
/// \file ClientErrorCode.h
///
/// \brief CloudTV CLient error codes.
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

namespace ctvc {

/// \brief
/// Client error codes as defined in 'CloudTV Client Error Code Specification' version 1.4
/// OK, I know it's silly to define values using values, but there are no mnemonics defined in
/// the document mentioned above. Furthermore, the error codes have no unique mapping so they
/// can't be assigned a proper name either.
/// At least this way it's easy to find all codes that can be generated.
/// It's possible to use the error codes as proper enum values or as simple integer values.
/// It's up to the client software to decide.
enum ClientErrorCode
{
    CLIENT_ERROR_CODE_OK = 0, ///< OK, no error, resume normal operation if a session was closed.
    CLIENT_ERROR_CODE_OK_AND_DO_NOT_RETUNE = 1, ///< OK, but do not retune after having closed the session.
    CLIENT_ERROR_CODE_110 = 110, ///< TCP error 'connection refused', HTTP response 404 (application not found).
    CLIENT_ERROR_CODE_115 = 115, ///< The RFB-TV server version number is not supported by this version of the SDK.
    CLIENT_ERROR_CODE_120 = 120, ///< DNS error 'host not found', HTTP response 500 (internal server configuration error), HTTP response 400 (client id missing) or 401 (credentials missing), RFB-TV 'session configuration error'.
    CLIENT_ERROR_CODE_130 = 130, ///< Timeout while trying to open a TCP connection, timeout while waiting for a reply in the session open request.
    CLIENT_ERROR_CODE_131 = 131, ///< Too many redirects.
    CLIENT_ERROR_CODE_140 = 140, ///< RFB-TV 'client ID missing or not understood', 'application not found', client unable to resolve StreamUrl hostname, DNS error 'host not found' for stream, HTTP 404 error for stream, timeout waiting for a stream to start.
    CLIENT_ERROR_CODE_150 = 150, ///< Session-end message with 'insufficient bandwidth'.
    CLIENT_ERROR_CODE_160 = 160, ///< HTTP response 503 (server busy), RFB-TV SessionNoResources (server busy, no more resources).
    CLIENT_ERROR_CODE_170 = 170, ///< Session-end message with 'latency too large'.
    CLIENT_ERROR_CODE_190 = 190, ///< This is generally the 'unknown error' response, RFB-TV 'unspecified error'.
    CLIENT_ERROR_CODE_200 = 200, ///< RFB-TV 'ping timeout' error.
    CLIENT_ERROR_CODE_210 = 210, ///< RFB-TV 'internal server execution error'.
    CLIENT_ERROR_CODE_220 = 220, ///< RFB-TV 'server is shutting down'.
    CLIENT_ERROR_CODE_230 = 230, ///< RFB-TV 'failed to setup application stream'.
    CLIENT_ERROR_CODE_240 = 240  ///< RFB-TV 'insufficient or invalid parameters'.
};

} // namespace
