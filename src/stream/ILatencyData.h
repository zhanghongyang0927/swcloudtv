///
/// \file ILatencyData.h
///
/// \brief CloudTV Nano SDK Stream interface for Latency Data.
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <porting_layer/TimeStamp.h>

namespace ctvc {

/// \brief Stream Player interface.
/// The stream player plays a stream (typically MPEG-2 TS) that enters through IStream.
struct ILatencyData
{

    enum LatencyDataType
    {
        KEY_PRESS,
        FIRST_PAINT,
        APP_COMPLETE
    };

    ILatencyData()
    {
    }

    virtual ~ILatencyData()
    {
    }

    /// \brief Called when there is new latency data from the stream.
     virtual void latency_stream_data(LatencyDataType data_type, TimeStamp pts, TimeStamp original_event_time) = 0;
};

} // namespace
