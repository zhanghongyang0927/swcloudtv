///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <rplayer/ts/TsMux.h>
#include <rplayer/ts/TimeStamp.h>

#include "common.h"

#include <inttypes.h>

#include <vector>

namespace rplayer {

struct IPacketSink;
struct IDataSource;

class TsMux::Impl
{
public:
    Impl();
    ~Impl();

    void reset();

    unsigned muxPackets(TimeStamp currentPcr, int muxFlags, unsigned maxPackets);

    struct StreamInfo;

    void setupStreamTypes();

    uint32_t putTsPacketFromData(const uint8_t *data, uint32_t size, bool isEncrypted, StreamInfo &streamInfo, bool sendPesHeader, TimeStamp pts, TimeStamp dts, TimeStamp pcr);
    void putTsPacketFromSource(IDataSource &dataSource, StreamInfo &streamInfo, StreamInfo &ecmStreamInfo, TimeStamp pcr);

    void putCetsEcmPacket(IDataSource &dataSource, StreamInfo &ecmStreamInfo);

    // Tables generation
    void addPatEntry(const StreamInfo &info, int programId, std::vector<uint8_t> &data);
    void addPmtEntry(const StreamInfo &, std::vector<uint8_t> &data);
    void addTableHeader(uint8_t tableId, uint32_t tableIdExtension, bool privateIndicator, std::vector<uint8_t> &data);
    void putPat();
    void putPmt();
    void putSit();
    void tablesSection(StreamInfo &streamInfo, const std::vector<uint8_t> &payload);

    TimeStamp checkAndGetPcr(const StreamInfo &streamInfo, TimeStamp pcr);

    bool isSeparatePcrPid();

    double estimateInputBandwidth(double audioPesPacketsPerSecond, double videoPesPacketsPerSecond, double outputBandwidthInBitsPerSecond);

    IPacketSink *m_output;
    IDataSource *m_videoSource;
    IDataSource *m_audioSource;
    IDataSource *m_logSource;

    TimeStamp m_psiPeriod;
    TimeStamp m_pcrOfLastSentPsi;

    TimeStamp m_pcrPeriod;
    TimeStamp m_pcrOfLastSentPcr;
    bool m_pcrDiscontinuity;

    int m_transportStreamId;
    int m_programNumber;

    struct StreamInfo
    {
        StreamInfo(int pid) :
            m_pid(pid),
            m_cc(0),
            m_streamType(PMT_STREAM_TYPE_RESERVED),
            m_streamId(0),
            m_hasPesSyntax(true),
            m_tableVersion(0),
            m_tableCrc(0),
            m_currentScramblingControl(0)
        {
        }

        void reinitialize()
        {
            m_cc = 0;
            m_streamType = PMT_STREAM_TYPE_RESERVED;
            m_streamId = 0;
            m_hasPesSyntax = true;
            m_tableVersion = 0;
            m_tableCrc = 0;
            m_currentScramblingControl = 0;
        }

        void setStreamId(const PesStreamId &streamId)
        {
            m_streamId = streamId.m_value;
            m_hasPesSyntax = streamId.m_hasPesSyntax;
        }

        bool isEnabled() const
        {
            return m_pid != INVALID_PID;
        }

        int m_pid; // For streaming & PSI tables
        int m_cc; // For streaming & PSI tables
        TsProgramMapStreamType m_streamType; // In PMT
        int m_streamId; // In PES header
        bool m_hasPesSyntax; // For creating PES header
        int m_tableVersion; // For PSI tables
        uint32_t m_tableCrc; // For PSI tables
        int m_currentScramblingControl; // For PES/PSI scrambling
        std::vector<uint8_t> m_staticDescriptors; // In PMT
        std::vector<uint8_t> m_dynamicDescriptors; // In PMT
    };

    StreamInfo m_patInfo;
    StreamInfo m_pmtInfo;
    StreamInfo m_sitInfo;
    StreamInfo m_pcrAndProgramInfo; // PCR PID and program descriptors
    StreamInfo m_videoInfo;
    StreamInfo m_audioInfo;
    StreamInfo m_videoEcmInfo;
    StreamInfo m_audioEcmInfo;
    StreamInfo m_logInfo;

    unsigned m_packetsSent;

    bool isVideoEnabled()
    {
        return m_videoSource && m_videoInfo.isEnabled();
    }

    bool isAudioEnabled()
    {
        return m_audioSource && m_audioInfo.isEnabled();
    }

    bool isLogEnabled()
    {
        return m_logSource && m_logInfo.isEnabled();
    }
};

} // namespace rplayer
