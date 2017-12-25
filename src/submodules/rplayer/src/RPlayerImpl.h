///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <rplayer/RPlayer.h>
#include <rplayer/rams/Rams.h>
#include <rplayer/ts/TsDemux.h>
#include <rplayer/underrun_mitigator/UnderrunMitigator.h>

namespace rplayer {

struct IPacketSink;
struct IPacketSinkWithMetaData;
struct IEventSink;

class RPlayer::Impl
{
public:
    Impl();
    ~Impl();

    void adjustRouting();

    TsDemux m_demux;
    Rams m_rams;
    UnderrunMitigator m_underrunMitigator;
    IPacketSink *m_packetIn;
    IPacketSinkWithMetaData *m_packetOut;
    IEventSink *m_eventOut;
    Feature m_enabledFeatures;
};

} // namespace rplayer
