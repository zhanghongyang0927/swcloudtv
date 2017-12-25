/// \file HttpLoader.cpp
/// \brief The Porting HTTP download utility
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include <stream/HttpLoader.h>

#include <http_client/HttpClient.h>
#include <http_client/IHttpData.h>

#include <porting_layer/Log.h>
#include <stream/IStream.h>
#include <porting_layer/Thread.h>

using namespace ctvc;

const ResultCode HttpLoader::ERROR_WHILE_DOWNLOADING_STREAM("Error during HTTP stream download");

class HttpLoader::Router : public IHttpDataSink
{
public:
    Router(IStream &sink) :
        m_sink(sink)
    {
    }

    void write(const char *buf, uint32_t len)
    {
        m_sink.stream_data(reinterpret_cast<const uint8_t *>(buf), len);
    }

private:
    IStream &m_sink;
};

HttpLoader::HttpLoader() :
    m_client(0)
{
}

HttpLoader::~HttpLoader()
{
}

ResultCode HttpLoader::setup()
{
    m_client = new HttpClient();

    // Start up the transfer (blocking)
    return m_client->get(m_uri.c_str());
}

void HttpLoader::teardown()
{
    delete m_client;
    m_client = 0;
}

bool HttpLoader::run()
{
    CTVC_LOG_INFO("Starting for URL: '%s'", m_uri.c_str());

    assert(m_client);
    assert(m_stream_sink);

    Router router(*m_stream_sink);

    const char *headers[] = { "User-Agent", "avplay" };

    m_client->set_custom_headers(headers, sizeof(headers) / (sizeof(headers[0]) * 2));

    // Do the transfer (blocking)
    ResultCode ret = m_client->receive(&router);

    if (ret == Socket::THREAD_SHUTDOWN) {
        CTVC_LOG_DEBUG("Thread shutdown");
    } else if (ret.is_error()) {
        CTVC_LOG_ERROR("Receive error %s. url:%s", ret.get_description(), m_uri.c_str());
    } else {
        CTVC_LOG_DEBUG("Abort requested");
    }

    // Signal end-of-stream with result code.
    m_stream_sink->stream_error(ret);

    return true; // Exit thread
}
