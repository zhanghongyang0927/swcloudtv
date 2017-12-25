///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <rplayer/ts/TimeStamp.h>
#include <rplayer/ts/TsCommon.h>
#include <rplayer/ts/DecryptInfo.h>

#include <string>
#include <vector>

#include <inttypes.h>

namespace rplayer {

//
// This interface is a callback from the TsMux to the user.
// It is called whenever a piece of data needs to be queried or obtained from the respective stream.
//
struct IDataSource
{
    IDataSource() {}
    virtual ~IDataSource() {}

    virtual StreamType getStreamType() = 0;
    virtual const uint8_t *getDrmSystemId() = 0; // Must return the address of a 16-byte GUID or NULL if no DRM system is to be used.
    virtual bool isNewFrame(TimeStamp &pts, TimeStamp &dts) = 0; // If this returns true, a new PES header is inserted; PTS and optionally DTS are expected to be set then,
    virtual const uint8_t *getData() = 0;
    virtual bool isDataEncrypted() = 0; // Returns true if the current data chunk is encrypted
    virtual uint32_t getBytesAvailable(TimeStamp pcr) = 0;
    virtual void readBytes(uint32_t n) = 0;
    virtual const std::string getLanguage() = 0;
    virtual const std::vector<DecryptInfo> getScramblingParameters() = 0; // First entry of the vector is data for upcoming PES packet; other entries are optional
};

// Base implementation for unscrambled data sources
class UnscrambledDataSource : public IDataSource
{
public:
    const uint8_t *getDrmSystemId()
    {
        return 0;
    }

    bool isDataEncrypted()
    {
        return false;
    }

    const std::vector<DecryptInfo> getScramblingParameters()
    {
        return std::vector<DecryptInfo>();
    }
};

} // namespace rplayer
