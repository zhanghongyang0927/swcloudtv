///
/// \file DefaultContentLoader.h
///
/// \brief CloudTV Nano SDK default implementation of the Content Loader
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <core/IContentLoader.h>
#include <http_client/HttpClient.h>
#include <http_client/IHttpData.h>
#include <porting_layer/Thread.h>
#include <porting_layer/Mutex.h>
#include <porting_layer/Semaphore.h>

#include <vector>
#include <list>
#include <string>

namespace ctvc
{

class DefaultContentLoader : public IContentLoader
{
public:
    DefaultContentLoader();
    virtual ~DefaultContentLoader();

    virtual IContentLoader::IContentResult *load_content(const std::string &url, std::vector<uint8_t> &buffer);
    virtual void release_content_result(IContentResult *content_result);

    // "0" means no threading at all
    // returns "false" if less threads than requested were created
    bool start(uint8_t num_threads);

    // This call blocks until all threads are stopped
    void stop();

private:
    DefaultContentLoader(const DefaultContentLoader &);
    DefaultContentLoader &operator=(const DefaultContentLoader &);

    template<typename T> void purge_pointers_vector(std::vector<T> &v)
    {
        while (!v.empty()) {
            delete v.back();
            v.pop_back();
        }
    }

    class ContentDescriptor : public IContentResult
    {
    public:
        ContentDescriptor() :
            m_buffer(0)
        {
        }

        virtual ~ContentDescriptor()
        {
        }

        void set_request(const std::string &url, std::vector<uint8_t> &buffer)
        {
            m_url = url;
            m_buffer = &buffer;
        }

        const std::string &get_url()
        {
            return m_url;
        }

        std::vector<uint8_t> *get_buffer()
        {
            return m_buffer;
        }

        void set_result(ResultCode result)
        {
            m_result = result;
            m_sem.post();
        }

        virtual ResultCode wait_for_result()
        {
            m_sem.wait();
            return m_result;
        }

    private:
        std::string m_url; // URL to be loaded
        std::vector<uint8_t> *m_buffer; // Buffer where to store the result
        Semaphore m_sem; // Semaphore that will be set to 1 when the result is known
        ResultCode m_result;
    };

    ContentDescriptor *get_next_pending_content_descriptor();

    class ContentHandler : public Thread::IRunnable, public IHttpDataSink
    {
    public:
        ContentHandler(DefaultContentLoader &parent);
        ~ContentHandler();

        ResultCode download_content(const std::string &url, std::vector<uint8_t> &buffer);

        virtual bool run();
        virtual void write(const char *buf, uint32_t len);

    private:
        ContentHandler(const ContentHandler &);
        ContentHandler &operator=(const ContentHandler &);

        HttpClient m_http_client;
        DefaultContentLoader &m_parent;
        std::vector<uint8_t> *m_buffer;
    };
    friend class ContentHandler; // Needed for Metrowerks

    Mutex m_mutex;
    Semaphore m_pending_requests_semaphore;
    Semaphore m_closing_thread_semaphore;

    ContentHandler m_single_content_handler;

    enum
    {
        STOPPED,
        STOPPING,
        STARTED
    } m_state;

    std::vector<Thread *> m_threads;
    std::vector<ContentHandler *> m_content_handlers;

    std::list<ContentDescriptor *> m_pending_requests;
    std::vector<ContentDescriptor *> m_pool_requests;
    std::vector<ContentDescriptor *> m_content_descriptors;
};

}
