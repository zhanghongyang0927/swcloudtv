///
/// \file LoaderBase.h
///
/// \brief CloudTV Nano SDK Stream loader base class.
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <stream/IStreamLoader.h>
#include <porting_layer/Thread.h>

#include <string>

namespace ctvc {

struct IStream;

class LoaderBase : public IStreamLoader, public Thread::IRunnable
{
public:
    LoaderBase();
    ~LoaderBase();

    virtual ResultCode open_stream(const std::string &uri, IStream &stream_sink);
    virtual void close_stream();

protected:
    std::string m_uri;
    IStream *m_stream_sink;

    Thread m_thread;

private:
    LoaderBase(const LoaderBase &);
    LoaderBase &operator=(const LoaderBase &);

    // Implementation of Thread::IRunnable
    virtual bool run() = 0;

    // Implementation-specific loader setup and teardown
    virtual ResultCode setup() = 0;
    virtual void teardown() = 0;
};

} // namespace
