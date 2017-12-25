///
/// \file SessionImpl.h
///
/// \brief CloudTV Nano SDK northbound interface implementation.
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include "RfbtvProtocol.h"
#include "EchoProtocolExtension.h"
#include "ReportManager.h"
#include "PlaybackReport.h"
#include "LatencyReport.h"
#include "LogReport.h"
#include "TcpConnection.h"
#include "EventQueue.h"
#include "IEvent.h"
#include "BoundEvent.h"
#include "KeyFilter.h"

#include <core/Session.h>
#include <core/ICdmSession.h>
#include <core/IControl.h>
#include <core/IInput.h>
#include <core/IOverlayCallbacks.h>

#include <stream/IMediaPlayer.h>
#include <stream/Streamer.h>
#include <stream/ILatencyData.h>
#include <stream/IStallEvent.h>

#include <porting_layer/ClientContext.h>
#include <porting_layer/Mutex.h>
#include <porting_layer/Atomic.h>
#include <porting_layer/TimeStamp.h>

#include <utils/TimerEngine.h>

#include <string>
#include <map>
#include <list>

#include <inttypes.h>

namespace ctvc {

struct IMediaPlayerFactory;
struct PictureParameters;
struct IOverlayCallbacks;
struct IContentLoader;

class Session::Impl : public IProtocolExtension::IReply, public IMediaPlayer::ICallback, public IControl, public IInput, public IReportTransmitter, public ILogOutput, public RfbtvProtocol::ICallbacks, public Thread::IRunnable, public IStream, public ILatencyData, public IStallEvent
{
public:
    static const ResultCode CONNECTION_TIMEOUT;
    static const ResultCode INVALID_STATE;
    static const ResultCode UNSUPPORTED_PROTOCOL;
    static const ResultCode TOO_MANY_REDIRECTS;

    Impl(ClientContext &context, ISessionCallbacks *session_callbacks, IOverlayCallbacks *overlay_callbacks);

    virtual ~Impl();

    State get_state() const;

    bool register_content_loader(IContentLoader *content_loader);
    bool register_protocol_extension(IProtocolExtension &protocol_extension);
    bool unregister_protocol_extension(IProtocolExtension &protocol_extension);
    bool register_drm_system(ICdmSessionFactory &factory);
    bool unregister_drm_system(ICdmSessionFactory &factory);
    bool register_handoff_handler(const std::string &handoff_scheme, IHandoffHandler &handoff_handler);
    bool unregister_handoff_handler(const std::string &handoff_scheme);

private:
    friend class Session;

    enum RFBTV_STATE
    {
        INIT, INITIATED, REDIRECTED, CONNECTING, OPENING, ACTIVE, SUSPENDED, ERROR
    };

    // Private helper class
    // Manages the CdmSession, its lifetime and its callback
    class CdmSessionContainer : public ICdmSession::ICallback
    {
    public:
        CdmSessionContainer(Impl &session, const std::string &cdm_session_id, ICdmSession &cdm_session, ICdmSessionFactory &cdm_session_factory) :
            m_session(session),
            m_cdm_session_id(cdm_session_id),
            m_cdm_session(cdm_session),
            m_cdm_session_factory(cdm_session_factory)
        {
        }

        ~CdmSessionContainer()
        {
            m_cdm_session_factory.destroy(&m_cdm_session);
        }

        void setup(const std::string &session_type, const std::map<std::string, std::string> &init_data)
        {
            m_cdm_session.setup(session_type, init_data, *this);
        }

        void terminate_session(RfbtvProtocol::CdmSessionTerminateResponseReason reason)
        {
            m_terminate_reason = reason;
            m_cdm_session.terminate(*this);
        }

        IStreamDecrypt *get_stream_decrypt_engine()
        {
            return m_cdm_session.get_stream_decrypt_engine();
        }

    private:
        CdmSessionContainer(const CdmSessionContainer &);
        CdmSessionContainer &operator=(CdmSessionContainer &);

        Impl &m_session;
        const std::string m_cdm_session_id;
        ICdmSession &m_cdm_session;
        ICdmSessionFactory &m_cdm_session_factory;
        RfbtvProtocol::CdmSessionTerminateResponseReason m_terminate_reason;

        // Implements ICdmSession::ICallback
        void terminate_indication(ICdmSession::ICallback::TerminateReason reason)
        {
            m_session.cdm_session_terminate_indication(m_cdm_session_id, reason);
        }

        void setup_result(ICdmSession::SetupResult result, const std::map<std::string, std::string> &response)
        {
            m_session.cdm_setup_result(m_cdm_session_id, result, response, this);
        }

        void terminate_result(const std::map<std::string, std::string> &stop_data)
        {
            m_session.cdm_terminate_result(m_cdm_session_id, m_terminate_reason, stop_data, this);
        }

    };
    friend class CdmSessionContainer; // Needed for Metrowerks

    // Empty event, only used for trigger and carries no data
    class TriggerEvent : public BoundEvent<Impl, TriggerEvent>
    {
    public:
        TriggerEvent(Impl &impl, void (Impl::*handler)(const TriggerEvent &)) :
            BoundEvent<Impl, TriggerEvent>(impl, handler)
        {
        }
    };

    class InitiateEvent : public BoundEvent<Impl, InitiateEvent>
    {
    public:
        InitiateEvent(Impl &impl, void (Impl::*handler)(const InitiateEvent &), const std::string &host, const std::string &url, uint32_t screen_width, uint32_t screen_height,
            const std::map<std::string, std::string> &optional_parameters, const TimeStamp &start_time) :
                BoundEvent<Impl, InitiateEvent>(impl, handler),
            m_host(host),
            m_url(url),
            m_screen_width(screen_width),
            m_screen_height(screen_height),
            m_optional_parameters(optional_parameters),
            m_start_time(start_time)
        {
        }

        const std::string &host() const
        {
            return m_host;
        }

        const std::string &url() const
        {
            return m_url;
        }

        uint32_t screen_width() const
        {
            return m_screen_width;
        }

        uint32_t screen_height() const
        {
            return m_screen_height;
        }

        const std::map<std::string, std::string> &optional_parameters() const
        {
            return m_optional_parameters;
        }

        const TimeStamp &start_time() const
        {
            return m_start_time;
        }

    private:
        std::string m_host;
        std::string m_url;
        uint32_t m_screen_width;
        uint32_t m_screen_height;
        std::map<std::string, std::string> m_optional_parameters;
        TimeStamp m_start_time;
    };

    class TerminateEvent : public BoundEvent<Impl, TerminateEvent>
    {
    public:
        TerminateEvent(Impl &impl, void (Impl::*handler)(const TerminateEvent &), ClientErrorCode result_code) :
            BoundEvent<Impl, TerminateEvent>(impl, handler),
            m_result_code(result_code)
        {
        }

        ClientErrorCode result_code() const
        {
            return m_result_code;
        }

    private:
        ClientErrorCode m_result_code;
    };

    class ParameterUpdateEvent : public BoundEvent<Impl, ParameterUpdateEvent>
    {
    public:
        ParameterUpdateEvent(Impl &impl, void (Impl::*handler)(const ParameterUpdateEvent &), const std::map<std::string, std::string> &optional_parameters) :
            BoundEvent<Impl, ParameterUpdateEvent>(impl, handler),
            m_optional_parameters(optional_parameters)
        {
        }

        const std::map<std::string, std::string> &optional_parameters() const
        {
            return m_optional_parameters;
        }

    private:
        std::map<std::string, std::string> m_optional_parameters;
    };

    class KeyEvent : public BoundEvent<Impl, KeyEvent>
    {
    public:
        KeyEvent(Impl &impl, void (Impl::*handler)(const KeyEvent &), X11KeyCode keycode, IInput::Action action) :
            BoundEvent<Impl, KeyEvent>(impl, handler),
            m_x11_key(keycode),
            m_action(action)
        {
        }

        X11KeyCode x11_key() const
        {
            return m_x11_key;
        }

        IInput::Action action() const
        {
            return m_action;
        }

    private:
        X11KeyCode m_x11_key;
        IInput::Action m_action;
    };

    class PointerEvent : public BoundEvent<Impl, PointerEvent>
    {
    public:
        PointerEvent(Impl &impl, void (Impl::*handler)(const PointerEvent &), uint32_t x, uint32_t y, IInput::Button button, IInput::Action action) :
            BoundEvent<Impl, PointerEvent>(impl, handler),
            m_x(x),
            m_y(y),
            m_button(button),
            m_action(action)
        {
        }

        uint32_t x() const
        {
            return m_x;
        }

        uint32_t y() const
        {
            return m_y;
        }

        IInput::Button button() const
        {
            return m_button;
        }

        IInput::Action action() const
        {
            return m_action;
        }

    private:
        uint32_t m_x;
        uint32_t m_y;
        IInput::Button m_button;
        IInput::Action m_action;
    };

    class PlayerEvent : public BoundEvent<Impl, PlayerEvent>
    {
    public:
        PlayerEvent(Impl &impl, void (Impl::*handler)(const PlayerEvent &), IMediaPlayer::PlayerEvent event) :
            BoundEvent<Impl, PlayerEvent>(impl, handler),
            m_event(event)
        {
        }

        IMediaPlayer::PlayerEvent event() const
        {
            return m_event;
        }

    private:
        IMediaPlayer::PlayerEvent m_event;
    };

    class StreamDataEvent : public BoundEvent<Impl, StreamDataEvent>
    {
    public:
        StreamDataEvent(Impl &impl, void (Impl::*handler)(const StreamDataEvent &), const uint8_t *data, uint32_t size) :
            BoundEvent<Impl, StreamDataEvent>(impl, handler),
            m_data(data),
            m_size(size)
        {
        }

        ~StreamDataEvent()
        {
            delete[] m_data;
        }

        const uint8_t *data() const
        {
            return m_data;
        }

        uint32_t size() const
        {
            return m_size;
        }

    private:
        const uint8_t *m_data;
        uint32_t m_size;
    };

    class StreamErrorEvent : public BoundEvent<Impl, StreamErrorEvent>
    {
    public:
        StreamErrorEvent(Impl &impl, void (Impl::*handler)(const StreamErrorEvent &), int result_code) :
            BoundEvent<Impl, StreamErrorEvent>(impl, handler),
            m_result_code(result_code)
        {
        }

        int result_code() const
        {
            return m_result_code;
        }

    private:
        int m_result_code;
    };

    class LatencyDataEvent : public BoundEvent<Impl, LatencyDataEvent>
    {
    public:
        LatencyDataEvent(Impl &impl, void (Impl::*handler)(const LatencyDataEvent &), ILatencyData::LatencyDataType data_type, TimeStamp pts, TimeStamp original_event_time) :
            BoundEvent<Impl, LatencyDataEvent>(impl, handler),
            m_data_type(data_type),
            m_pts(pts),
            m_original_event_time(original_event_time)
        {
        }

        ILatencyData::LatencyDataType data_type() const
        {
            return m_data_type;
        }

        const TimeStamp &pts() const
        {
            return m_pts;
        }

        const TimeStamp &original_event_time() const
        {
            return m_original_event_time;
        }

    private:
        ILatencyData::LatencyDataType m_data_type;
        TimeStamp m_pts;
        TimeStamp m_original_event_time;
    };

    class StallEvent : public BoundEvent<Impl, StallEvent>
    {
    public:
        StallEvent(Impl &impl, void (Impl::*handler)(const StallEvent &), const std::string &id, bool is_audio_not_video, TimeStamp stall_duration) :
            BoundEvent<Impl, StallEvent>(impl, handler),
            m_id(id),
            m_is_audio_not_video(is_audio_not_video),
            m_stall_duration(stall_duration)
        {
        }

        const std::string &id() const
        {
            return m_id;
        }

        bool is_audio_not_video() const
        {
            return m_is_audio_not_video;
        }

        const TimeStamp &stall_duration() const
        {
            return m_stall_duration;
        }

    private:
        std::string m_id;
        bool m_is_audio_not_video;
        TimeStamp m_stall_duration;
    };

    class CdmSessionTerminateEvent : public BoundEvent<Impl, CdmSessionTerminateEvent>
    {
    public:
        CdmSessionTerminateEvent(Impl &impl, void (Impl::*handler)(const CdmSessionTerminateEvent &), const std::string &cdm_session_id, RfbtvProtocol::CdmSessionTerminateResponseReason reason) :
            BoundEvent<Impl, CdmSessionTerminateEvent>(impl, handler),
            m_cdm_session_id(cdm_session_id),
            m_reason(reason)
        {
        }

        const std::string &cdm_session_id() const
        {
            return m_cdm_session_id;
        }

        RfbtvProtocol::CdmSessionTerminateResponseReason reason() const
        {
            return m_reason;
        }

    private:
        std::string m_cdm_session_id;
        RfbtvProtocol::CdmSessionTerminateResponseReason m_reason;
    };

    class CdmSetupResultEvent : public BoundEvent<Impl, CdmSetupResultEvent>
    {
    public:
        CdmSetupResultEvent(Impl &impl, void (Impl::*handler)(const CdmSetupResultEvent &), const std::string &cdm_session_id, ICdmSession::SetupResult result, const std::map<std::string, std::string> &response, CdmSessionContainer *container) :
            BoundEvent<Impl, CdmSetupResultEvent>(impl, handler),
            m_cdm_session_id(cdm_session_id),
            m_result(result),
            m_response(response),
            m_container(container)
        {
        }

        const std::string &cdm_session_id() const
        {
            return m_cdm_session_id;
        }

        ICdmSession::SetupResult result() const
        {
            return m_result;
        }

        const std::map<std::string, std::string> &response() const
        {
            return m_response;
        }

        CdmSessionContainer *container() const
        {
            return m_container;
        }

    private:
        const std::string m_cdm_session_id;
        ICdmSession::SetupResult m_result;
        const std::map<std::string, std::string> m_response;
        CdmSessionContainer *m_container;
    };


    class CdmTerminateResultEvent : public BoundEvent<Impl, CdmTerminateResultEvent>
    {
    public:
        CdmTerminateResultEvent(Impl &impl, void (Impl::*handler)(const CdmTerminateResultEvent &), const std::string &cdm_session_id, RfbtvProtocol::CdmSessionTerminateResponseReason reason, const std::map<std::string, std::string> &stop_data, CdmSessionContainer *container) :
            BoundEvent<Impl, CdmTerminateResultEvent>(impl, handler),
            m_cdm_session_id(cdm_session_id),
            m_reason(reason),
            m_stop_data(stop_data),
            m_container(container)
        {
        }

        const std::string &cdm_session_id() const
        {
            return m_cdm_session_id;
        }

        RfbtvProtocol::CdmSessionTerminateResponseReason reason() const
        {
            return m_reason;
        }

        const std::map<std::string, std::string> &stop_data() const
        {
            return m_stop_data;
        }

        CdmSessionContainer *container() const
        {
            return m_container;
        }

    private:
        const std::string m_cdm_session_id;
        RfbtvProtocol::CdmSessionTerminateResponseReason m_reason;
        const std::map<std::string, std::string> m_stop_data;
        CdmSessionContainer *m_container;
    };

    class ProtocolExtensionSendEvent : public BoundEvent<Impl, ProtocolExtensionSendEvent>
    {
    public:
        ProtocolExtensionSendEvent(Impl &impl, void (Impl::*handler)(const ProtocolExtensionSendEvent &), const std::string &protocol_id, const std::vector<uint8_t> &data) :
            BoundEvent<Impl, ProtocolExtensionSendEvent>(impl, handler),
            m_protocol_id(protocol_id),
            m_data(data)
        {
        }

        const std::string &protocol_id() const
        {
            return m_protocol_id;
        }

        const std::vector<uint8_t> &data() const
        {
            return m_data;
        }

    private:
        std::string m_protocol_id;
        std::vector<uint8_t> m_data;
    };

    class OverlayHandler : public Thread::IRunnable
    {
    public:
        OverlayHandler(Impl &impl, IOverlayCallbacks *overlay_callbacks);
        ~OverlayHandler();

        void start(IContentLoader *content_loader);
        void stop();
        void process_images(const std::vector<PictureParameters> &images, bool clear_flag, bool commit_flag);

    private:
        class OverlaysAvailableEvent : public BoundEvent<Impl::OverlayHandler, OverlaysAvailableEvent>
        {
        public:
            OverlaysAvailableEvent(Impl::OverlayHandler &impl, void (Impl::OverlayHandler::*handler)(const OverlaysAvailableEvent &), const std::vector<PictureParameters> &images, bool clear_flag, bool commit_flag) :
                BoundEvent<Impl::OverlayHandler, OverlaysAvailableEvent>(impl, handler),
                m_images(images),
                m_clear_flag(clear_flag),
                m_commit_flag(commit_flag)
            {
            }
            std::vector<PictureParameters> &images() const
            {
                return m_images;
            }
            bool clear_flag() const
            {
                return m_clear_flag;
            }
            bool commit_flag() const
            {
                return m_commit_flag;
            }
        private:
            mutable std::vector<PictureParameters> m_images;
            bool m_clear_flag;
            bool m_commit_flag;
        };

        bool run();
        void handle_overlay_event(const OverlaysAvailableEvent &event);

        Impl &m_impl;
        IOverlayCallbacks *m_overlay_callbacks;
        Thread m_thread;
        IContentLoader *m_content_loader;
        EventQueue m_new_overlays_available;
    };

    mutable Mutex m_mutex;

    std::string m_current_stream_uri;

    ClientContext &m_context;
    ISessionCallbacks *m_session_callbacks;
    IOverlayCallbacks *m_overlay_callbacks;

    EchoProtocolExtension m_echo_protocol;

    std::map<std::string, IProtocolExtension *> m_protocol_extensions;
    IDefaultProtocolHandler *m_default_handler;
    std::vector<ICdmSessionFactory *> m_drm_systems;
    std::map<std::string, CdmSessionContainer *> m_active_cdm_sessions;
    std::map<std::string, IHandoffHandler *> m_handoff_handlers;

    Streamer m_streamer;
    TimerEngine m_timer;

    // Current content loader
    IContentLoader *m_content_loader;

    // Handler for framebuffer updates that use URL encoding
    OverlayHandler m_overlay_handler;

    // Reporting state
    PlaybackReport m_playback_report;
    ReportManager m_playback_report_manager;
    BoundTimerEngineTimer<Session::Impl> m_playback_report_periodic_trigger;
    LatencyReport m_latency_report;
    ReportManager m_latency_report_manager;
    LogReport m_log_report;
    ReportManager m_log_report_manager;
    TimeStamp m_session_start_time;

    TimeStamp m_stalled_timestamp;
    ILogOutput *m_prev_log_output;
    Mutex m_log_mutex;
    std::vector<std::pair<LogMessageType, std::string> > m_log_backlog;
    bool m_is_logging;

    // Client-controlled session parameters
    std::string m_session_url;
    uint16_t m_screen_width;
    uint16_t m_screen_height;
    std::map<std::string, std::string> m_param_list;

    // Dynamic session state
    std::string m_session_id;
    uint8_t m_rfbtv_button_mask;
    std::string m_local_udp_url;
    unsigned int m_redirect_count;
    RFBTV_STATE m_rfbtv_state;
    Atomic<State> m_state;
    bool m_closing_suspended;
    int m_connect_attempts;

    // Misc session state
    EventQueue m_event_queue;   // Needs to be constructed before m_connection since m_connection might write into m_stream_queue upon destruction (stopping m_connection in the destructor should solve this, but apparently this doesn't solve all cases...)
    TcpConnection m_connection;
    Thread m_event_handling_thread;
    RfbtvMessage m_rx_message;
    RfbtvProtocol m_rfbtv_protocol;
    KeyFilter m_key_filter;
    BoundTimerEngineTimer<Session::Impl> m_connection_backoff_time_callback;

    // Stream & stream error handling
    BoundTimerEngineTimer<Session::Impl> m_stream_error_callback;
    BoundTimerEngineTimer<Streamer> m_streamer_periodic_trigger;
    enum StreamConfirmSentState {
        STREAM_CONFIRM_NOT_SENT,
        STREAM_CONFIRM_OK_SENT,
        STREAM_CONFIRM_ERROR_SENT
    };
    StreamConfirmSentState m_stream_confirm_sent_state;

    // Implements IProtocolExtension::IReply
    void send(const IProtocolExtension &protocol_extension, const uint8_t *data, uint32_t length);

    // Implements IMediaPlayer::ICallback
    void player_event(IMediaPlayer::PlayerEvent event);

    // Implements IControl
    void initiate(const std::string &host, const std::string &url, uint32_t screen_width, uint32_t screen_height, const std::map<std::string, std::string> &optional_parameters);
    void terminate();
    void suspend();
    void resume();
    void update_session_optional_parameters(const std::map<std::string, std::string> &key_value_pairs);

    // Implements IInput
    void send_keycode(int key, Action action, bool &client_must_handle_key_code /*out*/);
    void send_pointer_event(uint32_t x, uint32_t y, Button button, Action action);

    // Implements ILatencyData
    void latency_stream_data(ILatencyData::LatencyDataType data_type, TimeStamp pts, TimeStamp original_event_time);

    // Implements IStallEvent
    void stall_detected(const std::string &id, bool is_audio_not_video, const TimeStamp &stall_duration);

    // Implements IReportTransmitter
    ResultCode request_transmission(ReportBase &);

    // Implements ILogOutput
    void log_message(LogMessageType message_type, const char *message);

    // Implements IStream
    void stream_data(const uint8_t *data, uint32_t length);
    void stream_error(ResultCode result);

    // Implements RfbtvProtocol::ICallbacks
    // Our mutex is already locked when these are called
    ResultCode frame_buffer_update(std::vector<PictureParameters> &images, bool clear_flag, bool commit_flag);
    ResultCode session_setup_response(RfbtvProtocol::ICallbacks::SessionSetupResult result, const std::string &session_id, const std::string &redirect_url, const std::string &cookie);
    ResultCode session_terminate_request(RfbtvProtocol::ICallbacks::SessionTerminateReason code);
    ResultCode ping();
    ResultCode stream_setup_request(const std::string &uri, const std::map<std::string, std::string> &stream_params);
    ResultCode passthrough(const std::string &protocol_id, const std::vector<uint8_t> &data);
    ResultCode server_command_keyfilter_control(const std::string &local_keys, const std::string &remote_keys);
    ResultCode server_command_playback_control(ReportMode report_mode, uint32_t interval_in_ms);
    ResultCode server_command_latency_control(ReportMode report_mode, bool is_duration, bool is_event);
    ResultCode server_command_log_control(ReportMode report_mode, LogMessageType min_log_level);
    ResultCode server_command_video_control(VideoMode mode);
    ResultCode server_command_underrun_mitigation_control(const std::map<std::string, std::string> &parameter_value_pairs);
    ResultCode handoff_request(const std::string &uri, bool resume_session_when_done);
    ResultCode cdm_setup_request(const std::string &cdm_session_id, const uint8_t (&drm_system_id)[16], const std::string &session_type, const std::map<std::string, std::string> &init_data);
    ResultCode cdm_terminate_request(const std::string &cdm_session_id, RfbtvProtocol::ICallbacks::CdmSessionTerminateReason reason);

    // Implements Thread::IRunnable
    bool run();

    // Private helper methods
    void start_message_handling_thread();
    void stop_message_handling_thread();

    void stop_streaming();

    void set_state(State state, ClientErrorCode reason);

    bool is_idle() const; // Returns true if the session is idle (either in INIT or ERROR state)
    bool is_active() const; // Returns true if the session is active (in ACTIVE state)
    bool is_suspended() const; // Returns true if the session is suspended (in SUSPENDED state)

    // Event handlers (from event queue)
    void handle_initiate_event(const InitiateEvent &event);
    void handle_terminate_event(const TerminateEvent &event);
    void handle_suspend_event(const TriggerEvent &event);
    void handle_resume_event(const TriggerEvent &event);
    void handle_frame_buffer_update_request_event(const TriggerEvent &event);
    void handle_update_session_optional_parameters_event(const ParameterUpdateEvent &event);
    void handle_send_keycode_event(const KeyEvent &event);
    void handle_pointer_event(const PointerEvent &event);
    void handle_player_event(const PlayerEvent &event);
    void handle_stream_data_event(const StreamDataEvent &event);
    void handle_stream_error_event(const StreamErrorEvent &event);
    void handle_latency_data_event(const LatencyDataEvent &event);
    void handle_stall_event(const StallEvent &event);
    void handle_connect_event(const TriggerEvent &event);
    void handle_cdm_session_terminate_event(const CdmSessionTerminateEvent &event);
    void handle_cdm_terminate_result(const CdmTerminateResultEvent &event);
    void handle_cdm_setup_result(const CdmSetupResultEvent &event);
    void handle_protocol_extension_send_event(const ProtocolExtensionSendEvent &event);
    void handle_stream_timeout_expired_event(const TriggerEvent &event);
    void handle_playback_report_trigger_event(const TriggerEvent &event);

    void cdm_session_terminate_indication(const std::string &cdm_session_id, ICdmSession::ICallback::TerminateReason reason);
    void cdm_setup_result(const std::string &cdm_session_id, ICdmSession::SetupResult result, const std::map<std::string, std::string> &response, CdmSessionContainer *container);
    void cdm_terminate_result(const std::string &cdm_session_id, RfbtvProtocol::CdmSessionTerminateResponseReason reason, const std::map<std::string, std::string> &stop_data, CdmSessionContainer *container);
    ResultCode rfbtvpm_cdm_session_terminate(const std::string &cdm_session_id, RfbtvProtocol::CdmSessionTerminateResponseReason reason);
    void rfbtvpm_register_active_cdm_stream_decrypt_engine();
    void rfbtvpm_clean_active_cdm_sessions();

    void close_session_in_case_of_error(ResultCode error);

    void rfbtvpm_set_state(enum RFBTV_STATE value, ClientErrorCode reason);
    static const char *rfbtvpm_get_state_name(RFBTV_STATE);
    static const char *rfbtvpm_get_state_name(State state);

    ResultCode rfbtvpm_session_stop(ClientErrorCode error_code, RfbtvProtocol::SessionTerminateReason reason);
    ResultCode rfbtvpm_session_suspend();
    void rfbtvpm_close_connection();
    ResultCode rfbtvpm_send_message(const RfbtvMessage &msg);
    void rfbtvpm_reconnect(bool do_immediately);
    ResultCode rfbtvpm_handle_rfbtv_version_string(RfbtvMessage &message);
    void rfbtvpm_send_appropriate_stream_confirm_error(const IMediaPlayer::PlayerEvent &event);

    void connection_backoff_time_expired();
    void stream_timeout_expired();
    void playback_report_periodic_trigger();
    void post_frame_buffer_update_request();
};

} // namespace
