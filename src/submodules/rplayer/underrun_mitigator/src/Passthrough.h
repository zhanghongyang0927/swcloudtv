///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include "UnderrunAlgorithmBase.h"

namespace rplayer {

class Passthrough: public UnderrunAlgorithmBase
{
public:
    Passthrough(StreamBuffer &source, const UnderrunAlgorithmParams &params, ICallback &callback);
    ~Passthrough();

private:
    Passthrough(const Passthrough &);
    Passthrough &operator=(const Passthrough &);

    Frame *getNextFrame(TimeStamp pcr);
};

} // namespace
