///
/// \file RfbtvProtocol.cpp
/// \brief Includes the rfbtv protocol translation routines.
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include "RfbtvProtocol.h"
#include "RfbtvMessage.h"
#include "PlaybackReport.h"
#include "LatencyReport.h"
#include "LogReport.h"

#include <core/IHandoffHandler.h>
#include <core/IOverlayCallbacks.h>

#include <porting_layer/Log.h>

#include <utils/create_map.h>
#include <utils/utils.h>

#include <map>
#include <string>
#include <vector>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "swcloudtv_priv.h"

using namespace ctvc;

const ResultCode RfbtvProtocol::NEED_MORE_DATA("Not enough data to process the message");
const ResultCode RfbtvProtocol::PARSING_MESSAGE("The message could not be parsed");
const ResultCode RfbtvProtocol::INVALID_SERVER_VERSION("Invalid version received from the server");

enum RFBClientMessageType
{
    // RFB-TV 1.3.2
    RFBClientMessageType_SetEncodings = 2,
    RFBClientMessageType_FramebufferUpdateRequest = 3,
    RFBClientMessageType_KeyEvent = 4,
    RFBClientMessageType_PointerEvent = 5,
    RFBClientMessageType_ClientReport = 16, // The name in RFB-TV 1.3.2 is PlaybackReport. Both have the same message number.
    RFBClientMessageType_SessionTerminateIndication = 17,
    RFBClientMessageType_SessionSetup = 18,
    RFBClientMessageType_StreamConfirm = 19,
    RFBClientMessageType_StreamSetupResponse = 20,
    RFBClientMessageType_Pong = 21,
    RFBClientMessageType_InputEvent = 22,
    RFBClientMessageType_PassThrough = 23,

    // RFB-TV 2.0.0
    RFBClientMessageType_SessionUpdate = 24,
    RFBClientMessageType_HandoffResult = 25,
    RFBClientMessageType_KeyTimeEvent = 26,
    RFBClientMessageType_CdmSetupResponse = 27,
    RFBClientMessageType_CdmTerminateIndication = 28
};

enum RFBServerMessageType
{
    // RFB-TV 1.3.2
    RFBServerMessageType_FramebufferUpdate = 0,
    RFBServerMessageType_SessionSetupResponse = 16,
    RFBServerMessageType_SessionTerminateRequest = 17,
    RFBServerMessageType_Ping = 18,
    RFBServerMessageType_StreamSetupRequest = 19, // In RFB-TV 1.3.2 was called StreamSetup
    RFBServerMessageType_PassThrough = 21,

    // RFB-TV 2.0.0
    RFBServerMessageType_ServerCommand = 22,
    RFBServerMessageType_HandoffRequest = 23,
    RFBServerMessageType_CdmSetupRequest = 24,
    RFBServerMessageType_CdmTerminateRequest = 25,
};

const unsigned int RFBTV_VERSION_SIZE = 15;

static const int RFB_ENCODING_PICTURE_OBJECT = 42;
static const int RFB_ENCODING_URL = 43;

// Local helper function
static inline std::string get_map_value(const std::map<std::string, std::string> &key_value_pairs, const std::string &key)
{
    std::map<std::string, std::string>::const_iterator i = key_value_pairs.find(key);
    if (i != key_value_pairs.end()) {
        return i->second;
    }
    return std::string();
}

RfbtvProtocol::RfbtvProtocol(ICallbacks &callbacks) :
    m_protocol_version(RFBTV_PROTOCOL_UNKNOWN),
    m_callbacks(callbacks)
{
}

RfbtvProtocol::~RfbtvProtocol()
{
}

void RfbtvProtocol::set_version(ProtocolVersion protocol_version)
{
    m_protocol_version = protocol_version;

    m_message_map.clear();

    // Setup the message handler table according to the current protocol
    // All protocol versions
    m_message_map[RFBServerMessageType_FramebufferUpdate] = &RfbtvProtocol::parse_frame_buffer_update;
    m_message_map[RFBServerMessageType_SessionSetupResponse] = &RfbtvProtocol::parse_session_setup_response;
    m_message_map[RFBServerMessageType_SessionTerminateRequest] = &RfbtvProtocol::parse_session_terminate_request;
    m_message_map[RFBServerMessageType_Ping] = &RfbtvProtocol::parse_ping;
    m_message_map[RFBServerMessageType_StreamSetupRequest] = &RfbtvProtocol::parse_stream_setup_request;
    m_message_map[RFBServerMessageType_PassThrough] = &RfbtvProtocol::parse_passthrough;
	CLOUDTV_LOG_DEBUG("set_version...\n");

    // Protocol V2.0
    if (m_protocol_version == RfbtvProtocol::RFBTV_PROTOCOL_V2_0) {
        m_message_map[RFBServerMessageType_ServerCommand] = &RfbtvProtocol::parse_server_command;
        m_message_map[RFBServerMessageType_HandoffRequest] = &RfbtvProtocol::parse_handoff_request;
        m_message_map[RFBServerMessageType_CdmSetupRequest] = &RfbtvProtocol::parse_cdm_setup_request;
        m_message_map[RFBServerMessageType_CdmTerminateRequest] = &RfbtvProtocol::parse_cdm_terminate_request;
    }
}

RfbtvProtocol::ProtocolVersion RfbtvProtocol::get_version() const
{
    return m_protocol_version;
}

RfbtvMessage RfbtvProtocol::create_set_encodings(bool is_url_encoding_supported)
{
    CTVC_LOG_DEBUG("");

    RfbtvMessage msg;

    msg.write_uint8(RFBClientMessageType_SetEncodings);
    msg.write_uint8(0); // Padding

    if (is_url_encoding_supported) {
        CTVC_LOG_DEBUG("Tell server we can handle both 'Picture Object' and 'URL' encodings");
        msg.write_uint16(2); // Number of encodings
        msg.write_uint32(RFB_ENCODING_PICTURE_OBJECT);
        msg.write_uint32(RFB_ENCODING_URL);
    } else {
        CTVC_LOG_DEBUG("Tell server we can only handle 'Picture Object' encoding");
        msg.write_uint16(1); // Number of encodings
        msg.write_uint32(RFB_ENCODING_PICTURE_OBJECT);
    }

    return msg;
}

RfbtvMessage RfbtvProtocol::create_frame_buffer_update_request(uint16_t screen_width, uint16_t screen_height)
{
    CTVC_LOG_DEBUG("%dx%d", screen_width, screen_height);

    RfbtvMessage msg;

    msg.write_uint8(RFBClientMessageType_FramebufferUpdateRequest);
    msg.write_uint8(1); // Incremental
    msg.write_uint16(0); // x position
    msg.write_uint16(0); // y position
    msg.write_uint16(screen_width);
    msg.write_uint16(screen_height);

    return msg;
}

RfbtvMessage RfbtvProtocol::create_key_event(X11KeyCode key, KeyAction key_action)
{
    RfbtvMessage msg;

    msg.write_uint8(RFBClientMessageType_KeyEvent);
    msg.write_uint8(key_action); // "event" in the protocol
    msg.write_uint16(0);
    msg.write_uint32(key);

    return msg;
}

RfbtvMessage RfbtvProtocol::create_pointer_event(int button_mask, int x, int y)
{
    RfbtvMessage msg;

    msg.write_uint8(RFBClientMessageType_PointerEvent);
    msg.write_uint8(button_mask);
    msg.write_uint16(x);
    msg.write_uint16(y);

    return msg;
}

void RfbtvProtocol::append_histogram(std::string &out, const std::string &name, const Histogram &histogram)
{
    out += ",\"" + name + "\":[";
    uint32_t n_bins = histogram.get_bin_definition().get_n_bins();
    for (uint32_t j = 0; j < n_bins; j++) {
        if (j > 0) {
            out += ',';
        }
        string_printf_append(out, "%u", histogram.get_entry(j));
    }
    out += "]";
}

RfbtvMessage RfbtvProtocol::create_playback_client_report(const PlaybackReport &playback_report)
{
    // This method only implements the RFB-TV 2.0 version of the playback report; the RFB-TV 1.3
    // version is not supported because it is not implemented in any RFB-TV 1.3 version server.
    RfbtvMessage msg;

    if (m_protocol_version == RFBTV_PROTOCOL_V1_3) {
        // We don't support the RFB-TV 1.3 playback control message, so if we ever want to send it, we'll return an empty message that won't disrupt the protocol
        CTVC_LOG_WARNING("Message not supported in RFB-TV 1.3");
        return msg;
    }

    msg.write_uint8(RFBClientMessageType_ClientReport);

    msg.write_string("playback");

    std::map<std::string, std::string> fields;

    if (playback_report.m_playback_state.is_set()) {
        switch (playback_report.m_playback_state.get()) {
        case PlaybackReport::STARTING:
            fields["playstate"] = "starting";
            break;
        case PlaybackReport::PLAYING:
            fields["playstate"] = "playing";
            break;
        case PlaybackReport::STALLED:
            fields["playstate"] = "stalled";
            break;
        case PlaybackReport::STOPPED:
            fields["playstate"] = "stopped";
            break;
        }
    }

    if (playback_report.m_stalled_duration_in_ms.is_set()) {
        string_printf(fields["duration_stalled"], "%u", playback_report.m_stalled_duration_in_ms.get());
    }

    if (playback_report.m_current_pts.is_set()) {
        string_printf(fields["current_pts"], "%s", uint64_to_string(playback_report.m_current_pts.get()).c_str());
    }

    if (playback_report.m_pcr_delay.is_set()) {
        string_printf(fields["delay"], "%u", playback_report.m_pcr_delay.get());
    }

    if (playback_report.m_bandwidth.is_set()) {
        string_printf(fields["bandwidth"], "%u", playback_report.m_bandwidth.get());
    }

    std::string histogram_data;
    for (std::map<std::string, std::pair<Histogram *, Histogram *> >::const_iterator i = playback_report.m_stalled_duration_histograms.begin(); i != playback_report.m_stalled_duration_histograms.end(); ++i) {
        Histogram *audio_histogram(i->second.first);
        Histogram *video_histogram(i->second.second);

        if (!histogram_data.empty()) {
            histogram_data += ",";
        }
        histogram_data += "{\"id\":\"" + i->first + "\"";

        if (audio_histogram) {
            append_histogram(histogram_data, "A", *audio_histogram);
        }

        if (video_histogram) {
            append_histogram(histogram_data, "V", *video_histogram);
        }

        histogram_data += "}";
    }
    if (!histogram_data.empty()) {
        fields["histograms"] = "[" + histogram_data + "]";
    }

    msg.write_key_value_pairs(fields);

    return msg;
}

RfbtvMessage RfbtvProtocol::create_latency_client_report(const LatencyReport &latency_report)
{
    // Create comma-separated value strings
    std::string subtypes, labels, data;
    for (uint32_t i = 0; i < latency_report.get_n_entries(); i++) {
        if (i > 0) {
            subtypes += ",";
            labels += ",";
            data += ",";
        }

        switch (latency_report.get_subtype(i)) {
        case LatencyReport::SUBTYPE_SESSION_START_TO_STREAM:
            subtypes += "session_start_to_stream";
            break;
        case LatencyReport::SUBTYPE_SESSION_START_TO_FIRSTPAINT:
            subtypes += "session_start_to_firstpaint";
            break;
        case LatencyReport::SUBTYPE_SESSION_START_TO_COMPLETE:
            subtypes += "session_start_to_complete";
            break;
        case LatencyReport::SUBTYPE_KEY_TO_DISPLAY:
            subtypes += "key_to_display";
            break;
        case LatencyReport::SUBTYPE_SESSION_START_BEGIN:
            subtypes += "session_start_begin";
            break;
        case LatencyReport::SUBTYPE_SESSION_START_STREAM:
            subtypes += "session_start_stream";
            break;
        case LatencyReport::SUBTYPE_SESSION_START_FIRSTPAINT_DISPLAY:
            subtypes += "session_start_firstpaint_display";
            break;
        case LatencyReport::SUBTYPE_SESSION_START_COMPLETE_DISPLAY:
            subtypes += "session_start_complete_display";
            break;
        case LatencyReport::SUBTYPE_KEY_SENT:
            subtypes += "key_sent";
            break;
        case LatencyReport::SUBTYPE_KEY_DISPLAY:
            subtypes += "key_display";
            break;
        }

        labels += latency_report.get_label(i);
        string_printf_append(data, "%s", uint64_to_string(latency_report.get_data(i)).c_str());
    }

    RfbtvMessage msg;

    msg.write_uint8(RFBClientMessageType_ClientReport);
    msg.write_string("latency");

    // There always 3 pairs
    msg.write_uint8(3);

    msg.write_key_value_pair("subtypes", subtypes.c_str());
    msg.write_key_value_pair("labels", labels.c_str());
    msg.write_key_value_pair("data", data.c_str());

    return msg;
}

RfbtvMessage RfbtvProtocol::create_log_client_report(const LogReport &log_report)
{
    const char *level_str = "";
    switch (log_report.get_max_level()) {
    case LOG_DEBUG:
        level_str = "debug";
        break;
    case LOG_INFO:
        level_str = "info";
        break;
    case LOG_WARNING:
        level_str = "warning";
        break;
    case LOG_ERROR:
        level_str = "error";
        break;
    }

    RfbtvMessage msg;

    msg.write_uint8(RFBClientMessageType_ClientReport);
    msg.write_string("log");

    // There are always 2 pairs (we don't send any scope field)
    msg.write_uint8(2);

    msg.write_key_value_pair("level", level_str);
    msg.write_key_value_pair("text", log_report.get_text().c_str());
    // msg.write_key_value_pair("scope", "avn"); // Scope is always "avn"; We don't send it currently

    return msg;
}

RfbtvMessage RfbtvProtocol::create_session_terminate_indication(SessionTerminateReason reason)
{
    RfbtvMessage msg;

    msg.write_uint8(RFBClientMessageType_SessionTerminateIndication);
    msg.write_uint8(reason);

    return msg;
}

RfbtvMessage RfbtvProtocol::create_session_setup(const std::string &client_id, const std::map<std::string, std::string> &param_list, const std::string &session_id, const std::string &cookie)
{
    RfbtvMessage msg;

    msg.write_uint8(RFBClientMessageType_SessionSetup);

    // Client ID for RFB-TV 2.0, a non-optional string
    if (m_protocol_version == RFBTV_PROTOCOL_V2_0) {
        CTVC_LOG_DEBUG("client_id:%s", client_id.c_str());
        msg.write_string(client_id);
    }

    uint32_t param_num_position = msg.size();

    // First write all optional parameter as key-value pair list
    msg.write_key_value_pairs(param_list);
    int number_of_parameters = param_list.size();

    // Client ID for RFB-TV 1.3.2, mandatory
    if (m_protocol_version == RFBTV_PROTOCOL_V1_3) {
        CTVC_LOG_DEBUG("client_id:%s", client_id.c_str());
        msg.write_key_value_pair("clientid", client_id.c_str());

        number_of_parameters++;
    }

    // Session ID, only set when resuming a session
    if (!session_id.empty()) {
        CTVC_LOG_DEBUG("session_id:%s", session_id.c_str());
        msg.write_key_value_pair("session_id", session_id.c_str());

        number_of_parameters++;
    }

    // Cookie, only set when we have one
    if (!cookie.empty()) {
        CTVC_LOG_DEBUG("Stored cookie:%s", cookie.c_str());
        msg.write_key_value_pair("cookie", cookie.c_str());

        number_of_parameters++;
    }

    // Patch the number of parameters in the message
    msg[param_num_position] = number_of_parameters;

    return msg;
}

RfbtvMessage RfbtvProtocol::create_stream_setup_response(StreamSetupResponseCode result, const std::map<std::string, std::string> &parameters, const std::string &local_udp_url)
{
    // Map the result to an RFB-TV 2.0 or RFB-TV 1.3.2 code
    bool is_rfbtv_1_3 = m_protocol_version == RfbtvProtocol::RFBTV_PROTOCOL_V1_3;
    uint8_t code = 0;
    switch (result) {
    case STREAM_SETUP_SUCCESS:
        code = 0;
        break;
    case STREAM_SETUP_CABLE_TUNING_ERROR:
        code = 20;
        break;
    case STREAM_SETUP_IP_RESOURCE_ERROR:
        code = 21;
        break;
    case STREAM_SETUP_UNSUPPORTED_URI:
        code = 22;
        break;
    case STREAM_SETUP_CONNECTION_FAILED:
        code = is_rfbtv_1_3 ? 21 : 24;
        break;
    case STREAM_SETUP_UNSPECIFIED_ERROR:
        code = is_rfbtv_1_3? 21 : 255;
        break;
    }

    RfbtvMessage msg;

    msg.write_uint8(RFBClientMessageType_StreamSetupResponse);
    msg.write_uint8(code);

    if (m_protocol_version == RFBTV_PROTOCOL_V2_0) {
        msg.write_key_value_pairs(parameters);
    } else {
        msg.write_string(local_udp_url);
    }

    return msg;
}

RfbtvMessage RfbtvProtocol::create_stream_confirm(StreamConfirmCode result)
{
    // Map the result to an RFB-TV 2.0 or RFB-TV 1.3.2 code
    bool is_rfbtv_1_3 = m_protocol_version == RfbtvProtocol::RFBTV_PROTOCOL_V1_3;
    uint8_t code = 0;
    switch (result) {
    case STREAM_CONFIRM_SUCCESS:
        code = 0;
        break;
    case STREAM_CONFIRM_DESCRAMBLE_ERROR:
        code = 30;
        break;
    case STREAM_CONFIRM_DECODE_ERROR:
        code = 31;
        break;
    case STREAM_CONFIRM_TSID_ERROR:
        code = 32;
        break;
    case STREAM_CONFIRM_NID_ERROR:
        code = 33;
        break;
    case STREAM_CONFIRM_PID_ERROR:
        code = 34;
        break;
    case STREAM_CONFIRM_PHYSICAL_ERROR:
        code = 35;
        break;
    case STREAM_CONFIRM_UNSPECIFIED_ERROR:
        code = is_rfbtv_1_3? 36 : 255;
        break;
    }

    RfbtvMessage msg;

    msg.write_uint8(RFBClientMessageType_StreamConfirm);
    msg.write_uint8(code);

    return msg;
}

RfbtvMessage RfbtvProtocol::create_pong()
{
    RfbtvMessage msg;

    msg.write_uint8(RFBClientMessageType_Pong);

    return msg;
}

// TODO: (CNP-1987) ResultCode RfbtvProtocol::create_input_event();

RfbtvMessage RfbtvProtocol::create_passthrough(const std::string &protocol_id, const std::vector<uint8_t> &data)
{
    RfbtvMessage msg;

    msg.write_uint8(RFBClientMessageType_PassThrough);
    msg.write_string(protocol_id);
    msg.write_blob(data);

    return msg;
}

RfbtvMessage RfbtvProtocol::create_session_update(const std::map<std::string, std::string> &changed_params)
{
    RfbtvMessage msg;

    if (m_protocol_version == RFBTV_PROTOCOL_V1_3) {
        // Not supported in RFB-TV 1.3, so if we ever want to send it, we'll return an empty message that won't disrupt the protocol
        CTVC_LOG_WARNING("Message not supported in RFB-TV 1.3");
        return msg;
    }

    msg.write_uint8(RFBClientMessageType_SessionUpdate);
    msg.write_key_value_pairs(changed_params);

    return msg;
}

RfbtvMessage RfbtvProtocol::create_handoff_result(IHandoffHandler::HandoffResult result, const std::string &player_specific_error)
{
    static const std::map<IHandoffHandler::HandoffResult, uint8_t> s_handoff_result_map = create_map<IHandoffHandler::HandoffResult, uint8_t>
        (IHandoffHandler::HANDOFF_UNSUPPORTED_URI, 22)
        (IHandoffHandler::HANDOFF_FAILED_TO_DESCRAMBLE_STREAM, 30)
        (IHandoffHandler::HANDOFF_FAILED_TO_DECODE_STREAM, 31)
        (IHandoffHandler::HANDOFF_NO_TRANSPORT_STREAM_WITH_INDICATED_ID, 32)
        (IHandoffHandler::HANDOFF_NO_NETWORK_WITH_INDICATED_ID, 33)
        (IHandoffHandler::HANDOFF_NO_PROGRAM_WITH_INDICATED_ID, 34)
        (IHandoffHandler::HANDOFF_PHYSICAL_LAYER_ERROR, 35)
        (IHandoffHandler::HANDOFF_REQUIRED_MEDIA_PLAYER_ABSENT, 41)
        (IHandoffHandler::HANDOFF_ERRONEOUS_REQUEST, 42)
        (IHandoffHandler::HANDOFF_ASSET_NOT_FOUND, 43)
        (IHandoffHandler::HANDOFF_TRANSPORT_LAYER_ERROR, 50)
        (IHandoffHandler::HANDOFF_PLAYER_ERROR, 51)
        (IHandoffHandler::HANDOFF_APP_NOT_FOUND, 52);

    uint8_t code = 255; // IHandoffHandler::HANDOFF_UNSPECIFIED_ERROR will map to this as well
    std::map<IHandoffHandler::HandoffResult, uint8_t>::const_iterator i = s_handoff_result_map.find(result);
    if (i != s_handoff_result_map.end()) {
        code = i->second;
    }

    RfbtvMessage msg;

    if (m_protocol_version == RFBTV_PROTOCOL_V1_3) {
        // Not supported in RFB-TV 1.3, so if we ever want to send it, we'll return an empty message that won't disrupt the protocol
        CTVC_LOG_WARNING("Message not supported in RFB-TV 1.3");
        return msg;
    }

    msg.write_uint8(RFBClientMessageType_HandoffResult);
    msg.write_uint8(code);
    msg.write_string(result == IHandoffHandler::HANDOFF_PLAYER_ERROR ? player_specific_error : ""); // This string is only relevant in case of a player error, see RFB-TV 2.0 spec

    return msg;
}

RfbtvMessage RfbtvProtocol::create_key_time_event(X11KeyCode key, KeyAction key_action, const std::string &timestamp)
{
    RfbtvMessage msg;

    if (m_protocol_version == RFBTV_PROTOCOL_V1_3) {
        // Not supported in RFB-TV 1.3, so if we ever want to send it, we'll return an empty message that won't disrupt the protocol
        CTVC_LOG_WARNING("Message not supported in RFB-TV 1.3");
        return msg;
    }

    msg.write_uint8(RFBClientMessageType_KeyTimeEvent);
    msg.write_uint8(key_action); // "event" in the protocol
    msg.write_uint32(key);
    msg.write_string(timestamp);

    return msg;
}

RfbtvMessage RfbtvProtocol::create_cdm_setup_response(const std::string &cdm_session_id, CdmSessionSetupResponseResult result, const std::map<std::string, std::string> &response_fields)
{
    RfbtvMessage msg;

    if (m_protocol_version == RFBTV_PROTOCOL_V1_3) {
        // Not supported in RFB-TV 1.3, so if we ever want to send it, we'll return an empty message that won't disrupt the protocol
        CTVC_LOG_WARNING("Message not supported in RFB-TV 1.3");
        return msg;
    }

    msg.write_uint8(RFBClientMessageType_CdmSetupResponse);
    msg.write_string(cdm_session_id);
    msg.write_uint8(result);
    msg.write_key_value_pairs(response_fields);

    return msg;
}

RfbtvMessage RfbtvProtocol::create_cdm_terminate_indication(const std::string &cdm_session_id, CdmSessionTerminateResponseReason reason, const std::map<std::string, std::string> &data)
{
    RfbtvMessage msg;

    if (m_protocol_version == RFBTV_PROTOCOL_V1_3) {
        // Not supported in RFB-TV 1.3, so if we ever want to send it, we'll return an empty message that won't disrupt the protocol
        CTVC_LOG_WARNING("Message not supported in RFB-TV 1.3");
        return msg;
    }

    msg.write_uint8(RFBClientMessageType_CdmTerminateIndication);
    msg.write_string(cdm_session_id);
    msg.write_uint8(reason);
    msg.write_key_value_pairs(data);

    return msg;
}

ResultCode RfbtvProtocol::parse_version_string(RfbtvMessage &message, const char *&client_version_string/*out*/)
{
    CTVC_LOG_DEBUG("");

    std::string server_version_string = message.read_raw_as_string(RFBTV_VERSION_SIZE);

    if (message.has_data_underflow()) {
        return NEED_MORE_DATA;
    }

    static struct
    {
        ProtocolVersion m_protocol_version;
        const char *m_protocol_version_string;
    }
    supported_versions[] =
    {
        // Versions must rank from high to low
        { RFBTV_PROTOCOL_V2_0, "RFB-TV 002.000\n" },
        { RFBTV_PROTOCOL_V1_3, "RFB-TV 001.001\n" }
    };

    // Find the highest version the server supports.
    // For the supported versions, this can be based on plain ASCII string comparison
    // (Resetting the protocol version and map is not strictly needed here as they should already be reset by now.)
    ProtocolVersion protocol_version = RFBTV_PROTOCOL_UNKNOWN;
    int vh, vl;
    if (sscanf(server_version_string.c_str(), "RFB-TV %03d.%03d\n", &vh, &vl) == 2) { // Must be some RFB-TV version
        for (unsigned int i = 0; i < sizeof(supported_versions) / sizeof(supported_versions[0]); i++) {
            if (server_version_string.compare(supported_versions[i].m_protocol_version_string) >= 0) {
                // If the server version is greater than or equal to our supported version, we can pick ours
                protocol_version = supported_versions[i].m_protocol_version;
                client_version_string = supported_versions[i].m_protocol_version_string;
                break;
            }
            // Otherwise try the next lower version until a match is found.
        }
    }

    set_version(protocol_version);

    if (protocol_version == RFBTV_PROTOCOL_UNKNOWN) {
        CTVC_LOG_ERROR("Cannot find a matching server version:%s", server_version_string.c_str());
        return INVALID_SERVER_VERSION;
    }

    CTVC_LOG_DEBUG("RX Server Version %.14s", server_version_string.c_str());
    CTVC_LOG_DEBUG("TX Client Version %.14s", client_version_string);

    return ResultCode::SUCCESS;
}

ResultCode RfbtvProtocol::parse_message(RfbtvMessage &message)
{
    CTVC_LOG_DEBUG("");

    uint8_t message_type = message.read_uint8();

    // Early return in case of underflow
    if (message.has_data_underflow()) {
        return NEED_MORE_DATA;
    }

    CTVC_LOG_DEBUG("Received message type %d", message_type);

    std::map<uint8_t, MessageHandler>::iterator i = m_message_map.find(message_type);
    if (i == m_message_map.end()) {
        CTVC_LOG_ERROR("Stream parse error, unknown message type %d", message_type);
        return PARSING_MESSAGE;
    }

    return (this->*i->second)(message);
}

ResultCode RfbtvProtocol::rect_read(RfbtvMessage &rx_message, PictureParameters &rect)
{
    rect.x = rx_message.read_uint16();
    rect.y = rx_message.read_uint16();
    rect.w = rx_message.read_uint16();
    rect.h = rx_message.read_uint16();
    int32_t encoding_type = rx_message.read_uint32();

    // Early return in case of underflow
    if (rx_message.has_data_underflow()) {
        return NEED_MORE_DATA;
    }

    switch (encoding_type) {
    case RFB_ENCODING_PICTURE_OBJECT:
        rect.alpha = rx_message.read_uint8();
        rect.m_data = rx_message.read_blob();
        CTVC_LOG_DEBUG("Read data for picture object encoded rectangle at (%d, %d) %d x %d", rect.x, rect.y, rect.w, rect.h);
        break;

    case RFB_ENCODING_URL: {
        rect.alpha = rx_message.read_uint8();
        rect.m_url = rx_message.read_string();
        CTVC_LOG_DEBUG("Read data for URL encoded rectangle at (%d, %d) %d x %d", rect.x, rect.y, rect.w, rect.h);
        break;
    }

    default:
        CTVC_LOG_ERROR("Framebuffer has unexpected encoding type %d", encoding_type);
        // cannot continue since we don't know how many bytes to read for this encoding
        // this would be a server side error, "should never happen in real life"
        CTVC_LOG_ERROR("Bailing out!");
        return PARSING_MESSAGE;
    }

    // Early return in case of underflow
    if (rx_message.has_data_underflow()) {
        return NEED_MORE_DATA;
    }

    return ResultCode::SUCCESS;
}

ResultCode RfbtvProtocol::parse_frame_buffer_update(RfbtvMessage &rx_message)
{
    const int RFB_RECT_FLIP_BIT = 0x1; // Called 'commit' in RFB-TV 2.0
    const int RFB_RECT_CLEAR_BIT = 0x2;

    CTVC_LOG_DEBUG("");

    // Bitmap containing possible flip and/or clear bit. Order on receive is:
    //   - First check clear bit, if set, clear the display;
    //   - Render received rectangle(s) to shadow copy of display;
    //   - Check flip bit, if set, flip shadow copy to become visible on display;
    uint8_t bitmap = rx_message.read_uint8();
    uint16_t nr_of_rects = rx_message.read_uint16();

    // Early return in case of underflow
    if (rx_message.has_data_underflow()) {
        return NEED_MORE_DATA;
    }

    std::vector<PictureParameters> rectangles(nr_of_rects);

    // First try to read all rectangle data, which may be a lot and even incomplete in this call.
    for (int i = 0; i < nr_of_rects; i++) {
        ResultCode ret = rect_read(rx_message, rectangles[i]);
        if (ret.is_error()) {
            return ret;
        }
    }

    return m_callbacks.frame_buffer_update(rectangles, (bitmap & RFB_RECT_CLEAR_BIT) != 0, (bitmap & RFB_RECT_FLIP_BIT) != 0);
}

ResultCode RfbtvProtocol::parse_stream_setup_request(RfbtvMessage &rx_message)
{
    CTVC_LOG_DEBUG("");

    std::string uri;
    std::map<std::string, std::string> stream_params;
    if (m_protocol_version == RfbtvProtocol::RFBTV_PROTOCOL_V2_0) {
        uri = rx_message.read_string();
        stream_params = rx_message.read_key_value_pairs();
    } else {
        // We map the parameters to known keys in RFB-TV 2.0
        string_printf(stream_params["video_width"], "%u", rx_message.read_uint16());
        string_printf(stream_params["video_height"], "%u", rx_message.read_uint16());

        // Audio codec
        switch (rx_message.read_uint8()) {
        case 0:
            stream_params["audio_codec"] = "mpa";
            break;
        case 1:
            stream_params["audio_codec"] = "aac";
            break;
        case 2:
            stream_params["audio_codec"] = "ac3";
            break;
        default:
            break;
        }

        // Video codec
        switch (rx_message.read_uint8()) {
        case 0:
            stream_params["video_codec"] = "avc";
            break;
        case 1:
            stream_params["video_codec"] = "mpeg2";
            break;
        default:
            break;
        }

        uri = rx_message.read_string();
    }

    // Early return in case of underflow
    if (rx_message.has_data_underflow()) {
        return NEED_MORE_DATA;
    }

    return m_callbacks.stream_setup_request(uri, stream_params);
}

ResultCode RfbtvProtocol::parse_session_setup_response(RfbtvMessage &rx_message)
{
	CLOUDTV_LOG_DEBUG("parse_session_setup_response.\n");
    CTVC_LOG_DEBUG("");

    // Server informs us of session setup result
    uint8_t result = rx_message.read_uint8();

    // Check if the session id is stored as a int32 or string
    std::string session_id;
    if (m_protocol_version == RfbtvProtocol::RFBTV_PROTOCOL_V2_0) {
        session_id = rx_message.read_string();
    } else {
        uint32_t id = rx_message.read_uint32();

        // Session id is being stored as a string
        string_printf(session_id, "%u", id);
    }

    std::string redirect_url = rx_message.read_string();
    std::string cookie = rx_message.read_string();

    // Early return in case of underflow
    if (rx_message.has_data_underflow()) {
        return NEED_MORE_DATA;
    }

    CTVC_LOG_DEBUG("result:%d, session_id:%s, redirect_url:%s, cookie:%s", result, session_id.c_str(), redirect_url.c_str(), cookie.c_str());

    static const std::map<int, ICallbacks::SessionSetupResult> s_session_setup_result_map = create_map<int, ICallbacks::SessionSetupResult>
        (0, ICallbacks::SESSION_SETUP_OK)
        (1, ICallbacks::SESSION_SETUP_REDIRECT)
        (2, ICallbacks::SESSION_SETUP_INVALID_CLIENT_ID)
        (3, ICallbacks::SESSION_SETUP_APP_NOT_FOUND)
        (4, ICallbacks::SESSION_SETUP_CONFIG_ERROR)
        (5, ICallbacks::SESSION_SETUP_NO_RESOURCES)
        (6, ICallbacks::SESSION_SETUP_UNSPECIFIED_ERROR)
        (7, ICallbacks::SESSION_SETUP_APP_NOT_FOUND)
        (8, ICallbacks::SESSION_SETUP_INVALID_PARAMETERS)
        (9, ICallbacks::SESSION_SETUP_INTERNAL_SERVER_ERROR)
        (255, ICallbacks::SESSION_SETUP_UNSPECIFIED_ERROR); // RFB-TV 2.0 only

    ICallbacks::SessionSetupResult code = ICallbacks::SESSION_SETUP_UNDEFINED_ERROR;
    std::map<int, ICallbacks::SessionSetupResult>::const_iterator i = s_session_setup_result_map.find(result);
    if (i != s_session_setup_result_map.end()) {
        code = i->second;
    }

    return m_callbacks.session_setup_response(code, session_id, redirect_url, cookie);
}

ResultCode RfbtvProtocol::parse_session_terminate_request(RfbtvMessage &rx_message)
{
    uint8_t reason = rx_message.read_uint8();

    // Early return in case of underflow
    if (rx_message.has_data_underflow()) {
        return NEED_MORE_DATA;
    }

    CTVC_LOG_DEBUG("reason:%d", reason);

    static const std::map<int, ICallbacks::SessionTerminateReason> s_session_terminate_result_map = create_map<int, ICallbacks::SessionTerminateReason>
        (0, ICallbacks::SESSION_TERMINATE_USER_STOP)
        (10, ICallbacks::SESSION_TERMINATE_INSUFFICIENT_BANDWIDTH)
        (11, ICallbacks::SESSION_TERMINATE_LATENCY_TOO_LARGE)
        (12, ICallbacks::SESSION_TERMINATE_SUSPEND)
        (13, ICallbacks::SESSION_TERMINATE_UNSPECIFIED_ERROR)
        (14, ICallbacks::SESSION_TERMINATE_DO_NOT_RETUNE)
        (15, ICallbacks::SESSION_TERMINATE_PING_TIMEOUT)
        (16, ICallbacks::SESSION_TERMINATE_INTERNAL_SERVERERROR)
        (17, ICallbacks::SESSION_TERMINATE_SERVER_SHUTTINGDOWN)
        (18, ICallbacks::SESSION_TERMINATE_FAILED_APPLICATION_STREAM_SETUP)
        (255, ICallbacks::SESSION_TERMINATE_UNSPECIFIED_ERROR); // RFB-TV 2.0 only

    ICallbacks::SessionTerminateReason code = ICallbacks::SESSION_TERMINATE_UNDEFINED_ERROR;
    std::map<int, ICallbacks::SessionTerminateReason>::const_iterator i = s_session_terminate_result_map.find(reason);
    if (i != s_session_terminate_result_map.end()) {
        code = i->second;
    }

    return m_callbacks.session_terminate_request(code);
}

ResultCode RfbtvProtocol::parse_ping(RfbtvMessage &/*message*/)
{
    CTVC_LOG_DEBUG("");

    // Ping is 1 byte message type, message type is already read, nothing to do

    return m_callbacks.ping();
}

ResultCode RfbtvProtocol::parse_server_command(RfbtvMessage &rx_message)
{
    CTVC_LOG_DEBUG("");

    // Read command and key-value list
    std::string command = rx_message.read_string();
    std::map<std::string, std::string> key_value_pairs = rx_message.read_key_value_pairs();

    // Early return in case of underflow
    if (rx_message.has_data_underflow()) {
        return NEED_MORE_DATA;
    }

    // Handle the command if possible. If we can't handle the command that's no fatal error.
    if (command == "keyfilter_control") {
        std::string local_keys = get_map_value(key_value_pairs, "localkeys");
        std::string remote_keys = get_map_value(key_value_pairs, "remotekeys");

        return m_callbacks.server_command_keyfilter_control(local_keys, remote_keys);
    } else if (command == "playback_control") {
        std::string report_mode = get_map_value(key_value_pairs, "report_mode");
        std::string interval = get_map_value(key_value_pairs, "interval");

        uint32_t interval_in_ms = 0; // Disabled/not present by default
        if (!interval.empty()) {
            interval_in_ms = atoi(interval.c_str());
        }

        ICallbacks::ReportMode mode = ICallbacks::REPORT_NOCHANGE;
        if (report_mode == "oneshot") {
            mode = ICallbacks::REPORT_ONE_SHOT;
        } else if (report_mode == "automatic") {
            mode = ICallbacks::REPORT_AUTOMATIC;
        } else if (report_mode == "disabled") {
            mode = ICallbacks::REPORT_DISABLED;
        } else if (!report_mode.empty()) {
            CTVC_LOG_WARNING("Unknown report_mode in server command %s:%s", command.c_str(), report_mode.c_str());
        }

        return m_callbacks.server_command_playback_control(mode, interval_in_ms);
    } else if (command == "latency_control") {
        std::string report_mode = get_map_value(key_value_pairs, "report_mode");
        std::string measurement_mode = get_map_value(key_value_pairs, "measurement_mode");

        ICallbacks::ReportMode mode = ICallbacks::REPORT_NOCHANGE;
        if (report_mode == "oneshot") {
            mode = ICallbacks::REPORT_ONE_SHOT;
        } else if (report_mode == "automatic") {
            mode = ICallbacks::REPORT_AUTOMATIC;
        } else if (report_mode == "disabled") {
            mode = ICallbacks::REPORT_DISABLED;
        } else if (!report_mode.empty()) {
            CTVC_LOG_WARNING("Unknown report_mode in server command %s:%s", command.c_str(), report_mode.c_str());
        }

        // Process measurement_mode "duration", "event". Should be comma-separated list,
        // but simply finding the keywords should do fine.
        bool is_duration = measurement_mode.find("duration") != std::string::npos;
        bool is_event = measurement_mode.find("event") != std::string::npos;

        if (measurement_mode.empty()) {
            // Disable reporting if empty, see RFB-TV spec
            mode = ICallbacks::REPORT_DISABLED;
        }

        return m_callbacks.server_command_latency_control(mode, is_duration, is_event);
    } else if (command == "log_control") {
        std::string report_mode = get_map_value(key_value_pairs, "report_mode");
        std::string log_level = get_map_value(key_value_pairs, "log_level");
        // std::string scope = get_map_value(key_value_pairs, "scope"); // Scope field is ignored currently

        LogMessageType min_log_level = static_cast<LogMessageType>(-1); // No change
        if (log_level == "error") {
            min_log_level = LOG_ERROR;
        } else if (log_level == "warning") {
            min_log_level = LOG_WARNING;
        } else if (log_level == "info") {
            min_log_level = LOG_INFO;
        } else if (log_level == "debug") {
            min_log_level = LOG_DEBUG;
        } else if (!log_level.empty()) { // All others
            min_log_level = LOG_DEBUG;
        }

        ICallbacks::ReportMode mode = ICallbacks::REPORT_NOCHANGE;
        if (report_mode == "oneshot") {
            mode = ICallbacks::REPORT_ONE_SHOT;
        } else if (report_mode == "accumulate") {
            mode = ICallbacks::REPORT_ACCUMULATE;
        } else if (report_mode == "automatic") {
            mode = ICallbacks::REPORT_AUTOMATIC;
        } else if (report_mode == "disabled") {
            mode = ICallbacks::REPORT_DISABLED;
        } else if (!report_mode.empty()) {
            CTVC_LOG_WARNING("Unknown report_mode in server command %s:%s", command.c_str(), report_mode.c_str());
        }

        return m_callbacks.server_command_log_control(mode, min_log_level);
    } else if (command == "video_control") {
        std::string mode = get_map_value(key_value_pairs, "mode");
        ICallbacks::VideoMode video_mode = ICallbacks::MODE_NOCHANGE;
        if (mode == "gui-optimized") {
            video_mode = ICallbacks::MODE_GUI_OPTIMIZED;
        } else if (mode == "motion-optimized") {
            video_mode = ICallbacks::MODE_VIDEO_OPTIMIZED;
        } else if (!mode.empty()) {
            CTVC_LOG_WARNING("mode not recognized:%s", mode.c_str());
        }

        return m_callbacks.server_command_video_control(video_mode);
    } else if (command == "underrun_mitigation_control") {
        return m_callbacks.server_command_underrun_mitigation_control(key_value_pairs);
    }

    CTVC_LOG_WARNING("Unrecognized server command:%s", command.c_str());

    return ResultCode::SUCCESS;
}

ResultCode RfbtvProtocol::parse_handoff_request(RfbtvMessage &rx_message)
{
    CTVC_LOG_DEBUG("");

    uint8_t suspend = rx_message.read_uint8();
    std::string handoff_uri = rx_message.read_string();

    // Early return in case of underflow
    if (rx_message.has_data_underflow()) {
        return NEED_MORE_DATA;
    }

    return m_callbacks.handoff_request(handoff_uri, suspend != 0/*see spec*/);
}

ResultCode RfbtvProtocol::parse_passthrough(RfbtvMessage &rx_message)
{
    CTVC_LOG_DEBUG("");

    std::string protocol_id = rx_message.read_string();
    std::vector<uint8_t> protocol_data = rx_message.read_blob();

    // Early return in case of underflow
    if (rx_message.has_data_underflow()) {
        return NEED_MORE_DATA;
    }

    return m_callbacks.passthrough(protocol_id, protocol_data);
}

ResultCode RfbtvProtocol::parse_cdm_setup_request(RfbtvMessage &rx_message)
{
    CTVC_LOG_DEBUG("");

    std::string cdm_session_id = rx_message.read_string();
    std::string drm_type = rx_message.read_string();
    std::string session_type = rx_message.read_string();
    std::map<std::string, std::string> init_data = rx_message.read_key_value_pairs();

    // Early return in case of underflow
    if (rx_message.has_data_underflow()) {
        return NEED_MORE_DATA;
    }

    uint8_t drm_system_id[16];
    parse_guid_formatted_string(drm_type, drm_system_id);

    return m_callbacks.cdm_setup_request(cdm_session_id, drm_system_id, session_type, init_data);
}

ResultCode RfbtvProtocol::parse_cdm_terminate_request(RfbtvMessage &rx_message)
{
    CTVC_LOG_DEBUG("");

    std::string cdm_session_id = rx_message.read_string();
    uint8_t reason = rx_message.read_uint8();

    // Early return in case of underflow
    if (rx_message.has_data_underflow()) {
        return NEED_MORE_DATA;
    }

    return m_callbacks.cdm_terminate_request(cdm_session_id, reason == 0 ? ICallbacks::CDM_SESSION_TERMINATE_REASON_USER_STOP : ICallbacks::CDM_SESSION_TERMINATE_REASON_OTHER);
}
