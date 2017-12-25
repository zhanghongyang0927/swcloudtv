///
/// \file IStallEvent.h
///
/// \brief CloudTV Nano SDK Stream interface for stall events.
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <porting_layer/TimeStamp.h>

#include <string>

namespace ctvc {

/// \brief Stall event interface.
/// This interface is used to pass rplayer stall events to the clients of the Streamer class.
struct IStallEvent
{
    IStallEvent()
    {
    }

    virtual ~IStallEvent()
    {
    }

    /// \brief Called when there is new stall event from the stream.
    virtual void stall_detected(const std::string &id, bool is_audio_not_video, const TimeStamp &stall_duration) = 0;
};

} // namespace
