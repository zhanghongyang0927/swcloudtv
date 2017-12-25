///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <string>

#include <inttypes.h>

namespace rplayer {

struct IPacketSink;
struct IPacketSinkWithMetaData;
struct IDecryptEngineFactory;
struct IEventSink;
struct IStreamDecrypt;
struct IRamsChunkAllocator;
class TimeStamp;

class RPlayer
{
public:
    // Enabling of rplayer features
    enum Feature
    {
        FEATURE_NONE = 0, FEATURE_RAMS_DECODER = 1, FEATURE_CENC_DECRYPTION = 2, FEATURE_UNDERRUN_MITIGATION = 4
    };

    struct ICallback
    {
        virtual ~ICallback()
        {
        }

        virtual void stallDetected(const std::string &id, bool isAudioNotVideo, const TimeStamp &stallDuration) = 0;
    };

    RPlayer();
    ~RPlayer();

    // Reinitialize the RPlayer
    // This will reset all RPlayer parameters and dynamic state but it won't
    // unregister any registered decrypt engines, chunk allocators or the like.
    void reinitialize();

    // Reset the RPlayer
    // This flushes any pending output (if necessary or applicable) and
    // prepares the RPlayer to receive a fresh new stream.
    void reset();

    // Set the RPlayer enabled features
    void setEnabledFeatures(Feature enabledFeatures);

    // Set rplayer parameter
    // This is a generic interface to enable debugging and transparent extension of parameters.
    // The current parameters are:
    //   PARAMETER                  VALUE                   DEFAULT      EXPLANATION
    // enabled_features           rams | cenc | underrun               Logical 'or' of zero or more of these features
    // min_audio_frame_distance   time in ms                   1       Minimum (PTS) distance between audio frames to be ensured
    // min_video_frame_distance   time in ms                  15       Minimum (PTS) distance between video frames to be ensured
    // min_audio_delay            time in ms                  15       Minimum (PTS-PCR) delay of audio frames to be ensured
    // min_video_delay            time in ms                  15       Minimum (PTS-PCR) delay of video frames to be ensured
    // audio_delay                time in ms                   0       Audio delay (added to PTS) before further processing
    // video_delay                time in ms                   0       Video delay (added to PTS) before further processing
    // pcr_resync_threshold       time in ms                   0       If non-zero, the time the real-time PCR must lead the
    //                                                                 transmitted PCR before resynchronization will take place
    // audio_correction           off | adjust_pts                     Mitigation mechanism for audio (select one)
    // video_correction           off | adjust_pts                     Mitigation mechanism for video (select one)
    void setParameter(const std::string &parameter, const std::string &value);

    // Get status of the RPlayer, if available
    // Status values that are unavailable remain unchanged.
    // Currently, the values are only available if underrun mitigation is enabled.
    void getStatus(uint64_t &currentStreamTimeIn90kHzTicks, uint32_t &stalledDurationInMs, uint32_t &pcrDelayIn90kHzTicks);

    // Set the callback object
    void registerCallback(ICallback *);

    // Registration of available decrypt engine factories
    // Note: FEATURE_CENC_DECRYPTION must be enabled for this to have any effect.
    void registerDecryptEngineFactory(IDecryptEngineFactory &decryptEngineFactory);
    void unregisterDecryptEngineFactory(IDecryptEngineFactory &decryptEngineFactory);

    // Registration of any (RAMS-addressed) decrypt engine
    // Note: FEATURE_RAMS_DECODER must be enabled for this to have any effect.
    void registerStreamDecryptEngine(IStreamDecrypt *streamDecryptEngine);

    // Registration of a RAMS chunk allocator
    // Note: FEATURE_RAMS_DECODER must be enabled for this to have any effect.
    void registerRamsChunkAllocator(IRamsChunkAllocator *ramsChunkAllocator);

    // Registration of a TS output that will receive the processed transport stream.
    // It will receive any successfully parsed, potentially RAMS-decoded, potentially decrypted, potentially underrun-mitigated transport packet.
    void setTsPacketOutput(IPacketSinkWithMetaData *packetOut);

    // Registration of an output (demux) event sink object
    // This object will receive any (demux) events that occur at the output, at the moment the data leaves the RPlayer.
    // Note: either FEATURE_CENC_DECRYPTION or FEATURE_UNDERRUN_MITIGATION must be enabled for this to have any effect.
    void registerOutputEventSink(IEventSink *eventOut);

    // Call this to parse Transport Stream or RAMS data (if FEATURE_RAMS_DECODER is enabled), typically one or more TS or RAMS packets.
    void parse(const uint8_t *data, uint32_t size);

    // Set current real time in ms. The time may (and will) wrap around. This is no problem.
    // It should be continuous, however, meaning that any difference in the real time should
    // equal the difference in the time passed.
    // The origin of the absolute value does not matter.
    // A real-time thread can/will call this on regular basis.
    // If used, this method must be called immediately prior to each call to parse() for time
    // management to properly operate.
    void setCurrentTime(uint16_t timeInMs);

private:
    RPlayer(const RPlayer &);
    RPlayer &operator=(const RPlayer &);

    class Impl;
    Impl &m_impl;
};

// Enable type-safe logically or-ing RPlayer::Feature flags
inline RPlayer::Feature operator|(RPlayer::Feature lhs, RPlayer::Feature rhs)
{
    return static_cast<RPlayer::Feature>(static_cast<int>(lhs) | static_cast<int>(rhs));
}

} // namespace rplayer
