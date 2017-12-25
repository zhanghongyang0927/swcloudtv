///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include "TsMuxImpl.h"

#include <rplayer/IPacketSink.h>
#include <rplayer/ts/IDataSource.h>
#include <rplayer/utils/BitWriter.h>
#include <rplayer/utils/Logger.h>

#include <algorithm>

#include <string.h>
#include <stdint.h>
#include <assert.h>

using namespace rplayer;

// Defaults
static const int DEFAULT_PMT_PID = 64;
static const int DEFAULT_VIDEO_PID = 65;
static const int DEFAULT_AUDIO_PID = 66;
static const int DEFAULT_PCR_PID = 67;
static const int DEFAULT_LOG_PID = INVALID_PID;
static const int DEFAULT_SIT_PID = INVALID_PID;
static const int DEFAULT_ECM_PID_RANGE_START = 80; // First of the ECM PID range

static const int DEFAULT_TRANSPORT_STREAM_ID = 512;
static const int DEFAULT_PROGRAM_NUMBER = 1;
static const int DEFAULT_PSI_PERIOD = 400; // ms
static const int DEFAULT_PCR_PERIOD = 80; // ms

// Empty transport stream packet.
static const uint8_t g_emptyPacket[TS_PACKET_SIZE] = { TS_SYNC_BYTE, 0x1F, 0xFF, 0x10 };

TsMux::TsMux() :
    m_impl(*new TsMux::Impl())
{
}

TsMux::~TsMux()
{
    delete &m_impl;
}

void TsMux::reset()
{
    m_impl.reset();
}

void TsMux::setOutput(IPacketSink *output)
{
    m_impl.m_output = output;
}

void TsMux::setVideoInput(IDataSource *videoInput)
{
    m_impl.m_videoSource = videoInput;
}

void TsMux::setAudioInput(IDataSource *audioInput)
{
    m_impl.m_audioSource = audioInput;
}

void TsMux::setLogInput(IDataSource *logInput)
{
    m_impl.m_logSource = logInput;
}

void TsMux::muxStuffing()
{
    if (m_impl.m_output) {
        m_impl.m_output->put(g_emptyPacket, TS_PACKET_SIZE);
    }
    m_impl.m_packetsSent++;
}

TsMux::Impl::Impl() :
    m_output(0),
    m_videoSource(0),
    m_audioSource(0),
    m_logSource(0),
    m_pcrDiscontinuity(true),
    m_transportStreamId(DEFAULT_TRANSPORT_STREAM_ID),
    m_programNumber(DEFAULT_PROGRAM_NUMBER),
    m_patInfo(PAT_PID),
    m_pmtInfo(DEFAULT_PMT_PID),
    m_sitInfo(DEFAULT_SIT_PID),
    m_pcrAndProgramInfo(DEFAULT_PCR_PID),
    m_videoInfo(DEFAULT_VIDEO_PID),
    m_audioInfo(DEFAULT_AUDIO_PID),
    m_videoEcmInfo(INVALID_PID),
    m_audioEcmInfo(INVALID_PID),
    m_logInfo(DEFAULT_LOG_PID),
    m_packetsSent(0)
{
    m_psiPeriod.setAsMilliseconds(DEFAULT_PSI_PERIOD);
    m_pcrPeriod.setAsMilliseconds(DEFAULT_PCR_PERIOD);
}

TsMux::Impl::~Impl()
{
}

void TsMux::Impl::reset()
{
    m_pcrOfLastSentPsi.invalidate();
    m_pcrOfLastSentPcr.invalidate();
    m_pcrDiscontinuity = true;

    m_patInfo.reinitialize();
    m_pmtInfo.reinitialize();
    m_sitInfo.reinitialize();
    m_pcrAndProgramInfo.reinitialize();
    m_videoInfo.reinitialize();
    m_audioInfo.reinitialize();
    m_videoEcmInfo.reinitialize();
    m_audioEcmInfo.reinitialize();
    m_logInfo.reinitialize();

    m_packetsSent = 0;
}

double TsMux::estimateInputBandwidth(double audioPesPacketsPerSecond, double videoPesPacketsPerSecond, double outputBandwidthInBitsPerSecond)
{
    return m_impl.estimateInputBandwidth(audioPesPacketsPerSecond, videoPesPacketsPerSecond, outputBandwidthInBitsPerSecond);
}

void TsMux::setTransportStreamId(int transportStreamId)
{
    m_impl.m_transportStreamId = transportStreamId;
}

int TsMux::getTransportStreamId() const
{
    return m_impl.m_transportStreamId;
}

void TsMux::setProgramNumber(int programNumber)
{
    m_impl.m_programNumber = programNumber;
}

int TsMux::getProgramNumber() const
{
    return m_impl.m_programNumber;
}

void TsMux::setSitPid(int sitPid)
{
    m_impl.m_sitInfo.m_pid = sitPid;
}

int TsMux::getSitPid() const
{
    return m_impl.m_sitInfo.m_pid;
}

void TsMux::setPmtPid(int pmtPid)
{
    m_impl.m_pmtInfo.m_pid = pmtPid;
}

int TsMux::getPmtPid() const
{
    return m_impl.m_pmtInfo.m_pid;
}

void TsMux::setPcrPid(int pcrPid)
{
    m_impl.m_pcrAndProgramInfo.m_pid = pcrPid;
}

int TsMux::getPcrPid() const
{
    return m_impl.m_pcrAndProgramInfo.m_pid;
}

void TsMux::setVideoPid(int videoPid)
{
    m_impl.m_videoInfo.m_pid = videoPid;
}

int TsMux::getVideoPid() const
{
    return m_impl.m_videoInfo.m_pid;
}

void TsMux::setAudioPid(int audioPid)
{
    m_impl.m_audioInfo.m_pid = audioPid;
}

int TsMux::getAudioPid() const
{
    return m_impl.m_audioInfo.m_pid;
}

void TsMux::setLogPid(int logPid)
{
    m_impl.m_logInfo.m_pid = logPid;
}

int TsMux::getLogPid() const
{
    return m_impl.m_logInfo.m_pid;
}

void TsMux::setProgramDescriptors(const std::vector<uint8_t> &programDescriptors)
{
    m_impl.m_pcrAndProgramInfo.m_staticDescriptors = programDescriptors;
}

std::vector<uint8_t> TsMux::getProgramDescriptors() const
{
    return m_impl.m_pcrAndProgramInfo.m_staticDescriptors;
}

void TsMux::setVideoDescriptors(const std::vector<uint8_t> &videoDescriptors)
{
    m_impl.m_videoInfo.m_staticDescriptors = videoDescriptors;
}

std::vector<uint8_t> TsMux::getVideoDescriptors() const
{
    return m_impl.m_videoInfo.m_staticDescriptors;
}

void TsMux::setAudioDescriptors(const std::vector<uint8_t> &audioDescriptors)
{
    m_impl.m_audioInfo.m_staticDescriptors = audioDescriptors;
}

std::vector<uint8_t> TsMux::getAudioDescriptors() const
{
    return m_impl.m_audioInfo.m_staticDescriptors;
}

void TsMux::setPsiPeriodInMs(int psiPeriodInMs)
{
    m_impl.m_psiPeriod.setAsMilliseconds(std::max(psiPeriodInMs, 1)); // A period of 0ms or less is not allowed
}

int TsMux::getPsiPeriodInMs() const
{
    return m_impl.m_psiPeriod.getAsMilliseconds();
}

void TsMux::setPcrPeriodInMs(int pcrPeriodInMs)
{
    m_impl.m_pcrPeriod.setAsMilliseconds(std::max(pcrPeriodInMs, 1)); // A period of 0ms or less is not allowed
}

int TsMux::getPcrPeriodInMs() const
{
    return m_impl.m_pcrPeriod.getAsMilliseconds();
}

TimeStamp TsMux::Impl::checkAndGetPcr(const StreamInfo &streamInfo, TimeStamp pcr)
{
    if (streamInfo.m_pid != m_pcrAndProgramInfo.m_pid) {
        return TimeStamp();
    }

    if (!m_pcrOfLastSentPcr.isValid() || pcr >= m_pcrOfLastSentPcr + m_pcrPeriod) { // Will also send first time
        return pcr;
    }

    return TimeStamp();
}

bool TsMux::Impl::isSeparatePcrPid()
{
    // Check and flag whether to use a separate PCR PID
    return !((isAudioEnabled() && m_pcrAndProgramInfo.m_pid == m_audioInfo.m_pid) ||
             (isVideoEnabled() && m_pcrAndProgramInfo.m_pid == m_videoInfo.m_pid));
}

double TsMux::Impl::estimateInputBandwidth(double audioPesPacketsPerSecond, double videoPesPacketsPerSecond, double outputBandwidthInBitsPerSecond)
{
    double overheadInPacketsPerSecond = 0.0;
    double overheadInBytesPerSecond = 0.0;

    // PSI overhead
    overheadInPacketsPerSecond += (2 + (m_sitInfo.isEnabled())) / m_psiPeriod.getAsSeconds(); // PSI overhead

    // PCR overhead
    if (isSeparatePcrPid()) {
        overheadInPacketsPerSecond += 1.0 / m_pcrPeriod.getAsSeconds(); // PCR overhead if sent as separate packets
    } else {
        overheadInBytesPerSecond += 8 / m_pcrPeriod.getAsSeconds(); // PCR overhead if sent within an existing stream
    }

    // ECM overhead
    // With encrypted streams, typically there is a (single-packet) ECM per PES packet.
    // CENC-TS encryption typically uses additional packets to transfer headers in the clear,
    // We estimate that as being 1 additional for both audio and video, although this is a rather coarse estimate.
    if (m_audioEcmInfo.isEnabled()) {
        overheadInPacketsPerSecond += audioPesPacketsPerSecond * 2.0;
    }
    if (m_videoEcmInfo.isEnabled()) {
        overheadInPacketsPerSecond += videoPesPacketsPerSecond * 2.0;
    }

    // We calculate the PES header overhead as fixed rather than relative because we can say more about
    // the frequency of PES headers than the repetitive overhead they will take with respect to a stream...
    int overheadPerPesPacket = 14; // PES packet header overhead, with PTS but no DTS.
    overheadPerPesPacket += TS_MAX_PAYLOAD_SIZE / 2; // On average, we'll also have half a packet payload of overhead due to PES packets not completely fitting an integer number of TS packets.
    overheadInBytesPerSecond += (audioPesPacketsPerSecond + videoPesPacketsPerSecond) * overheadPerPesPacket;

    // First subtract the required additional packets from the output bandwidth
    double inputBandwidthInBitsPerSecond = outputBandwidthInBitsPerSecond - overheadInPacketsPerSecond * TS_PACKET_SIZE * 8;

    // What's left must be rescaled to account for the TS packet header
    inputBandwidthInBitsPerSecond = inputBandwidthInBitsPerSecond * TS_MAX_PAYLOAD_SIZE / TS_PACKET_SIZE; // TS header overhead

    // We then subtract the payload overhead that's induced
    inputBandwidthInBitsPerSecond -= overheadInBytesPerSecond * 8;

    return inputBandwidthInBitsPerSecond;
}

unsigned TsMux::muxPackets(TimeStamp currentPcr, int muxFlags, unsigned maxPackets)
{
    return m_impl.muxPackets(currentPcr, muxFlags, maxPackets);
}

unsigned TsMux::Impl::muxPackets(TimeStamp currentPcr, int muxFlags, unsigned maxPackets)
{
    assert(maxPackets > 0);
    m_packetsSent = 0;

    if (!m_pcrOfLastSentPsi.isValid() || currentPcr >= m_pcrOfLastSentPsi + m_psiPeriod) { // Will also send first time
        // setupStreamTypes() will prepare the contents of the PSI tables
        // TODO: This may need to be done sooner if one of the stream properties
        // change. Now, it will follow the stream contents, but there will be a
        // lag between the stream changing and the tables being updated. However,
        // checking this proactively will need the use of dirty flags and/or storage
        // and repetitive comparison of stream properties.
        setupStreamTypes();

        // Don't multiplex anything until we've got valid audio and video frames so we know what to put in the PMT
        if ((isAudioEnabled() && m_audioInfo.m_streamType == PMT_STREAM_TYPE_RESERVED) ||
            (isVideoEnabled() && m_videoInfo.m_streamType == PMT_STREAM_TYPE_RESERVED)) {
            return m_packetsSent;
        }

        m_pcrOfLastSentPsi = currentPcr;

        // Immediately send all PSI tables
        // Actually, we should send them a packet at a time, but since they need to
        // be sent asap and there's little use in holding them up until the next time
        // slot, they might as well be sent right away.
        putPat();
        if (m_sitInfo.isEnabled()) {
            putSit();
        }
        putPmt();

        if (m_packetsSent >= maxPackets) {
            return m_packetsSent;
        }
    }

    // Mux a separate PCR if requested
    if ((muxFlags & (MUX_PCR | MUX_FORCE_PCR)) && isSeparatePcrPid()) {
        if ((muxFlags & MUX_FORCE_PCR) || checkAndGetPcr(m_pcrAndProgramInfo, currentPcr).isValid()) {
            putTsPacketFromData(0, 0, false, m_pcrAndProgramInfo, false, TimeStamp(), TimeStamp(), currentPcr);

            if (m_packetsSent >= maxPackets) {
                return m_packetsSent;
            }
        }
    }

    // Mux audio, if available
    if ((muxFlags & MUX_AUDIO) && isAudioEnabled()) {
        while (m_audioSource->getBytesAvailable(currentPcr) > 0) {
            // Send a TS packet full of data (Including PES header and/or PCR if needed)
            putTsPacketFromSource(*m_audioSource, m_audioInfo, m_audioEcmInfo, currentPcr);

            if (m_packetsSent >= maxPackets) {
                return m_packetsSent;
            }
        }
    }

    // Mux video, if available
    if ((muxFlags & MUX_VIDEO) && isVideoEnabled()) {
        while (m_videoSource->getBytesAvailable(currentPcr) > 0) {
            // Send a TS packet full of data (Including PES header and/or PCR if needed)
            putTsPacketFromSource(*m_videoSource, m_videoInfo, m_videoEcmInfo, currentPcr);

            if (m_packetsSent >= maxPackets) {
                return m_packetsSent;
            }
        }
    }

    // Mux logs, if available
    if ((muxFlags & MUX_LOG) && isLogEnabled()) {
        const uint8_t *data = m_logSource->getData();
        uint32_t bytesAvailable = m_logSource->getBytesAvailable(currentPcr);

        while (data && bytesAvailable > 0) {
            // This leads to a stream that is not compatible with MPEG-2 TS (no PES or section start, no adaptation field allowed) but compatible with CloudTV player
            uint8_t packet[TS_MAX_PAYLOAD_SIZE];
            uint32_t bytesSent = 0;
            if (bytesAvailable < TS_MAX_PAYLOAD_SIZE) {
                memcpy(packet, data, bytesAvailable);
                memset(packet + bytesAvailable, 0, TS_MAX_PAYLOAD_SIZE - bytesAvailable);

                putTsPacketFromData(packet, TS_MAX_PAYLOAD_SIZE, false, m_logInfo, false, TimeStamp(), TimeStamp(), TimeStamp());
                bytesSent = bytesAvailable;
            } else {
                bytesSent = putTsPacketFromData(data, bytesAvailable, false, m_logInfo, false, TimeStamp(), TimeStamp(), TimeStamp());
            }

            m_logSource->readBytes(bytesSent);

            if (m_packetsSent >= maxPackets) {
                return m_packetsSent;
            }

            data = m_logSource->getData();
            bytesAvailable = m_logSource->getBytesAvailable(currentPcr);
        }
    }

    return m_packetsSent;
}

static void addAc3Descriptor(std::vector<uint8_t> &descriptors)
{
    descriptors.push_back(AC3_DESCRIPTOR);
    descriptors.push_back(0x01); // descriptor_length
    descriptors.push_back(0x00); // component_type_flag(1) + bsid_flag(1) + mainid_flag(1) + asvc_flag(1) + reserved_flags(4)
}

static void addIso639LanguageDescriptor(std::vector<uint8_t> &descriptors, const std::string &language)
{
    // ISO_639_language_descriptor
    const char *s = "eng";
    if (language.size() >= 3) {
        s = language.c_str();
    }
    descriptors.push_back(ISO_639_LANGUAGE_DESCRIPTOR); // ISO_639_language_descriptor
    descriptors.push_back(4); // descriptor_length
    descriptors.push_back(s[0]); // ISO_639_language_code (8/24)
    descriptors.push_back(s[1]); // ISO_639_language_code (8/24)
    descriptors.push_back(s[2]); // ISO_639_language_code (8/24)
    descriptors.push_back(0x0); // audio_type (0 == undefined)
}

static void addCaDescriptor(std::vector<uint8_t> &descriptors, const uint8_t *drmSystemId, int pid)
{
    descriptors.push_back(CA_DESCRIPTOR);
    descriptors.push_back(0x22); // descriptor_length
    descriptors.push_back((CETS_CA_SYSTEM_ID >> 8) & 0xFF);
    descriptors.push_back(CETS_CA_SYSTEM_ID & 0xFF);
    descriptors.push_back(0xE0 | ((pid >> 8) & 0x1F));
    descriptors.push_back(pid & 0xFF);

    descriptors.push_back((SCHM_SCHEME_TYPE >> 24) & 0xFF);
    descriptors.push_back((SCHM_SCHEME_TYPE >> 16) & 0xFF);
    descriptors.push_back((SCHM_SCHEME_TYPE >> 8) & 0xFF);
    descriptors.push_back((SCHM_SCHEME_TYPE >> 0) & 0xFF);
    descriptors.push_back((SCHM_SCHEME_VERSION >> 24) & 0xFF);
    descriptors.push_back((SCHM_SCHEME_VERSION >> 16) & 0xFF);
    descriptors.push_back((SCHM_SCHEME_VERSION >> 8) & 0xFF);
    descriptors.push_back((SCHM_SCHEME_VERSION >> 0) & 0xFF);
    descriptors.push_back(1); // num_systems
    descriptors.push_back(0); // encryption_algorithm 1/3
    descriptors.push_back(0); // encryption_algorithm 2/3
    descriptors.push_back(1); // encryption_algorithm 3/3

    for (uint8_t i = 0; i < 16; i++) {
        descriptors.push_back(drmSystemId[i]);
    }

    descriptors.push_back(0xFF); // pssh_pid (INVALID_PID) 1/2
    descriptors.push_back(0xFF); // pssh_pid (INVALID_PID) 2/2
}

void TsMux::Impl::setupStreamTypes()
{
    // Disable ECM streams; they will be (re)enabled while adding the CA descriptors
    m_audioEcmInfo.m_pid = INVALID_PID;
    m_videoEcmInfo.m_pid = INVALID_PID;

    if (isAudioEnabled()) {
        // Determine and set the audio stream properties
        StreamType audioStreamType = m_audioSource->getStreamType();
        if (audioStreamType != STREAM_TYPE_UNKNOWN) {
            switch (audioStreamType) {
            case STREAM_TYPE_MPEG1_AUDIO:
                m_audioInfo.m_streamType = PMT_STREAM_TYPE_MPEG1_AUDIO;
                break;
            case STREAM_TYPE_MPEG2_AUDIO:
                m_audioInfo.m_streamType = PMT_STREAM_TYPE_MPEG2_AUDIO;
                break;
            case STREAM_TYPE_AAC_AUDIO:
                m_audioInfo.m_streamType = PMT_STREAM_TYPE_AAC_AUDIO;
                break;
            case STREAM_TYPE_AC3_AUDIO:
                m_audioInfo.m_streamType = PMT_STREAM_TYPE_AC3_AUDIO;
                break;
            default:
                assert(!"unknown audio stream type");
            }

            if (m_audioInfo.m_streamType == PMT_STREAM_TYPE_AC3_AUDIO) {
                m_audioInfo.setStreamId(PES_PRIVATE1_STREAM_ID);
            } else {
                m_audioInfo.setStreamId(PES_AUDIO_STREAM_ID);
            }

            // Fill in audio descriptors
            m_audioInfo.m_dynamicDescriptors.clear();
            if (m_audioInfo.m_streamType == PMT_STREAM_TYPE_AC3_AUDIO) {
                addAc3Descriptor(m_audioInfo.m_dynamicDescriptors);
            }

            addIso639LanguageDescriptor(m_audioInfo.m_dynamicDescriptors, m_audioSource->getLanguage());

            const uint8_t *drmSystemId = m_audioSource->getDrmSystemId();
            if (drmSystemId) {
                m_audioEcmInfo.setStreamId(PES_ECM_STREAM_ID);
                m_audioEcmInfo.m_pid = DEFAULT_ECM_PID_RANGE_START + 1;
                addCaDescriptor(m_audioInfo.m_dynamicDescriptors, drmSystemId, m_audioEcmInfo.m_pid);
            }
        }
    }

    if (isVideoEnabled()) {
        // Determine and set the video stream properties
        StreamType videoStreamType = m_videoSource->getStreamType();
        if (videoStreamType != STREAM_TYPE_UNKNOWN) {
            switch (videoStreamType) {
            case STREAM_TYPE_MPEG2_VIDEO:
                m_videoInfo.m_streamType = PMT_STREAM_TYPE_MPEG2_VIDEO;
                break;
            case STREAM_TYPE_H264_VIDEO:
                m_videoInfo.m_streamType = PMT_STREAM_TYPE_H264_VIDEO;
                break;
            default:
                assert(!"unknown video stream type");
            }

            m_videoInfo.setStreamId(PES_VIDEO_STREAM_ID);

            const uint8_t *drmSystemId = m_videoSource->getDrmSystemId();
            if (drmSystemId) {
                m_videoEcmInfo.setStreamId(PES_ECM_STREAM_ID);
                m_videoEcmInfo.m_pid = DEFAULT_ECM_PID_RANGE_START;
                addCaDescriptor(m_videoInfo.m_dynamicDescriptors, drmSystemId, m_videoEcmInfo.m_pid);
            }
        }
    }

    if (isLogEnabled()) {
        // Determine and set the log stream properties
        m_logInfo.m_streamType = PMT_STREAM_TYPE_PRIVATE;
    }
}

void TsMux::Impl::putCetsEcmPacket(IDataSource &dataSource, StreamInfo &ecmStreamInfo)
{
    uint8_t data[TS_MAX_PAYLOAD_SIZE];
    BitWriter b(data, sizeof(data));

    const std::vector<DecryptInfo> decryptInfo(dataSource.getScramblingParameters());
    if (decryptInfo.empty()) {
        RPLAYER_LOG_WARNING("ECM: No decryption info available");
        return;
    }

    unsigned int numStates = decryptInfo.size();
    if (numStates > 3) {
        numStates = 3;
    }

    b.write(numStates, 2); // num_states (multiple AUs per PES packet not supported)
    b.write(0, 1); // next_key_id_flag (no next key ID)
    b.write(~0, 3); // reserved
    b.write(16, 8); // iv_size (fixed to 16 here)
    b.writeBytes(decryptInfo[0].m_keyIdentifier, 16); // default_key_id

    for (unsigned int i = 0; i < numStates; i++) {
        b.write(((ecmStreamInfo.m_currentScramblingControl + i) % 3) + 1, 2); // transport_scrambling_control
        b.write(1, 6); // num_au (fixed to 1, multiple AUs per PES packet not supported)
        // Loop the following if multiple AUs per PES packet are supported
        bool keyIdFlag = i != 0; // Use key ID for all but the first state (that first uses the default key ID)
        b.write(keyIdFlag, 1); // key_id_flag
        b.write(~0, 3); // reserved
        b.write(0, 4); // au_byte_offset_size (fixed to 0, multiple AUs per PES packet not supported)
        if (keyIdFlag) {
            b.writeBytes(decryptInfo[i].m_keyIdentifier, 16); // key_id
        }
        // au_byte_offset (not written, multiple AUs per PES packet not supported)
        b.writeBytes(decryptInfo[i].m_initializationVector, 16); // initialization_vector
    }

    // next_key_id_flag not set, so no countdown_sec, reserved, next_key_id

    b.close();

    putTsPacketFromData(data, b.getNBytesWritten(), false, ecmStreamInfo, true, TimeStamp(), TimeStamp(), TimeStamp());
}

void TsMux::Impl::putTsPacketFromSource(IDataSource &dataSource, StreamInfo &streamInfo, StreamInfo &ecmStreamInfo, TimeStamp pcr)
{
    TimeStamp pts, dts;
    bool sendPesHeader = dataSource.isNewFrame(pts, dts);

    // Check whether we can send an ECM
    if (sendPesHeader && ecmStreamInfo.isEnabled()) {
        streamInfo.m_currentScramblingControl++;
        streamInfo.m_currentScramblingControl %= 3;
        ecmStreamInfo.m_currentScramblingControl = streamInfo.m_currentScramblingControl;
        putCetsEcmPacket(dataSource, ecmStreamInfo);
    }

    // Send a TS packet full of data (Including PES header and/or PCR if needed)
    uint32_t bytesSent = putTsPacketFromData(dataSource.getData(), dataSource.getBytesAvailable(pcr), dataSource.isDataEncrypted(), streamInfo, sendPesHeader, pts, dts, checkAndGetPcr(streamInfo, pcr));

    dataSource.readBytes(bytesSent);
}

uint32_t TsMux::Impl::putTsPacketFromData(const uint8_t *data, uint32_t size, bool isEncrypted, StreamInfo &streamInfo, bool sendPesHeader, TimeStamp pts, TimeStamp dts, TimeStamp pcr)
{
    // Optimization opportunity: size >= TS_MAX_PAYLOAD_SIZE && !sendPesHeader && !pcrPresent

    if (dts == pts || !pts.isValid()) {
        dts.invalidate();
    }

    uint32_t potentialPayloadSize = size;
    uint8_t pesHeaderDataLength = (pts.isValid() ? 5 : 0) + (dts.isValid() ? 5 : 0);
    if (sendPesHeader) {
        potentialPayloadSize += 9 + pesHeaderDataLength;
    }

    bool payloadPresent = (potentialPayloadSize > 0) || sendPesHeader;
    bool adaptationFieldPresent = pcr.isValid() || (potentialPayloadSize < TS_MAX_PAYLOAD_SIZE);
    int transportScramblingControl = isEncrypted ? streamInfo.m_currentScramblingControl + 1 : 0;

    uint8_t pkt[TS_PACKET_SIZE];
    uint8_t *p = pkt;

    // TS packet header
    (*p++) = TS_SYNC_BYTE;
    (*p++) = (sendPesHeader ? 0x40 : 0x00) | ((streamInfo.m_pid >> 8) & 0x1F);
    (*p++) = streamInfo.m_pid & 0xFF;
    (*p++) = ((transportScramblingControl & 0x03) << 6) | (payloadPresent ? 0x10 : 0x00) | (adaptationFieldPresent ? 0x20 : 0x00) | (streamInfo.m_cc & 0x0F);

    if (payloadPresent) {
        streamInfo.m_cc++;
    }

    // Adaptation field
    if (adaptationFieldPresent) {
        int32_t adaptationFieldLength = pcr.isValid() ? 7 : 0;
        int32_t stuffing = 183 - adaptationFieldLength - potentialPayloadSize;
        if (stuffing < 0) {
            // No stuffing needed
            stuffing = 0;
        }
        adaptationFieldLength += stuffing;
        (*p++) = (uint8_t)adaptationFieldLength;
        if (adaptationFieldLength > 0) {
            (*p++) = (pcr.isValid() ? 0x10 : 0x00) | ((pcr.isValid() && m_pcrDiscontinuity) ? 0x80 : 0x00);
            if (pcr.isValid()) {
                const int64_t pcrBase = pcr.getAs90kHzTicks();
                m_pcrDiscontinuity = false;
                (*p++) = (uint8_t)((pcrBase >> (33 - 8)) & 0xFF);
                (*p++) = (uint8_t)((pcrBase >> (33 - 16)) & 0xFF);
                (*p++) = (uint8_t)((pcrBase >> (33 - 24)) & 0xFF);
                (*p++) = (uint8_t)((pcrBase >> (33 - 32)) & 0xFF);
                (*p++) = 0x7E | (uint8_t)((pcrBase & 0x1) ? 0x80 : 0x00);
                (*p++) = 0x00; // TODO: pcr_clock_reference_extension

                m_pcrOfLastSentPcr = pcr;
            }
            if (adaptationFieldLength == stuffing) {
                stuffing--;
            }
            if (stuffing > 0) {
                memset(p, 0xFF, stuffing);
                p += stuffing;
            }
        }
    }

    // PES packet header
    if (sendPesHeader) {
        uint32_t pesPacketLength = size + 3 + pesHeaderDataLength;
        if ((streamInfo.m_streamId & PES_VIDEO_STREAM_ID.m_mask) == PES_VIDEO_STREAM_ID.m_value) {
            pesPacketLength = 0; // Set to 0 for video stream
        }
        //assert(pesPacketLength < 0x10000); // Must fit in 16 bits
        if (pesPacketLength >= 0x10000) {
            RPLAYER_LOG_ERROR("pesPacketLength=%d, too big for streamId=0x%02X, pid=%d", pesPacketLength, streamInfo.m_streamId, streamInfo.m_pid);
            pesPacketLength = 0;
        }

        (*p++) = 0x00;
        (*p++) = 0x00;
        (*p++) = 0x01;
        (*p++) = streamInfo.m_streamId;
        (*p++) = (uint8_t)((pesPacketLength >> 8) & 0xFF);
        (*p++) = (uint8_t)(pesPacketLength & 0xFF);

        if (streamInfo.m_hasPesSyntax) {
            (*p++) = 0x80;
            (*p++) = (pts.isValid() ? 0x80 : 0x00) | (dts.isValid() ? 0x40 : 0x00);
            (*p++) = pesHeaderDataLength;
            if (pts.isValid()) {
                const int64_t ptsBase = pts.getAs90kHzTicks();
                *(p++) = (uint8_t)((dts.isValid() ? 0x31 : 0x21) | (((ptsBase >> (33 - 3)) & 0x7) << 1));
                *(p++) = (uint8_t)(((ptsBase >> (33 - 11)) & 0xFF));
                *(p++) = (uint8_t)(0x01 | (((ptsBase >> (33 - 18)) & 0x7F) << 1));
                *(p++) = (uint8_t)(((ptsBase >> (33 - 26)) & 0xFF));
                *(p++) = (uint8_t)(0x01 | ((ptsBase & 0x7F) << 1));
                if (dts.isValid()) {
                    const int64_t dtsBase = dts.getAs90kHzTicks();
                    *(p++) = (uint8_t)(0x11 | (((dtsBase >> (33 - 3)) & 0x7) << 1));
                    *(p++) = (uint8_t)(((dtsBase >> (33 - 11)) & 0xFF));
                    *(p++) = (uint8_t)(0x01 | (((dtsBase >> (33 - 18)) & 0x7F) << 1));
                    *(p++) = (uint8_t)(((dtsBase >> (33 - 26)) & 0xFF));
                    *(p++) = (uint8_t)(0x01 | ((dtsBase & 0x7F) << 1));
                }
            }
        }
    }

    assert(p <= pkt + TS_PACKET_SIZE);

    uint32_t payloadSize = TS_PACKET_SIZE - (p - pkt);

    // The memcpy is not good practice, but it saves a double call to the output interface.
    // You win some, you lose some. The good thing is that the output interface gets a little
    // simpler and is called exactly once per packet. No checks are needed for first/second
    // sends and such.
    if (payloadSize) {
        memcpy(p, data, payloadSize);
    }

    // Header & Payload
    if (m_output) {
        m_output->put(pkt, TS_PACKET_SIZE);
    }
    m_packetsSent++;

    return payloadSize;
}

void TsMux::Impl::addTableHeader(uint8_t tableId, uint32_t tableIdExtension, bool privateIndicator, std::vector<uint8_t> &data)
{
    data.push_back(tableId); // table_id
    data.push_back(0xB0 | (privateIndicator ? 0x40 : 0x00)); // section_syntax_indicator(1) + private_indicator(1) + reserved(2) + section_length(4/12) (will be patched in tablesSection())
    data.push_back(0x00); // section_length(8/12) (will be patched in tablesSection())
    data.push_back((tableIdExtension >> 8) & 0xFF); // table_id_extension
    data.push_back(tableIdExtension & 0xFF); // table_id_extension
    data.push_back(0xC1); // reserved(2) + version_number(5) + current_next_indicator(1) (version number will be patched in tablesSection())
    data.push_back(0x00); // section_number
    data.push_back(0x00); // last_section_number
}

void TsMux::Impl::addPatEntry(const StreamInfo &info, int programId, std::vector<uint8_t> &data)
{
    data.push_back((programId >> 8) & 0xFF);
    data.push_back(programId & 0xFF);
    data.push_back(0xE0 | ((info.m_pid >> 8) & 0x1F));
    data.push_back(info.m_pid & 0xFF);
}

void TsMux::Impl::addPmtEntry(const StreamInfo &info, std::vector<uint8_t> &data)
{
    data.push_back(info.m_streamType); // stream_type
    data.push_back(0xE0 | ((info.m_pid >> 8) & 0x1F)); // reserved(3) + elementary_PID(5/13)
    data.push_back(info.m_pid & 0xFF); // elementary_PID(8/13)
    uint32_t esInfoLength = info.m_staticDescriptors.size() + info.m_dynamicDescriptors.size();
    data.push_back(0xF0 | ((esInfoLength >> 8) & 0x0F)); // reserved(4) + ES_info_length(4/12)
    data.push_back(esInfoLength & 0xFF); // ES_info_length(8/12)
    data.insert(data.end(), info.m_staticDescriptors.begin(), info.m_staticDescriptors.end()); // for (...) { descriptor() }
    data.insert(data.end(), info.m_dynamicDescriptors.begin(), info.m_dynamicDescriptors.end()); // for (...) { descriptor() }
}

void TsMux::Impl::putPat()
{
    std::vector<uint8_t> data;

    addTableHeader(PAT_TABLE_ID, m_transportStreamId, false, data);

    if (m_sitInfo.isEnabled()) {
        addPatEntry(m_sitInfo, 0, data);
    }

    addPatEntry(m_pmtInfo, m_programNumber, data);

    tablesSection(m_patInfo, data);
}

void TsMux::Impl::putPmt()
{
    std::vector<uint8_t> data;

    addTableHeader(PMT_TABLE_ID, m_programNumber, false, data);

    m_pcrAndProgramInfo.m_streamType = static_cast<TsProgramMapStreamType>(data.back()); // Hack: PCR PID and program_info don't have the stream_type field so read back last_section_number to rewrite as stream_type
    data.pop_back();
    addPmtEntry(m_pcrAndProgramInfo, data);

    if (isVideoEnabled()) {
        addPmtEntry(m_videoInfo, data);
    }

    if (isAudioEnabled()) {
        addPmtEntry(m_audioInfo, data);
    }

    if (isLogEnabled()) {
        addPmtEntry(m_logInfo, data);
    }

    tablesSection(m_pmtInfo, data);
}

void TsMux::Impl::putSit()
{
    std::vector<uint8_t> data;

    addTableHeader(SIT_TABLE_ID, 0xFFFF, true, data);

    // Was configured in devices.xml as follows:
    //<configitem name="TSTablePid" type="int">31</configitem> -> Will be sitPid parameter later on?
    //<configitem name="TSTableProgram" type="int">0</configitem>
    //<configitem name="TSTableBytes" type="string">7f f0 19 ff ff c1 00 00 f0 0a 63 08 c0 af c8 ff ff ff ff ff 00 01 80 00</configitem>
    // SIT, required for Blu-ray
    //
    // The actual data is f0 0a 63 08 c0 af c8 ff ff ff ff ff 00 01 80 00
    // From doc. peak_rate etc. are 125000 (* 400 bits/s = 50Mbit/s), 37500 (* 400 bits/s = 15Mbit/s) and 0x3FFF (undefined)
    // ActiveVideo chose 45000 (* 400 bits/s = 18Mbit/s), 0x3FFFFF (undefined) and 0x3FFF (undefined)
    uint32_t peakRate = 45000;
    uint32_t minimumOverallSmoothingRate = 0x3FFFFF;
    uint32_t maximumOverallSmoothingBuffer = 0x3FFF;
    uint32_t serviceId = 1;
    data.push_back(0xF0); // reserved(4) + transmission_info_loop_length(4/12)
    data.push_back(0x0A); // transmission_info_loop_length(8/12)
    data.push_back(PARTIAL_TRANSPORT_STREAM_DESCRIPTOR); // descriptor: partial_transport_stream_descriptor
    data.push_back(0x08); // descriptor_length
    data.push_back(0xC0 | ((peakRate >> 16) & 0x3F)); // reserved(2) + peak_rate(6/22)
    data.push_back((peakRate >> 8) & 0xFF); // peak_rate(8/22)
    data.push_back(peakRate & 0xFF); // peak_rate(8/22)
    data.push_back(0xC0 | ((minimumOverallSmoothingRate >> 16) & 0x3F)); // reserved(2) + minimum_overall_smoothing_rate(6/22)
    data.push_back((minimumOverallSmoothingRate >> 8) & 0xFF); // minimum_overall_smoothing_rate(8/22)
    data.push_back(minimumOverallSmoothingRate & 0xFF); // minimum_overall_smoothing_rate(8/22)
    data.push_back(0xC0 | ((maximumOverallSmoothingBuffer >> 8) & 0x3F)); // reserved(2) + maximum_overall_smoothing_buffer(6/14)
    data.push_back(maximumOverallSmoothingBuffer & 0xFF); // maximum_overall_smoothing_buffer(8/14)
    data.push_back((serviceId >> 8) & 0xFF); // service_id(8/16)
    data.push_back(serviceId & 0xFF); // service_id(8/16)
    data.push_back(0x80); // reserved(1) + running_status(3) + service_loop_length(4/12)
    data.push_back(0x00); // service_loop_length(8/12)

    tablesSection(m_sitInfo, data);
}

void TsMux::Impl::tablesSection(StreamInfo &streamInfo, const std::vector<uint8_t> &payload)
{
    uint8_t packet[TS_PACKET_SIZE];

    uint32_t size = payload.size();

    assert(size + 9 < TS_PACKET_SIZE); // If a table spans multiple TS packets, we must change this implementation

    packet[0] = TS_SYNC_BYTE;
    packet[1] = 0x40 | (uint8_t)(streamInfo.m_pid >> 8); // payload_unit_start == 1
    packet[2] = 0x00 | (uint8_t)(streamInfo.m_pid & 0xFF);
    packet[3] = 0x10 | (uint8_t)(streamInfo.m_cc++ & 0x0F); // payload present, no adaptation field
    packet[4] = 0; // pointer field

    uint8_t *payloadStart = packet + 5;
    memcpy(payloadStart, &payload[0], size);

    // Fill in section length (now we know the definite length)
    payloadStart[1] |= (uint8_t)(((size + 1) >> 8) & 0x0F);
    payloadStart[2] |= (uint8_t)((size + 1) & 0xFF);

    // Fill in the version number
    payloadStart[5] |= (uint8_t)((streamInfo.m_tableVersion << 1) & 0x3E); // reserved(2) + version_number(5) + current_next_indicator(1)

    // Compute the CRC
    uint32_t crc = crc32_13818AnnexA(payloadStart, size);

    // Check whether the crc is different from last time.
    // If so, we'll increment the table version number, recompute the CRC
    // and store it. This way, we'll automatically increment the version
    // number any time the table changes.
    // We can change this into version management with marking of dirty
    // contents, but this is more error prone.
    if (crc != streamInfo.m_tableCrc) {
        if (streamInfo.m_tableCrc != 0) { // The first time we don't increment the version
            streamInfo.m_tableVersion++;
            payloadStart[5] |= (uint8_t)((streamInfo.m_tableVersion << 1) & 0x3E); // reserved(2) + version_number(5) + current_next_indicator(1)
        }
        crc = crc32_13818AnnexA(payloadStart, size);
        streamInfo.m_tableCrc = crc; // Store for next time
    }

    // Fill in the CRC
    payloadStart[size + 0] = (uint8_t)((crc >> 24) & 0xFF);
    payloadStart[size + 1] = (uint8_t)((crc >> 16) & 0xFF);
    payloadStart[size + 2] = (uint8_t)((crc >> 8) & 0xFF);
    payloadStart[size + 3] = (uint8_t)(crc & 0xFF);

    memset(packet + size + 9, 0xFF, TS_PACKET_SIZE - size - 9);

    if (m_output) {
        m_output->put(packet, TS_PACKET_SIZE);
    }
    m_packetsSent++;
}
