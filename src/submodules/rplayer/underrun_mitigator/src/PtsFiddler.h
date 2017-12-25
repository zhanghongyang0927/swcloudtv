///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include "UnderrunAlgorithmBase.h"

namespace rplayer {

class PtsFiddler: public UnderrunAlgorithmBase
{
public:
    PtsFiddler(StreamBuffer &source, const UnderrunAlgorithmParams &params, ICallback &callback);
    ~PtsFiddler();

    void clear();

private:
    PtsFiddler(const PtsFiddler &);
    PtsFiddler &operator=(const PtsFiddler &);

    Frame *getNextFrame(TimeStamp pcr);

    TimeStamp m_lastDts;
};

} // namespace
