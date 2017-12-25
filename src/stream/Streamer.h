/// \file Streamer.h
/// \brief The API of the CloudTV stream library
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include "IStream.h"
#include "IMediaPlayer.h"
#include "ILatencyData.h"
#include "IStallEvent.h"

#include <porting_layer/ResultCode.h>
#include <porting_layer/Mutex.h>
#include <porting_layer/Atomic.h>

#include <string>
#include <map>

namespace rplayer {
    class RPlayer;
}

namespace ctvc {

struct IStreamDecrypt;
struct IMediaChunkAllocator;
class RamsChunkAllocator;

class Streamer : public IStream, public IMediaPlayer::ICallback
{
public:
    static const ResultCode INVALID_PARAMETER; //!< One or more of the parameters are invalid.
    static const ResultCode PROTOCOL_NOT_REGISTERED; //!< The protocol indicated in the URI was not registered.
    static const ResultCode CANNOT_CREATE_MEDIA_PLAYER; //!< Cannot create a media player for the registered scheme.
    static const ResultCode CANNOT_DECODE_STREAM; //!< Cannot decode a stream with given parameters.

    Streamer();
    virtual ~Streamer();

    // Bring the Streamer back into its state similar to after construction
    // This will reset all RPlayer parameters and dynamic state but it won't
    // unregister any registered media player factories, stream decrypt engines or the like.
    void reinitialize();

    // Setting RPlayer parameters
    void set_rplayer_parameter(const std::string &parameter, const std::string &value);

    // Read RPlayer status information such as current stream time, stalled duration and PCR delay
    // If any of the values is unknown or cannot be obtained, they will remain unchanged
    void get_rplayer_status(uint64_t &current_stream_time, uint32_t &stalled_duration_in_ms, uint32_t &pcr_delay);

    // Registration of a media player
    bool register_media_player(const std::string &protocol, IMediaPlayerFactory &factory);
    bool unregister_media_player(const std::string &protocol);

    // Registration of a stream decryption engine
    void register_stream_decrypt_engine(IStreamDecrypt *stream_decrypt_engine);

    // Registration of a chunked media memory allocator
    void register_media_chunk_allocator(IMediaChunkAllocator *media_chunk_allocator);

    // Registration of Latency data callback
    void register_latency_data_callback(ILatencyData *latency_data_callback);

    // Registration of stall event callback
    void register_stall_event_callback(IStallEvent *stall_event_callback);

    // Registration of media player callback
    void register_media_player_callback(IMediaPlayer::ICallback *media_player_callback);

    // Forwards player info query to the currently active player.
    void get_player_info(IMediaPlayer::PlayerInfo &info);

    //!
    //! \brief Start receiving a stream.
    //! \param[in] uri The URI of the stream
    //! \param[in] stream_params Stream properties that may help the decoder verifying whether it can play the stream.
    //! \result \link ResultCode \endlink
    //!
    //! This function should be called to start receiving a new stream.
    //! stop_stream() should be called to stop it.
    //!
    ResultCode start_stream(const std::string &uri, const std::map<std::string, std::string> &stream_params);

    //!
    //! \brief Stops a stream.
    //!
    //! This function is called to stop a stream.
    //!
    void stop_stream();

    //!
    //! \brief Trigger evaluation of timed actions.
    //!
    //! This function should be called regularly, preferably every 10ms,
    //! so the underlying timed logic can be evaluated.
    //!
    void trigger();

private:
    class StreamDecryptForwarder;
    class PacketReceptacle;
    friend class PacketReceptacle; // Needed for Metrowerks
    class LatencyEventSink;
    class StallEventSink;

    Mutex m_mutex;
    Mutex m_player_event_mutex;

    LatencyEventSink &m_rplayer_latency_event_sink;
    StallEventSink &m_rplayer_stall_event_sink;
    rplayer::RPlayer &m_rplayer;
    PacketReceptacle *m_packet_receptacle;
    std::map<std::string, IMediaPlayerFactory *> m_media_player_factories;
    IMediaPlayer *m_current_media_player;
    IMediaPlayerFactory *m_current_media_player_factory;
    IStream *m_current_stream_player;
    StreamDecryptForwarder *m_stream_decrypt_forwarder;
    RamsChunkAllocator *m_rams_chunk_allocator;
    IMediaPlayer::ICallback *m_media_player_callback;
    uint64_t m_stream_timout_mark_time_in_ms; // Stream timeout is measured with this timestamp as base time (typically the time the last data was received).
    bool m_was_stream_data_sent;

    // Implements IStream
    virtual void stream_data(const uint8_t *data, uint32_t length);
    virtual void stream_error(ResultCode result);

    // Implements IMediaPlayer::ICallback
    void player_event(IMediaPlayer::PlayerEvent event);

    void stream_data_from_rplayer(const uint8_t *data, uint32_t length);
};

} // namespace
