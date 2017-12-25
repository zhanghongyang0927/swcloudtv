///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <core/ProtocolExtensionBase.h>

class MyProtocolExtension : public ctvc::ProtocolExtensionBase
{
public:
    MyProtocolExtension();

protected:
    // implement ProtocolExtensionBase interface
    void received(const uint8_t *data, uint32_t length);
};
