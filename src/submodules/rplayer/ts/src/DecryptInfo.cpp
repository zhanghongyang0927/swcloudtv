///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include <rplayer/ts/DecryptInfo.h>

#include <string.h>

using namespace rplayer;

DecryptInfo::DecryptInfo() :
    m_auByteOffset(0)
{
    memset(m_keyIdentifier, 0, sizeof(m_keyIdentifier));
    memset(m_initializationVector, 0, sizeof(m_initializationVector));
}
