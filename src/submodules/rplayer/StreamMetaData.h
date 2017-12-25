///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

namespace rplayer {


// Could perhaps also model this as an event and/or enum.
// States/data:
// Uninitialized or blank
// RAMS stream ID / No stream ID (passed TS)
// encrypted / clear (and/or CENC encrypted, TS encrypted?)
//
class StreamMetaData
{
public:
    // Stream type can be used to determine what kind of stream is passed.
    // This is especially useful when e.g. a demux wants to demultiplex a stream
    // and needs to know whether this is at all possible.
    enum Type {
        UNDEFINED,
        CLEAR_TS, // Decodable transport stream
        ENCRYPTED_TS // Scrambled TS, might even not be syncable (TODO: define more clearly and/or add another variant, e.g. ECB encrypted, CENC encrypted, TS encrypted, PES encrypted, OPAQUE)
    };

    // Stream ID can be used to e.g. identify the RAMS stream source. These range from 0-15 inclusive.
    // If not set or ot applicable, the value is 255 (NO_ID).
    static const uint8_t NO_ID = 255;

    StreamMetaData() :
        m_type(UNDEFINED),
        m_id(NO_ID)
    {
    }

    StreamMetaData(Type type) :
        m_type(type),
        m_id(NO_ID)
    {
    }

    StreamMetaData(Type type, uint8_t id) :
        m_type(type),
        m_id(id)
    {
    }

    ~StreamMetaData()
    {
    }

    bool operator==(const StreamMetaData &rhs) const
    {
        return m_type == rhs.m_type && m_id == rhs.m_id;
    }

    bool operator!=(const StreamMetaData &rhs) const
    {
        return !(*this == rhs);
    }

    Type getType() const
    {
        return m_type;
    }

    uint8_t getId() const
    {
        return m_id;
    }

private:
    Type m_type;
    uint8_t m_id;
};

} // namespace rplayer
