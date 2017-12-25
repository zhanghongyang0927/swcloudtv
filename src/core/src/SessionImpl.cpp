///
/// \file SessionImpl.cpp
///
/// \brief CloudTV Nano SDK northbound interface implementation.
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include "SessionImpl.h"
#include "EchoProtocolExtension.h"

#include <core/Version.h>
#include <core/IOverlayCallbacks.h>
#include <core/IDefaultProtocolHandler.h>
#include <core/IHandoffHandler.h>
#include <core/ICdmSession.h>
#include <core/IContentLoader.h>
#include <stream/IMediaPlayer.h>
#include <porting_layer/AutoLock.h>
#include <porting_layer/Socket.h>
#include <utils/utils.h>
#include <utils/create_map.h>

#include <map>
#include <algorithm>

#include <string.h>
#include <assert.h>

#include "swcloudtv_priv.h"

using namespace ctvc;

static const unsigned int STREAMER_TRIGGER_PERIOD_IN_MS = 10; // Trigger period for the timer, interval to kick the Streamer/RPlayer/RAMS real-time clock
static const unsigned int REPORT_TRIGGER_PERIOD_IN_MS = 100; // Trigger period for the timer, interval to kick the report manager(s)

const ResultCode Session::Impl::CONNECTION_TIMEOUT("A timeout occurred while trying to open the connection");
const ResultCode Session::Impl::INVALID_STATE("The function cannot be called in the current state");
const ResultCode Session::Impl::UNSUPPORTED_PROTOCOL("Unsupported protocol");
const ResultCode Session::Impl::TOO_MANY_REDIRECTS("Too many redirects");

PictureParameters::PictureParameters() :
    x(0),
    y(0),
    w(0),
    h(0),
    alpha(0)
{
}

const unsigned int MAX_RFBTV_REDIRECTS = 20;

const int DEFAULT_RFBTV_SERVER_PORT = 8095;

// Stream error timeout in ms.
const uint32_t STREAM_ERROR_TIMEOUT_IN_MS = 5000;

static const uint8_t RFBTV_MOUSE_BUTTON_LEFT = 1;
static const uint8_t RFBTV_MOUSE_BUTTON_MIDDLE = 2;
static const uint8_t RFBTV_MOUSE_BUTTON_RIGHT = 4;
static const uint8_t RFBTV_MOUSE_WHEEL_UP = 8;
static const uint8_t RFBTV_MOUSE_WHEEL_DOWN = 16;

// Construct a session object instance
Session::Session(ClientContext &context, ISessionCallbacks *session_callbacks, IOverlayCallbacks *overlay_callbacks) :
    m_impl(*new Session::Impl(context, session_callbacks, overlay_callbacks))
{
}

Session::~Session()
{
    delete &m_impl;
}

IControl &Session::get_control() const
{
    return m_impl;
}

IInput &Session::get_input() const
{
    return m_impl;
}

Session::State Session::get_state() const
{
    return m_impl.get_state();
}

bool Session::register_media_player(const std::string &protocol_id, IMediaPlayerFactory &factory)
{
    return m_impl.m_streamer.register_media_player(protocol_id, factory);
}

bool Session::unregister_media_player(const std::string &protocol_id)
{
    return m_impl.m_streamer.unregister_media_player(protocol_id);
}

bool Session::register_content_loader(IContentLoader *content_loader)
{
    return m_impl.register_content_loader(content_loader);
}

bool Session::register_protocol_extension(IProtocolExtension &protocol_extension)
{
    return m_impl.register_protocol_extension(protocol_extension);
}

bool Session::unregister_protocol_extension(IProtocolExtension &protocol_extension)
{
    return m_impl.unregister_protocol_extension(protocol_extension);
}

void Session::register_default_protocol_handler(IDefaultProtocolHandler *protocol_handler)
{
    m_impl.m_default_handler = protocol_handler;
}

void Session::register_media_chunk_allocator(IMediaChunkAllocator *media_chunk_allocator)
{
    m_impl.m_streamer.register_media_chunk_allocator(media_chunk_allocator);
}

bool Session::register_drm_system(ICdmSessionFactory &factory)
{
    return m_impl.register_drm_system(factory);
}

bool Session::unregister_drm_system(ICdmSessionFactory &factory)
{
    return m_impl.unregister_drm_system(factory);
}

bool Session::register_handoff_handler(const std::string &handoff_scheme, IHandoffHandler &handoff_handler)
{
    return m_impl.register_handoff_handler(handoff_scheme, handoff_handler);
}

bool Session::unregister_handoff_handler(const std::string &handoff_scheme)
{
    return m_impl.unregister_handoff_handler(handoff_scheme);
}

/* ***************************** IMPLEMENTATION ***************************** */

Session::Impl::Impl(ClientContext &context, ISessionCallbacks *session_callbacks, IOverlayCallbacks *overlay_callbacks) :
    m_context(context),
    m_session_callbacks(session_callbacks),
    m_overlay_callbacks(overlay_callbacks),
    m_default_handler(NULL),
    m_timer("Session and stream timer"),
    m_content_loader(0),
    m_overlay_handler(*this, overlay_callbacks),
    m_playback_report_manager(m_playback_report, *this),
    m_playback_report_periodic_trigger(*this, &Session::Impl::playback_report_periodic_trigger, 0),
    m_latency_report_manager(m_latency_report, *this),
    m_log_report_manager(m_log_report, *this),
    m_prev_log_output(0),
    m_is_logging(false),
    m_screen_width(0),
    m_screen_height(0),
    m_rfbtv_button_mask(0),
    m_redirect_count(0),
    m_rfbtv_state(INIT),
    m_state(STATE_DISCONNECTED),
    m_closing_suspended(false),
    m_connect_attempts(0),
    m_connection("RFB-TV TCP connection"),
    m_event_handling_thread("Session event handler"),
    m_rfbtv_protocol(*this),
    m_connection_backoff_time_callback(*this, &Session::Impl::connection_backoff_time_expired, 0),
    m_stream_error_callback(*this, &Session::Impl::stream_timeout_expired, 0),
    m_streamer_periodic_trigger(m_streamer, &Streamer::trigger, 0),
    m_stream_confirm_sent_state(STREAM_CONFIRM_NOT_SENT)
{
    m_streamer.register_latency_data_callback(this);
    m_streamer.register_stall_event_callback(this);
    m_streamer.register_media_player_callback(this);
    register_protocol_extension(m_echo_protocol);
}

Session::Impl::~Impl()
{
    CLOUDTV_LOG_DEBUG("test");

    // Stop the timer before stopping the message handling to prevent further events to be posted in the event queue.
    m_timer.stop();

    m_overlay_handler.stop();

    // Stop the message handling thread so we don't process any further events from now on.
    stop_message_handling_thread();

    // Close the connection after having stopped the message handling thread; it might still open a new connection otherwise.
    rfbtvpm_close_connection();

    unregister_protocol_extension(m_echo_protocol);
    // Unregister all left-over protocol extensions
    for (std::map<std::string, IProtocolExtension *>::iterator i = m_protocol_extensions.begin(); i != m_protocol_extensions.end(); ++i) {
        i->second->register_reply_path(0);
    }
    rfbtvpm_clean_active_cdm_sessions();
    m_streamer.register_media_player_callback(0);
    m_streamer.register_stall_event_callback(0);
    m_streamer.register_latency_data_callback(0);
    // Unregister our log report as log output if registered.
    // (We may also register ourselves at construction time and save all registration hassle when ServerCommands are received.)
    ClientContext::instance().unregister_log_output(*this);
}

Session::State Session::Impl::get_state() const
{
    // No need to lock because m_state is atomic
    return m_state;
}

void Session::Impl::initiate(const std::string &host, const std::string &url, uint32_t screen_width, uint32_t screen_height, const std::map<std::string, std::string> &optional_parameters)
{
    //sw_log_info(TAG,"host:%s, url:%s, %dx%d", host.c_str(), url.c_str(), screen_width, screen_height);
    start_message_handling_thread();
    m_overlay_handler.start(m_content_loader);
    m_timer.start(Thread::PRIO_HIGHEST);
    m_event_queue.put(new InitiateEvent(*this, &Impl::handle_initiate_event, host, url, screen_width, screen_height, optional_parameters, TimeStamp::now()));
}

void Session::Impl::terminate()
{
    CLOUDTV_LOG_DEBUG("test\n");
    m_event_queue.put(new TerminateEvent(*this, &Impl::handle_terminate_event, CLIENT_ERROR_CODE_OK));
}

void Session::Impl::suspend()
{
    CLOUDTV_LOG_DEBUG("test\n");
    m_event_queue.put(new TriggerEvent(*this, &Impl::handle_suspend_event));
}

void Session::Impl::resume()
{
    CLOUDTV_LOG_DEBUG("test\n");
    m_event_queue.put(new TriggerEvent(*this, &Impl::handle_resume_event));
}

void Session::Impl::update_session_optional_parameters(const std::map<std::string, std::string> &key_value_pairs)
{
    CLOUDTV_LOG_DEBUG("test\n");
    m_event_queue.put(new ParameterUpdateEvent(*this, &Impl::handle_update_session_optional_parameters_event, key_value_pairs));
}

void Session::Impl::send_keycode(int native_key, Action action, bool &client_must_handle_key_code)
{
   // CLOUDTV_LOG_DEBUG("Native key:%x, action:%d", native_key, action);
	CLOUDTV_LOG_DEBUG("native_key:0x%x.\n", native_key);
    X11KeyCode x11_key = m_context.get_keymap().translate(native_key);
    if (x11_key == X11_INVALID) {
        CTVC_LOG_WARNING("Cannot translate native key code 0x%X to X11", native_key);
        return;
    }

    bool server_must_handle_key_code;
    m_key_filter.find_filter_for_key(x11_key, client_must_handle_key_code, server_must_handle_key_code);

    if (!server_must_handle_key_code) {
        // Done, no need to send the key code to the server
        return;
    }

	CLOUDTV_LOG_DEBUG("x11 Key:%x, action:%d\n", x11_key, action);

    m_event_queue.put(new KeyEvent(*this, &Impl::handle_send_keycode_event, x11_key, action));
}

void Session::Impl::send_pointer_event(uint32_t x, uint32_t y, Button button, Action action)
{
    CLOUDTV_LOG_DEBUG("type:%d, x:%d, y:%d, button:%d", action, x, y, button);
    m_event_queue.put(new PointerEvent(*this, &Impl::handle_pointer_event, x, y, button, action));
}

void Session::Impl::player_event(IMediaPlayer::PlayerEvent event)
{
    CLOUDTV_LOG_DEBUG("test");
    m_event_queue.put(new PlayerEvent(*this, &Impl::handle_player_event, event));
}

void Session::Impl::stream_data(const uint8_t *data, uint32_t length)
{
    CLOUDTV_LOG_DEBUG("test");
    m_event_queue.put(new StreamDataEvent(*this, &Impl::handle_stream_data_event, data, length));
}

void Session::Impl::stream_error(ResultCode result)
{
    CLOUDTV_LOG_DEBUG("ResultCode:%d\n", result.get_code());
    m_event_queue.put(new StreamErrorEvent(*this, &Impl::handle_stream_error_event, result.get_code()));
}

void Session::Impl::latency_stream_data(ILatencyData::LatencyDataType data_type, TimeStamp pts, TimeStamp original_event_time)
{
    CLOUDTV_LOG_DEBUG("data_type:%d", data_type);
    m_event_queue.put(new LatencyDataEvent(*this, &Impl::handle_latency_data_event, data_type, pts, original_event_time));
}

void Session::Impl::stall_detected(const std::string &id, bool is_audio_not_video, const TimeStamp &stall_duration)
{
    CLOUDTV_LOG_DEBUG("id:%s, audio=%d, duration:%u", id.c_str(), is_audio_not_video, static_cast<unsigned>(stall_duration.get_as_milliseconds()));
    m_event_queue.put(new StallEvent(*this, &Impl::handle_stall_event, id, is_audio_not_video, stall_duration));
}

void Session::Impl::send(const IProtocolExtension &protocol_extension, const uint8_t *data, uint32_t length)
{
    CLOUDTV_LOG_DEBUG("test");
    m_event_queue.put(new ProtocolExtensionSendEvent(*this, &Session::Impl::handle_protocol_extension_send_event, protocol_extension.get_protocol_id(), std::vector<uint8_t>(data, data + length)));
}

ResultCode Session::Impl::request_transmission(ReportBase &report)
{
    // This is called from ReportManager::generate_report(), which is called from either ReportManager::timer_tick(),
    // ReportManager::report_updated() or directly from Session::Impl.
    // The session mutex is locked in all cases so doesn't need to be locked again here.
    // More importantly, we don't need to consider creating an event for this particular case.

    // Our mutex is already locked here

    CLOUDTV_LOG_DEBUG("test");

    // Should not happen, but if triggered when not in ACTIVE state, we should not bother the client.
    if (!is_active()) {
        CLOUDTV_LOG_DEBUG("Session is not running");
        return INVALID_STATE;
    }

    ResultCode ret = ResultCode::SUCCESS;

    if (&report == &m_playback_report) {
        // Update playback report data if possible
        IMediaPlayer::PlayerInfo info;
        const uint64_t UNSET_64_BIT_VAL = ~0ULL;
        const uint32_t UNSET_32_BIT_VAL = ~0U;
        info.current_pts = UNSET_64_BIT_VAL;

        m_streamer.get_player_info(info);

        if (info.current_pts != UNSET_64_BIT_VAL) {
            m_playback_report.m_current_pts = info.current_pts;
        }

        // Get info from the rplayer
        uint64_t current_pts = UNSET_64_BIT_VAL;
        uint32_t stalled_duration_in_ms = UNSET_32_BIT_VAL;
        uint32_t pcr_delay = UNSET_32_BIT_VAL;
        m_streamer.get_rplayer_status(current_pts, stalled_duration_in_ms, pcr_delay);

        if (current_pts != UNSET_64_BIT_VAL) {
            // Currently, the rplayer PCR (aka current PTS) has precedence.
            // This is because the use case is about feeding back the stream time
            // to the web application. The rplayer PCR is closer to the web app
            // so this time should be the one returned.
            // It may be changed at a later moment, e.g. by adding a different
            // report field for the rplayer-measured value.
            m_playback_report.m_current_pts = current_pts;
        }
        if (stalled_duration_in_ms != UNSET_32_BIT_VAL) {
            // Currently, the rplayer stalled duration and the player stalled duration
            // return similar but independent measurements of stalled time. Typically,
            // only one of them can be non-zero at a time. However, in the hypothetical
            // case that both are non-zero, it makes sense to add them up rather than
            // take one of them or take the maximum value. Regretfully, adding the values
            // is quite hard, so we'll take the maximum.
            // This may be changed at a later moment, e.g. by adding a different
            // report field for the rplayer-measured value.
            if (!m_playback_report.m_stalled_duration_in_ms.is_set() || stalled_duration_in_ms > m_playback_report.m_stalled_duration_in_ms.get()) {
                m_playback_report.m_stalled_duration_in_ms = stalled_duration_in_ms;
            }
        }
        if (pcr_delay != UNSET_32_BIT_VAL) {
            m_playback_report.m_pcr_delay = pcr_delay;
        }

        // Send the report
        ret = rfbtvpm_send_message(m_rfbtv_protocol.create_playback_client_report(m_playback_report));

        // And reset
        m_playback_report.m_current_pts.reset();
        m_playback_report.m_pcr_delay.reset();
    } else if (&report == &m_latency_report) {
        // Send the report
        ret = rfbtvpm_send_message(m_rfbtv_protocol.create_latency_client_report(m_latency_report));

        // And reset
        m_latency_report.reset();
    } else if (&report == &m_log_report) {
        // Send the report
        ret = rfbtvpm_send_message(m_rfbtv_protocol.create_log_client_report(m_log_report));

        // And reset
        m_log_report.reset();
    }

    return ret;
}

void Session::Impl::log_message(LogMessageType message_type, const char *message)
{
    // Lock special logger mutex here since logging can be called from anywhere...
    AutoLock lck(m_log_mutex);

    // Don't allow recursive logging
    if (m_is_logging) {
        return;
    }

    m_is_logging = true;

    // Send the report only if we can lock our mutex
    if (m_mutex.trylock()) {
        // Add any old logs from the backlog to the report
        if (!m_log_backlog.empty()) {
            for (size_t i = 0; i < m_log_backlog.size(); i++) {
                m_log_report.add_log(m_log_backlog[i].first, m_log_backlog[i].second.c_str());
            }
            m_log_backlog.clear();
        }

        // Add the new log to the report
        m_log_report.add_log(message_type, message);

        // Trigger if there's something to send
        if (!m_log_report.get_text().empty()) {
            m_log_report_manager.report_updated();
        }

        // We locked, so we must unlock
        m_mutex.unlock();
    } else {
        // If not, we'll add the log to the log backlog
        m_log_backlog.push_back(std::pair<LogMessageType, std::string>(message_type, message));
    }

    m_is_logging = false;
}

bool Session::Impl::register_content_loader(IContentLoader *content_loader)
{
    CLOUDTV_LOG_DEBUG("test");

    AutoLock lck(m_mutex);

    if (!is_idle()) {
        CLOUDTV_LOG_DEBUG("Content loader can only be changed when the session is idle");
        return false;
    }

    m_content_loader = content_loader;

    return true;
}

bool Session::Impl::register_protocol_extension(IProtocolExtension &protocol_extension)
{
    CLOUDTV_LOG_DEBUG("test");

    AutoLock lck(m_mutex);

    if (protocol_extension.get_protocol_id().empty()) {
        CLOUDTV_LOG_DEBUG("Invalid protocol identifier");
        return false;
    }
    protocol_extension.register_reply_path(this);
    m_protocol_extensions[protocol_extension.get_protocol_id()] = &protocol_extension;
    return true;
}

bool Session::Impl::unregister_protocol_extension(IProtocolExtension &protocol_extension)
{
    CLOUDTV_LOG_DEBUG("test");

    AutoLock lck(m_mutex);

    if (protocol_extension.get_protocol_id().empty()) {
        CLOUDTV_LOG_DEBUG("Invalid protocol identifier");
        return false;
    }
    protocol_extension.register_reply_path(0);
    if (m_protocol_extensions.erase(protocol_extension.get_protocol_id()) != 1) {
        CTVC_LOG_WARNING("Attempt to unregister protocol '%s' that wasn't registered", protocol_extension.get_protocol_id().c_str());
    }
    return true;
}

bool Session::Impl::register_drm_system(ICdmSessionFactory &factory)
{
    CLOUDTV_LOG_DEBUG("test");

    AutoLock lck(m_mutex);

    // Check for duplicate registration.
    if (std::find(m_drm_systems.begin(), m_drm_systems.end(), &factory) != m_drm_systems.end()) {
        // Already registered
        CTVC_LOG_WARNING("Attempt to register a DRM system twice");
        return true;
    }

    // Check for duplicate DRM system IDs.
    uint8_t new_id[16];
    factory.get_drm_system_id(new_id);
    for (std::vector<ICdmSessionFactory *>::iterator it = m_drm_systems.begin(); it != m_drm_systems.end(); ++it) {
        uint8_t id[16];
        (*it)->get_drm_system_id(id);

        if (memcmp(id, new_id, sizeof(id)) == 0) {
            // Same DRM system id.
            CLOUDTV_LOG_DEBUG("Attempt to register a DRM system with the same ID as an already registered DRM system (%s)", id_to_guid_string(new_id).c_str());
            return false;
        }
    }

    m_drm_systems.push_back(&factory);
    return true;
}

bool Session::Impl::unregister_drm_system(ICdmSessionFactory &factory)
{
    CLOUDTV_LOG_DEBUG("test");

    AutoLock lck(m_mutex);

    std::vector<ICdmSessionFactory *>::iterator it = std::find(m_drm_systems.begin(), m_drm_systems.end(), &factory);
    if (it == m_drm_systems.end()) {
        // Not found
        CTVC_LOG_WARNING("Attempt to unregister a DRM system that wasn't registered");
        return false;
    }

    // Found
    m_drm_systems.erase(it);

    // Delete all running CDM sessions, if any.
    // This is a little overdone because we might also be deleting CDM sessions that the
    // unregistered factory did *not* create, but it's otherwise safe and clear behavior.
    rfbtvpm_clean_active_cdm_sessions();

    return true;
}

bool Session::Impl::register_handoff_handler(const std::string &handoff_scheme, IHandoffHandler &handoff_handler)
{
    CLOUDTV_LOG_DEBUG("test");

    AutoLock lck(m_mutex);

    if (handoff_scheme.empty()) {
        CLOUDTV_LOG_DEBUG("Invalid handoff scheme");
        return false;
    }

    m_handoff_handlers[handoff_scheme] = &handoff_handler;
    return true;
}

bool Session::Impl::unregister_handoff_handler(const std::string &handoff_scheme)
{
    CLOUDTV_LOG_DEBUG("test");

    AutoLock lck(m_mutex);

    if (handoff_scheme.empty()) {
        CLOUDTV_LOG_DEBUG("Invalid handoff scheme");
        return false;
    }

    if (m_handoff_handlers.erase(handoff_scheme) != 1) {
        CTVC_LOG_WARNING("Attempt to unregister handoff handler scheme '%s' that wasn't registered", handoff_scheme.c_str());
        return false;
    }
    return true;
}

void Session::Impl::close_session_in_case_of_error(ResultCode result)
{
    // Our mutex is already locked here

    if (result.is_error()) {
        if (m_rfbtv_state == ERROR) {
            CLOUDTV_LOG_DEBUG("Error (%s) reported, but already in error state", result.get_description());
            return;
        }

        CLOUDTV_LOG_DEBUG("Error (%s), session closed and entering error state", result.get_description());
        // Translate error codes into client error codes according to 'CloudTV Client Error Code Specification' version 1.4
        ClientErrorCode error_code;
        if (result == Socket::CONNECTION_REFUSED) {
            error_code = CLIENT_ERROR_CODE_110;
        } else if (result == Socket::HOST_NOT_FOUND) {
            error_code = CLIENT_ERROR_CODE_120;
        } else if (result == Socket::CONNECT_TIMEOUT) {
            error_code = CLIENT_ERROR_CODE_130;
        } else {
            error_code = CLIENT_ERROR_CODE_190;
        }

        rfbtvpm_session_stop(error_code, RfbtvProtocol::SESSION_TERMINATE_NORMAL);
    }
}

ResultCode Session::Impl::rfbtvpm_send_message(const RfbtvMessage &msg)
{
    // Our mutex is already locked here

    CLOUDTV_LOG_DEBUG("length:%u", msg.size());

    return m_connection.send_data(msg.data(), msg.size());
}

const char *Session::Impl::rfbtvpm_get_state_name(RFBTV_STATE value)
{
    static const std::map<RFBTV_STATE, const char *> s_state_name_map = create_map<RFBTV_STATE, const char *>
        (INIT, "INIT")
        (INITIATED, "INITIATED")
        (REDIRECTED, "REDIRECTED")
        (CONNECTING, "CONNECTING")
        (OPENING, "OPENING")
        (ACTIVE, "ACTIVE")
        (SUSPENDED, "SUSPENDED")
        (ERROR, "ERROR");

    std::map<RFBTV_STATE, const char *>::const_iterator i = s_state_name_map.find(value);
    assert(i != s_state_name_map.end()); // If this fails, we must add the missing state to the map

    return i->second;
}

const char *Session::Impl::rfbtvpm_get_state_name(State state)
{
    static const std::map<State, const char *> s_state_name_map = create_map<State, const char *>
        (STATE_DISCONNECTED, "STATE_DISCONNECTED")
        (STATE_CONNECTING, "STATE_CONNECTING")
        (STATE_CONNECTED, "STATE_CONNECTED")
        (STATE_SUSPENDED, "STATE_SUSPENDED")
        (STATE_ERROR, "STATE_ERROR");

    std::map<State, const char *>::const_iterator i = s_state_name_map.find(state);
    assert(i != s_state_name_map.end()); // If this fails, we must add the missing state to the map

    return i->second;
}

void Session::Impl::rfbtvpm_set_state(RFBTV_STATE value, ClientErrorCode error_code)
{
    // Our mutex is already locked here

    CLOUDTV_LOG_DEBUG("state:%s->%s\n", rfbtvpm_get_state_name(m_rfbtv_state), rfbtvpm_get_state_name(value));
    m_rfbtv_state = value;

	switch(value){
    case INIT:
        set_state(STATE_DISCONNECTED, error_code);
        break;

    case INITIATED:
    case REDIRECTED:
    case CONNECTING:
    case OPENING:
        set_state(STATE_CONNECTING, error_code);
        break;

    case ACTIVE:
        set_state(STATE_CONNECTED, error_code);
        break;

    case SUSPENDED:
        set_state(STATE_SUSPENDED, error_code);
        break;

    case ERROR:
        set_state(STATE_ERROR, error_code);
        break;
    }
}

void Session::Impl::rfbtvpm_reconnect(bool do_immediately)
{
    // Our mutex is already locked here

    CLOUDTV_LOG_DEBUG("state:%s\n", rfbtvpm_get_state_name(m_rfbtv_state));

    // We have our first connect
    m_connect_attempts = 0;

    if (do_immediately) {
        // Issue a new connection request immediately
        m_event_queue.put(new TriggerEvent(*this, &Impl::handle_connect_event));
    } else {
        // Do an initial back-off of 5-15 seconds; apparently the server just died
        int timeout_in_ms = 5000 + rand() % 10000;

        m_timer.start_timer(m_connection_backoff_time_callback, timeout_in_ms, TimerEngine::ONE_SHOT);
    }

    rfbtvpm_set_state(CONNECTING, CLIENT_ERROR_CODE_OK);
}

ResultCode Session::Impl::rfbtvpm_session_stop(ClientErrorCode error_code, RfbtvProtocol::SessionTerminateReason reason)
{
    // Our mutex is already locked here
    CLOUDTV_LOG_DEBUG("error_code:%d, state:%s\n", error_code, rfbtvpm_get_state_name(m_rfbtv_state));

    // Don't do anything if we're already stopped or need to suspend from an already suspended session
    if (is_idle()) {
        return ResultCode::SUCCESS;
    }

    // NB: This log will also flush any accumulated logs in log_message() that were not able to be transmitted yet...
    //sw_log_info(TAG,"Close session with error code %d", error_code);

    // The next session will register the log output again, if required; Any remaining logs should have been flushed by the line above.
    ClientContext::instance().unregister_log_output(*this);

    m_closing_suspended = false;

    // Re-open session to send a terminate indication if we're currently suspended.
    if (is_suspended()) {
        m_closing_suspended = true;
        rfbtvpm_reconnect(true);
        return ResultCode::SUCCESS;
    }

    // Clean up any active CDM sessions
    rfbtvpm_clean_active_cdm_sessions();

    // When suspending, we keep the session ID, otherwise we clear it
    m_session_id = "";

    // We should not send any session terminate indication if the session is still being set up
    bool send_session_terminate_indication = !(m_rfbtv_state == CONNECTING || m_rfbtv_state == OPENING);

    if (error_code == CLIENT_ERROR_CODE_OK_AND_DO_NOT_RETUNE) {
        // We always must stop the stream error timer, if necessary.
        m_timer.cancel_timer(m_stream_error_callback);
    } else {
        // If the player is streaming, we'll stop it in the next lines and it will
        // send a PLAYER_STOPPED event in that case. However, we will not be able
        // to send that event to the server because the session will have been closed
        // by then. Therefore, we'll pretend the PLAYER_STOPPED event being sent
        // immediately and 'handle' it here. This will update the playback state and
        // send a playback report if necessary. (CTV-25091)
        //
        // (Using a local variable prevents a compiler error with mipsel-linux-uclibc-g++ 4.2.0)
        PlayerEvent player_stopped(*this, &Impl::handle_player_event, IMediaPlayer::PLAYER_STOPPED);
        handle_player_event(player_stopped);

        // Make sure the stream is stopped, if we had any running.
        stop_streaming();
    }

    // Disable reporting
    m_playback_report_manager.disable_reports();
    m_timer.cancel_timer(m_playback_report_periodic_trigger);
    m_latency_report_manager.disable_reports();
    m_log_report_manager.disable_reports();

    ResultCode ret = ResultCode::SUCCESS;
    if (send_session_terminate_indication) {
        ret = rfbtvpm_send_message(m_rfbtv_protocol.create_session_terminate_indication(reason));
    }

    rfbtvpm_close_connection();

    // Set the state and tell the client code that the session has been closed.
    rfbtvpm_set_state((error_code == CLIENT_ERROR_CODE_OK || error_code == CLIENT_ERROR_CODE_OK_AND_DO_NOT_RETUNE) ? INIT : ERROR, error_code);

    return ret;
}

ResultCode Session::Impl::rfbtvpm_session_suspend()
{
    // Our mutex is already locked here
    CLOUDTV_LOG_DEBUG("state:%s", rfbtvpm_get_state_name(m_rfbtv_state));

    // Don't do anything if we're already suspended
    if (is_suspended()) {
        return ResultCode::SUCCESS;
    }

    // Suspend session if active; otherwise close with an error.
    if (!is_active()) {
        CLOUDTV_LOG_DEBUG("Session is not running");
        return INVALID_STATE;
    }

    ResultCode ret = rfbtvpm_send_message(m_rfbtv_protocol.create_session_terminate_indication(RfbtvProtocol::SESSION_TERMINATE_SUSPEND));

    // Make sure the stream is stopped, if we had any running.
    stop_streaming();

    rfbtvpm_close_connection();

    // Tell the client code that the session has been suspended.
    rfbtvpm_set_state(SUSPENDED, CLIENT_ERROR_CODE_OK);

    return ret;
}

ResultCode Session::Impl::rfbtvpm_handle_rfbtv_version_string(RfbtvMessage &message)
{
    // Our mutex is already locked here

    CLOUDTV_LOG_DEBUG("test");

    // Parse the server version string
    const char *client_version_string = 0;
    ResultCode ret = m_rfbtv_protocol.parse_version_string(message, client_version_string);
    if (ret.is_error()) {
        CTVC_LOG_WARNING("RFB-TV version parsing error");
        return ret;
    }

    // Send the client version string
    RfbtvMessage msg;
    msg.write_raw((uint8_t*)client_version_string, strlen(client_version_string));

    ret = rfbtvpm_send_message(msg);
    if (ret.is_error()) {
        CTVC_LOG_WARNING("Unable to send version to server!");
        return ret;
    }

    // Compose the client identifier according to RFB-TV specification
    std::string client_id = std::string(m_context.get_manufacturer()) + "-" + m_context.get_device_type() + "_" + m_context.get_unique_id();
    CLOUDTV_LOG_DEBUG("client_id:%s", client_id.c_str());

    // Cookie, only set when we have one
    std::string cookie;
    ClientContext::instance().get_data_store().get_data("cookie.txt", cookie);

    // Send the session setup message
    ret = rfbtvpm_send_message(m_rfbtv_protocol.create_session_setup(client_id, m_param_list, m_session_id, cookie));
    if (ret.is_error()) {
        CTVC_LOG_WARNING("Unable to send session setup to server!");
        return ret;
    }

    rfbtvpm_set_state(OPENING, CLIENT_ERROR_CODE_OK);

    return ResultCode::SUCCESS;
}

void Session::Impl::rfbtvpm_send_appropriate_stream_confirm_error(const IMediaPlayer::PlayerEvent &event)
{
    CLOUDTV_LOG_DEBUG("test");

    RfbtvProtocol::StreamConfirmCode code = RfbtvProtocol::STREAM_CONFIRM_UNSPECIFIED_ERROR;
    switch (event) {
    case IMediaPlayer::PLAYER_STARTING:
    case IMediaPlayer::PLAYER_STARTED:
    case IMediaPlayer::PLAYER_STOPPED:
        // Unexpected state, send 'unspecified' error
        break;

    case IMediaPlayer::PLAYER_BUFFER_UNDERRUN:
    case IMediaPlayer::PLAYER_BUFFER_OVERRUN:
    case IMediaPlayer::PLAYER_RECOVERABLE_ERROR:
    case IMediaPlayer::PLAYER_UNRECOVERABLE_ERROR:
    case IMediaPlayer::PLAYER_DECODE_ERROR:
        code = RfbtvProtocol::STREAM_CONFIRM_DECODE_ERROR;
        break;

    case IMediaPlayer::PLAYER_DESCRAMBLE_ERROR:
        code = RfbtvProtocol::STREAM_CONFIRM_DESCRAMBLE_ERROR;
        break;

    case IMediaPlayer::PLAYER_TRANSPORT_STREAM_ID_ERROR:
        code = RfbtvProtocol::STREAM_CONFIRM_TSID_ERROR;
        break;

    case IMediaPlayer::PLAYER_NETWORK_ID_ERROR:
        code = RfbtvProtocol::STREAM_CONFIRM_NID_ERROR;
        break;

    case IMediaPlayer::PLAYER_PROGRAM_ID_ERROR:
        code = RfbtvProtocol::STREAM_CONFIRM_PID_ERROR;
        break;

    case IMediaPlayer::PLAYER_PHYSICAL_ERROR:
        code = RfbtvProtocol::STREAM_CONFIRM_PHYSICAL_ERROR;
        break;
    }

    if (m_stream_confirm_sent_state != STREAM_CONFIRM_ERROR_SENT) {
        m_stream_confirm_sent_state = STREAM_CONFIRM_ERROR_SENT;
        rfbtvpm_send_message(m_rfbtv_protocol.create_stream_confirm(code));
    }
}

Session::Impl::OverlayHandler::OverlayHandler(Impl &impl, IOverlayCallbacks *overlay_callbacks) :
    m_impl(impl),
    m_overlay_callbacks(overlay_callbacks),
    m_thread("Session overlay handler"),
    m_content_loader(0)
{
}

Session::Impl::OverlayHandler::~OverlayHandler()
{
    stop();
}

void Session::Impl::OverlayHandler::start(IContentLoader *content_loader)
{
    if (m_overlay_callbacks) {
        // Always stop because content_loader may have changed.
        stop();
        m_content_loader = content_loader;
        ResultCode result = m_thread.start(*this, Thread::PRIO_NORMAL);
        if (result.is_error()) {
            CLOUDTV_LOG_DEBUG("m_thread::start() failed:%s", result.get_description());
        }
    }
}

void Session::Impl::OverlayHandler::stop()
{
    if (m_overlay_callbacks) {
        m_thread.stop();
        m_new_overlays_available.put(new NullEvent());
        m_thread.wait_until_stopped();
    }
}

void Session::Impl::OverlayHandler::process_images(const std::vector<PictureParameters> &images, bool clear_flag, bool commit_flag)
{
    m_new_overlays_available.put(new OverlaysAvailableEvent(*this, &Session::Impl::OverlayHandler::handle_overlay_event, images, clear_flag, commit_flag));
}

bool Session::Impl::OverlayHandler::run()
{
    const IEvent *event = m_new_overlays_available.get();
    event->handle();
    delete event;
    return false;
}

void Session::Impl::OverlayHandler::handle_overlay_event(const OverlaysAvailableEvent &event)
{
    assert(m_overlay_callbacks);

    // It is actual safe to modify the content - and we need to when downloading via URL - of the images.
    std::vector<PictureParameters> &images = event.images();

    CLOUDTV_LOG_DEBUG("Request to handle framebuffer update with %u rectangles (m_content_loader:%p)", (uint32_t)images.size(), m_content_loader);

    // If necessary, fetch images from the remote server
    if (m_content_loader) {
        std::vector<IContentLoader::IContentResult *> loading_results(images.size());
        for (size_t i = 0; i < images.size(); i++) {
            if (!images[i].m_url.empty()) {
                //sw_log_info(TAG,"Downloading image from [%s]", images[i].m_url.c_str());
                loading_results[i] = m_content_loader->load_content(images[i].m_url, images[i].m_data);
            }
        }

        // Wait for ALL images to be downloaded, so we can blit them in the correct order.
        for (size_t i = 0; i < images.size() && !m_thread.must_stop(); i++) {
            if (!images[i].m_url.empty()) {
                if (loading_results[i] != 0) {
                    // This call blocks until the result (positive or negative) is known
                    ResultCode ret = loading_results[i]->wait_for_result();
                    if (ret.is_error()) {
                        // The user of the SDK has to deal with this error by showing an empty image or an error image
                        CTVC_LOG_WARNING("There was an error downloading image from [%s]", images[i].m_url.c_str());
                    }
                    m_content_loader->release_content_result(loading_results[i]);
                } else {
                    CLOUDTV_LOG_DEBUG("IContentLoader::IContentResult NULL object was returned from IContentLoader::load_content()");
                }
            }
        }
    }

    if (!m_thread.must_stop()) {
        // Indicate to server that we're ready to receive the next update (if any).
        m_impl.post_frame_buffer_update_request();

        if (event.clear_flag()) {
            CLOUDTV_LOG_DEBUG("CLEAR");
            m_overlay_callbacks->overlay_clear();
        }

        for (size_t i = 0; i < event.images().size(); i++) {
            if (!event.images()[i].m_data.empty()) {
                CLOUDTV_LOG_DEBUG("IMAGE");
                m_overlay_callbacks->overlay_blit_image(event.images()[i]);
            }
        }

        if (event.commit_flag()) {
            CLOUDTV_LOG_DEBUG("FLIP");
            m_overlay_callbacks->overlay_flip();
        }
    }
}

ResultCode Session::Impl::frame_buffer_update(std::vector<PictureParameters> &images, bool clear_flag, bool commit_flag)
{
    // Called from m_rfbtv_protocol.parse_message() as part of RfbtvProtocol::ICallbacks; our mutex is already locked here

    CLOUDTV_LOG_DEBUG("%lu rectangles, clear_flag:%d, commit_flag:%d", static_cast<unsigned long>(images.size()), clear_flag, commit_flag);

    if (!m_overlay_callbacks) {
        CLOUDTV_LOG_DEBUG("Received a framebuffer update, but client has not installed a handler for it");
        return ResultCode::SUCCESS;
    }

    m_overlay_handler.process_images(images, clear_flag, commit_flag);

    return ResultCode::SUCCESS;
}

ResultCode Session::Impl::session_setup_response(RfbtvProtocol::ICallbacks::SessionSetupResult result, const std::string &session_id, const std::string &redirect_url, const std::string &cookie)
{
    // Called from m_rfbtv_protocol.parse_message() as part of RfbtvProtocol::ICallbacks; our mutex is already locked here

    CLOUDTV_LOG_DEBUG("result:%d, session_id:%s, redirect_url:%s, cookie:%s", result, session_id.c_str(), redirect_url.c_str(), cookie.c_str());

    m_session_id = session_id;

    CLOUDTV_LOG_DEBUG("Storing cookie:%s", cookie.c_str());
    ResultCode ret = ClientContext::instance().get_data_store().set_data("cookie.txt", cookie);
    if (ret.is_error()) {
        CLOUDTV_LOG_DEBUG("Can't store cookie");
        return ret;
    }

    if (result == RfbtvProtocol::ICallbacks::SESSION_SETUP_REDIRECT) {
        //sw_log_info(TAG,"Received redirect to %s", redirect_url.c_str());
        rfbtvpm_close_connection();

        if (m_redirect_count >= MAX_RFBTV_REDIRECTS) {
            CLOUDTV_LOG_DEBUG("Too many redirects");
            return TOO_MANY_REDIRECTS;
        }

        //sw_log_info(TAG,"Redirect session to %s", redirect_url.c_str());

        m_redirect_count++;

        rfbtvpm_set_state(REDIRECTED, CLIENT_ERROR_CODE_OK);

        std::string url; // Posting an empty url will keep m_param_list["url"] intact in handle_initiate_event()
        m_event_queue.put(new InitiateEvent(*this, &Impl::handle_initiate_event, redirect_url, url, m_screen_width, m_screen_height, m_param_list, m_session_start_time));

        return ResultCode::SUCCESS;
    }

    if (result == RfbtvProtocol::ICallbacks::SESSION_SETUP_OK) {
        CLOUDTV_LOG_DEBUG("Session Setup complete");
    } else {
        CLOUDTV_LOG_DEBUG("Server setup error %d", result);

        // Map the setup result codes to client error codes according to 'CloudTV Client Error Code Specification' version 1.4
        static const std::map<RfbtvProtocol::ICallbacks::SessionSetupResult, ClientErrorCode> s_session_setup_result_map = create_map<RfbtvProtocol::ICallbacks::SessionSetupResult, ClientErrorCode>
            (RfbtvProtocol::ICallbacks::SESSION_SETUP_INVALID_CLIENT_ID, CLIENT_ERROR_CODE_140)
            (RfbtvProtocol::ICallbacks::SESSION_SETUP_APP_NOT_FOUND, CLIENT_ERROR_CODE_140)
            (RfbtvProtocol::ICallbacks::SESSION_SETUP_CONFIG_ERROR, CLIENT_ERROR_CODE_120)
            (RfbtvProtocol::ICallbacks::SESSION_SETUP_NO_RESOURCES, CLIENT_ERROR_CODE_160)
            (RfbtvProtocol::ICallbacks::SESSION_SETUP_UNSPECIFIED_ERROR, CLIENT_ERROR_CODE_190)
            (RfbtvProtocol::ICallbacks::SESSION_SETUP_INVALID_PARAMETERS, CLIENT_ERROR_CODE_240)
            (RfbtvProtocol::ICallbacks::SESSION_SETUP_INTERNAL_SERVER_ERROR, CLIENT_ERROR_CODE_210)
            (RfbtvProtocol::ICallbacks::SESSION_SETUP_UNDEFINED_ERROR, CLIENT_ERROR_CODE_190);

        ClientErrorCode error_code = CLIENT_ERROR_CODE_190; // Other codes will map to this one as well...
        std::map<RfbtvProtocol::ICallbacks::SessionSetupResult, ClientErrorCode>::const_iterator i = s_session_setup_result_map.find(result);
        if (i != s_session_setup_result_map.end()) {
            error_code = i->second;
        }

        return rfbtvpm_session_stop(error_code, RfbtvProtocol::SESSION_TERMINATE_NORMAL);
    }

    // The session has been set up now
    rfbtvpm_set_state(ACTIVE, CLIENT_ERROR_CODE_OK);

    // If reconnecting because we're about to close a suspended session, terminate normally now
    if (m_closing_suspended) {
        //sw_log_info(TAG,"Closing suspended session");
        return rfbtvpm_session_stop(CLIENT_ERROR_CODE_OK, RfbtvProtocol::SESSION_TERMINATE_NORMAL);
    }

    // Send the list of supported encodings
    ret = rfbtvpm_send_message(m_rfbtv_protocol.create_set_encodings(m_content_loader != 0));
    if (ret.is_error()) {
        CTVC_LOG_WARNING("Unable to send encodings to server!");
        return ret;
    }

    // Tell server we are ready for receiving update requests, even if the client did not register an overlay handler.
    ret = rfbtvpm_send_message(m_rfbtv_protocol.create_frame_buffer_update_request(m_screen_width, m_screen_height));
    if (ret.is_error()) {
        CTVC_LOG_WARNING("Unable to send frame buffer update request to server!");
        return ret;
    }

    return ResultCode::SUCCESS;
}

ResultCode Session::Impl::session_terminate_request(RfbtvProtocol::ICallbacks::SessionTerminateReason code)
{
    // Called from m_rfbtv_protocol.parse_message() as part of RfbtvProtocol::ICallbacks; our mutex is already locked here

    CLOUDTV_LOG_DEBUG("code:%d", code);

    if (code == RfbtvProtocol::ICallbacks::SESSION_TERMINATE_SUSPEND) {
        return rfbtvpm_session_suspend();
    }

    // Map the session terminate request codes to client error codes according to 'CloudTV Client Error Code Specification' version 1.4
    static const std::map<RfbtvProtocol::ICallbacks::SessionTerminateReason, ClientErrorCode> s_session_setup_result_map = create_map<RfbtvProtocol::ICallbacks::SessionTerminateReason, ClientErrorCode>
        (RfbtvProtocol::ICallbacks::SESSION_TERMINATE_USER_STOP, CLIENT_ERROR_CODE_OK)
        (RfbtvProtocol::ICallbacks::SESSION_TERMINATE_INSUFFICIENT_BANDWIDTH, CLIENT_ERROR_CODE_150)
        (RfbtvProtocol::ICallbacks::SESSION_TERMINATE_LATENCY_TOO_LARGE, CLIENT_ERROR_CODE_170)
        (RfbtvProtocol::ICallbacks::SESSION_TERMINATE_UNSPECIFIED_ERROR, CLIENT_ERROR_CODE_190)
        (RfbtvProtocol::ICallbacks::SESSION_TERMINATE_DO_NOT_RETUNE, CLIENT_ERROR_CODE_OK_AND_DO_NOT_RETUNE)
        (RfbtvProtocol::ICallbacks::SESSION_TERMINATE_PING_TIMEOUT, CLIENT_ERROR_CODE_200)
        (RfbtvProtocol::ICallbacks::SESSION_TERMINATE_INTERNAL_SERVERERROR, CLIENT_ERROR_CODE_210)
        (RfbtvProtocol::ICallbacks::SESSION_TERMINATE_SERVER_SHUTTINGDOWN, CLIENT_ERROR_CODE_220)
        (RfbtvProtocol::ICallbacks::SESSION_TERMINATE_FAILED_APPLICATION_STREAM_SETUP, CLIENT_ERROR_CODE_230)
        (RfbtvProtocol::ICallbacks::SESSION_TERMINATE_UNDEFINED_ERROR, CLIENT_ERROR_CODE_190);

    ClientErrorCode error_code = CLIENT_ERROR_CODE_190; // Other codes will map to this one as well...
    std::map<RfbtvProtocol::ICallbacks::SessionTerminateReason, ClientErrorCode>::const_iterator i = s_session_setup_result_map.find(code);
    if (i != s_session_setup_result_map.end()) {
        error_code = i->second;
    }

    return rfbtvpm_session_stop(error_code, RfbtvProtocol::SESSION_TERMINATE_NORMAL);
}

ResultCode Session::Impl::ping()
{
    // Called from m_rfbtv_protocol.parse_message() as part of RfbtvProtocol::ICallbacks; our mutex is already locked here

    CLOUDTV_LOG_DEBUG("test");

    // Ping is 1 byte message type, message type is already read, nothing to do
    // Send back pong to indicate we are alive
    return rfbtvpm_send_message(m_rfbtv_protocol.create_pong());
}

ResultCode Session::Impl::stream_setup_request(const std::string &uri, const std::map<std::string, std::string> &stream_params)
{
    // Called from m_rfbtv_protocol.parse_message() as part of RfbtvProtocol::ICallbacks; our mutex is already locked here
    CLOUDTV_LOG_DEBUG("Received %lu parameters", static_cast<unsigned long>(stream_params.size()));
    for (std::map<std::string, std::string>::const_iterator it = stream_params.begin(); it != stream_params.end(); ++it) {
        CLOUDTV_LOG_DEBUG("StreamSetupRequest parameter: [%s]->[%s]", it->first.c_str(), it->second.c_str());
    }

    //sw_log_info(TAG,"Opening url '%s'", uri.c_str());

    // Server may be recovering from cluster fail-over
    if (m_current_stream_uri.compare(uri) == 0) {
        //sw_log_info(TAG,"Current URI already playing, request ignored");

        ResultCode ret = rfbtvpm_send_message(m_rfbtv_protocol.create_stream_setup_response(RfbtvProtocol::STREAM_SETUP_SUCCESS, std::map<std::string, std::string>(), m_local_udp_url));
        if (ret.is_error()) {
            return ret;
        }

        return rfbtvpm_send_message(m_rfbtv_protocol.create_stream_confirm(RfbtvProtocol::STREAM_CONFIRM_SUCCESS)); // TODO (CTV-27819): Send another StreamConfirm code if there was an error before the reconnect.
    }

    // Stop any running stream
    stop_streaming();

    // Clear stalled_duration of last playback report because it's accumulative and needs reinitialization
    // before the next stream is started.
    m_playback_report.m_stalled_duration_in_ms.reset();

    // Set the current uri that we (intend to) play
    m_current_stream_uri = uri;

    // Server indicates to stop playing and blank the screen
    // (Stopping a stream is indicated by opening an empty URL)
    if (uri.empty()) {
        ResultCode ret = rfbtvpm_send_message(m_rfbtv_protocol.create_stream_setup_response(RfbtvProtocol::STREAM_SETUP_SUCCESS, std::map<std::string, std::string>(), m_local_udp_url));
        if (ret.is_error()) {
            return ret;
        }

        ret = rfbtvpm_send_message(m_rfbtv_protocol.create_stream_confirm(RfbtvProtocol::STREAM_CONFIRM_SUCCESS));
        if (ret.is_error()) {
            return ret;
        }

        if (m_overlay_callbacks) {
            m_overlay_callbacks->overlay_clear(); // Specified by the RFB-TV protocol
        }

        return ret;
    }

    // Start the player and the streamer
    bool all_succeeded = false;

    ResultCode ret = m_timer.start_timer(m_streamer_periodic_trigger, STREAMER_TRIGGER_PERIOD_IN_MS, TimerEngine::PERIODIC);
    if (ret.is_ok()) {
        ret = m_streamer.start_stream(uri, stream_params);
    }

    // Check the error codes and signal the correct replies
    if (ret.is_ok()) {
        ret = rfbtvpm_send_message(m_rfbtv_protocol.create_stream_setup_response(RfbtvProtocol::STREAM_SETUP_SUCCESS, std::map<std::string, std::string>(), m_local_udp_url));
        if (ret.is_ok()) {
            all_succeeded = true;
        }
    } else {
        RfbtvProtocol::StreamSetupResponseCode code = RfbtvProtocol::STREAM_SETUP_UNSPECIFIED_ERROR;

        if (ret == Streamer::INVALID_PARAMETER || ret == Streamer::PROTOCOL_NOT_REGISTERED) {
            code = RfbtvProtocol::STREAM_SETUP_UNSUPPORTED_URI;
        } else if (ret == IMediaPlayer::CABLE_TUNING_ERROR) {
            code = RfbtvProtocol::STREAM_SETUP_CABLE_TUNING_ERROR;
        } else if (ret == Streamer::CANNOT_CREATE_MEDIA_PLAYER) {
            code = RfbtvProtocol::STREAM_SETUP_IP_RESOURCE_ERROR;
        } else {
            // All others, including IMediaPlayer::CONNECTION_FAILED and user-defined error codes, fall into this category.
            code = RfbtvProtocol::STREAM_SETUP_CONNECTION_FAILED;
        }

        // The stream setup error is handled by RFB-TV so we overwrite the error code.
        ret = rfbtvpm_send_message(m_rfbtv_protocol.create_stream_setup_response(code, std::map<std::string, std::string>(), m_local_udp_url));
    }

    // Stop the streamer and the player if there were any errors
    if (!all_succeeded) {
        stop_streaming();
    }

    return ret;
}

ResultCode Session::Impl::passthrough(const std::string &protocol_id, const std::vector<uint8_t> &data)
{
    // Called from m_rfbtv_protocol.parse_message() as part of RfbtvProtocol::ICallbacks; our mutex is already locked here

    CLOUDTV_LOG_DEBUG("test");

    std::map<std::string, IProtocolExtension *>::iterator it = m_protocol_extensions.find(protocol_id);
    if (it == m_protocol_extensions.end()) {
        if (m_default_handler) {
            CLOUDTV_LOG_DEBUG("Sending message to default handler.");
            m_default_handler->received(protocol_id.c_str(), &data[0], data.size());
        } else {
            CTVC_LOG_WARNING("Received passthrough for protocol '%s', but there's neither handler registered nor default handler.", protocol_id.c_str());
        }

        return ResultCode::SUCCESS;
    }

    //sw_log_info(TAG,"Received passthrough for protocol '%s'", protocol_id.c_str());
    (*it).second->received(&data[0], data.size());

    return ResultCode::SUCCESS;
}

ResultCode Session::Impl::server_command_keyfilter_control(const std::string &local_keys, const std::string &remote_keys)
{
    // Called from m_rfbtv_protocol.parse_message() as part of RfbtvProtocol::ICallbacks; our mutex is already locked here

    CLOUDTV_LOG_DEBUG("test");

    m_key_filter.parse_lists(local_keys, remote_keys);

    return ResultCode::SUCCESS;
}

ResultCode Session::Impl::server_command_playback_control(ReportMode report_mode, uint32_t interval_in_ms)
{
    // Called from m_rfbtv_protocol.parse_message() as part of RfbtvProtocol::ICallbacks; our mutex is already locked here

    CLOUDTV_LOG_DEBUG("test");

    switch (report_mode) {
    case REPORT_DISABLED:
        m_playback_report_manager.disable_reports();
        m_timer.cancel_timer(m_playback_report_periodic_trigger);
        break;
    case REPORT_ONE_SHOT:
        m_playback_report_manager.generate_report();
        break;
    case REPORT_AUTOMATIC:
        m_playback_report_manager.enable_triggered_reports();
        m_playback_report_manager.enable_periodic_reports(interval_in_ms);
        if (interval_in_ms > 0) {
            // Start the timer to regularly call the playback report manager at a fixed rate.
            // We could also call the report manager at the intended rate. This would be more
            // efficient but the report manager then has no way to account for reports being
            // sent in between periodic calls (even after a redesign). Therefore we fall back
            // to simply periodically calling the report manager.
            ResultCode ret = m_timer.start_timer(m_playback_report_periodic_trigger, REPORT_TRIGGER_PERIOD_IN_MS, TimerEngine::PERIODIC);
            if (ret.is_error() && ret != TimerEngine::TIMER_ALREADY_REGISTERED) {
                CLOUDTV_LOG_DEBUG("Unable to start playback report timer");
                // This error is not fatal enough to stop the session for it. Otherwise, return ret;
            }
        } else {
            m_timer.cancel_timer(m_playback_report_periodic_trigger);
        }
        break;
    default:
        // No change
        break;
    }

    return ResultCode::SUCCESS;
}

ResultCode Session::Impl::server_command_latency_control(ReportMode report_mode, bool is_duration, bool is_event)
{
    // Called from m_rfbtv_protocol.parse_message() as part of RfbtvProtocol::ICallbacks; our mutex is already locked here

    CLOUDTV_LOG_DEBUG("test");

    switch (report_mode) {
    case REPORT_DISABLED:
        m_latency_report_manager.disable_reports();
        break;
    case REPORT_ONE_SHOT:
        m_latency_report_manager.generate_report();
        break;
    case REPORT_AUTOMATIC:
        m_latency_report_manager.enable_triggered_reports();
        break;
    case REPORT_NOCHANGE:
    default:
        // No change
        break;
    }

    int mode = 0;
    mode |= is_duration ? LatencyReport::MEASUREMENT_MODE_DURATION : 0;
    mode |= is_event ? LatencyReport::MEASUREMENT_MODE_EVENT : 0;
    m_latency_report.set_measurement_mode(mode);

    return ResultCode::SUCCESS;
}

ResultCode Session::Impl::server_command_log_control(ReportMode report_mode, LogMessageType min_log_level)
{
    // Called from m_rfbtv_protocol.parse_message() as part of RfbtvProtocol::ICallbacks; our mutex is already locked here

    CLOUDTV_LOG_DEBUG("test");

    switch (min_log_level) {
    case LOG_ERROR:
    case LOG_WARNING:
    case LOG_INFO:
    case LOG_DEBUG:
        m_log_report.set_min_level(min_log_level);
        break;
    default:
        // No change
        break;
    }

    switch (report_mode) {
    case REPORT_DISABLED:
        ClientContext::instance().unregister_log_output(*this);
        m_log_report_manager.disable_reports();
        break;
    case REPORT_ONE_SHOT:
        m_log_report_manager.generate_report();
        break;
    case REPORT_AUTOMATIC:
        ClientContext::instance().register_log_output(*this);
        m_log_report_manager.enable_triggered_reports();
        break;
    case REPORT_ACCUMULATE:
        ClientContext::instance().register_log_output(*this);
        m_log_report_manager.disable_reports();
        break;
    case REPORT_NOCHANGE:
    default:
        // No change
        break;
    }

    return ResultCode::SUCCESS;
}

ResultCode Session::Impl::server_command_video_control(VideoMode mode)
{
    // Called from m_rfbtv_protocol.parse_message() as part of RfbtvProtocol::ICallbacks; our mutex is already locked here

    CLOUDTV_LOG_DEBUG("test");

    switch (mode) {
    case MODE_GUI_OPTIMIZED:
        // TODO: (CNP-2031) Switch client decoder to low-latency 'game mode'
        break;
    case MODE_VIDEO_OPTIMIZED:
        // TODO: (CNP-2031) Switch client decoder to regular decoding mode
        break;
    case MODE_NOCHANGE:
    default:
        // No change
        break;
    }

    return ResultCode::SUCCESS;
}

ResultCode Session::Impl::server_command_underrun_mitigation_control(const std::map<std::string, std::string> &parameter_value_pairs)
{
    // Called from m_rfbtv_protocol.parse_message() as part of RfbtvProtocol::ICallbacks; our mutex is already locked here

    CLOUDTV_LOG_DEBUG("test");

    for (std::map<std::string, std::string>::const_iterator i = parameter_value_pairs.begin(); i != parameter_value_pairs.end(); ++i) {
        if (i->first == "enabled") {
            // This is a special parameter and can be set to 'true' or 'false'
            // Since the rplayer can have multiple features enabled, we should actually merge
            // enabling the underrun mitigator with the other rplayer enabled features.
            // However, for simplicitly we assume that only RAMS is enabled by default.
            // That is true currently (set in Streamer.cpp) but may change unnoticed.
            // TODO: Change when the underrun mitigation feature and its control matures
            // so we know better what to control and how.
            m_streamer.set_rplayer_parameter("enabled_features", i->second == "true" ? "rams | underrun" : "rams");
        } else {
            // Otherwise just pass the parameter/value pair straight on to rplayer.
            m_streamer.set_rplayer_parameter(i->first, i->second);
        }
    }

    return ResultCode::SUCCESS;
}

ResultCode Session::Impl::handoff_request(const std::string &uri, bool resume_session_when_done)
{
    // Called from m_rfbtv_protocol.parse_message() as part of RfbtvProtocol::ICallbacks; our mutex is already locked here

    //sw_log_info(TAG,"Received handoff request with uri '%s'", uri.c_str());

    IHandoffHandler::HandoffResult result = IHandoffHandler::HANDOFF_UNSUPPORTED_URI;

    std::string::size_type n = uri.find(':');
    if (n != std::string::npos) {
        std::string scheme = uri.substr(0, n);
        std::string arg = uri.substr(n + 1);

        std::map<std::string, IHandoffHandler *>::iterator it = m_handoff_handlers.find(scheme);
        if (it != m_handoff_handlers.end()) {
            result = it->second->handoff_request(scheme, arg, resume_session_when_done);

            if (result == IHandoffHandler::HANDOFF_SUCCESS) {
                ResultCode ret;
                if (resume_session_when_done) {
                    //sw_log_info(TAG,"Close session with reason SESSION_TERMINATE_SUSPEND");
                    // Report to server that handoff was successful and this session suspends
                    ret = rfbtvpm_session_suspend();
                } else {
                    //sw_log_info(TAG,"Close session with reason SESSION_TERMINATE_HANDOFF");
                    // Report to server that handoff was successful and this session terminates
                    ret = rfbtvpm_session_stop(CLIENT_ERROR_CODE_OK, RfbtvProtocol::SESSION_TERMINATE_HANDOFF);
                }
                return ret;
            } else {
                CTVC_LOG_WARNING("Received handoff request for scheme '%s', but the handler returned an error.", scheme.c_str());
            }
        } else {
            CTVC_LOG_WARNING("Received handoff request for scheme '%s', but there's no handler registered.", scheme.c_str());
        }
    } else {
        CTVC_LOG_WARNING("Received handoff request without scheme '%s'.", uri.c_str());
    }

    return rfbtvpm_send_message(m_rfbtv_protocol.create_handoff_result(result, ""));
}

ResultCode Session::Impl::cdm_setup_request(const std::string &cdm_session_id, const uint8_t (&drm_system_id)[16], const std::string &session_type, const std::map<std::string, std::string> &init_data)
{
    // Called from m_rfbtv_protocol.parse_message() as part of RfbtvProtocol::ICallbacks; our mutex is already locked here

    //sw_log_info(TAG,"Setting up CDM session with ID '%s'", cdm_session_id.c_str());

    std::map<std::string, CdmSessionContainer *>::iterator i = m_active_cdm_sessions.find(cdm_session_id);
    if (i != m_active_cdm_sessions.end()) {
        CTVC_LOG_WARNING("CDM session with cdm_session_id '%s' already active", cdm_session_id.c_str());
        // CDM session already exists, destroy it first and remove from the set of sessions
        // It usually is an error condition, but it may be that a failing and recovering CSM retries
        // setting up the same CDM session from a different node.

        // Unregister any active decrypt engine.
        m_streamer.register_stream_decrypt_engine(0);

        delete i->second;
        m_active_cdm_sessions.erase(i);

        // Update the active stream decrypt engine, if any.
        rfbtvpm_register_active_cdm_stream_decrypt_engine();
    }

    std::map<std::string, std::string> response;

    // Find the corresponding DRM system
    ICdmSessionFactory *factory = 0;
    for (std::vector<ICdmSessionFactory *>::iterator it = m_drm_systems.begin(); it != m_drm_systems.end(); ++it) {
        uint8_t id[16];
        (*it)->get_drm_system_id(id);

        if (memcmp(id, drm_system_id, sizeof(id)) == 0) {
            factory = *it;
            break;
        }
    }

    if (!factory) {
        CLOUDTV_LOG_DEBUG("No registered DRM system found with given DRM system ID (%s)", id_to_guid_string(drm_system_id).c_str());
        return rfbtvpm_send_message(m_rfbtv_protocol.create_cdm_setup_response(cdm_session_id, RfbtvProtocol::CDM_SESSION_SETUP_RESPONSE_RESULT_DRM_SYSTEM_NOT_INSTALLED, response));
    }

    // Create and register the new session
    ICdmSession *session = factory->create();

    if (!session) {
        CLOUDTV_LOG_DEBUG("CDM session could not be created");
        return rfbtvpm_send_message(m_rfbtv_protocol.create_cdm_setup_response(cdm_session_id, RfbtvProtocol::CDM_SESSION_SETUP_RESPONSE_RESULT_DRM_SYSTEM_ERROR, response));
    }

    CdmSessionContainer *container = new CdmSessionContainer(*this, cdm_session_id, *session, *factory);

    // Setup the session (asynchronously), result will be reported via the cdm_setup_result() callback
    container->setup(session_type, init_data);

    return ResultCode::SUCCESS;
}

ResultCode Session::Impl::cdm_terminate_request(const std::string &cdm_session_id, RfbtvProtocol::ICallbacks::CdmSessionTerminateReason /*reason*/)
{
    // Called from m_rfbtv_protocol.parse_message() as part of RfbtvProtocol::ICallbacks; our mutex is already locked here

    return rfbtvpm_cdm_session_terminate(cdm_session_id, RfbtvProtocol::CDM_SESSION_TERMINATE_RESPONSE_REASON_SERVER_REQUEST);
}

void Session::Impl::cdm_session_terminate_indication(const std::string &cdm_session_id, ICdmSession::ICallback::TerminateReason reason)
{
    // Asynchronously called from an ICdmSession instance

    //sw_log_info(TAG,"CDM indicated it terminated with reason %d", (int) reason);

    RfbtvProtocol::CdmSessionTerminateResponseReason code;
    switch (reason) {
    case ICdmSession::ICallback::TERMINATE_USER_STOP:
        code = RfbtvProtocol::CDM_SESSION_TERMINATE_RESPONSE_REASON_USER_STOP;
        break;
    case ICdmSession::ICallback::TERMINATE_END_OF_STREAM:
        code = RfbtvProtocol::CDM_SESSION_TERMINATE_RESPONSE_REASON_END_OF_STREAM;
        break;
    case ICdmSession::ICallback::TERMINATE_LICENSE_EXPIRED:
        code = RfbtvProtocol::CDM_SESSION_TERMINATE_RESPONSE_REASON_LICENSE_EXPIRED;
        break;
    case ICdmSession::ICallback::TERMINATE_UNSPECIFIED:
    default:
        code = RfbtvProtocol::CDM_SESSION_TERMINATE_RESPONSE_REASON_OTHER;
        break;
    }

    m_event_queue.put(new CdmSessionTerminateEvent(*this, &Impl::handle_cdm_session_terminate_event, cdm_session_id, code));
}

void Session::Impl::cdm_setup_result(const std::string &cdm_session_id, ICdmSession::SetupResult result, const std::map<std::string, std::string> &response, CdmSessionContainer *container)
{
    m_event_queue.put(new CdmSetupResultEvent(*this, &Impl::handle_cdm_setup_result, cdm_session_id, result, response, container));
}

void Session::Impl::handle_cdm_setup_result(const CdmSetupResultEvent &event)
{
    AutoLock lck(m_mutex);

    if (m_state != STATE_CONNECTED) {
        //sw_log_info(TAG,"Got CDM session setup result in state %s", rfbtvpm_get_state_name(m_state));
        delete event.container();
        return;
    }

    // If the setup succeeded add it to our list of active CDM sessions else destroy the CDM session.
    if (event.result() == ICdmSession::SETUP_OK) {
        m_active_cdm_sessions[event.cdm_session_id()] = event.container();

        // Update the active stream decrypt engine, if any.
        rfbtvpm_register_active_cdm_stream_decrypt_engine();
    } else {
        delete event.container();
    }

    RfbtvProtocol::CdmSessionSetupResponseResult rfbtv_result;
    switch (event.result()) {
    case ICdmSession::SETUP_OK:
        rfbtv_result = RfbtvProtocol::CDM_SESSION_SETUP_RESPONSE_RESULT_SUCCESS;
        break;
    case ICdmSession::SETUP_DRM_SYSTEM_ERROR:
        rfbtv_result = RfbtvProtocol::CDM_SESSION_SETUP_RESPONSE_RESULT_DRM_SYSTEM_ERROR;
        break;
    case ICdmSession::SETUP_NO_LICENSE_SERVER:
        rfbtv_result = RfbtvProtocol::CDM_SESSION_SETUP_RESPONSE_RESULT_NO_LICENSE_SERVER;
        break;
    case ICdmSession::SETUP_LICENSE_NOT_FOUND:
        rfbtv_result = RfbtvProtocol::CDM_SESSION_SETUP_RESPONSE_RESULT_LICENSE_NOT_FOUND;
        break;
    case ICdmSession::SETUP_UNSPECIFIED_ERROR:
    default:
        rfbtv_result = RfbtvProtocol::CDM_SESSION_SETUP_RESPONSE_RESULT_UNSPECIFIED_ERROR;
        break;
    }

    // Send the CdmSetupResponse message
    rfbtvpm_send_message(m_rfbtv_protocol.create_cdm_setup_response(event.cdm_session_id(), rfbtv_result, event.response()));
}

ResultCode Session::Impl::rfbtvpm_cdm_session_terminate(const std::string &cdm_session_id, RfbtvProtocol::CdmSessionTerminateResponseReason reason)
{
    // Our mutex is already locked here

    //sw_log_info(TAG,"Terminating CDM session with ID '%s'", cdm_session_id.c_str());

    std::map<std::string, std::string> stop_data;

    // Find the indicated session
    std::map<std::string, CdmSessionContainer *>::iterator i = m_active_cdm_sessions.find(cdm_session_id);
    if (i == m_active_cdm_sessions.end()) {
        CTVC_LOG_WARNING("CDM session with cdm_session_id '%s' not found", cdm_session_id.c_str());
        if (reason == RfbtvProtocol::CDM_SESSION_TERMINATE_RESPONSE_REASON_SERVER_REQUEST) {
            return rfbtvpm_send_message(m_rfbtv_protocol.create_cdm_terminate_indication(cdm_session_id, RfbtvProtocol::CDM_SESSION_TERMINATE_RESPONSE_REASON_UNKNOWN_SESSION, stop_data));
        } else {
            return ResultCode::SUCCESS;
        }
    }

    // First get the container (the iterator will be invalidated by std::map::erase)
    CdmSessionContainer *container = i->second;

    // Remove the CdmSessionContainer object from our list
    m_active_cdm_sessions.erase(i);

    // Unregister any active decrypt engine.
    // This must be done before the call to container->terminate_session()
    // because the decrypt engine might be destroyed by it.
    // The fact that we may potentially leave undecrypted data dangling
    // we take for granted. The probability is low anyway, because the CDM
    // session is terminated, which probably has a reason.
    m_streamer.register_stream_decrypt_engine(0);

    // Terminate the CDM session (asynchronously, result will be reported via the cdm_terminate_result() callback)
    container->terminate_session(reason);

    return ResultCode::SUCCESS;
}

void Session::Impl::cdm_terminate_result(const std::string &cdm_session_id, RfbtvProtocol::CdmSessionTerminateResponseReason reason, const std::map<std::string, std::string> &stop_data, CdmSessionContainer *container)
{
    m_event_queue.put(new CdmTerminateResultEvent(*this, &Impl::handle_cdm_terminate_result, cdm_session_id, reason, stop_data, container));
}

void Session::Impl::handle_cdm_terminate_result(const CdmTerminateResultEvent &event)
{
    AutoLock lck(m_mutex);

    // Update the active stream decrypt engine, if any.
    rfbtvpm_register_active_cdm_stream_decrypt_engine();

    // Send the CdmTerminateIndication message
    rfbtvpm_send_message(m_rfbtv_protocol.create_cdm_terminate_indication(event.cdm_session_id(), event.reason(), event.stop_data()));

    delete event.container();
}

void Session::Impl::rfbtvpm_register_active_cdm_stream_decrypt_engine()
{
    // Find an active CDM session and register it's stream decrypt engine if it has one.
    // Continue until one is found, or unregister the current.
    // This probably is not correct if there are multiple active CDM sessions that use
    // multiple different decrypt engines, but it should suffice for a single CDM session
    // or when multiple concurrent sessions share the same decrypt engine. Luckily, this
    // is quite likely...
    IStreamDecrypt *p = 0;
    for (std::map<std::string, CdmSessionContainer *>::iterator i = m_active_cdm_sessions.begin(); i != m_active_cdm_sessions.end(); ++i) {
        p = i->second->get_stream_decrypt_engine();
        if (p) {
            break;
        }
    }

    m_streamer.register_stream_decrypt_engine(p);
}

void Session::Impl::rfbtvpm_clean_active_cdm_sessions()
{
    if (!m_active_cdm_sessions.empty()) {
        //sw_log_info(TAG,"Cleaning up %lu CDM sessions", static_cast<unsigned long>(m_active_cdm_sessions.size()));

        // Unregister any active decrypt engine.
        m_streamer.register_stream_decrypt_engine(0);
        // Delete all active CdmSession objects
        for (std::map<std::string, CdmSessionContainer *>::iterator i = m_active_cdm_sessions.begin(); i != m_active_cdm_sessions.end(); ++i) {
            delete i->second;
        }
        m_active_cdm_sessions.clear();
    }
}

void Session::Impl::rfbtvpm_close_connection()
{
    // Our mutex is already locked here

    CLOUDTV_LOG_DEBUG("test:%d\n", __LINE__);

    m_timer.cancel_timer(m_connection_backoff_time_callback);

    ResultCode ret = m_connection.close();
    if (ret.is_error()) {
        CLOUDTV_LOG_DEBUG("m_connection.close() failed:%s", ret.get_description());
    }
}

void Session::Impl::start_message_handling_thread()
{
    if (!m_event_handling_thread.is_running()) {
        CLOUDTV_LOG_DEBUG("Starting message handling thread.\n");
        ResultCode ret = m_event_handling_thread.start(*this, Thread::PRIO_HIGH);
        if (ret.is_error()) {
            CLOUDTV_LOG_DEBUG("m_event_handling_thread.start() failed:%s.\n", ret.get_description());
        }
    }
}

void Session::Impl::stop_message_handling_thread()
{
    m_event_handling_thread.stop();

    m_event_queue.put(new NullEvent());

    ResultCode ret = m_event_handling_thread.wait_until_stopped();
    if (ret.is_error()) {
        CLOUDTV_LOG_DEBUG("m_event_handling_thread.wait_until_stopped() failed:%s", ret.get_description());
    }
}

void Session::Impl::stop_streaming()
{
    // Our mutex is already locked here

    // Stop the stream error and trigger timers, if necessary.
    m_timer.cancel_timer(m_stream_error_callback);
    m_timer.cancel_timer(m_streamer_periodic_trigger);

    // Clear the stream uri so we won't mistakenly think the stream is already running
    m_current_stream_uri = "";

    // And the StreamConfirm sent state
    m_stream_confirm_sent_state = STREAM_CONFIRM_NOT_SENT;

    // Stop streaming, if necessary.
    // Should typically send out a playback report, if enabled, because of the PLAYER_STOPPED event.
    m_streamer.stop_stream();
}

void Session::Impl::set_state(State state, ClientErrorCode error_code)
{
    // Our mutex is already locked here
	CLOUDTV_LOG_DEBUG("just for test.\n");

    if (state != m_state) {
		CLOUDTV_LOG_DEBUG("just for test.\n");
        //sw_log_info(TAG,"Change state from %s to %s", rfbtvpm_get_state_name(m_state), rfbtvpm_get_state_name(state));

        m_state = state;
        if (m_session_callbacks) {
            m_session_callbacks->state_update(state, error_code);
			CLOUDTV_LOG_DEBUG("just for test.\n");
        }
    }
}

bool Session::Impl::is_idle() const
{
    return m_rfbtv_state == INIT || m_rfbtv_state == ERROR;
}

bool Session::Impl::is_active() const
{
    return m_rfbtv_state == ACTIVE;
}

bool Session::Impl::is_suspended() const
{
    return m_rfbtv_state == SUSPENDED;
}

void Session::Impl::connection_backoff_time_expired()
{
    CLOUDTV_LOG_DEBUG("test,line:%d\n", __LINE__);
    m_event_queue.put(new TriggerEvent(*this, &Impl::handle_connect_event));
}

void Session::Impl::stream_timeout_expired()
{
    CLOUDTV_LOG_DEBUG("test,line:%d\n", __LINE__);
    m_event_queue.put(new TriggerEvent(*this, &Session::Impl::handle_stream_timeout_expired_event));
}

void Session::Impl::playback_report_periodic_trigger()
{
    CLOUDTV_LOG_DEBUG("test,line:%d\n", __LINE__);
    m_event_queue.put(new TriggerEvent(*this, &Session::Impl::handle_playback_report_trigger_event));
}

void Session::Impl::handle_initiate_event(const InitiateEvent &event)
{
    AutoLock lck(m_mutex);

    CLOUDTV_LOG_DEBUG("state:%s\n", rfbtvpm_get_state_name(m_rfbtv_state));

    // Can only start when a session is not running
    if (!is_idle() && m_rfbtv_state != REDIRECTED) {
        CLOUDTV_LOG_DEBUG("Invalid state:%s", rfbtvpm_get_state_name(m_rfbtv_state));
        return;
    }

    // If we're REDIRECTED, we have received a redirect request and should not reset the redirect counter.
    if (m_rfbtv_state != REDIRECTED) {
        m_redirect_count = 0;
    }

    rfbtvpm_set_state(INITIATED, CLIENT_ERROR_CODE_OK);

	CLOUDTV_LOG_DEBUG( "\n");
    // Take the event parameters
    m_session_url = event.host();
    m_screen_width = event.screen_width();
    m_screen_height = event.screen_height();
    m_param_list = event.optional_parameters(); // Reset the parameter list to whatever is given
    if (!event.url().empty()) {
        // If URL is empty, the user might be using launch_parameters to start the application
        m_param_list["url"] = event.url();
    }
    m_session_start_time = event.start_time();
	CLOUDTV_LOG_DEBUG( " ");

    // Initialize the session
    m_rx_message.clear();
    m_streamer.reinitialize();
    m_rfbtv_protocol.set_version(RfbtvProtocol::RFBTV_PROTOCOL_UNKNOWN);
    m_current_stream_uri = "";
    m_key_filter.clear();
    m_playback_report.reset();
    m_playback_report_manager.disable_reports();
    m_timer.cancel_timer(m_playback_report_periodic_trigger);
    m_latency_report.reset();
    m_latency_report.set_measurement_mode(0);
    m_latency_report_manager.disable_reports();
    // m_log_report.reset(); // We explicitly don't reset the log report. We could use this to keep post-mortem logs from a client that experienced an error and decided to close the session.
    m_log_report.set_min_level(LOG_DEBUG); // Accept all logs if the level is never set.
    m_log_report_manager.disable_reports();
    m_stalled_timestamp.invalidate();
	CLOUDTV_LOG_DEBUG( "\n");
    assert(!m_is_logging);
	CLOUDTV_LOG_DEBUG( "\n");
    m_closing_suspended = false;

    //sw_log_info(TAG,"CloudTV Nano SDK Version:%s", get_sdk_version().c_str());
    //sw_log_info(TAG,"Setting up session to '%s' with app url '%s', resolution:%dx%d", event.host().c_str(), event.url().c_str(), m_screen_width, m_screen_height);

    rfbtvpm_reconnect(true);
}

void Session::Impl::handle_terminate_event(const TerminateEvent &event)
{
    AutoLock lck(m_mutex);

    CLOUDTV_LOG_DEBUG("state:%s\n", rfbtvpm_get_state_name(m_rfbtv_state));

    if (m_rfbtv_state == INITIATED || m_rfbtv_state == REDIRECTED || m_rfbtv_state == CONNECTING) {
        //sw_log_info(TAG,"Connection in progress, close it");
        rfbtvpm_close_connection();
        rfbtvpm_set_state(INIT, CLIENT_ERROR_CODE_OK);
        return;
    }

    if (is_idle()) {
        CLOUDTV_LOG_DEBUG("Not connected\n");
        return;
    }

    rfbtvpm_session_stop(event.result_code(), RfbtvProtocol::SESSION_TERMINATE_NORMAL);
}

void Session::Impl::handle_suspend_event(const TriggerEvent &/*event*/)
{
    AutoLock lck(m_mutex);

    CLOUDTV_LOG_DEBUG("state:%s", rfbtvpm_get_state_name(m_rfbtv_state));

    // Don't do anything if we're already suspended
    if (is_suspended()) {
        CTVC_LOG_WARNING("Already in suspended state");
        return;
    }

    ResultCode ret = rfbtvpm_session_suspend();
    close_session_in_case_of_error(ret);
}

void Session::Impl::handle_resume_event(const TriggerEvent &/*event*/)
{
    AutoLock lck(m_mutex);

    CLOUDTV_LOG_DEBUG("state:%s\n", rfbtvpm_get_state_name(m_rfbtv_state));

    if (!is_suspended()) {
        CTVC_LOG_WARNING("Resuming a session that is not suspended, request ignored");
        return;
    }

    m_redirect_count = 0;

    rfbtvpm_reconnect(true);
}

void Session::Impl::post_frame_buffer_update_request()
{
    CLOUDTV_LOG_DEBUG("test\n");
    m_event_queue.put(new TriggerEvent(*this, &Impl::handle_frame_buffer_update_request_event));
}

void Session::Impl::handle_frame_buffer_update_request_event(const TriggerEvent &/*event*/)
{
    AutoLock lck(m_mutex);

    CLOUDTV_LOG_DEBUG("state:%s\n", rfbtvpm_get_state_name(m_rfbtv_state));

    ResultCode ret = rfbtvpm_send_message(m_rfbtv_protocol.create_frame_buffer_update_request(m_screen_width, m_screen_height));
    if (ret.is_error()) {
        CTVC_LOG_WARNING("Unable to send frame buffer update request to server!");
    }
}

void Session::Impl::handle_update_session_optional_parameters_event(const ParameterUpdateEvent &event)
{
    AutoLock lck(m_mutex);

    CLOUDTV_LOG_DEBUG("state:%s", rfbtvpm_get_state_name(m_rfbtv_state));

    // Can only update a property when a session is in active state
    if (!is_active()) {
        CLOUDTV_LOG_DEBUG("Session is not running");
        return;
    }

    std::map<std::string, std::string> update_map;
    for (std::map<std::string, std::string>::const_iterator i = event.optional_parameters().begin(); i != event.optional_parameters().end(); ++i) {
        const std::string &key(i->first);
        const std::string &value(i->second);

        CLOUDTV_LOG_DEBUG("key:%s, value:%s\n", key.c_str(), value.c_str());

        // If active, check whether the parameter is new or whether it changed and build an update map from these
        std::map<std::string, std::string>::iterator j = m_param_list.find(key);
        if (j == m_param_list.end() || j->second != value) {
            update_map[key] = value;
        }

        // Always update the active parameter list
        m_param_list[key] = value;
    }

    // Send an update message if necessary
    if (!update_map.empty()) {
        ResultCode ret = rfbtvpm_send_message(m_rfbtv_protocol.create_session_update(update_map));
        close_session_in_case_of_error(ret);
    }
}

void Session::Impl::handle_send_keycode_event(const KeyEvent &event)
{
    AutoLock lck(m_mutex);

    CLOUDTV_LOG_DEBUG("state:%s\n", rfbtvpm_get_state_name(m_rfbtv_state));

    if (!is_active()) {
        CLOUDTV_LOG_DEBUG("Session is not running\n");
        return;
    }

    RfbtvProtocol::KeyAction key_action = RfbtvProtocol::KEY_DOWN;
    switch (event.action()) {
    case IInput::ACTION_NONE:
        return;

    case IInput::ACTION_DOWN:
    case IInput::ACTION_DOWN_AND_UP: // First key to send
        key_action = RfbtvProtocol::KEY_DOWN;
        break;

    case IInput::ACTION_UP:
        key_action = RfbtvProtocol::KEY_UP;
        break;

    case IInput::ACTION_KEYINPUT:
        if ((m_rfbtv_protocol.get_version() != RfbtvProtocol::RFBTV_PROTOCOL_V2_0)) {
            CLOUDTV_LOG_DEBUG("Keyinput is only available in RFB-TV version 2.0\n");
            return;
        }
        key_action = RfbtvProtocol::KEYINPUT;
        break;
    }
	CLOUDTV_LOG_DEBUG("key_action:%d.\n", key_action);

    ResultCode ret;
    if (m_rfbtv_protocol.get_version() == RfbtvProtocol::RFBTV_PROTOCOL_V2_0) {
        std::string timestamp;
        if (m_latency_report_manager.is_enabled()) {
            timestamp = uint64_to_string(TimeStamp::now().get_as_milliseconds());

            CLOUDTV_LOG_DEBUG("timestamp:[%s]", timestamp.c_str());
        }

	CLOUDTV_LOG_DEBUG("key_action:%d.\n", key_action);
        ret = rfbtvpm_send_message(m_rfbtv_protocol.create_key_time_event(event.x11_key(), key_action, timestamp));
	CLOUDTV_LOG_DEBUG("key_action:%d.\n", key_action);

        if (event.action() == IInput::ACTION_DOWN_AND_UP && ret.is_ok()) {
            ret = rfbtvpm_send_message(m_rfbtv_protocol.create_key_time_event(event.x11_key(), RfbtvProtocol::KEY_UP, timestamp));
	CLOUDTV_LOG_DEBUG("key_action:%d.\n", key_action);
        }
    } else {
        ret = rfbtvpm_send_message(m_rfbtv_protocol.create_key_event(event.x11_key(), key_action));
	CLOUDTV_LOG_DEBUG("key_action:%d.\n", key_action);

        if (event.action() == IInput::ACTION_DOWN_AND_UP && ret.is_ok()) {
            ret =rfbtvpm_send_message(m_rfbtv_protocol.create_key_event(event.x11_key(), RfbtvProtocol::KEY_UP));
	CLOUDTV_LOG_DEBUG("key_action:%d.\n", key_action);
        }
    }

    close_session_in_case_of_error(ret);
}

void Session::Impl::handle_pointer_event(const PointerEvent &event)
{
    AutoLock lck(m_mutex);

    CLOUDTV_LOG_DEBUG("state:%s", rfbtvpm_get_state_name(m_rfbtv_state));

    if (!is_active()) {
        CLOUDTV_LOG_DEBUG("Session is not running");
        return;
    }

    int mask = 0;
    switch (event.button()) {
    case IInput::NO_BUTTON:
        break;
    case IInput::LEFT_BUTTON:
        mask = RFBTV_MOUSE_BUTTON_LEFT;
        break;
    case IInput::RIGHT_BUTTON:
        mask = RFBTV_MOUSE_BUTTON_RIGHT;
        break;
    case IInput::MIDDLE_BUTTON:
        mask = RFBTV_MOUSE_BUTTON_MIDDLE;
        break;
    case IInput::WHEEL_UP:
        mask = RFBTV_MOUSE_WHEEL_UP;
        break;
    case IInput::WHEEL_DOWN:
        mask = RFBTV_MOUSE_WHEEL_DOWN;
        break;
    }

    switch (event.action()) {
    case IInput::ACTION_NONE:
        break;
    case IInput::ACTION_DOWN:
        m_rfbtv_button_mask |= mask;
        break;
    case IInput::ACTION_UP:
        m_rfbtv_button_mask &= ~mask;
        break;
    case IInput::ACTION_DOWN_AND_UP:
        if ((m_rfbtv_button_mask & mask) == 0) { // Don't send if already down
            rfbtvpm_send_message(m_rfbtv_protocol.create_pointer_event(m_rfbtv_button_mask | mask, event.x(), event.y()));
        }
        m_rfbtv_button_mask &= ~mask;
        break;
    case IInput::ACTION_KEYINPUT:
        CLOUDTV_LOG_DEBUG("Error in parameter. Keyinput is not valid as pointer event.");
        return;
    }

    ResultCode ret = rfbtvpm_send_message(m_rfbtv_protocol.create_pointer_event(m_rfbtv_button_mask, event.x(), event.y()));
    close_session_in_case_of_error(ret);
}

void Session::Impl::handle_player_event(const PlayerEvent &event)
{
    AutoLock lck(m_mutex);

    CLOUDTV_LOG_DEBUG("state:%s, event:%d\n", rfbtvpm_get_state_name(m_rfbtv_state), event.event());

    // Determine the new state and send any StreamConfirm message if appropriate.
    PlaybackReport::PlaybackState state = PlaybackReport::STOPPED;
    switch (event.event()) {
    case IMediaPlayer::PLAYER_STARTING:
        state = PlaybackReport::STARTING;
        break;

    case IMediaPlayer::PLAYER_STARTED:
        m_timer.cancel_timer(m_stream_error_callback);
        state = PlaybackReport::PLAYING;

        // Send stream confirm OK if required.
        if (m_stream_confirm_sent_state == STREAM_CONFIRM_NOT_SENT) {
            // Also update the stream-to-start latency measurement when we send an 'ok' StreamConfirm
            m_latency_report.add_entry(LatencyReport::SUBTYPE_SESSION_START_TO_STREAM, "SUBTYPE_SESSION_START_TO_STREAM", TimeStamp::now().get_as_milliseconds() - m_session_start_time.get_as_milliseconds());
            m_stream_confirm_sent_state = STREAM_CONFIRM_OK_SENT;
            rfbtvpm_send_message(m_rfbtv_protocol.create_stream_confirm(RfbtvProtocol::STREAM_CONFIRM_SUCCESS));
        }
        break;

    case IMediaPlayer::PLAYER_STOPPED:
        m_timer.cancel_timer(m_stream_error_callback);
        state = PlaybackReport::STOPPED;
        break;

    case IMediaPlayer::PLAYER_BUFFER_UNDERRUN:
    case IMediaPlayer::PLAYER_RECOVERABLE_ERROR:
        // Recoverable errors.
        // Start a timer to send a stream confirm error in case we didn't recover within STREAM_ERROR_TIMEOUT_IN_MS milliseconds.
        m_timer.start_timer(m_stream_error_callback, STREAM_ERROR_TIMEOUT_IN_MS, TimerEngine::ONE_SHOT);
        state = PlaybackReport::STALLED;
        break;

    case IMediaPlayer::PLAYER_BUFFER_OVERRUN:
    case IMediaPlayer::PLAYER_UNRECOVERABLE_ERROR:
    case IMediaPlayer::PLAYER_DESCRAMBLE_ERROR:
    case IMediaPlayer::PLAYER_DECODE_ERROR:
    case IMediaPlayer::PLAYER_TRANSPORT_STREAM_ID_ERROR:
    case IMediaPlayer::PLAYER_NETWORK_ID_ERROR:
    case IMediaPlayer::PLAYER_PROGRAM_ID_ERROR:
    case IMediaPlayer::PLAYER_PHYSICAL_ERROR:
        // Unrecoverable errors.
        m_timer.cancel_timer(m_stream_error_callback);
        state = PlaybackReport::STALLED;
        // Fatal error condition so send the appropriate error.
        rfbtvpm_send_appropriate_stream_confirm_error(event.event());
        break;
    }

    // Keep state whether report was changed by this event; regard as changed if playback state was not set before or if it changed
    bool has_report_changed = !m_playback_report.m_playback_state.is_set() || (state != m_playback_report.m_playback_state.get());

    // Set the new state
    m_playback_report.m_playback_state.set(state);

    // Reset the stalled time entry if it was not set before; if we get here, we'll actively maintain the stalled duration.
    if (!m_playback_report.m_stalled_duration_in_ms.is_set()) {
        m_playback_report.m_stalled_duration_in_ms.set(0);
    }

    // Compute the stalled time
    switch (event.event()) {
    case IMediaPlayer::PLAYER_STARTING:
        // Don't use or modify the stalled time stamp
        break;
    case IMediaPlayer::PLAYER_STARTED:
    case IMediaPlayer::PLAYER_STOPPED:
        // Accumulate any stalled time if necessary and reset
        if (m_stalled_timestamp.is_valid()) {
            TimeStamp diff = (TimeStamp::now() - m_stalled_timestamp); // Measure the duration of the stall
            m_playback_report.m_stalled_duration_in_ms.set(m_playback_report.m_stalled_duration_in_ms.get() + diff.get_as_milliseconds());
            has_report_changed = true; // (Superfluous, actually; can never be false here.)
            m_stalled_timestamp.invalidate();
        }
        break;
    case IMediaPlayer::PLAYER_BUFFER_UNDERRUN:
    case IMediaPlayer::PLAYER_BUFFER_OVERRUN:
    case IMediaPlayer::PLAYER_RECOVERABLE_ERROR:
    case IMediaPlayer::PLAYER_UNRECOVERABLE_ERROR:
    case IMediaPlayer::PLAYER_DESCRAMBLE_ERROR:
    case IMediaPlayer::PLAYER_DECODE_ERROR:
    case IMediaPlayer::PLAYER_TRANSPORT_STREAM_ID_ERROR:
    case IMediaPlayer::PLAYER_NETWORK_ID_ERROR:
    case IMediaPlayer::PLAYER_PROGRAM_ID_ERROR:
    case IMediaPlayer::PLAYER_PHYSICAL_ERROR:
        // Sample stalled time stamp if it was not sampled before in some error state
        if (!m_stalled_timestamp.is_valid()) {
            m_stalled_timestamp = TimeStamp::now();
        }
        break;
    }

    // Signal the update if the report has changed
    if (has_report_changed) {
        m_playback_report_manager.report_updated();
    }
}

void Session::Impl::handle_stream_data_event(const StreamDataEvent &event)
{
    AutoLock lck(m_mutex);

    CLOUDTV_LOG_DEBUG("state:%s", rfbtvpm_get_state_name(m_rfbtv_state));

    // Process the data
    m_rx_message.write_raw(event.data(), event.size());
    CLOUDTV_LOG_DEBUG("Got data size:%u at bytes in buffer:%u", event.size(), m_rx_message.size());

    ResultCode result;
    // we process the buffer while there are data in the buffer and there are no errors
    do {
        CLOUDTV_LOG_DEBUG("length:%u, state:%s", m_rx_message.size(), rfbtvpm_get_state_name(m_rfbtv_state));

        // Return invalid state error if the message is not handled
        result = INVALID_STATE;

        switch (m_rfbtv_state) {
        case CONNECTING:
            result = rfbtvpm_handle_rfbtv_version_string(m_rx_message);
            break;
        case OPENING:
        case ACTIVE:
            result = m_rfbtv_protocol.parse_message(m_rx_message);
            break;
        default:
            CTVC_LOG_WARNING("Data received in state %s, ignoring it", rfbtvpm_get_state_name(m_rfbtv_state));
            m_rx_message.clear(); // CTV-26040: ignore all data
            return;
        }

        if (result == RfbtvProtocol::NEED_MORE_DATA) {
            // We got a message underrun so we return NEED_MORE_DATA
            CLOUDTV_LOG_DEBUG("Message needs more data (bytes in buffer:%u)", m_rx_message.size());
            m_rx_message.rewind();
        } else if (result.is_error()) {
            CLOUDTV_LOG_DEBUG("Message handling failed:%s", result.get_description());
            m_rx_message.discard_bytes_read();

            // Translate message processing error codes into client error codes according to 'CloudTV Client Error Code Specification' version 1.4
            ClientErrorCode reason_code;
            if (result == TOO_MANY_REDIRECTS) {
                reason_code = CLIENT_ERROR_CODE_131;
            } else if (result == RfbtvProtocol::INVALID_SERVER_VERSION) {
                reason_code = CLIENT_ERROR_CODE_115;
            } else {
                reason_code = CLIENT_ERROR_CODE_190;
            }

            // Stop the session if it's not already stopped.
            rfbtvpm_session_stop(reason_code, RfbtvProtocol::SESSION_TERMINATE_CLIENT_EXECUTION_ERROR);
        } else { // OK
            CLOUDTV_LOG_DEBUG("Message Processed: bytes_read:%d, message size:%u", m_rx_message.bytes_read(), m_rx_message.size());
            m_rx_message.discard_bytes_read();
        }
    } while ((result.is_ok()) && (m_rx_message.size() > 0));
}

void Session::Impl::handle_stream_error_event(const StreamErrorEvent &event)
{
    AutoLock lck(m_mutex);

    CLOUDTV_LOG_DEBUG("state:%s\n", rfbtvpm_get_state_name(m_rfbtv_state));

    m_rx_message.clear();

    if (is_suspended() || m_rfbtv_state == REDIRECTED) {
        // If we are suspended or redirected there is no need to do anything.
        return;
    }

    ResultCode result(event.result_code());
    if (result.is_ok() || result == Socket::READ_ERROR) {
        if (m_rfbtv_state == INITIATED || is_active()) {
            //sw_log_info(TAG,"Connection %s in state %s, try to reconnect", is_active() ? "closed" : "request", rfbtvpm_get_state_name(m_rfbtv_state));
            // This was a socket shutdown so reconnect.
            rfbtvpm_reconnect(m_rfbtv_state == INITIATED); // Only do an immediate retry in state 'initiated'
        } else {
            // This was a socket shutdown while in another state. (TODO: Should we retry instead?)
            rfbtvpm_session_stop(CLIENT_ERROR_CODE_210, RfbtvProtocol::SESSION_TERMINATE_NORMAL);
        }
    } else {
        if (result == Socket::THREAD_SHUTDOWN) {
            // This must be an intended action, so ignore it without taking further action.
            CLOUDTV_LOG_DEBUG("Receive failed, ret:(%s)", result.get_description());
            return;
        }

        CLOUDTV_LOG_DEBUG("Receive failed, ret:(%s)", result.get_description());

        if (m_rfbtv_state != CONNECTING) {
            // Not connecting: this is a real error so close the session
            close_session_in_case_of_error(result);
        } else {
            // In the process of connecting, so check whether we need to retry and when.
            size_t index = m_connect_attempts++;
            const int backoff_times[] = { 10, 20, 40, 80 }; // Reconnect back-off times

            // Fail after we retried the max. amount of times or when retrying makes no sense (e.g. when the host is not found).
            if (index >= sizeof(backoff_times) / sizeof(backoff_times[0]) || result == Socket::HOST_NOT_FOUND) {
                // We failed to (re)connect. Close the session.
                CLOUDTV_LOG_DEBUG("Failed to reconnect after %d attempts, closing the session", m_connect_attempts);
                close_session_in_case_of_error(result);
                return;
            }

            // Do a back-off of 5+N seconds
            int timeout_in_ms = 5000 + rand() % (1000 * backoff_times[index] + 1);

            //sw_log_info(TAG,"Retry scheduled in %dms", timeout_in_ms);
            m_timer.start_timer(m_connection_backoff_time_callback, timeout_in_ms, TimerEngine::ONE_SHOT);
        }
    }
}

void Session::Impl::handle_latency_data_event(const LatencyDataEvent &event)
{
    AutoLock lck(m_mutex);

    CLOUDTV_LOG_DEBUG("data_type:%d", event.data_type());

    switch(event.data_type()) {
    case ILatencyData::KEY_PRESS:
        m_latency_report.add_entry(LatencyReport::SUBTYPE_KEY_TO_DISPLAY, "", (event.pts() - event.original_event_time()).get_as_milliseconds());
        break;
    case ILatencyData::FIRST_PAINT:
        m_latency_report.add_entry(LatencyReport::SUBTYPE_SESSION_START_TO_FIRSTPAINT, "", (event.pts() - m_session_start_time).get_as_milliseconds());
        break;
    case ILatencyData::APP_COMPLETE:
        m_latency_report.add_entry(LatencyReport::SUBTYPE_SESSION_START_TO_COMPLETE, "", (event.pts() - m_session_start_time).get_as_milliseconds());
        break;
    }

    m_latency_report_manager.report_updated();
}

void Session::Impl::handle_stall_event(const StallEvent &event)
{
    AutoLock lck(m_mutex);

    CLOUDTV_LOG_DEBUG("id:%s, audio=%d, duration:%u", event.id().c_str(), event.is_audio_not_video(), static_cast<unsigned>(event.stall_duration().get_as_milliseconds()));

    m_playback_report.add_stalled_duration_sample(event.id(), event.is_audio_not_video(), event.stall_duration().get_as_milliseconds());

    // We don't trigger m_playback_report_manager.report_updated() because a stall event should not lead to an unsolicited report.
}

void Session::Impl::handle_connect_event(const TriggerEvent &/*event*/)
{
    AutoLock lck(m_mutex);

    CLOUDTV_LOG_DEBUG("uri:%s, state:%s\n", m_session_url.c_str(), rfbtvpm_get_state_name(m_rfbtv_state));

    if (m_rfbtv_state != CONNECTING) {
        CLOUDTV_LOG_DEBUG("Unexpected state:%s\n", rfbtvpm_get_state_name(m_rfbtv_state));
    }

    // Close any connection if open, just to be sure.
    rfbtvpm_close_connection();

    // Reset some session state
    m_rfbtv_protocol.set_version(RfbtvProtocol::RFBTV_PROTOCOL_UNKNOWN);
	CLOUDTV_LOG_DEBUG("set_version deal end.\n");
    m_rfbtv_button_mask = 0;

    std::string proto;
    std::string authorization;
    std::string server;
    int port;
    std::string path;
    url_split(m_session_url, proto, authorization, server, port, path);

    bool is_secure_connection = false;
    if (proto.compare("rfbtvs") == 0) {
        CLOUDTV_LOG_DEBUG("SECURE\n");
        is_secure_connection = true;
    } else if (proto.compare("rfbtv") == 0) {
        CLOUDTV_LOG_DEBUG("Not Secure\n");
    } else {
        CLOUDTV_LOG_DEBUG("Invalid URL protocol:%s. Only the rfbtv scheme is supported. uri:%s\n", proto.c_str(), m_session_url.c_str());
        close_session_in_case_of_error(UNSUPPORTED_PROTOCOL);
        return;
    }

    if (port == -1) {
        // default port in case none is given
        port = DEFAULT_RFBTV_SERVER_PORT;
    }

	CLOUDTV_LOG_DEBUG( "just for test server:%s,port:%d!\n", server.c_str(), port);
    ResultCode ret = m_connection.open(server, port, is_secure_connection, *this);
    if (ret.is_error()) {
        // Some fatal error while opening the connection (this is not yet the result of connect() itself...)
		CLOUDTV_LOG_DEBUG( "open error.\n");
        close_session_in_case_of_error(ret);
        return;
    }

    CLOUDTV_LOG_DEBUG("RFB-TV socket open, waiting for connect...\n");
}

void Session::Impl::handle_cdm_session_terminate_event(const CdmSessionTerminateEvent &event)
{
    AutoLock lck(m_mutex);

    //sw_log_info(TAG,"CDM-initiated termination of CDM session with ID '%s'", event.cdm_session_id().c_str());

    // Terminate the CDM session using the proper response code
    rfbtvpm_cdm_session_terminate(event.cdm_session_id(), event.reason());
}

void Session::Impl::handle_protocol_extension_send_event(const ProtocolExtensionSendEvent &event)
{
    AutoLock lck(m_mutex);

    CLOUDTV_LOG_DEBUG("test");

    if (!is_active()) {
        CLOUDTV_LOG_DEBUG("Session is not running");
        return;
    }

    ResultCode ret = rfbtvpm_send_message(m_rfbtv_protocol.create_passthrough(event.protocol_id(), event.data()));
    close_session_in_case_of_error(ret);
}

void Session::Impl::handle_stream_timeout_expired_event(const TriggerEvent &/*event*/)
{
    AutoLock lck(m_mutex);

    CLOUDTV_LOG_DEBUG("test");

    // We either get here with PLAYER_BUFFER_UNDERRUN or PLAYER_DECODE_ERROR; both
    // will translate into STREAM_CONFIRM_DECODE_ERROR when sent to the server...
    rfbtvpm_send_appropriate_stream_confirm_error(IMediaPlayer::PLAYER_DECODE_ERROR);
}

void Session::Impl::handle_playback_report_trigger_event(const TriggerEvent &/*event*/)
{
    AutoLock lck(m_mutex);

    CLOUDTV_LOG_DEBUG("test");

    m_playback_report_manager.timer_tick();
}

bool Session::Impl::run()
{
    const IEvent *event = m_event_queue.get();

    event->handle();
    delete event;

    return false;
}
