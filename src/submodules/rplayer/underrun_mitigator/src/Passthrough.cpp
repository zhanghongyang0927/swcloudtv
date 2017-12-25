///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include "Passthrough.h"

using namespace rplayer;

Passthrough::Passthrough(StreamBuffer &source, const UnderrunAlgorithmParams &params, ICallback &callback) :
    UnderrunAlgorithmBase(source, params, callback)
{
}

Passthrough::~Passthrough()
{
}

Frame *Passthrough::getNextFrame(TimeStamp /*pcr*/)
{
    return checkSource();
}
