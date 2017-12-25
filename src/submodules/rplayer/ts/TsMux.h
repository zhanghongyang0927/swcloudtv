///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <rplayer/ts/TimeStamp.h>

#include <vector>

#include <inttypes.h>

namespace rplayer {

struct IPacketSink;
struct IDataSource;

class TsMux
{
public:
    TsMux();
    ~TsMux();

    // Muxing flags
    static const int MUX_NONE = 0;
    static const int MUX_AUDIO = 1;
    static const int MUX_VIDEO = 2;
    static const int MUX_LOG = 4;
    static const int MUX_PCR = 8; // Only has effect if a separate PCR PID is used; otherwise, the PCR is silently muxed along with the appropriate stream.
    static const int MUX_FORCE_PCR = 16; // Force multiplexing a PCR if a separate PCR PID is used and if there is still room to send more packets.
    static const int MUX_ALL = MUX_AUDIO | MUX_VIDEO | MUX_LOG | MUX_PCR;

    // Reset dynamic state
    void reset();

    // Attach output
    void setOutput(IPacketSink *output);

    // Attach individual inputs
    void setVideoInput(IDataSource *videoInput);
    void setAudioInput(IDataSource *audioInput);
    void setLogInput(IDataSource *logInput);

    // Multiplex a single NULL packet. The output interface will be called exactly once.
    // No other calls will be done, no TsMux state is changed.
    void muxStuffing();

    // Multiplexes zero or more transport stream packets.
    // The multiplexing is capped to the maxPackets count given,
    // with the sole exception of PSI (PAT, PMT and SIT) which
    // are always transmitted as a group, if sent.
    // It will send out PAT, PMT, PCR and ECMs as required.
    // It queries the IDataSource inputs for data and determine what to send.
    // Sending priority is 1. audio, 2. video and 3. log if all are enabled.
    // All data of a stream is sent until the stream's getBytesAvailable()
    // method returns 0 or the maxPackets count has been reached.
    // The muxer only tries the flagged streams, which allows you to steer
    // the muxing behavior in detail.
    // PAT, PMT and optionally PCR will be muxed even if no streams are enabled.
    //
    // Returns the number of packets sent
    unsigned muxPackets(TimeStamp currentPcr, int muxFlags, unsigned maxPackets);

    // Estimate the total available input bandwidth in bits/s given the current settings,
    // an estimated number of PES packets per second and the total available output bandwidth.
    double estimateInputBandwidth(double audioPesPacketsPerSecond, double videoPesPacketsPerSecond, double outputBandwidthInBitsPerSecond);

    void setTransportStreamId(int);
    int getTransportStreamId() const;
    void setProgramNumber(int);
    int getProgramNumber() const;
    void setSitPid(int);
    int getSitPid() const;
    void setPmtPid(int);
    int getPmtPid() const;
    void setPcrPid(int);
    int getPcrPid() const;
    void setVideoPid(int);
    int getVideoPid() const;
    void setAudioPid(int);
    int getAudioPid() const;
    void setLogPid(int);
    int getLogPid() const;
    void setProgramDescriptors(const std::vector<uint8_t> &);
    std::vector<uint8_t> getProgramDescriptors() const;
    void setVideoDescriptors(const std::vector<uint8_t> &);
    std::vector<uint8_t> getVideoDescriptors() const;
    void setAudioDescriptors(const std::vector<uint8_t> &);
    std::vector<uint8_t> getAudioDescriptors() const;
    void setPsiPeriodInMs(int);
    int getPsiPeriodInMs() const;
    void setPcrPeriodInMs(int);
    int getPcrPeriodInMs() const;

private:
    TsMux(const TsMux &);
    TsMux &operator=(const TsMux &);

    class Impl;
    Impl &m_impl;
};

} // namespace rplayer
