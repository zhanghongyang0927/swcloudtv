///
/// \file RfbtvProtocol.h
///
/// \brief Class that can create and parse RFB-TV messages.
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include "RfbtvMessage.h"

#include <core/IHandoffHandler.h>

#include <porting_layer/ResultCode.h>
#include <porting_layer/X11KeyMap.h>
#include <porting_layer/Log.h>

#include <string>
#include <vector>
#include <map>

namespace ctvc {

class PlaybackReport;
class LatencyReport;
class LogReport;
struct PictureParameters;
class Histogram;

class RfbtvProtocol
{
public:
    static const ResultCode NEED_MORE_DATA;
    static const ResultCode PARSING_MESSAGE;
    static const ResultCode INVALID_SERVER_VERSION;

    struct ICallbacks
    {
        virtual ResultCode frame_buffer_update(std::vector<PictureParameters> &images, bool clear_flag, bool commit_flag) = 0;

        enum SessionSetupResult
        {
            SESSION_SETUP_OK,
            SESSION_SETUP_REDIRECT,
            SESSION_SETUP_INVALID_CLIENT_ID,
            SESSION_SETUP_APP_NOT_FOUND,
            SESSION_SETUP_CONFIG_ERROR,
            SESSION_SETUP_NO_RESOURCES,
            SESSION_SETUP_UNSPECIFIED_ERROR,
            SESSION_SETUP_INVALID_PARAMETERS,
            SESSION_SETUP_INTERNAL_SERVER_ERROR,
            SESSION_SETUP_UNDEFINED_ERROR
        };
        virtual ResultCode session_setup_response(SessionSetupResult result, const std::string &session_id, const std::string &redirect_url, const std::string &cookie) = 0;

        enum SessionTerminateReason
        {
            SESSION_TERMINATE_USER_STOP = 0, /*!< Session is stopping due to natural causes.*/
            SESSION_TERMINATE_INSUFFICIENT_BANDWIDTH, /*!< Session is stopping because the bandwidth is insufficient.*/
            SESSION_TERMINATE_LATENCY_TOO_LARGE, /*!< Session is stopping because the latency is too large.*/
            SESSION_TERMINATE_SUSPEND, /*!< Session is stopping because the session needs to be suspended.*/
            SESSION_TERMINATE_UNSPECIFIED_ERROR, /*!< Session is stopping because an unspecified server error.*/
            SESSION_TERMINATE_DO_NOT_RETUNE, /*!< Session is stopping normally but the client should not tune away from what is currently showing.*/
            SESSION_TERMINATE_PING_TIMEOUT, /*!< Session is stopping because the ping message timed out.*/
            SESSION_TERMINATE_INTERNAL_SERVERERROR, /*!< Session is stopping because of an internal server error during a session.*/
            SESSION_TERMINATE_SERVER_SHUTTINGDOWN, /*!< Session is stopping because the server is shutting down.*/
            SESSION_TERMINATE_FAILED_APPLICATION_STREAM_SETUP, /*!< Session is stopping because the server could not setup the application stream.*/
            SESSION_TERMINATE_UNDEFINED_ERROR /*! None of the above. */
        };
        virtual ResultCode session_terminate_request(SessionTerminateReason code) = 0;

        virtual ResultCode ping() = 0;

        virtual ResultCode stream_setup_request(const std::string &uri, const std::map<std::string, std::string> &stream_params) = 0;

        virtual ResultCode passthrough(const std::string &protocol_id, const std::vector<uint8_t> &data) = 0;

        enum ReportMode
        {
            REPORT_NOCHANGE, REPORT_DISABLED, REPORT_ONE_SHOT, REPORT_AUTOMATIC, REPORT_ACCUMULATE
        };
        enum VideoMode
        {
            MODE_NOCHANGE, MODE_GUI_OPTIMIZED, MODE_VIDEO_OPTIMIZED
        };
        virtual ResultCode server_command_keyfilter_control(const std::string &local_keys, const std::string &remote_keys) = 0;
        virtual ResultCode server_command_playback_control(ReportMode report_mode, uint32_t interval_in_ms) = 0;
        virtual ResultCode server_command_latency_control(ReportMode report_mode, bool is_duration, bool is_event) = 0;
        virtual ResultCode server_command_log_control(ReportMode report_mode, LogMessageType min_log_level) = 0;
        virtual ResultCode server_command_video_control(VideoMode mode) = 0;
        virtual ResultCode server_command_underrun_mitigation_control(const std::map<std::string, std::string> &parameter_value_pairs) = 0;

        virtual ResultCode handoff_request(const std::string &uri, bool resume_session_when_done) = 0;

        virtual ResultCode cdm_setup_request(const std::string &cdm_session_id, const uint8_t (&drm_system_id)[16], const std::string &session_type, const std::map<std::string, std::string> &init_data) = 0;

        enum CdmSessionTerminateReason
        {
            CDM_SESSION_TERMINATE_REASON_USER_STOP, CDM_SESSION_TERMINATE_REASON_OTHER
        };
        virtual ResultCode cdm_terminate_request(const std::string &cdm_session_id, ICallbacks::CdmSessionTerminateReason reason) = 0;
    };

    RfbtvProtocol(ICallbacks &callbacks);
    ~RfbtvProtocol();

    enum ProtocolVersion
    {
        RFBTV_PROTOCOL_UNKNOWN, RFBTV_PROTOCOL_V1_3, RFBTV_PROTOCOL_V2_0
    };

    // Set the protocol version to use
    // May be used to reset to default or to force a version when unit testing.
    void set_version(ProtocolVersion version);

    // Get the current protocol version
    // Should not be necessary but can be used to switch the use of certain messages.
    ProtocolVersion get_version() const;

    //
    // Methods to create an RFB-TV message
    //
    RfbtvMessage create_set_encodings(bool is_url_encoding_supported);

    RfbtvMessage create_frame_buffer_update_request(uint16_t screen_width, uint16_t screen_height);

    enum KeyAction
    {
        KEY_UP = 0, KEY_DOWN = 1, KEYINPUT = 2
    };
    RfbtvMessage create_key_event(X11KeyCode key, KeyAction key_action);

    RfbtvMessage create_pointer_event(int button_mask, int x, int y);

    enum SessionTerminateReason
    {
        SESSION_TERMINATE_NORMAL = 0,
        SESSION_TERMINATE_SUSPEND = 1,
        SESSION_TERMINATE_HANDOFF = 2,
        SESSION_TERMINATE_CLIENT_EXECUTION_ERROR = 3
    };
    RfbtvMessage create_session_terminate_indication(SessionTerminateReason reason);

    RfbtvMessage create_playback_client_report(const PlaybackReport &playback_report);

    RfbtvMessage create_latency_client_report(const LatencyReport &latency_report);

    RfbtvMessage create_log_client_report(const LogReport &log_report);

    RfbtvMessage create_session_setup(const std::string &client_id, const std::map<std::string, std::string> &param_list, const std::string &session_id, const std::string &cookie);

    enum StreamSetupResponseCode
    {
        STREAM_SETUP_SUCCESS, /*!< The client is ready to receive the stream.*/
        STREAM_SETUP_CABLE_TUNING_ERROR, /*!< There was a tuning error when trying to tune to a channel.*/
        STREAM_SETUP_IP_RESOURCE_ERROR, /*!< The specified resource could not be found*/
        STREAM_SETUP_UNSUPPORTED_URI, /*!< The client does not support the specified URI scheme.*/
        STREAM_SETUP_CONNECTION_FAILED, /*!< Connection to remote-host could not be established, RFB-TV 2.0.*/
        STREAM_SETUP_UNSPECIFIED_ERROR /*!< Unspecified error (if no one applies), RFB-TV 2.0.*/
    };
    RfbtvMessage create_stream_setup_response(StreamSetupResponseCode result, const std::map<std::string, std::string> &parameters, const std::string &local_udp_url);

    enum StreamConfirmCode
    {
        STREAM_CONFIRM_SUCCESS, /*!< The client can successfully stream the URI.*/
        STREAM_CONFIRM_DESCRAMBLE_ERROR, /*!< The client failed to descramble the stream.*/
        STREAM_CONFIRM_DECODE_ERROR, /*!< The client failed to decode the stream.*/
        STREAM_CONFIRM_TSID_ERROR, /*!< No transport stream with the indicated TSID was found.*/
        STREAM_CONFIRM_NID_ERROR, /*!< No network with the indicated NID was found.*/
        STREAM_CONFIRM_PID_ERROR, /*!< No program with the indicated PID was found.*/
        STREAM_CONFIRM_PHYSICAL_ERROR, /*!< Unrecoverable error at the physical layer.*/
        STREAM_CONFIRM_UNSPECIFIED_ERROR /*!< Unspecified error (if no other applies).*/
    };
    RfbtvMessage create_stream_confirm(StreamConfirmCode result);

    RfbtvMessage create_pong();

    // TODO (CNP-1987): RfbtvMessage create_input_event();

    RfbtvMessage create_passthrough(const std::string &protocol_id, const std::vector<uint8_t> &data);

    // New messages in RFB-TV 2.0
    RfbtvMessage create_session_update(const std::map<std::string, std::string> &changed_params);

    RfbtvMessage create_handoff_result(IHandoffHandler::HandoffResult result, const std::string &player_specific_error);

    RfbtvMessage create_key_time_event(X11KeyCode key, KeyAction key_action, const std::string &timestamp);

    enum CdmSessionSetupResponseResult // (All RFB-TV 2.0.)
    {
        CDM_SESSION_SETUP_RESPONSE_RESULT_SUCCESS = 0, /*!< Success (license available) */
        CDM_SESSION_SETUP_RESPONSE_RESULT_LICENSE_NOT_FOUND = 60, /*!< License was not found */
        CDM_SESSION_SETUP_RESPONSE_RESULT_DRM_SYSTEM_NOT_INSTALLED = 61, /*!< DRM system not installed */
        CDM_SESSION_SETUP_RESPONSE_RESULT_DRM_SYSTEM_ERROR = 62, /*!< DRM system error */
        CDM_SESSION_SETUP_RESPONSE_RESULT_NO_LICENSE_SERVER = 68, /*!< No license server location */
        CDM_SESSION_SETUP_RESPONSE_RESULT_UNSPECIFIED_ERROR = 255 /*!< Unspecified error */
    };
    RfbtvMessage create_cdm_setup_response(const std::string &cdm_session_id, CdmSessionSetupResponseResult result, const std::map<std::string, std::string> &response_fields);

    enum CdmSessionTerminateResponseReason // (All RFB-TV 2.0.)
    {
        CDM_SESSION_TERMINATE_RESPONSE_REASON_USER_STOP = 0, /*!< User stop */
        CDM_SESSION_TERMINATE_RESPONSE_REASON_OTHER = 1, /*!< Other */
        CDM_SESSION_TERMINATE_RESPONSE_REASON_SERVER_REQUEST = 2, /*!< Server request */
        CDM_SESSION_TERMINATE_RESPONSE_REASON_END_OF_STREAM = 3, /*!< End of stream */
        CDM_SESSION_TERMINATE_RESPONSE_REASON_LICENSE_EXPIRED = 4, /*!< License expired */
        CDM_SESSION_TERMINATE_RESPONSE_REASON_UNKNOWN_SESSION = 5 /*!< Unknown session */
    };
    RfbtvMessage create_cdm_terminate_indication(const std::string &cdm_session_id, CdmSessionTerminateResponseReason reason, const std::map<std::string, std::string> &data);

    //
    // Methods to parse an RFB-TV message
    //
    ResultCode parse_version_string(RfbtvMessage &message, const char *&client_version_string/*out*/);

    ResultCode parse_message(RfbtvMessage &message);

private:
    ProtocolVersion m_protocol_version;
    ICallbacks &m_callbacks;

    // Message handling
    typedef ResultCode (RfbtvProtocol::*MessageHandler)(RfbtvMessage &message);
    std::map<uint8_t, MessageHandler> m_message_map;

    ResultCode rect_read(RfbtvMessage &rx_message, PictureParameters &rect);
    static void append_histogram(std::string &out, const std::string &name, const Histogram &histogram); // Helper method

    // RFB-TV message handlers
    ResultCode parse_frame_buffer_update(RfbtvMessage &rx_message);
    ResultCode parse_session_setup_response(RfbtvMessage &rx_message);
    ResultCode parse_session_terminate_request(RfbtvMessage &rx_message);
    ResultCode parse_ping(RfbtvMessage &message);
    ResultCode parse_stream_setup_request(RfbtvMessage &rx_message);
    ResultCode parse_passthrough(RfbtvMessage &rx_message);
    ResultCode parse_server_command(RfbtvMessage &rx_message);
    ResultCode parse_handoff_request(RfbtvMessage &rx_message);
    ResultCode parse_cdm_setup_request(RfbtvMessage &rx_message);
    ResultCode parse_cdm_terminate_request(RfbtvMessage &rx_message);
};

} // namespace
