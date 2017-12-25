///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <rplayer/ts/IDataSink.h>
#include <rplayer/IDecryptEngine.h>
#include <rplayer/ts/TsDemux.h>
#include <rplayer/ts/TimeStamp.h>

#include "common.h"
#include "LatencyDataParser.h"

#include <inttypes.h>

#include <string>
#include <vector>
#include <map>

namespace rplayer {

struct IEventSink;

class TsDemux::Impl
{
public:
    Impl();
    ~Impl();

    void parse(const uint8_t *data, uint32_t size);

    IEventSink *m_eventOut;
    IDataSink *m_videoOut;
    IDataSink *m_keyFrameVideoOut;
    IDataSink *m_audioOut;
    IPacketSinkWithMetaData *m_packetOut;

    struct ICaDecryptor
    {
        ICaDecryptor() {}
        virtual ~ICaDecryptor() {}

        virtual bool decrypt(uint8_t *data, uint32_t size, int scramblingControlBits) = 0;
    };

    class Parser
    {
    public:
        Parser() :
            m_continuityCounter(0),
            m_discontinuityIndicator(true),
            m_caDecryptor(0)
        {
        }
        virtual ~Parser()
        {
        }

        virtual void parse(const uint8_t *data, uint32_t size, bool payloadUnitStartIndicator) = 0;
        virtual void reset() = 0;

        int m_continuityCounter;
        bool m_discontinuityIndicator;
        ICaDecryptor *m_caDecryptor;
    };

    // Defined in the cpp file
    class PsiParser;
    class PatPsiParser;
    class PmtPsiParser;
    class PesParser;
    class CaModule;
    class CetsCaModule;

    uint8_t m_packetBuffer[TS_PACKET_SIZE];
    uint32_t m_remainingPacketBytes;

    std::map<int, Parser *> m_parsers;

    std::string m_preferredLanguage;

    struct StreamInfo
    {
        StreamInfo(TsProgramMapStreamType streamType, int elementaryPid, const std::string &language, bool isKeyFrameStream) :
            m_streamType(streamType),
            m_elementaryPid(elementaryPid),
            m_language(language),
            m_isKeyFrameStream(isKeyFrameStream)
        {
        }

        TsProgramMapStreamType m_streamType;
        int m_elementaryPid;
        std::string m_language;
        bool m_isKeyFrameStream;
    };

    std::vector<StreamInfo> m_streams;
    std::vector<CaModule *> m_caModules;
    int m_audioPid;
    int m_videoPid;
    int m_keyFrameVideoPid;
    int m_pcrPid;
    int m_latencyDataPid;
    LatencyDataParser m_latencyDataParser;

    std::vector<IDecryptEngineFactory *> m_decryptEngineFactories;

    void cleanup();
    void parseTsPacket(const uint8_t *p, const uint8_t *&pOut);
    void parsePsiSection(const uint8_t *p, uint32_t size);
    void setPmt(int pmtPid);
    void addPesParser(int elementaryPid, IDataSink *dataSink, PesStreamId streamId);
    void removeParser(int elementaryPid);
    void addAudioStream(TsProgramMapStreamType streamType, int elementaryPid, const std::string &language);
    void addVideoStream(TsProgramMapStreamType streamType, int elementaryPid, bool isKeyFrameStream);
    void addEcmStream(int elementaryPid, int encryptedStreamPid, IDecryptEngineFactory &decryptEngineFactory);
    void addLatencyStream(int elementaryPid);
    void setupPat();

    void clearCaModules();
    IDecryptEngineFactory *findDecryptEngineFactory(const uint8_t (&systemId)[16]);

    void clearElementaryStreamInfo();
    void addElementaryStreamInfo(TsProgramMapStreamType streamType, int elementaryPid, const std::string &language, bool isKeyFrameStream);
    void selectElementaryStreams();
};

} // namespace rplayer
