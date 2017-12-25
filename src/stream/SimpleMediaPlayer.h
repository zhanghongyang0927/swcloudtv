///
/// \file SimpleMediaPlayer.h
///
/// \brief CloudTV Nano SDK class to combine a given StreamLoader with a given StreamPlayer.
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include "IMediaPlayer.h"
#include "IStreamPlayer.h"
#include "IStreamLoader.h"

#include <string>

#include <inttypes.h>

namespace ctvc {

template<class STREAM_LOADER_TYPE> class SimpleMediaPlayer : public IMediaPlayer, public IStream
{
public:
    SimpleMediaPlayer(IStreamPlayer &stream_player) :
        m_stream_player(stream_player),
        m_callback(0),
        m_stream_out(0),
        m_has_seen_stream(false)
    {
    }

    ~SimpleMediaPlayer()
    {
    }

    // Implements IMediaPlayer
    ResultCode open_stream(const std::string &uri, const std::map<std::string, std::string> &/*stream_params*/, IStream &stream_out, IStream *&stream_in/*out*/)
    {
        player_event(PLAYER_STARTING);

        m_stream_out = &stream_out;
        stream_in = &m_stream_player;
        ResultCode ret;
        // CNP-2652: First start the media player, because otherwise we may miss the initial
        // frame(s) when the client (middleware) media player uses UDP for local streaming.
        ret =  m_stream_player.start();
        if (ret.is_error()) {
            return ret;
        }
        return m_stream_loader.open_stream(uri, *this);
    }

    void close_stream()
    {
        m_stream_player.stop();
        m_stream_loader.close_stream();
        m_stream_out = 0;
        m_has_seen_stream = false;

        player_event(PLAYER_STOPPED);
    }

    void get_player_info(PlayerInfo &/*info*/)
    {
    }

    void register_callback(ICallback *callback)
    {
        m_callback = callback;
    }

private:
    SimpleMediaPlayer(const SimpleMediaPlayer &);
    SimpleMediaPlayer &operator=(const SimpleMediaPlayer &);

    IStreamPlayer &m_stream_player;
    STREAM_LOADER_TYPE m_stream_loader;
    ICallback *m_callback;
    IStream *m_stream_out;
    bool m_has_seen_stream;

    void player_event(PlayerEvent event)
    {
        if (m_callback) {
            m_callback->player_event(event);
        }
    }

    void stream_data(const uint8_t *data, uint32_t length)
    {
        if (!m_has_seen_stream && data && length > 0) {
            m_has_seen_stream = true;
            player_event(PLAYER_STARTED);
        }

        if (m_stream_out) {
            m_stream_out->stream_data(data, length);
        }
    }

    void stream_error(ResultCode result)
    {
        if (m_stream_out) {
            m_stream_out->stream_error(result);
        }
    }
};

template<class STREAM_LOADER_TYPE> class SimpleMediaPlayerFactory : public IMediaPlayerFactory
{
public:
    SimpleMediaPlayerFactory(IStreamPlayer &stream_player) :
        m_stream_player(stream_player)
    {
    }

    IMediaPlayer *create()
    {
        return new SimpleMediaPlayer<STREAM_LOADER_TYPE>(m_stream_player);
    }

    void destroy(IMediaPlayer *p)
    {
        delete p;
    }

private:
    IStreamPlayer &m_stream_player;
};

} // namespace
