///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <rplayer/IPacketSink.h>

#include <inttypes.h>

namespace rplayer {

struct IDataSink;
struct IEventSink;
struct IDecryptEngineFactory;

class TsDemux : public IPacketSinkWithMetaData
{
public:
    TsDemux();
    ~TsDemux();

    // Set or remove the sinks individually.
    // For proper operation they must be set or reset before the stream is being parsed.
    void setEventOutput(IEventSink *eventOut);
    void setVideoOutput(IDataSink *videoOut);
    void setKeyFrameVideoOutput(IDataSink *keyFrameVideoOut);
    void setAudioOutput(IDataSink *audioOut);

    // Call to check whether the supplied data matches the start of a transport stream.
    static bool isMatch(const uint8_t *data, uint32_t size);

    // IPacketSink implementation
    // Call this to parse Transport Stream data, typically one or more TS packets.
    void put(const uint8_t *data, uint32_t size);

    void setMetaData(const StreamMetaData &metaData);

    // Call this if the Transport Stream is discontinuous, e.g. after a seek.
    // Any stored unprocessed data will be lost.
    // The attached IDataSink objects will also receive a call to reset().
    void reset();

    // Call this to set the preferred language. May lead to new stream selection if PAT and PMT already have been received.
    void setPreferredLanguage(const char *language);

    // Call these to check whether audio, video and key frame video streams are present (i.e. listed in the PMT) or not.
    // A PAT and PMT must have been received for these to ever return true.
    bool hasAudio() const;
    bool hasVideo() const;
    bool hasKeyFrameVideo() const;

    // For decryption/remultiplexing purposes, an IPacketSink interface can be passed, which
    // will receive any successfully parsed and potentially decrypted transport packet.
    // Only one packet will be sent at a time, and the sent size will always be 188 bytes.
    void setTsPacketOutput(IPacketSinkWithMetaData *packetOut);

    // To enable stream decryption, registed the appropriate decrypt engine factories.
    void registerDecryptEngineFactory(IDecryptEngineFactory &factory);
    void unregisterDecryptEngineFactory(IDecryptEngineFactory &factory);

private:
    TsDemux(const TsDemux &);
    TsDemux &operator=(const TsDemux &);

    class Impl;
    Impl &m_impl;
};

} // namespace rplayer
