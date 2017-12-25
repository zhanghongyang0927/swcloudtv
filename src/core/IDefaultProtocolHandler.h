///
/// \file IDefaultProtocolHandler.h
///
/// \brief CloudTV Nano SDK Default Protocol Receiver.
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

/// \brief Default handler of non-managed RFB-TV Protocol extensions.
struct IDefaultProtocolHandler
{
    IDefaultProtocolHandler() {}
    virtual ~IDefaultProtocolHandler() {}

    /// \brief Handle an incoming request for all non-handled extensions
    /// \param [in] protocol_id Protocol identified of the received message
    /// \param [in] data Pointer to buffer with received data.
    /// \param [in] length Length of \a data in bytes.
    virtual void received(const std::string &protocol_id, const uint8_t *data, uint32_t length) = 0;
};

} // namespace
