/// \file Streamer.cpp
/// \brief Implementation of the stream library.
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include <stream/Streamer.h>
#include <stream/IMediaPlayer.h>
#include <stream/IStreamDecrypt.h>

#include "RamsChunkAllocator.h"
#include "DefaultMediaChunkAllocator.h"

#include <rplayer/RPlayer.h>
#include <rplayer/IStreamDecrypt.h>
#include <rplayer/IPacketSink.h>
#include <rplayer/ts/IEventSink.h>
#include <rplayer/ts/TimeStamp.h>
#include <rplayer/ILog.h>
#include <utils/utils.h>
#include <porting_layer/Log.h>
#include <porting_layer/AutoLock.h>
#include <porting_layer/Thread.h>
#include <porting_layer/TimeStamp.h>

#include <assert.h>

using namespace ctvc;

const ResultCode Streamer::INVALID_PARAMETER("One or more of the parameters are invalid");
const ResultCode Streamer::PROTOCOL_NOT_REGISTERED("The protocol indicated in the URI was not registered");
const ResultCode Streamer::CANNOT_CREATE_MEDIA_PLAYER("Cannot create a media player for the registered scheme");
const ResultCode Streamer::CANNOT_DECODE_STREAM("Cannot decode a stream with given parameters");

static const uint32_t STREAM_TIMEOUT_IN_MS = 5000;

// Helper class that forwards IPacketSink-received packets to Streamer::stream_data_from_rplayer()
class Streamer::PacketReceptacle : public rplayer::IPacketSinkWithMetaData
{
public:
    PacketReceptacle(Streamer &streamer) :
        m_streamer(streamer)
    {
    }

    void put(const uint8_t *data, uint32_t size)
    {
        m_streamer.stream_data_from_rplayer(data, size);
    }

    void setMetaData(const rplayer::StreamMetaData &)
    {
        // Ignore as of now
    }

private:
    Streamer &m_streamer;
};

// Helper class to forward decryption requests from RPlayer to the Session and return decrypted data back to RPlayer
class Streamer::StreamDecryptForwarder : public rplayer::IStreamDecrypt
{
public:
    StreamDecryptForwarder(ctvc::IStreamDecrypt &stream_decrypt_engine, Mutex &mutex) :
        m_stream_decrypt_engine(stream_decrypt_engine),
        m_return_path(mutex)
    {
        m_stream_decrypt_engine.set_stream_return_path(&m_return_path);
    }

    ~StreamDecryptForwarder()
    {
        m_stream_decrypt_engine.set_stream_return_path(0);
    }

    virtual void setStreamReturnPath(rplayer::IPacketSink *stream_out)
    {
        m_return_path.set_stream_return_path(stream_out);
    }

    virtual void setKeyIdentifier(const uint8_t (&key_id)[16])
    {
        m_stream_decrypt_engine.set_key_identifier(key_id);
    }

    virtual void setInitializationVector(const uint8_t (&iv)[16])
    {
        m_stream_decrypt_engine.set_initialization_vector(iv);
    }

    virtual bool streamData(const uint8_t *data, uint32_t length)
    {
        m_stream_data_called_time = TimeStamp::now();
        return m_stream_decrypt_engine.stream_data(data, length);
    }

    void trigger()
    {
        static const TimeStamp TIMEOUT = TimeStamp::zero().add_milliseconds(20);
        TimeStamp now = TimeStamp::now();
        if (!m_stream_data_called_time.is_valid() || now > (m_stream_data_called_time + TIMEOUT)) {
            m_stream_data_called_time = now;
            m_stream_decrypt_engine.stream_data(0, 0);
        }
    }

private:
    class ReturnPath : public IStream
    {
    public:
        ReturnPath(Mutex &mutex) :
            m_stream_out(0),
            m_mutex(mutex)
        {
        }

        void stream_data(const uint8_t *data, uint32_t length)
        {
            AutoLock auto_lock(m_mutex);
            if (m_stream_out) {
                m_stream_out->put(data, length);
            }
        }

        void stream_error(ResultCode /*result*/)
        {
        }

        void set_stream_return_path(rplayer::IPacketSink *stream_out)
        {
            AutoLock auto_lock(m_mutex);
            m_stream_out = stream_out;
        }

    private:
        rplayer::IPacketSink *m_stream_out;
        Mutex &m_mutex;
    };

    ctvc::IStreamDecrypt &m_stream_decrypt_engine;
    ReturnPath m_return_path;
    TimeStamp m_stream_data_called_time;
};

// Helper class to catch private data events from RPlayer and forward them as latency events to the SessionImpl.
class Streamer::LatencyEventSink : public rplayer::IEventSink
{
public:
    LatencyEventSink() :
        m_latency_data_callback(0),
        m_last_pcr_90khz(0)
    {
    }

    void register_callback(ILatencyData *latency_data_callback)
    {
        m_latency_data_callback = latency_data_callback;
    }

    // Implements rplayer::IEventSink
    void privateStreamData(rplayer::IEventSink::PrivateDataType data_type, rplayer::TimeStamp pts, uint64_t data)
    {
        if (!m_latency_data_callback) {
            return;
        }

        TimeStamp adjusted_pts(m_time_of_last_pcr_reception);
        int64_t diff = pts.getAs90kHzTicks() - m_last_pcr_90khz;
        diff = (diff << 31) >> 31; // Sign extend from 33 bits
        if (diff > 0) {
            adjusted_pts.add_milliseconds(diff / 90);
        }

        switch (data_type) {
        case rplayer::IEventSink::KEY_PRESS:
            m_latency_data_callback->latency_stream_data(ILatencyData::KEY_PRESS, adjusted_pts, TimeStamp::zero().add_milliseconds(data) /* FIXME: Should be marked as absolute time, actually */);
            break;
        case rplayer::IEventSink::FIRST_PAINT:
            m_latency_data_callback->latency_stream_data(ILatencyData::FIRST_PAINT, adjusted_pts, TimeStamp::zero());
            break;
        case rplayer::IEventSink::APP_COMPLETE:
            m_latency_data_callback->latency_stream_data(ILatencyData::APP_COMPLETE, adjusted_pts, TimeStamp::zero());
            break;
        }
    }

    void pcrReceived(uint64_t pcr90kHz, int /*pcrExt27MHz*/, bool /*hasDiscontinuity*/)
    {
        m_time_of_last_pcr_reception = TimeStamp::now();
        m_last_pcr_90khz = pcr90kHz;
    }

    void tableVersionUpdate(int /*tableId*/, int /*version*/)
    {
    }

private:
    ILatencyData *m_latency_data_callback;
    uint64_t m_last_pcr_90khz;
    TimeStamp m_time_of_last_pcr_reception;
};

// Helper class to catch stall events from RPlayer and forward them as stall events to the SessionImpl.
class Streamer::StallEventSink : public rplayer::RPlayer::ICallback
{
public:
    StallEventSink() :
        m_stall_event_callback(0)
    {
    }

    void register_callback(IStallEvent *stall_event_callback)
    {
        m_stall_event_callback = stall_event_callback;
    }

    // Implements rplayer::RPlayer::ICallback
    void stallDetected(const std::string &id, bool is_audio_not_video, const rplayer::TimeStamp &stallDuration)
    {
        if (!m_stall_event_callback) {
            return;
        }

        m_stall_event_callback->stall_detected(id, is_audio_not_video, TimeStamp::zero().add_milliseconds(stallDuration.getAsMilliseconds()));
    }

private:
    IStallEvent *m_stall_event_callback;
};

// Helper class that forwards the RPlayer logging to our log output
class RPlayerLogger : public rplayer::ILog
{
public:
    RPlayerLogger()
    {
        rplayer::registerLogger(*this);
    }
    ~RPlayerLogger()
    {
        rplayer::unregisterLogger();
    }

    virtual void log_message(ILog::LogMessageType message_type, const char *file, int line, const char *function, const char *message)
    {
        ctvc::LogMessageType type;
        switch (message_type) {
        case ILog::LOG_DEBUG:
            type = ctvc::LOG_DEBUG;
            break;
        case ILog::LOG_INFO:
            type = ctvc::LOG_INFO;
            break;
        case ILog::LOG_WARNING:
            type = ctvc::LOG_WARNING;
            break;
        case ILog::LOG_ERROR:
        default:
            type = ctvc::LOG_ERROR;
            break;
        }

        ctvc::log_message(type, file, line, function, "%s", message);
    }
};

static RPlayerLogger s_rplayer_logger;

Streamer::Streamer() :
    m_rplayer_latency_event_sink(*new LatencyEventSink()),
    m_rplayer_stall_event_sink(*new StallEventSink()),
    m_rplayer(*new rplayer::RPlayer()),
    m_packet_receptacle(new PacketReceptacle(*this)),
    m_current_media_player(0),
    m_current_media_player_factory(0),
    m_current_stream_player(0),
    m_stream_decrypt_forwarder(0),
    m_rams_chunk_allocator(new RamsChunkAllocator),
    m_media_player_callback(0),
    m_stream_timout_mark_time_in_ms(0),
    m_was_stream_data_sent(false)
{
    m_rplayer.setEnabledFeatures(rplayer::RPlayer::FEATURE_RAMS_DECODER);
    m_rplayer.setTsPacketOutput(m_packet_receptacle);
    m_rplayer.registerOutputEventSink(&m_rplayer_latency_event_sink);
    m_rplayer.registerCallback(&m_rplayer_stall_event_sink);

    // Register default media chunk allocator
    static DefaultMediaChunkAllocator default_media_chunk_allocator;
    register_media_chunk_allocator(&default_media_chunk_allocator);
}

Streamer::~Streamer()
{
    stop_stream();

    m_rplayer.registerStreamDecryptEngine(0);
    m_rplayer.registerRamsChunkAllocator(0);
    m_rplayer.registerOutputEventSink(0);
    m_rplayer.setTsPacketOutput(0);
    m_rplayer.registerCallback(0);

    delete m_rams_chunk_allocator;
    delete m_stream_decrypt_forwarder;
    delete &m_rplayer;
    delete &m_rplayer_latency_event_sink;
    delete &m_rplayer_stall_event_sink;
    delete m_packet_receptacle;
}

void Streamer::reinitialize()
{
    stop_stream();

    AutoLock auto_lock(m_mutex);

    m_rplayer.reinitialize();
    m_rplayer.setEnabledFeatures(rplayer::RPlayer::FEATURE_RAMS_DECODER);
}

void Streamer::set_rplayer_parameter(const std::string &parameter, const std::string &value)
{
    AutoLock auto_lock(m_mutex);

    m_rplayer.setParameter(parameter, value);
}

void Streamer::get_rplayer_status(uint64_t &current_stream_time, uint32_t &stalled_duration_in_ms, uint32_t &pcr_delay)
{
    AutoLock auto_lock(m_mutex);

    m_rplayer.getStatus(current_stream_time, stalled_duration_in_ms, pcr_delay);
}

ResultCode Streamer::start_stream(const std::string &uri, const std::map<std::string, std::string> &stream_params)
{
    stop_stream();

    // Server requesting to start a new stream
    std::string protocol;
    std::string dummy;
    int port;

    url_split(uri, protocol, dummy, dummy, port, dummy);

    if (protocol.size() == 0) {
        CTVC_LOG_ERROR("Unable to determine protocol for uri '%s'", uri.c_str());
        return INVALID_PARAMETER;
    }

    AutoLock auto_lock(m_mutex);

    // Make sure the stream processing is properly set-up to receive a new stream
    m_rplayer.reset();

    assert(!m_current_media_player_factory);
    std::map<std::string, IMediaPlayerFactory *>::iterator it = m_media_player_factories.find(protocol);
    if (it == m_media_player_factories.end()) {
        CTVC_LOG_ERROR("Unable to get content source for protocol '%s' (uri:%s)", protocol.c_str(), uri.c_str());
        return PROTOCOL_NOT_REGISTERED;
    }

    m_current_media_player_factory = it->second;

    assert(!m_current_media_player);
    m_current_media_player = m_current_media_player_factory->create();
    if (!m_current_media_player) {
        CTVC_LOG_ERROR("Unable to create content source for protocol '%s' (uri:%s)", protocol.c_str(), uri.c_str());
        stop_stream();
        return CANNOT_CREATE_MEDIA_PLAYER;
    }

    m_current_media_player->register_callback(this);

    assert(!m_current_stream_player);
    ResultCode ret = m_current_media_player->open_stream(uri, stream_params, *this, m_current_stream_player);
    if (ret.is_error()) {
        CTVC_LOG_ERROR("Unable to open stream:%s", ret.get_description());
        stop_stream();
        return ret;
    }

    // Reset the last time data was received so we don't immediately get a timeout.
    m_stream_timout_mark_time_in_ms = TimeStamp::now().get_as_milliseconds();

    return ret;
}

void Streamer::stop_stream()
{
    IMediaPlayer *current_media_player = 0;
    IMediaPlayerFactory *current_media_player_factory = 0;

    {
        AutoLock auto_lock(m_mutex);

        current_media_player = m_current_media_player;
        current_media_player_factory = m_current_media_player_factory;
        m_current_media_player = 0;
        m_current_media_player_factory = 0;
        m_current_stream_player = 0;
        m_was_stream_data_sent = false;
    }

    if (current_media_player) {
        assert(current_media_player_factory);
        CTVC_LOG_INFO("Closing currently loading stream");
        current_media_player->close_stream(); // Needs to be out of the scoped lock
        current_media_player->register_callback(0);
        current_media_player_factory->destroy(current_media_player);
    }
}

void Streamer::stream_data(const uint8_t *data, uint32_t size)
{
    AutoLock auto_lock(m_mutex);

    uint64_t now_in_ms = TimeStamp::now().get_as_milliseconds();

    // Sample the last time data was received (in order to detect timeouts)
    m_stream_timout_mark_time_in_ms = now_in_ms;

    // Update the rplayer time with the current time (so any synchronization works properly)
    // TODO: Replacing this by a callback from rplayer to get the time only when needed will reduce overhead albeit more complex, though.
    // Need to call this in real time as well as just before parsing RAMS packets
    m_rplayer.setCurrentTime(now_in_ms);

    // Pass ingress data on to the rplayer
    m_rplayer.parse(data, size);
}

void Streamer::stream_error(ResultCode result)
{
    AutoLock auto_lock(m_mutex);

    // Bypass the rplayer and immediately forward ingress errors
    if (m_current_stream_player) {
        m_current_stream_player->stream_error(result);
    }
}

void Streamer::stream_data_from_rplayer(const uint8_t *data, uint32_t size)
{
    AutoLock auto_lock(m_mutex);

    // Forward data returned from rplayer
    if (m_current_stream_player) {
        m_current_stream_player->stream_data(data, size);
        m_was_stream_data_sent = true;
    }
}

void Streamer::trigger()
{
    AutoLock auto_lock(m_mutex);

    uint64_t now_in_ms = TimeStamp::now().get_as_milliseconds();

    // Need to call this in real time as well as just before parsing RAMS packets
    m_rplayer.setCurrentTime(now_in_ms);

    if (m_stream_decrypt_forwarder) {
        m_stream_decrypt_forwarder->trigger();
    }

    // Flush any stream player in case it's buffering
    // This is done by giving it 0 bytes
    if (m_was_stream_data_sent && m_current_stream_player) {
        m_current_stream_player->stream_data(0, 0);
        m_was_stream_data_sent = false;
    }

    // Check the last time data was received
    // We only should do that when streaming, i.e. when m_current_stream_player is valid.
    // If longer than STREAM_TIMEOUT_IN_MS, a time-out occurs and we need to signal stream absence.
    if (m_current_stream_player && static_cast<uint32_t>(now_in_ms - m_stream_timout_mark_time_in_ms) > STREAM_TIMEOUT_IN_MS) {
        CTVC_LOG_WARNING("Stream timeout occurs");
        // Make sure we'll send only one event in the next few seconds.
        m_stream_timout_mark_time_in_ms = now_in_ms;
        // Send an unrecoverable error event; this will lead to the same stream confirm error message
        // as would be sent in case a stream timeout would have been determined by Session::Impl itself.
        player_event(IMediaPlayer::PLAYER_UNRECOVERABLE_ERROR);
    }
}

void Streamer::player_event(IMediaPlayer::PlayerEvent event)
{
    AutoLock auto_lock(m_player_event_mutex);

    // We only forward events if allowed.
    if (m_media_player_callback) {
        m_media_player_callback->player_event(event);
    }
}

bool Streamer::register_media_player(const std::string &protocol, IMediaPlayerFactory &factory)
{
    if (protocol.length() == 0) {
        CTVC_LOG_ERROR("Invalid protocol identifier");
        return false;
    }

    bool must_stop_stream = false;
    {
        AutoLock auto_lock(m_mutex);

        // If an existing registration exists that is different than the new factory and a stream
        // is currently active using the previously registered factory, then stop the stream.
        std::map<std::string, IMediaPlayerFactory *>::iterator it = m_media_player_factories.find(protocol);
        if (it != m_media_player_factories.end() && it->second != &factory && it->second == m_current_media_player_factory) {
            must_stop_stream = true;
        }
        m_media_player_factories[protocol] = &factory;
    }
    // If needed, stop the stream; this must be done while not having the lock, however.
    if (must_stop_stream) {
        stop_stream();
    }
    return true;
}

bool Streamer::unregister_media_player(const std::string &protocol)
{
    bool must_stop_stream = false;
    {
        AutoLock auto_lock(m_mutex);

        std::map<std::string, IMediaPlayerFactory *>::iterator it = m_media_player_factories.find(protocol);
        if (it == m_media_player_factories.end()) {
            return false;
        }
        // Stop any stream if it happens to be currently running and is using the registered factory.
        if (it->second == m_current_media_player_factory) {
            must_stop_stream = true;
        }
        m_media_player_factories.erase(it);
    }
    // If needed, stop the stream; this must be done while not having the lock, however.
    if (must_stop_stream) {
        stop_stream();
    }
    return true;
}

void Streamer::register_stream_decrypt_engine(IStreamDecrypt *stream_decrypt_engine)
{
    AutoLock auto_lock(m_mutex);

    m_rplayer.registerStreamDecryptEngine(0);
    delete m_stream_decrypt_forwarder;
    m_stream_decrypt_forwarder = 0;

    if (stream_decrypt_engine) {
        m_stream_decrypt_forwarder = new StreamDecryptForwarder(*stream_decrypt_engine, m_mutex);

        m_rplayer.registerStreamDecryptEngine(m_stream_decrypt_forwarder);
    }
}

void Streamer::register_latency_data_callback(ILatencyData *latency_data_callback)
{
    m_rplayer_latency_event_sink.register_callback(latency_data_callback);
}

void Streamer::register_stall_event_callback(IStallEvent *stall_event_callback)
{
    m_rplayer_stall_event_sink.register_callback(stall_event_callback);
}

void Streamer::register_media_chunk_allocator(IMediaChunkAllocator *media_chunk_allocator)
{
    AutoLock auto_lock(m_mutex);

    // Free up all memory used by any old allocator and register our allocator with rplayer
    m_rplayer.registerRamsChunkAllocator(m_rams_chunk_allocator);
    // Register the new user-supplied allocator with our allocator; this will be used for any new allocations
    m_rams_chunk_allocator->register_media_chunk_allocator(media_chunk_allocator);
}

void Streamer::register_media_player_callback(IMediaPlayer::ICallback *media_player_callback)
{
    AutoLock auto_lock(m_player_event_mutex);

    m_media_player_callback = media_player_callback;
}

void Streamer::get_player_info(IMediaPlayer::PlayerInfo &info)
{
    AutoLock auto_lock(m_mutex);

    if (m_current_media_player) {
        m_current_media_player->get_player_info(info);
    }
}
