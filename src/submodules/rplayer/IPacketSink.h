///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <inttypes.h>

namespace rplayer {

class StreamMetaData;

//
// This interface is a callback from the TsMux to the user.
// It is called whenever a transport packet is ready to be sent.
// Also the TsDemux uses this interface to pass processed transport
// packets.
// The source can decide to send a single transport packet or to
// send multiple transport packets at once. They may or may not be
// aligned.
// Furthermore, this interface can be used to pass packetized data
// other than transport packets, if necessary.
//

struct IPacketSink
{
    IPacketSink() {}
    virtual ~IPacketSink() {}

    // For transport stream interfaces, this method is called when one or
    // more transport packets are available.
    // Data other than transport packets can also be sent over this interface.
    virtual void put(const uint8_t *data, uint32_t size) = 0;
};

struct IPacketSinkWithMetaData : public IPacketSink
{
    // Set the metadata belonging to the stream.
    // If not sent, it will be the default metadata.
    // If sent, the metadata will apply to all following calls to put() until
    // the next call to setMedaData.
    virtual void setMetaData(const StreamMetaData &) = 0;
};

} // namespace rplayer
