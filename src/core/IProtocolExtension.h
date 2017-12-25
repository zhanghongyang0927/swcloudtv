///
/// \file IProtocolExtension.h
///
/// \brief CloudTV Nano SDK Protocol Extension.
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

/// \brief RFB-TV Protocol extension interface.
struct IProtocolExtension
{
    IProtocolExtension() {}
    virtual ~IProtocolExtension() {}

    /// \brief Return the protocol identifier of this extension.
    /// \return Protocol identifier.
    virtual std::string get_protocol_id() const = 0;

    /// \brief Handle an incoming request for this extension.
    /// \param [in] data  Pointer to buffer with received data.
    /// \param [in] length Length of \a data in bytes
    virtual void received(const uint8_t *data, uint32_t length) = 0;

    /// \brief Reply interface for the protocol extension to send messages to.
    struct IReply
    {
        /// \brief Send an outgoing message for this extension.
        /// \param [in] origin Reference to object that implements this IProtocolExtension.
        /// \param [in] data Pointer to data to be sent.
        /// \param [in] length Number of bytes in \a data.
        virtual void send(const IProtocolExtension &origin, const uint8_t *data, uint32_t length) = 0;
    };

    /// \brief Register a reply path for this protocol extension.
    /// \param [in] reply_path Pointer to object instance of type IReply.
    virtual void register_reply_path(IReply *reply_path) = 0;
};

} // namespace
