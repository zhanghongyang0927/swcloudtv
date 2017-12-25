/// \file HttpLoader.h
/// \brief Downloads a HTTP stream
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include "LoaderBase.h"

namespace ctvc {

class HttpClient;

class HttpLoader : public LoaderBase
{
public:
    static const ResultCode ERROR_WHILE_DOWNLOADING_STREAM;

    HttpLoader();
    ~HttpLoader();

private:
    // Implementation of LoaderBase
    bool run();
    ResultCode setup();
    void teardown();

    class Router;

    HttpClient *m_client;
};

} // namespace
