///
/// \file IMediaPlayer.h
///
/// \brief CloudTV Nano SDK Stream loader interface.
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <porting_layer/ResultCode.h>

#include <inttypes.h>
#include <string>
#include <map>

namespace ctvc {

struct IStream;

/// \brief Player for content streams i.e. various sources of streaming video.
///
/// Subclass the IMediaPlayer to implement a media player that can resolve a media URI.
/// This is then bound to a particular type of content with Session::register_media_player().
/// A MediaPlayer is given URIs to content streams, and so must be able both to retrieve
/// the indicated resource and consume its content.
struct IMediaPlayer
{
public:
    // Specific errors that have a meaning in the context of IMediaPlayer
    // The following codes can typically be returned by open_stream(). Private error codes can be added.
    static const ResultCode CABLE_TUNING_ERROR; //!< There was a tuning error when trying to tune to a channel.
    static const ResultCode CONNECTION_FAILED; //!< Connection to a remote host could not be established.

    IMediaPlayer()
    {
    }
    virtual ~IMediaPlayer()
    {
    }

    /// \brief Called when streaming content should be opened (setup).
    /// If the media player will pass the stream through the SDK, the stream_in parameter
    /// should be filled in with the return path (i.e. the interface of the stream player).
    /// The stream_out parameter contains the SDK object the loaded stream can be sent to.
    /// If no routing is supported, stream_in must either be left unchanged or set to 0.
    /// \param[in] uri URI to open.
    /// \param[in] stream_params Stream parameters that the player can use to check if
    ///            playback is possible or not. In general, the stream parameters should
    ///            be regarded as a hint, the stream itself is always leading. However,
    ///            some applications require certain stream parameters to be processed
    ///            for proper operation. This is application-specific.
    ///            Valid parameters are documented in the RFB-TV specification (section
    ///            "Optional stream parameters"), e.g. "video_width", "audio_codec" or
    ///            "ca_data".
    ///            RFB-TV 1.3.2 streaming parameters are mapped to the keys/values
    ///            defined in RFB-TV 2.0.9.
    /// \param[in] stream_out The IStream object the loaded stream must be sent to.
    /// \param[out] stream_in The IStream object that will receive the processed stream.
    ///
    /// \result ResultCode
    virtual ResultCode open_stream(const std::string &uri, const std::map<std::string, std::string> &stream_params, IStream &stream_out, IStream *&stream_in/*out*/) = 0;

    /// \brief Called when the library wishes to stop the content.
    virtual void close_stream() = 0;

    /// \brief Player event definition
    enum PlayerEvent
    {
        PLAYER_STARTING,           ///< The player just started. This event should be sent as soon as the start() method of the player is called.
        PLAYER_STARTED,            ///< The player started. This event must be sent as soon as the first decoded frame is displayed (or as near as possible). Sent in response to a call to start().
        PLAYER_STOPPED,            ///< The player stopped. This event must be sent as soon as the last decoded frame was displayed (or as near as possible). Sent in response to a call to stop(), if started. May also be sent upon the call to register_callback().
        PLAYER_BUFFER_UNDERRUN,    ///< The player experienced a buffer underrun. Non-fatal. Once the underrun condition has stopped, the player should resume normal, minimal-latency decoding and the PLAYER_STARTED event MUST be sent.
        PLAYER_BUFFER_OVERRUN,     ///< The player experienced a buffer overrun. Fatal. The player can stop playing. It should expect a successive call to stop().
        PLAYER_RECOVERABLE_ERROR,  ///< The player experienced an error that is recoverable. Non-fatal. After recovery, the player should continue normal, minimal-latency decoding and the PLAYER_STARTED event MUST be sent.
        PLAYER_UNRECOVERABLE_ERROR,///< The player experienced an error that is unrecoverable. Fatal. The player can stop playing. It should expect a successive call to stop().
        PLAYER_DESCRAMBLE_ERROR,   ///< There was an error descrambling the stream. Fatal. The player can stop playing. It should expect a successive call to stop().
        PLAYER_DECODE_ERROR,       ///!< The client failed to decode the stream. Fatal.
        PLAYER_TRANSPORT_STREAM_ID_ERROR,///!< No transport stream with the indicated Transport Stream ID was found. Fatal.
        PLAYER_NETWORK_ID_ERROR,   ///!< No network with the indicated Network ID was found. Fatal.
        PLAYER_PROGRAM_ID_ERROR,   ///!< No program with the indicated Program ID was found. Fatal.
        PLAYER_PHYSICAL_ERROR      ///!< Unrecoverable error at the physical layer. Fatal.
    };

    /// \brief Callback interface for player status updates.
    struct ICallback
    {
        /// \brief Send a player event back to the stream originator.
        /// \param[in] event The event to pass.
        virtual void player_event(PlayerEvent event) = 0;
    };

    /// \brief Struct that contains player information (state and statistics)
    ///        to be passed back to the stream originator.
    struct PlayerInfo
    {
        uint64_t current_pts; ///< PTS value of the frame that is currently displayed on the screen, if available. This should be the PTS as present in the stream, in 90kHz ticks.
        // Might add frames_displayed, frames_skipped or other info in the future...
    };

    /// \brief Obtain player state and statistics.
    /// \param[in] info Player state and statistics information to be returned.
    ///                 Fields that can be filled should be set by the player
    ///                 implementation; fields for which information cannot be
    ///                 obtained should be left alone. This way, the caller
    ///                 knows which fields have been set and which not.
    ///
    /// \note This call always succeeds.
    virtual void get_player_info(PlayerInfo &info) = 0;

    /// \brief Register a callback interface
    /// \param[in] callback This object is to be used to signal playback events to.
    ///            Passing a null pointer will unregister the callback.
    virtual void register_callback(ICallback *callback) = 0;
};

/// \brief Class to create a specific media player
struct IMediaPlayerFactory
{
    IMediaPlayerFactory()
    {
    }
    virtual ~IMediaPlayerFactory()
    {
    }

    /// \brief Create a new instance of a media player object.
    /// \return Pointer to media player.
    /// \note Deletion of the returned object will be done by calling destroy().
    virtual IMediaPlayer *create() = 0;

    /// \brief Destroy a previously created instance of a media player object.
    /// \param[in] p Pointer to media player.
    virtual void destroy(IMediaPlayer *p) = 0;
};

} // namespace
