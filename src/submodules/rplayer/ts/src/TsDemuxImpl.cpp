///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include "TsDemuxImpl.h"

#include <rplayer/IPacketSink.h>
#include <rplayer/ts/DecryptInfo.h>
#include <rplayer/ts/IDataSink.h>
#include <rplayer/ts/IEventSink.h>
#include <rplayer/utils/BitReader.h>
#include <rplayer/utils/Logger.h>

#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <list>
#include <algorithm>

using namespace rplayer;

static const int64_t UNSET_PTS = -1;

TsDemux::TsDemux() :
    m_impl(*new TsDemux::Impl())
{
}

TsDemux::~TsDemux()
{
    delete &m_impl;
}

bool TsDemux::isMatch(const uint8_t *data, uint32_t size)
{
    return size > 0 && data[0] == TS_SYNC_BYTE;
}

class TsDemux::Impl::PsiParser : public TsDemux::Impl::Parser
{
public:
    PsiParser(TsDemux::Impl &owner, int tableId);

    virtual void parse(const uint8_t *data, uint32_t size, bool payloadUnitStartIndicator);
    virtual void reset();

protected:
    TsDemux::Impl &m_owner;

private:
    static const int INVALID_TABLE_VERSION = -1;

    const int m_tableId;
    int m_tableVersion;

    virtual void parseSpecific(const uint8_t *data, uint32_t size) = 0;
};

class TsDemux::Impl::PatPsiParser : public TsDemux::Impl::PsiParser
{
public:
    PatPsiParser(TsDemux::Impl &owner);

private:
    virtual void parseSpecific(const uint8_t *data, uint32_t size);
};

class TsDemux::Impl::PmtPsiParser : public TsDemux::Impl::PsiParser
{
public:
    PmtPsiParser(TsDemux::Impl &owner);

private:
    virtual void parseSpecific(const uint8_t *data, uint32_t size);
    void parseCaDescriptor(const uint8_t *data, uint32_t size, int esPid);
};

class TsDemux::Impl::PesParser : public TsDemux::Impl::Parser
{
public:
    PesParser(IDataSink *dataSink, PesStreamId pesStreamId);
    ~PesParser();

    virtual void parse(const uint8_t *data, uint32_t size, bool payloadUnitStartIndicator);
    virtual void reset();

private:
    IDataSink *m_parser;
    PesStreamId m_pesStreamId;
    int64_t m_lastPts;
    bool m_hasSeenPesHeader;
};

// Sub class of IDataSink parsers that parse ECMs of potentially different systems
// Also implements ICaDecryptor so a connected PES stream can be decrypted
class TsDemux::Impl::CaModule : public IDataSink, public TsDemux::Impl::ICaDecryptor
{
public:
    CaModule(int encryptedStreamPid, IDecryptEngineFactory &decryptEngineFactory);
    virtual ~CaModule();

    // IDataSink implementation, parses ECMs
    void newStream(StreamType, const char *) {} // Not called
    void pesHeader(TimeStamp, TimeStamp, uint32_t) {} // Called but irrelevant
    //void parse(const uint8_t *, uint32_t); // Main parsing of the ECMs
    void reset() {} // Called but irrelevant

    // ICaDecryptor implementation, enables in-place decryption of connected streams
    //bool decrypt(uint8_t *data, uint32_t size, int scramblingControlBits);

    int getEncryptedStreamPid() const { return m_encryptedStreamPid; }

protected:
    void announceKeyIdentifier(const uint8_t (&keyId)[16]);
    void applyDecryptInfo(const DecryptInfo &info);
    bool doDecrypt(uint8_t *data, uint32_t size);

private:
    IDecryptEngineFactory &m_decryptEngineFactory;
    IDecryptEngine *m_decryptEngine;
    const int m_encryptedStreamPid;
};


class TsDemux::Impl::CetsCaModule : public TsDemux::Impl::CaModule
{
public:
    CetsCaModule(int encryptedStreamPid, IDecryptEngineFactory &decryptEngineFactory);
    virtual ~CetsCaModule();

    void parse(const uint8_t *data, uint32_t size);
    bool decrypt(uint8_t *data, uint32_t size, int scramblingControlBits);

private:
    typedef std::list<DecryptInfo> InfoList;

    InfoList m_subStreams[3];
};

TsDemux::Impl::Impl() :
    m_eventOut(0),
    m_videoOut(0),
    m_keyFrameVideoOut(0),
    m_audioOut(0),
    m_packetOut(0),
    m_remainingPacketBytes(0),
    m_audioPid(INVALID_PID),
    m_videoPid(INVALID_PID),
    m_keyFrameVideoPid(INVALID_PID),
    m_pcrPid(INVALID_PID),
    m_latencyDataPid(INVALID_PID)
{
    setupPat();
}

TsDemux::Impl::~Impl()
{
    cleanup();
}

void TsDemux::setPreferredLanguage(const char *language)
{
    if (language && m_impl.m_preferredLanguage.compare(language) != 0) {
        m_impl.m_preferredLanguage = language;

        m_impl.selectElementaryStreams();
    }
}

void TsDemux::reset()
{
    m_impl.m_remainingPacketBytes = 0;

    // Clean up and make sure we'll get a new PAT, PMT & streams
    m_impl.cleanup();
    m_impl.setupPat();
}

void TsDemux::put(const uint8_t *data, uint32_t size)
{
    m_impl.parse(data, size);
}

void TsDemux::setMetaData(const StreamMetaData &metaData)
{
    // Pass any metadata straight downstream if applicable.
    // We don't yet do anything else with it.
    if (m_impl.m_packetOut) {
        m_impl.m_packetOut->setMetaData(metaData);
    }
}

void TsDemux::setEventOutput(IEventSink *eventOut)
{
    m_impl.m_eventOut = eventOut;
    m_impl.m_latencyDataParser.setEventOut(eventOut);
}

void TsDemux::setVideoOutput(IDataSink *videoOut)
{
    m_impl.m_videoOut = videoOut;
}

void TsDemux::setKeyFrameVideoOutput(IDataSink *keyFrameVideoOut)
{
    m_impl.m_keyFrameVideoOut = keyFrameVideoOut;
}

void TsDemux::setAudioOutput(IDataSink *audioOut)
{
    m_impl.m_audioOut = audioOut;
}

void TsDemux::setTsPacketOutput(IPacketSinkWithMetaData *packetOut)
{
    m_impl.m_packetOut = packetOut;
}

void TsDemux::registerDecryptEngineFactory(IDecryptEngineFactory &factory)
{
    m_impl.m_decryptEngineFactories.push_back(&factory);
}

void TsDemux::unregisterDecryptEngineFactory(IDecryptEngineFactory &factory)
{
    std::vector<IDecryptEngineFactory *> &factories(m_impl.m_decryptEngineFactories);
    for (std::vector<IDecryptEngineFactory *>::iterator i = factories.begin(); i != factories.end(); ++i) {
        if (*i == &factory) {
            factories.erase(i);
            break;
        }
    }
}

IDecryptEngineFactory *TsDemux::Impl::findDecryptEngineFactory(const uint8_t (&systemId)[16])
{
    for (std::vector<IDecryptEngineFactory *>::const_iterator i = m_decryptEngineFactories.begin(); i != m_decryptEngineFactories.end(); ++i) {
        IDecryptEngineFactory *entry(*i);

        if (memcmp(systemId, entry->getDrmSystemId(), sizeof(systemId)) == 0) {
            return entry;
        }
    }

    return 0;
}

void TsDemux::Impl::parse(const uint8_t *data, uint32_t size)
{
    //
    // assemble remaining bytes into a packet
    //
    if (m_remainingPacketBytes != 0) {
        uint32_t nToCopy = std::min(size, TS_PACKET_SIZE - m_remainingPacketBytes);
        memcpy(m_packetBuffer + m_remainingPacketBytes, data, nToCopy);
        m_remainingPacketBytes += nToCopy;
        size -= nToCopy;
        data += nToCopy;

        if (m_remainingPacketBytes < static_cast<uint32_t>(TS_PACKET_SIZE)) {
            return;
        }

        assert(m_remainingPacketBytes == static_cast<uint32_t>(TS_PACKET_SIZE));
        const uint8_t *pOut;
        parseTsPacket(m_packetBuffer, pOut);
        if (m_packetOut) {
            m_packetOut->put(pOut, TS_PACKET_SIZE);
        }
        m_remainingPacketBytes = 0;
    }

    //
    // parse middle bytes into packets
    //
    while (size > 0) {
        if (*data != TS_SYNC_BYTE) {
            RPLAYER_LOG_WARNING("No sync byte at expected location: found byte %02X instead of %02X, processing %d bytes", *data, TS_SYNC_BYTE, size);
            while (size > 0 && *data != TS_SYNC_BYTE) {
                data++;
                size--;
            }
            if (size > 0) {
                RPLAYER_LOG_WARNING("Sync found, %d bytes left", size);
            }
        }
        if (size < static_cast<uint32_t>(TS_PACKET_SIZE)) {
            break;
        }
        const uint8_t *pOut;
        parseTsPacket(data, pOut);
        if (m_packetOut) {
            m_packetOut->put(pOut, TS_PACKET_SIZE);
        }
        data += TS_PACKET_SIZE;
        size -= TS_PACKET_SIZE;
    }

    //
    // store remaining bytes that do not fill a packet
    //
    if (size > 0) {
        assert(m_remainingPacketBytes == 0);
        assert(size < static_cast<uint32_t>(TS_PACKET_SIZE));
        assert(*data == TS_SYNC_BYTE);
        memcpy(m_packetBuffer, data, size);
        m_remainingPacketBytes = size;
    }
}

void TsDemux::Impl::parseTsPacket(const uint8_t *packetStart, const uint8_t *&pOut)
{
    // Default output pointer in case of unmodified packet
    pOut = packetStart;

    const uint8_t *data = packetStart;
    assert(data[0] == TS_SYNC_BYTE);
    const bool payloadUnitStartIndicator = (data[1] & 0x40) != 0;
    const int pid = ((data[1] << 8) | data[2]) & 0x1FFF;
    const int transportScramblingControl = (data[3] >> 6) & 3;
    const bool adaptationFieldPresent = (data[3] & 0x20) != 0;
    const int payloadPresent = (data[3] & 0x10) != 0;
    const int continuityCounter = data[3] & 0x0F;

    if (pid == NULL_PACKET_PID) {
        // Null packet, discard
        return;
    }

    data += 4;
    uint32_t size = TS_PACKET_SIZE - 4;
    bool discontinuityIndicator = false;

    if (adaptationFieldPresent) {
        const int adaptationFieldLength = data[0];
        if (adaptationFieldLength > 0) {
            // There is a system time base discontinuity if discontinuityIndicator is true and this PID is a PCR PID (not necessarily containing a PCR).
            // The discontinuity itself starts when the first byte of a packet arrives with a PCR of the new time base.
            // It may be set prior to the first PCR and then will be set up to and including the packet that contains the first PCR of the new time base.
            // At least 2 PCRs shall arrive before the next PCR discontinuity.
            // At both ends of the PCR discontinuity, no PTS or DTS shall arrive that belongs to the other time base.
            //
            // Other packets may have CC discontinuities if discontinuityIndicator is set.
            // Also PSI sections may be discontinuous and the version number should be regarded as initial.
            // Standards-compliant multiplexers shall send a new, empty, PMT, followed by a new, non-empty PMT in that case.
            discontinuityIndicator = (data[1] & 0x80) != 0;

            const bool pcrFlag = (data[1] & 0x10) != 0;

            if (pcrFlag && pid == m_pcrPid) {
                int64_t pcrBase = (data[2] << 25) | (data[3] << 17) | (data[4] << 9) | (data[5] << 1) | (data[6] >> 7);
                int pcrExt = (((data[6] & 1) << 8) | data[7]);

                uint32_t pcrBase1 = pcrBase / 10;
                uint32_t pcrBase2 = pcrBase % 10;
                RPLAYER_LOG_DEBUG("PID=%d pcrBase=%u%u pcrExt=%d, discontinuityIndicator=%d, PCR_PID=%d", pid, pcrBase1, pcrBase2, pcrExt, discontinuityIndicator, m_pcrPid);

                if (m_eventOut) {
                    m_eventOut->pcrReceived(pcrBase, pcrExt, discontinuityIndicator);
                }
            }
        }
        data += adaptationFieldLength + 1;
        size -= adaptationFieldLength + 1;
    }
    if (size > static_cast<uint32_t>(TS_PACKET_SIZE)) { // < 0, unsigned
        RPLAYER_LOG_WARNING("Adaptation field length error");
        return;
    }

    Parser *stream = m_parsers[pid];
    if (!stream) {
        RPLAYER_LOG_DEBUG("No parser found for PID %d", pid);
        return;
    }

    const int expectedContinuityCounter = (stream->m_continuityCounter + (payloadPresent ? 1 : 0)) & 0x0F;
    if (expectedContinuityCounter != continuityCounter && !discontinuityIndicator && !stream->m_discontinuityIndicator) {
        // Continuity Counter error on pid
        // For mediasource, seeks will lead to CC errors. Because of this, the log level has been reduced to debug.
        RPLAYER_LOG_DEBUG("CC error: %d, expected %d (PID=%d)", continuityCounter, expectedContinuityCounter, pid);
    }

    stream->m_continuityCounter = continuityCounter;
    stream->m_discontinuityIndicator = false;

    if (transportScramblingControl != 0) {
        bool success = false;

        if (stream->m_caDecryptor) {
            // If we need to modify the packet and it does not already reside in the packet buffer,
            // copy it and set the output pointer accordingly.
            if (packetStart != m_packetBuffer) {
                memcpy(m_packetBuffer, packetStart, TS_PACKET_SIZE);
                pOut = m_packetBuffer;
            }

            // Let the data pointer point to the modifiable buffer and decrypt in-place.
            data = &m_packetBuffer[data - packetStart];
            success = stream->m_caDecryptor->decrypt(const_cast<uint8_t *>(data), size, transportScramblingControl);

            if (success) {
                // Reset the scrambling control bits in-place, as to signal a clear stream
                m_packetBuffer[3] &= ~0xC0;
            }
        }

        if (!success) {
            RPLAYER_LOG_WARNING("Transport descrambling failed, control bits=%d", transportScramblingControl);
            return;
        }
    }

    stream->parse(data, size, payloadUnitStartIndicator);
}

void TsDemux::Impl::cleanup()
{
    for (std::map<int, Parser *>::iterator i = m_parsers.begin(); i != m_parsers.end(); ++i) {
        delete i->second;
    }
    m_parsers.clear();
    m_audioPid = INVALID_PID;
    m_videoPid = INVALID_PID;
    m_keyFrameVideoPid = INVALID_PID;
    m_pcrPid = INVALID_PID;
    m_latencyDataPid = INVALID_PID;

    clearElementaryStreamInfo();
    clearCaModules();
}

bool TsDemux::hasAudio() const
{
    return m_impl.m_audioPid != INVALID_PID;
}

bool TsDemux::hasVideo() const
{
    return m_impl.m_videoPid != INVALID_PID;
}

bool TsDemux::hasKeyFrameVideo() const
{
    return m_impl.m_keyFrameVideoPid != INVALID_PID;
}

void TsDemux::Impl::setupPat()
{
    assert(!m_parsers[PAT_PID]);
    m_parsers[PAT_PID] = new PatPsiParser(*this);
}

TsDemux::Impl::PsiParser::PsiParser(TsDemux::Impl &owner, int tableId) :
    m_owner(owner),
    m_tableId(tableId),
    m_tableVersion(INVALID_TABLE_VERSION)
{
}

void TsDemux::Impl::PsiParser::parse(const uint8_t *data, uint32_t size, bool payloadUnitStartIndicator)
{
    if (payloadUnitStartIndicator) {
        const int pointerField = data[0];
        data += pointerField + 1;
        size -= pointerField + 1;
    }
    if (size > static_cast<uint32_t>(TS_PACKET_SIZE)) { // < 0, unsigned
        RPLAYER_LOG_WARNING("Pointer field length error");
        return;
    }

    if (!payloadUnitStartIndicator) {
        RPLAYER_LOG_WARNING("Sections spanning multiple packets is not supported"); // If not set, we need to concatenate sections, which is currently not supported
        return;
    }

    if (size < 3) {
        RPLAYER_LOG_WARNING("Not enough data for table");
        return;
    }

    const int tableId = data[0];
    const bool sectionSyntaxIndicator = (data[1] & 0x80) != 0;
    const unsigned int sectionLength = ((data[1] << 8) | data[2]) & 0xFFF;

    data += 3;
    size -= 3;

    if (sectionLength > size) {
        RPLAYER_LOG_WARNING("Table section length does not fit the data size: %u vs. %u", sectionLength, size); // True if we cannot concatenate sections
        return;
    }

    if (tableId != m_tableId) {
        RPLAYER_LOG_WARNING("Received unexpected table ID: %d vs. %d", tableId, m_tableId);
        return;
    }

    if (sectionSyntaxIndicator) {
        if (sectionLength < 9) {
            RPLAYER_LOG_WARNING("Table section length too small");
            return;
        }

        //const int tableIdExtension = (data[0] << 8) | data[1];
        const int versionNumber = (data[2] >> 1) & 0x1F;
        const bool currentNextIndicator = (data[2] & 0x01) != 0;
        const int sectionNumber = data[3];
        const int lastSectionNumber = data[4];
        const uint32_t crc = (data[sectionLength - 4] << 24) | (data[sectionLength - 3] << 16) | (data[sectionLength - 2] << 8) | data[sectionLength - 1];

        // Compute the CRC
        // We could include the CRC itself and compare for 0, but not including it will improve error logging
        uint32_t computedCrc = crc32_13818AnnexA(data - 3, sectionLength + 3 - 4);
        if (crc != computedCrc) {
            RPLAYER_LOG_WARNING("Table CRC error, got %08X, computed %08X", crc, computedCrc);
            return;
        }

        if (!currentNextIndicator) {
            RPLAYER_LOG_DEBUG("Skipping 'next'");
            return; // Ignore 'next' tables
        }
        if (sectionNumber != 0 || lastSectionNumber != 0) {
            RPLAYER_LOG_WARNING("Table spanning multiple sections is not supported");
            return;
        }
        if (versionNumber == m_tableVersion) {
            return; // Ignore repeated tables
        }

        m_tableVersion = versionNumber;

        // In order to enable seeking when in case of mediasource streams, Virga first
        // seeks the stream and then increments PAT and PMT version numbers. This is
        // to signal the compositor that the stream is disrupted and will continue at
        // a new location. Report this change back to the fragment.
        if (m_owner.m_eventOut) {
            m_owner.m_eventOut->tableVersionUpdate(m_tableId, m_tableVersion);
        }

        parseSpecific(data + 5, sectionLength - 9);
    } else {
        parseSpecific(data, sectionLength);
    }
}

void TsDemux::Impl::PsiParser::reset()
{
    m_discontinuityIndicator = true;
}

TsDemux::Impl::PatPsiParser::PatPsiParser(TsDemux::Impl &owner) :
    PsiParser(owner, PAT_TABLE_ID)
{
}

void TsDemux::Impl::PatPsiParser::parseSpecific(const uint8_t *data, uint32_t size)
{
    for (unsigned int i = 0; i < size; i += 4) {
        int programNumber = (data[i] << 8) | data[i + 1];
        int pid = ((data[i + 2] << 8) | data[i + 3]) & 0x1FFF;
        if (programNumber != 0) {
            m_owner.setPmt(pid);
            break;
        }
    }
}

TsDemux::Impl::PmtPsiParser::PmtPsiParser(TsDemux::Impl &owner) :
    PsiParser(owner, PMT_TABLE_ID)
{
}

void TsDemux::Impl::PmtPsiParser::parseCaDescriptor(const uint8_t *data, uint32_t size, int esPid)
{
    BitReader b(data, size, 0);
    uint16_t caSystemId = b.read(16);
    b.skip(3);
    uint16_t caPid = b.read(13);
    if (caSystemId == CETS_CA_SYSTEM_ID) {
        uint32_t schemeType = b.read(32);
        uint32_t schemeVersion = b.read(32);
        uint8_t numSystems = b.read(8);
        uint32_t encryptionAlgorithm = b.read(24);

        RPLAYER_LOG_INFO("CA Descriptor: caSystemId=%04X, caPid=%d, esPid=%d, schemeType=0x%08X, schemeVersion=0x%08X, numSystems=%d, encryptionAlgorithm=%06X",
            caSystemId, caPid, esPid, schemeType, schemeVersion, numSystems, encryptionAlgorithm);

        if (schemeType != SCHM_SCHEME_TYPE) {
            RPLAYER_LOG_WARNING("CA Descriptor: Unknown scheme type (schm.scheme_type): 0x%08X", schemeType);
            return;
        }
        if (schemeVersion != SCHM_SCHEME_VERSION) {
            RPLAYER_LOG_WARNING("CA Descriptor: Unknown scheme version (schm.scheme_version): 0x%08X", schemeVersion);
            return;
        }
        if (encryptionAlgorithm != 0 && encryptionAlgorithm != 1) {
            RPLAYER_LOG_WARNING("CA Descriptor: Unknown encryptionAlgorithm (tenc.IsEncrypted): 0x%06X", encryptionAlgorithm);
            return;
        }

        IDecryptEngineFactory *decryptEngineFactory(0);
        for (uint8_t i = 0; i < numSystems; i++) {
            uint8_t systemId[16];
            b.readBytes(systemId, sizeof(systemId));
            b.read(13); // pssh_pid, unused
            b.skip(3);

            // Take the first matching DRM system ID
            if (!decryptEngineFactory) {
                decryptEngineFactory = m_owner.findDecryptEngineFactory(systemId);
                if (!decryptEngineFactory) {
                    RPLAYER_LOG_DEBUG("CA Descriptor: Unsupported DRM system ID: 0x%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
                        systemId[0], systemId[1], systemId[2], systemId[3], systemId[4], systemId[5], systemId[6], systemId[7],
                        systemId[8], systemId[9], systemId[10], systemId[11], systemId[12], systemId[13], systemId[14], systemId[15]);
                }
            }
        }

        if (decryptEngineFactory) {
            m_owner.addEcmStream(caPid, esPid, *decryptEngineFactory);
        } else {
            RPLAYER_LOG_WARNING("CA Descriptor: No matching DRM system ID found");
        }
    } else {
        RPLAYER_LOG_ERROR("CA Descriptor: Unknown CA system: caSystemId=%04X, caPid=%d, private size=%d, esPid=%d", caSystemId, caPid, size, esPid);
        while (b.getNBitsAvailable() >= 8) {
            RPLAYER_LOG_DEBUG("0x%02X", b.read(8));
        }
    }
}

void TsDemux::Impl::PmtPsiParser::parseSpecific(const uint8_t *data, uint32_t size)
{
    m_owner.m_pcrPid = ((data[0] << 8) | data[1]) & 0x1FFF;
    int programInfoLength = ((data[2] << 8) | data[3]) & 0x0FFF;
    const uint8_t *programInfo = data + 4;
    data += programInfoLength + 4;
    size -= programInfoLength + 4;
    if (size > static_cast<uint32_t>(TS_PACKET_SIZE)) { // < 0, unsigned
        RPLAYER_LOG_WARNING("Program info length error");
        return;
    }

    // Reset the elementary stream info table inside the TS parser
    m_owner.clearElementaryStreamInfo();

    // Reset the CA modules inside the TS parser
    m_owner.clearCaModules();

    // Parse the descriptors and extract the relevant program parameters
    for (int j = 0; j < programInfoLength; /*intended*/) {
        int descriptorTag = programInfo[j];
        int descriptorLength = programInfo[j + 1];
        if (descriptorTag == CA_DESCRIPTOR) {
            parseCaDescriptor(programInfo + j + 2, descriptorLength, INVALID_PID);
        }
        j += 2 + descriptorLength;
    }

    // Find the streams we need from the PMT
    for (unsigned int i = 0; i < size; i += 5) {
        // Extract PMT entry
        bool is_valid_stream = true;
        TsProgramMapStreamType streamType = static_cast<TsProgramMapStreamType>(data[i]);
        int elementaryPid = ((data[i + 1] << 8) | data[i + 2]) & 0x1FFF;
        int esInfoLength = ((data[i + 3] << 8) | data[i + 4]) & 0x0FFF;
        const uint8_t *descriptors = data + i + 5;
        i += esInfoLength;

        // Parse the descriptors and extract the relevant stream parameters
        bool isKeyFrameStream = false;
        std::string language;
        for (int j = 0; j < esInfoLength; /*intended*/) {
            int descriptorTag = descriptors[j];
            int descriptorLength = descriptors[j + 1];
            const char *descriptor_as_string = reinterpret_cast<const char *>(descriptors + j + 2);
            switch (descriptorTag) {
            case AC3_DESCRIPTOR:
                // Expected with descriptorTag == PMT_STREAM_TYPE_AC3_AUDIO
                if (streamType != PMT_STREAM_TYPE_AC3_AUDIO) {
                    RPLAYER_LOG_WARNING("AC-3 descriptor found with non-AC-3 stream");
                }
                break;
            case ISO_639_LANGUAGE_DESCRIPTOR:
                if (descriptorLength > 0) {
                    language.assign(descriptor_as_string, descriptorLength - 1);
                }
                break;
            case CA_DESCRIPTOR:
                parseCaDescriptor(descriptors + j + 2, descriptorLength, elementaryPid);
                break;
            case KEYFRAME_DESCRIPTOR:
                if (descriptorLength == KEYFRAME_DESCRIPTOR_STRING_LENGTH && strncmp(descriptor_as_string, KEYFRAME_DESCRIPTOR_STRING, descriptorLength) == 0) {
                    isKeyFrameStream = true;
                }
                break;
            case LATENCY_DATA_DESCRIPTOR_TAG:
                if (streamType == PMT_STREAM_TYPE_LATENCY_DATA) {
                    if (descriptorLength != LATENCY_DATA_DESCRIPTOR_STRING_LENGTH || strncmp(descriptor_as_string, LATENCY_DATA_DESCRIPTOR_STRING, descriptorLength) != 0) {
                        // Invalid descriptor for the stream latency data
                        RPLAYER_LOG_WARNING("PMT_STREAM_TYPE_LATENCY_DATA has the wrong descriptor");
                        is_valid_stream = false;
                    }
                }
                break;
            default:
                break;
            }
            j += 2 + descriptorLength;
        }

        if (is_valid_stream) {
            // Add the elementary stream info to the elementary stream info table inside the TS parser
            m_owner.addElementaryStreamInfo(streamType, elementaryPid, language, isKeyFrameStream);
        }
    }

    // Select the proper elementary streams based on the info of the current PMT
    m_owner.selectElementaryStreams();
}

TsDemux::Impl::PesParser::PesParser(IDataSink *parser, PesStreamId pesStreamId) :
    m_parser(parser),
    m_pesStreamId(pesStreamId),
    m_lastPts(UNSET_PTS),
    m_hasSeenPesHeader(false)
{
}

TsDemux::Impl::PesParser::~PesParser()
{
    reset();
}

void TsDemux::Impl::PesParser::parse(const uint8_t *data, uint32_t size, bool payloadUnitStartIndicator)
{
    if (payloadUnitStartIndicator) {
        if (size < 7) {
            RPLAYER_LOG_WARNING("Data underflow");
            return;
        }
        if (!(data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x01)) { // Start code
            RPLAYER_LOG_WARNING("PES start code missing");
            return;
        }

        int streamId = data[3];
        uint16_t pesPacketLength = (data[4] << 8) | data[5];
        if ((streamId & m_pesStreamId.m_mask) != m_pesStreamId.m_value) {
            RPLAYER_LOG_WARNING("Unrecognized PES stream ID: %02X", streamId);
            return;
        }

        // Skip now-read generic PES header
        data += 6;
        size -= 6;

        // Recognized PES stream ID
        if (m_pesStreamId.m_hasPesSyntax) {
            if (size < 3) {
                RPLAYER_LOG_WARNING("Data underflow");
                return;
            }

            // Read the fixed part of the standardized PES header
            uint8_t pesFlags1 = data[0];
            uint8_t pesFlags2 = data[1];
            uint32_t headerSize = 3 + data[2]; // Header size including these 3 bytes...
            uint32_t pesPayloadLength = pesPacketLength >= headerSize ? pesPacketLength - headerSize : 0;

            // PES packet complies with standard syntax
            if ((pesFlags1 & 0xC0) != 0x80) {
                RPLAYER_LOG_WARNING("PES contents should start with bits '10'");
                return;
            }

            int pesScramblingControl = (pesFlags1 >> 4) & 3;
            if (pesScramblingControl != 0) {
                RPLAYER_LOG_WARNING("PES scrambling enabled: %d", pesScramblingControl);
            }

            if (size < headerSize) {
                RPLAYER_LOG_WARNING("Data underflow");
                return;
            }

            // Get pointers to header area
            const uint8_t *headerPtr = data + 3;
            const uint8_t *headerEnd = data + headerSize;

            TimeStamp pts;
            TimeStamp dts;

            if (pesFlags2 & 0x80) { // PTS
                if (headerPtr + 5 > headerEnd) {
                    RPLAYER_LOG_WARNING("Data underflow");
                    return;
                }
                int64_t pts90kHz = (static_cast<int64_t>(headerPtr[0] & 0x0E) << 29) | (headerPtr[1] << 22) | ((headerPtr[2] & 0xFE) << 14) | (headerPtr[3] << 7) | (headerPtr[4] >> 1);
                headerPtr += 5;

                if (m_lastPts == UNSET_PTS) {
                    // FIXME: this still has a chance to fail syncing audio and video
                    m_lastPts = 0;
                }
                pts90kHz = m_lastPts + (((pts90kHz - m_lastPts) << 31) >> 31); // Sign extend the difference and add, this takes care of PTS wraparounds

                m_lastPts = pts90kHz;
                pts.setAs90kHzTicks(pts90kHz);

                if ((pesFlags2 & 0xC0) == 0xC0) { // PTS and DTS
                    if (headerPtr + 5 > headerEnd) {
                        RPLAYER_LOG_WARNING("Data underflow");
                        return;
                    }
                    int64_t dts90kHz = (static_cast<int64_t>(headerPtr[0] & 0x0E) << 29) | (headerPtr[1] << 22) | ((headerPtr[2] & 0xFE) << 14) | (headerPtr[3] << 7) | (headerPtr[4] >> 1);
                    headerPtr += 5;

                    dts90kHz = pts90kHz + (((dts90kHz - pts90kHz) << 31) >> 31); // Sign extend the difference and add, this takes care of PTS/DTS wraparounds
                    dts.setAs90kHzTicks(dts90kHz);
                }
            }

            if (pesFlags2 & 0x01) { // PES_extension_flag
                if ((pesFlags2 & 0x3E) != 0) { // Either one of ESCR_flag, ES_rate_flag, DSM_trick_mode_flag, additional_copy_info_flag or PES_CRC_flag set
                    // We will need to read these fields first before processing the PES_extension header
                    // This is not yet supported.
                    RPLAYER_LOG_WARNING("Can't process PES_extension");
                } else {
                    if (headerPtr + 1 > headerEnd) {
                        RPLAYER_LOG_WARNING("Data underflow");
                        return;
                    }
                    uint8_t pesFlags3 = headerPtr[0];
                    headerPtr += 1;

                    if (pesFlags3 & 0x80) { // PES_private_data_flag
                        // Read 128 bits (16 bytes) of PES_private_data
                        // The first 4 bytes should be 'AVNL' to indicate that it's 'our' extension field.
                        // The next 4 bytes indicate the PES payload length.
                        // The remaining 8 bytes are filled with 0xFF.
                        // This is an alternative way to indicate the payload length in case the PES packet
                        // length doesn't fit 16 bits. But even in case of large video frames we want to
                        // pass the frame length so we know asap when a full frame has been received. This
                        // will reduce latency if frames are accumulated before they are sent to the decoder.
                        if (headerPtr + 16 > headerEnd) {
                            RPLAYER_LOG_WARNING("Data underflow");
                            return;
                        }

                        if (headerPtr[0] == 'A' && headerPtr[1] == 'V' && headerPtr[2] == 'N' && headerPtr[3] == 'L') {
                            pesPayloadLength = static_cast<uint32_t>(headerPtr[4]) << 24 | headerPtr[5] << 16 | headerPtr[6] << 8 | headerPtr[7];
                            if (pesPacketLength != 0) {
                                RPLAYER_LOG_WARNING("Expected pesPacketLength (%u) to be 0", pesPacketLength);
                            }
                            RPLAYER_LOG_DEBUG("PES_extension pesPayloadLength: %u (pesPacketLength=%u, headerSize=%u, size=%u)", pesPayloadLength, pesPacketLength, headerSize, size);
                        } else {
                            RPLAYER_LOG_WARNING("PES private data not recognized");
                        }
                    }

                    // Any other PES header data is silently discarded
                }
            }

            m_hasSeenPesHeader = true;

            if (m_parser) {
                m_parser->pesHeader(pts, dts, pesPayloadLength);
                m_parser->parse(data + headerSize, size - headerSize);
            }
        } else {
            // Stream without further MPEG2TS-defined syntax
            if (pesPacketLength < size) {
                size = pesPacketLength;
            }
            m_hasSeenPesHeader = true;
            if (m_parser) {
                m_parser->pesHeader(TimeStamp(), TimeStamp(), pesPacketLength);
                m_parser->parse(data, size);
            }
        }
    } else if (m_hasSeenPesHeader) { // Only process data that has seen a proper PES header
        if (m_parser) {
            m_parser->parse(data, size);
        }
    }
}

void TsDemux::Impl::PesParser::reset()
{
    m_discontinuityIndicator = true;
    m_lastPts = UNSET_PTS;
    m_hasSeenPesHeader = false;
    if (m_parser) {
        m_parser->reset();
    }
}

void TsDemux::Impl::setPmt(int pmtPid)
{
    Parser *patParser = m_parsers[PAT_PID]; // We keep the PAT parser in order to keep its state
    m_parsers.erase(PAT_PID);

    cleanup();

    assert(!m_parsers[PAT_PID]);
    assert(patParser);
    m_parsers[PAT_PID] = patParser;

    if (m_parsers[pmtPid]) {
        // Apparently pmtPid == PAT_PID...
        RPLAYER_LOG_ERROR("PMT PID conflicts with PAT PID: %d", pmtPid);
        return;
    }

    m_parsers[pmtPid] = new PmtPsiParser(*this);
}

void TsDemux::Impl::addPesParser(int elementaryPid, IDataSink *dataSink, PesStreamId streamId)
{
    if (m_parsers[elementaryPid]) {
        delete m_parsers[elementaryPid]; // In case there already was a parser present, which would be wrong...
        RPLAYER_LOG_WARNING("Duplicate stream PID encountered: %d", elementaryPid);
    }
    m_parsers[elementaryPid] = new PesParser(dataSink, streamId);
}

void TsDemux::Impl::removeParser(int elementaryPid)
{
    if (elementaryPid != INVALID_PID && m_parsers[elementaryPid]) {
        delete m_parsers[elementaryPid];
        m_parsers[elementaryPid] = 0;
    }
}

void TsDemux::Impl::addAudioStream(TsProgramMapStreamType streamType, int elementaryPid, const std::string &language)
{
    StreamType sinkStreamType = STREAM_TYPE_UNKNOWN;

    switch (streamType) {
    case PMT_STREAM_TYPE_MPEG1_AUDIO:
        sinkStreamType = STREAM_TYPE_MPEG1_AUDIO;
        break;
    case PMT_STREAM_TYPE_MPEG2_AUDIO:
        sinkStreamType = STREAM_TYPE_MPEG2_AUDIO;
        break;
    case PMT_STREAM_TYPE_AAC_AUDIO:
        sinkStreamType = STREAM_TYPE_AAC_AUDIO;
        break;
    case PMT_STREAM_TYPE_AC3_AUDIO:
        sinkStreamType = STREAM_TYPE_AC3_AUDIO;
        break;
    default:
        RPLAYER_LOG_WARNING("Unknown audio stream type in PMT: %d, pid=%d", streamType, elementaryPid);
        break;
    }

    m_audioPid = elementaryPid;
    if (m_audioOut) {
        // Only add a PES parser if there is a data sink to send the output to.
        m_audioOut->newStream(sinkStreamType, language.c_str());
    }
    addPesParser(elementaryPid, m_audioOut, streamType == PMT_STREAM_TYPE_AC3_AUDIO ? PES_PRIVATE1_STREAM_ID : PES_AUDIO_STREAM_ID);
}

void TsDemux::Impl::addVideoStream(TsProgramMapStreamType streamType, int elementaryPid, bool isKeyFrameStream)
{
    IDataSink *videoOut(isKeyFrameStream ? m_keyFrameVideoOut : m_videoOut);
    int &videoPid(isKeyFrameStream ? m_keyFrameVideoPid : m_videoPid);

    StreamType sinkStreamType = STREAM_TYPE_UNKNOWN;

    switch (streamType) {
    case PMT_STREAM_TYPE_MPEG2_VIDEO:
        sinkStreamType = STREAM_TYPE_MPEG2_VIDEO;
        break;
    case PMT_STREAM_TYPE_H264_VIDEO:
        sinkStreamType = STREAM_TYPE_H264_VIDEO;
        break;
    default:
        RPLAYER_LOG_WARNING("Unknown video stream type in PMT: %d, pid=%d", streamType, elementaryPid);
        break;
    }

    videoPid = elementaryPid;
    if (videoOut) {
        // Only add a PES parser if there is a data sink to send the output to.
        videoOut->newStream(sinkStreamType, 0);
    }
    addPesParser(elementaryPid, videoOut, PES_VIDEO_STREAM_ID);
}

void TsDemux::Impl::addEcmStream(int elementaryPid, int encryptedStreamPid, IDecryptEngineFactory &decryptEngineFactory)
{
    TsDemux::Impl::CaModule *caModule = new TsDemux::Impl::CetsCaModule(encryptedStreamPid, decryptEngineFactory);
    m_caModules.push_back(caModule);
    addPesParser(elementaryPid, caModule, PES_ECM_STREAM_ID);
}

void TsDemux::Impl::addLatencyStream(int elementaryPid)
{
    m_latencyDataPid = elementaryPid;
    addPesParser(elementaryPid, &m_latencyDataParser, PES_PRIVATE1_STREAM_ID);
}

void TsDemux::Impl::clearCaModules()
{
    for (std::vector<CaModule *>::iterator i = m_caModules.begin(); i != m_caModules.end(); ++i) {
        delete *i;
    }
    m_caModules.clear();
}

void TsDemux::Impl::clearElementaryStreamInfo()
{
    // Clear the elementary stream info table
    m_streams.clear();
}

void TsDemux::Impl::addElementaryStreamInfo(TsProgramMapStreamType streamType, int elementaryPid, const std::string &language, bool isKeyFrameStream)
{
    // Add an elementary stream to the elementary stream info table
    m_streams.push_back(StreamInfo(streamType, elementaryPid, language, isKeyFrameStream));
}

void TsDemux::Impl::selectElementaryStreams()
{
    // Select audio, video and other elementary streams to demux, based upon
    // the data gathered from the PMT and run through a selection mechanism.
    // This function may be repeatedly called if either the streams or a selection
    // criterion changes.
    // Basically, it selects the proper elementary streams from the elementary
    // stream info table

    // Initialize the streams we need to 'invalid'
    int audioPid = INVALID_PID;
    int videoPid = INVALID_PID;
    int keyFrameVideoPid = INVALID_PID;
    int latencyDataPid = INVALID_PID;
    TsProgramMapStreamType audioType = PMT_STREAM_TYPE_RESERVED;
    TsProgramMapStreamType videoType = PMT_STREAM_TYPE_RESERVED;
    TsProgramMapStreamType keyFrameVideoType = PMT_STREAM_TYPE_RESERVED;
    std::string selectedLanguage;

    // Find the streams we need from the table
    for (std::vector<StreamInfo>::iterator i = m_streams.begin(); i != m_streams.end(); ++i) {
        StreamInfo &streamInfo(*i);

        // Check whether we want to use the stream
        switch (streamInfo.m_streamType) {
        case PMT_STREAM_TYPE_MPEG2_VIDEO:
        case PMT_STREAM_TYPE_H264_VIDEO:
            if (streamInfo.m_isKeyFrameStream) {
                // Use the first from the list
                if (keyFrameVideoPid == INVALID_PID) {
                    keyFrameVideoPid = streamInfo.m_elementaryPid;
                    keyFrameVideoType = streamInfo.m_streamType;
                }
            } else {
                // Use the first from the list
                if (videoPid == INVALID_PID) {
                    videoPid = streamInfo.m_elementaryPid;
                    videoType = streamInfo.m_streamType;
                }
            }
            break;
        case PMT_STREAM_TYPE_MPEG1_AUDIO:
        case PMT_STREAM_TYPE_MPEG2_AUDIO:
        case PMT_STREAM_TYPE_AAC_AUDIO:
        case PMT_STREAM_TYPE_AC3_AUDIO:
            // Use the first from the list but change as soon as the language matches
            if (audioPid == INVALID_PID || (!m_preferredLanguage.empty() && m_preferredLanguage.find(streamInfo.m_language) != std::string::npos)) {
                audioPid = streamInfo.m_elementaryPid;
                audioType = streamInfo.m_streamType;
                selectedLanguage = streamInfo.m_language;
            }
            break;
        case PMT_STREAM_TYPE_LATENCY_DATA:
            if (latencyDataPid == INVALID_PID) {
                latencyDataPid = streamInfo.m_elementaryPid;
            }
            break;
        default:
            RPLAYER_LOG_WARNING("Unknown stream type in PMT: %d, pid=%d", streamInfo.m_streamType, streamInfo.m_elementaryPid);
            break;
        }
    }

    // Apply the streams found if they're new
    if (audioPid != m_audioPid) {
        removeParser(m_audioPid);
        if (audioPid != INVALID_PID) {
            addAudioStream(audioType, audioPid, selectedLanguage);
        }
    }
    if (videoPid != m_videoPid) {
        removeParser(m_videoPid);
        if (videoPid != INVALID_PID) {
            addVideoStream(videoType, videoPid, false);
        }
    }
    if (keyFrameVideoPid != m_keyFrameVideoPid) {
        removeParser(m_keyFrameVideoPid);
        if (keyFrameVideoPid != INVALID_PID) {
            addVideoStream(keyFrameVideoType, keyFrameVideoPid, true);
        }
    }
    if (latencyDataPid != m_latencyDataPid) {
        removeParser(m_latencyDataPid);
        if (latencyDataPid != INVALID_PID) {
            addLatencyStream(latencyDataPid);
        }
    }

    // Connect all CaModules to their respective streams
    for (std::vector<CaModule *>::iterator i = m_caModules.begin(); i != m_caModules.end(); ++i) {
        CaModule *module(*i);

        int pid = module->getEncryptedStreamPid();
        if (pid == INVALID_PID) { // All streams
            // TODO: Actually, this now regards all streams as a single stream as the
            // CaModule is shared, and so is its state. This is probably wrong.
            if (m_audioPid != INVALID_PID) {
                m_parsers[m_audioPid]->m_caDecryptor = module;
            }
            if (m_videoPid != INVALID_PID) {
                m_parsers[m_videoPid]->m_caDecryptor = module;
            }
            if (m_keyFrameVideoPid != INVALID_PID) {
                m_parsers[m_keyFrameVideoPid]->m_caDecryptor = module;
            }
        } else {
            if (!m_parsers[pid]) {
                RPLAYER_LOG_WARNING("CA encrypted stream not found: %d", pid);
            } else {
                m_parsers[pid]->m_caDecryptor = module;
            }
        }
    }
}

TsDemux::Impl::CaModule::CaModule(int encryptedStreamPid, IDecryptEngineFactory &decryptEngineFactory) :
    m_decryptEngineFactory(decryptEngineFactory),
    m_decryptEngine(decryptEngineFactory.createDecryptEngine()),
    m_encryptedStreamPid(encryptedStreamPid)
{
}

TsDemux::Impl::CaModule::~CaModule()
{
    m_decryptEngineFactory.destroyDecryptEngine(m_decryptEngine);
}

void TsDemux::Impl::CaModule::announceKeyIdentifier(const uint8_t (&keyId)[16])
{
    if (m_decryptEngine) {
        m_decryptEngine->announceKeyIdentifier(keyId);
    }
}

void TsDemux::Impl::CaModule::applyDecryptInfo(const DecryptInfo &info)
{
    if (m_decryptEngine) {
        // Set a new key, if needed
        m_decryptEngine->setKeyIdentifier(info.m_keyIdentifier);
        // Apply the initialization vector
        m_decryptEngine->setInitializationVector(info.m_initializationVector);
    }
}

bool TsDemux::Impl::CaModule::doDecrypt(uint8_t *data, uint32_t size)
{
    if (m_decryptEngine) {
        return m_decryptEngine->decrypt(data, size);
    }

    return false;
}

TsDemux::Impl::CetsCaModule::CetsCaModule(int encryptedStreamPid, IDecryptEngineFactory &decryptEngineFactory) :
    CaModule(encryptedStreamPid, decryptEngineFactory)
{
}

TsDemux::Impl::CetsCaModule::~CetsCaModule()
{
}

void TsDemux::Impl::CetsCaModule::parse(const uint8_t *data, uint32_t size)
{
    BitReader b(data, size, 0);

    int numStates = b.read(2);
    int nextKeyIdFlag = b.read(1);
    b.skip(3);
    unsigned int ivSize = b.read(8);
    uint8_t defaultKeyId[16];
    b.readBytes(defaultKeyId, sizeof(defaultKeyId));
    announceKeyIdentifier(defaultKeyId);

    RPLAYER_LOG_DEBUG("ECM: numStates=%d, nextKeyIdFlag=%d, ivSize=%d", numStates, nextKeyIdFlag, ivSize);
    if (ivSize != 8 && ivSize != 16) {
        RPLAYER_LOG_WARNING("ECM: Illegal initialization vector size: %d", ivSize);
        return;
    }

    for (int i = 0; i < numStates; i++) {
        int transportScramblingControl = b.read(2);
        int numAu = b.read(6);

        RPLAYER_LOG_DEBUG("ECM: state=%d, transportScramblingControl=%d, numAu=%d", i, transportScramblingControl, numAu);
        if (transportScramblingControl == 0) {
            RPLAYER_LOG_WARNING("ECM: transportScramblingControl bits are 00");
            return;
        }

        InfoList &streamInfoList(m_subStreams[transportScramblingControl - 1]);
        streamInfoList.clear();
        for (int j = 0; j < numAu; j++) {
            streamInfoList.push_back(DecryptInfo());
            DecryptInfo &info(streamInfoList.back());

            int keyIdFlag = b.read(1);
            b.skip(3);
            int auByteOffsetSize = b.read(4);
            if (keyIdFlag) {
                // We should send key requests to abroad and be able to
                // apply a callback mechanism to set the retreived key
                b.readBytes(info.m_keyIdentifier, sizeof(info.m_keyIdentifier));
                announceKeyIdentifier(info.m_keyIdentifier);
            } else {
                memcpy(info.m_keyIdentifier, defaultKeyId, sizeof(info.m_keyIdentifier));
            }
            if (auByteOffsetSize > 0) {
                if (auByteOffsetSize > 4) {
                    RPLAYER_LOG_ERROR("ECM: auByteOffsetSize of %d unsupported!", auByteOffsetSize);
                    return;
                }
                info.m_auByteOffset = b.read(auByteOffsetSize * 8);
                RPLAYER_LOG_DEBUG("ECM: auByteOffset=%d", info.m_auByteOffset);
            }
            for (unsigned int k = 0; k < ivSize && k < sizeof(info.m_initializationVector); k++) {
                info.m_initializationVector[k] = b.read(8);
            }
            if (j == 0 && info.m_auByteOffset != 0) {
                RPLAYER_LOG_WARNING("ECM: Unexpected first auByteOffset of %d", info.m_auByteOffset);
            }
        }
    }

    if (nextKeyIdFlag) {
        int countdownSec = b.read(4);
        b.skip(4);
        uint8_t nextKeyId[16];
        b.readBytes(nextKeyId, sizeof(nextKeyId));
        announceKeyIdentifier(nextKeyId);
        RPLAYER_LOG_DEBUG("ECM: countdownSec=%d", countdownSec);
    }
}

bool TsDemux::Impl::CetsCaModule::decrypt(uint8_t *data, uint32_t size, int scramblingControlBits)
{
    assert(scramblingControlBits != 0); // Packet should be marked as encrypted
    assert(scramblingControlBits <= 3); // control_bits are 2 bits only

    InfoList &streamInfoList(m_subStreams[scramblingControlBits - 1]);
    bool success = true;
    do {
        if (!streamInfoList.empty()) {
            // We must check whether a new AU starts and change decryption if needed.
            DecryptInfo &info(streamInfoList.front());
            if (info.m_auByteOffset == 0) {
                applyDecryptInfo(info); // Key should be valid by now
                streamInfoList.pop_front();
            }
        }

        if (streamInfoList.empty()) {
            // No next AU any more; we can simply decrypt everything
            return doDecrypt(data, size);
        }

        DecryptInfo &info(streamInfoList.front());
        uint32_t n = std::min(size, info.m_auByteOffset);

        success = doDecrypt(data, n) && success;
        info.m_auByteOffset -= n;
        size -= n;
        data += n;
    } while (size > 0);

    return success;
}
