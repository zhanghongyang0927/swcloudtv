///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <rplayer/ts/IDataSource.h>

namespace rplayer {

class StreamBuffer;
struct UnderrunAlgorithmParams;
class Frame;

class UnderrunAlgorithmBase: public UnscrambledDataSource
{
public:
    struct ICallback
    {
        virtual void stallDetected(const TimeStamp &stallDuration) = 0;
    };

    UnderrunAlgorithmBase(StreamBuffer &source, const UnderrunAlgorithmParams &params, ICallback &callback);
    virtual ~UnderrunAlgorithmBase();

    // IDataSource/UnscrambledDataSource Implementation
    StreamType getStreamType();
    bool isNewFrame(TimeStamp &pts, TimeStamp &dts);
    const uint8_t *getData();
    uint32_t getBytesAvailable(TimeStamp pcr);
    void readBytes(uint32_t n);
    const std::string getLanguage();

    // Status
    // Override if necessary.
    virtual TimeStamp getStalledDuration();

    // Use clear() when a new stream is started
    // This might be necessary for proper operation when opening new streams /or/ could be handled by newStream()
    virtual void clear();

protected:
    // Get (and possibly modify) next frame from input (if present) or create new frame (if necessary and possible).
    // The frame, if present, must have been created with 'new' and will be deleted by UnderrunAlgorithmBase.
    virtual Frame *getNextFrame(TimeStamp pcr) = 0;

    // Get next frame from input (if present).
    Frame *checkSource();

    // Underrun mitigator parameter access
    const UnderrunAlgorithmParams &getParams() const;

    // Called by derived class when a delay is detected (i.e. any received frame that experiences a delay > 0).
    void notifyDelay(const TimeStamp &delayTime);

private:
    UnderrunAlgorithmBase(const UnderrunAlgorithmBase &);
    UnderrunAlgorithmBase &operator=(const UnderrunAlgorithmBase &);

    StreamBuffer &m_source;
    const UnderrunAlgorithmParams &m_params;
    ICallback &m_callback;
    Frame *m_currentFrame;
    uint32_t m_nRead;

    TimeStamp m_previousDelay;
    TimeStamp m_accumulatedStalledDuration; // Accumulated stalled time over multiple stall periods.
};

} // namespace

