///
/// \file IStreamPlayer.h
///
/// \brief CloudTV Nano SDK Stream player interface.
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include "IStream.h"

namespace ctvc {

/// \brief Stream Player interface.
/// The stream player plays a stream (typically MPEG-2 TS) that enters through IStream.
/// \todo Implement the methods of IStreamPlayer in your own derived class.
struct IStreamPlayer : public IStream
{
    IStreamPlayer()
    {
    }

    virtual ~IStreamPlayer()
    {
    }

    /// \brief Start the player.
    /// \result ResultCode.
    ///
    /// \note Data may enter through IStream after start() has been called.
    /// Normally, data should not be received before start() has been called.
    /// However, the player should be able to handle this case, although its
    /// behavior is not defined.
    virtual ResultCode start() = 0;

    /// \brief Stop the player.
    ///
    /// \note This call always succeeds. All resources in use by the player
    /// can be freed. The player must support stop() being called multiple
    /// times without an intermediate call to start().
    /// Normally, data should not be received after stop() has been called.
    /// However, the player should be able to handle this case, although its
    /// behavior is not defined.
    virtual void stop() = 0;
};

} // namespace
