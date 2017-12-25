///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <rplayer/ts/TimeStamp.h>

#include <inttypes.h>

namespace rplayer {

//
// This interface is a callback from the TsDemux to the user.
// It is called whenever a notable event occurs while demultiplexing.
//

struct IEventSink
{
    enum PrivateDataType
    {
        KEY_PRESS, FIRST_PAINT, APP_COMPLETE
    };

    IEventSink() {}
    virtual ~IEventSink() {}

    // Called whenever a PCR is received in the relevant PCR stream.
    // The PCR_base and PCR_extension are passed as separate arguments.
    virtual void pcrReceived(uint64_t pcr90kHz, int pcrExt27MHz, bool hasDiscontinuity) = 0;

    // Called whenever a table version update is received, or when a new
    // table is found after a call to TsDemux::reset().
    // tableId is the MPEG-2 assigned or private table ID, which is 0 for PAT, 1 for CAT and 2 for PMT
    // version is the 5 bit table version number, having a value of 0-31.
    virtual void tableVersionUpdate(int tableId, int version) = 0;

    // Called whenever private data is received in the stream, such as latency data.
    // data_type is the type of data received,
    // pts is the PTS time stamp associated with this data.
    // data is the data. Contents depends on the data_type
    virtual void privateStreamData(PrivateDataType data_type, TimeStamp pts, uint64_t data) = 0;
};

} // namespace rplayer
