///
/// \file DefaultContentLoader.cpp
///
/// \brief CloudTV Nano SDK default implementation of the Content Loader
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include <core/DefaultContentLoader.h>

#include <porting_layer/AutoLock.h>
#include <porting_layer/Log.h>

#include <utils/utils.h>

#include <stddef.h>

using namespace ctvc;

DefaultContentLoader::DefaultContentLoader() :
    m_single_content_handler(*this),
    m_state(STOPPED)
{
}

DefaultContentLoader::~DefaultContentLoader()
{
    stop();
}

bool DefaultContentLoader::start(uint8_t num_threads)
{
    AutoLock lock(m_mutex);

    if (m_state == STARTED) {
        return num_threads == m_threads.size();
    } else if (m_state == STOPPING) {
        return false;
    }

    for (uint8_t i = 0; i < num_threads; i++) {
        std::string thread_name;
        string_printf(thread_name, "Content loader %d/%d", i + 1, num_threads);
        Thread *thread = new Thread(thread_name);
        ContentHandler *content_handler = new ContentHandler(*this);

        if (thread->start(*content_handler, Thread::PRIO_NORMAL).is_ok()) {
            m_threads.push_back(thread);
            m_content_handlers.push_back(content_handler);
        } else {
            CTVC_LOG_ERROR("Only %u threads out of %u were created", i + 1, num_threads);
            break;
        }
    }

    m_state = STARTED;

    return num_threads == m_threads.size();
}

void DefaultContentLoader::stop()
{
    AutoLock lock(m_mutex);

    if (m_state == STOPPED || m_state == STOPPING) {
        return;
    }

    m_state = STOPPING;

    // Signal all the threads to stop
    for (size_t i = 0; i < m_threads.size(); i++) {
        m_threads[i]->stop();
    }

    // Pending requests are canceled before they are handled by one of the threads
    while (!m_pending_requests.empty()) {
        m_pending_requests.back()->set_result(IContentResult::CANCELED_REQUEST);
        m_pending_requests.pop_back();
    }

    // Just in case there are threads waiting in the semaphore, we signal them as many as threads
    for (size_t i = 0; i < m_threads.size(); i++) {
        m_pending_requests_semaphore.post();
    }

    m_mutex.unlock(); // Wait until all threads are stopped
    for (size_t i = 0; i < m_threads.size(); i++) {
        m_threads[i]->wait_until_stopped();
    }
    m_mutex.lock();

    purge_pointers_vector<Thread *>(m_threads);
    purge_pointers_vector<ContentHandler *>(m_content_handlers);
    purge_pointers_vector<ContentDescriptor *>(m_content_descriptors);

    m_state = STOPPED;
}

IContentLoader::IContentResult *DefaultContentLoader::load_content(const std::string &url, std::vector<uint8_t> &buffer)
{
    AutoLock lock(m_mutex);

    if (m_state != STARTED) {
        CTVC_LOG_ERROR("Thread(s) has not been started");
        return NULL;
    }

    ContentDescriptor *content_descriptor;
    if (m_pool_requests.empty()) {
        content_descriptor = new ContentDescriptor();
        m_content_descriptors.push_back(content_descriptor);
    } else {
        content_descriptor = m_pool_requests.back();
        m_pool_requests.pop_back();
    }

    if (m_threads.empty()) { // There are no threads running attending requests, i.e. synchronous request
        content_descriptor->set_result(m_single_content_handler.download_content(url, buffer));
    } else { // We push the request to the pending requests and signal it to the threads
        content_descriptor->set_request(url, buffer);

        m_pending_requests.push_back(content_descriptor);

        m_pending_requests_semaphore.post();
    }

    return static_cast<IContentResult *>(content_descriptor);
}

void DefaultContentLoader::release_content_result(IContentResult *content_result)
{
    if (content_result == 0) {
        CTVC_LOG_ERROR("NULL pointer cannot be returned");
        return;
    }

    AutoLock lock(m_mutex);

    ContentDescriptor *content_descriptor = reinterpret_cast<ContentDescriptor *>(content_result);
    m_pool_requests.push_back(content_descriptor);
}

DefaultContentLoader::ContentDescriptor *DefaultContentLoader::get_next_pending_content_descriptor()
{
    m_pending_requests_semaphore.wait();

    AutoLock lock(m_mutex);

    ContentDescriptor *content_descriptor = 0;

    if (!m_pending_requests.empty()) {
        content_descriptor = m_pending_requests.front();
        m_pending_requests.pop_front();
    }

    return content_descriptor;
}

DefaultContentLoader::ContentHandler::ContentHandler(DefaultContentLoader &parent) :
    m_parent(parent),
    m_buffer(0)
{
}

DefaultContentLoader::ContentHandler::~ContentHandler()
{
}

ResultCode DefaultContentLoader::ContentHandler::download_content(const std::string &url, std::vector<uint8_t> &buffer)
{
    m_buffer = &buffer;
    ResultCode rc = m_http_client.get(url.c_str(), this);

    if (rc == HttpClient::UNRECOGNIZED_PROTOCOL) {
        rc = IContentLoader::IContentResult::REQUEST_ERROR;
    } else if (rc == HttpClient::PROTOCOL_ERROR) {
        rc = IContentLoader::IContentResult::REQUEST_ERROR;
    } else if (rc == HttpClient::CONNECTION_CLOSED) {
        rc = IContentLoader::IContentResult::SERVER_ERROR;
    } else if (rc == HttpClient::EXCEEDED_MAX_REDIRECTIONS) {
        rc = IContentLoader::IContentResult::SERVER_ERROR;
    }

    return rc;
}

bool DefaultContentLoader::ContentHandler::run()
{
    ContentDescriptor *content_descriptor = m_parent.get_next_pending_content_descriptor();

    // If the content descriptor is 0, the DefaultContentLoader wants to stop all threads
    if (content_descriptor == 0) {
        m_parent.m_closing_thread_semaphore.post();
        return true;
    }

    content_descriptor->set_result(download_content(content_descriptor->get_url(), *content_descriptor->get_buffer()));

    return false;
}

void DefaultContentLoader::ContentHandler::write(const char *buf, uint32_t len)
{
    if (m_buffer) {
        const uint8_t *p = reinterpret_cast<const uint8_t *>(buf);
        m_buffer->insert(m_buffer->end(), p, p + len);
    }
}
