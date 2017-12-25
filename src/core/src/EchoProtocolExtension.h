///
/// \file EchoProtocolExtension.h
///
/// \brief CloudTV Nano SDK Protocol Extension Base.
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <core/ProtocolExtensionBase.h>

namespace ctvc {

class EchoProtocolExtension : public ProtocolExtensionBase
{
public:
    EchoProtocolExtension() :
        ProtocolExtensionBase("echo")
    {
    }

    virtual void received(const uint8_t *data, uint32_t length)
    {
        send(data, length);
    }
};

} // namespace
