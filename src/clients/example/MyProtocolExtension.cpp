///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include "MyProtocolExtension.h"

MyProtocolExtension::MyProtocolExtension() :
    ProtocolExtensionBase("drm")
{
}

void MyProtocolExtension::received(const uint8_t *data, uint32_t length)
{
    /// \todo: parse data and do something useful with it in your protocol handler
    /// and perhaps send a response using send()
    (void)data;
    (void)length;
}
