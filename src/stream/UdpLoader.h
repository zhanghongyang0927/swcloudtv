/// \file UdpLoader.h
/// \brief Downloads a UDP stream
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include "LoaderBase.h"

#include <porting_layer/Socket.h>

namespace ctvc {

class UdpLoader : public LoaderBase
{
public:
    UdpLoader();
    ~UdpLoader();

private:
    // Implementation of LoaderBase
    bool run();
    ResultCode setup();
    void teardown();

    UdpSocket m_socket;
};

} // namespace
