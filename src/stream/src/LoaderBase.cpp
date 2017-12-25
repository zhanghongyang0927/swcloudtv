///
/// \file LoaderBase.cpp
///
/// \brief CloudTV Nano SDK Stream loader base class.
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include <stream/LoaderBase.h>

#include <porting_layer/Log.h>

using namespace ctvc;

LoaderBase::LoaderBase() :
    m_stream_sink(0),
    m_thread("Stream loader")
{
}

LoaderBase::~LoaderBase()
{
    close_stream(); // Just in case the stream is not closed before the object is being destroyed.
}

ResultCode LoaderBase::open_stream(const std::string &uri, IStream &stream_sink)
{
    CTVC_LOG_INFO("uri:%s", uri.c_str());

    close_stream(); // Just in case it was already open and we open a new stream.

    m_uri = uri;
    m_stream_sink = &stream_sink;

    ResultCode ret = setup();
    if (ret.is_error()) {
        CTVC_LOG_ERROR("LoaderBase::setup() failed");
        close_stream();
        return ret;
    }

    ret = m_thread.start(*this, Thread::PRIO_NORMAL);
    if (ret.is_error()) {
        CTVC_LOG_ERROR("m_thread.start() failed");
        close_stream();
        return ret;
    }

    return ResultCode::SUCCESS;
}

void LoaderBase::close_stream()
{
    CTVC_LOG_INFO("uri:%s", m_uri.c_str());

    m_thread.stop_and_wait_until_stopped();

    if (m_stream_sink) { // Prevent multiple callbacks if we close multiple times
        teardown();
    }

    m_stream_sink = 0;
    m_uri = "";

    CTVC_LOG_DEBUG("Done");
}
