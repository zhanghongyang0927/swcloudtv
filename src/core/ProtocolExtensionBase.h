///
/// \file ProtocolExtensionBase.h
///
/// \brief CloudTV Nano SDK Protocol Extension Base.
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include "IProtocolExtension.h"

namespace ctvc {

/// \brief RFB-TV Protocol extension base class.
///
/// Derive your own class and implement the request() method.
class ProtocolExtensionBase : public IProtocolExtension
{
public:
    ProtocolExtensionBase(const std::string &protocol_id) :
        m_protocol_id(protocol_id),
        m_reply_path(0)
    {
    }

protected:
    // Signal the derived class that a message has been received
    // Needs to be implemented by the derived class
    virtual void received(const uint8_t *data, uint32_t length) = 0;

    // Can be used by derived class to send a reply or initiate a message
    virtual void send(const uint8_t *data, uint32_t length)
    {
        if (m_reply_path) {
            m_reply_path->send(*this, data, length);
        }
    }

    // Implements part of IProtocolExtension, fixed
    virtual std::string get_protocol_id() const
    {
        return m_protocol_id;
    }

private:
    const std::string m_protocol_id;
    IReply *m_reply_path;

    // Implements part of IProtocolExtension, fixed
    virtual void register_reply_path(IReply *reply_path)
    {
        m_reply_path = reply_path;
    }
};

} // namespace
