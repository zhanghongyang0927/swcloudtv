///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

namespace rplayer {

//
// This interface is a log callback from the rplayer library to the user.
// It can log events at several levels. The user is to select the action to perform upon a certain log.
//

struct ILog
{
    ILog()
    {
    }
    virtual ~ILog()
    {
    }

    enum LogMessageType
    {
        LOG_ERROR, LOG_WARNING, LOG_INFO, LOG_DEBUG
    };

    //
    // These log methods are called under different circumstances
    // Typically, the circumstances are as follows:
    //  - Debug messages are for tracing the process in detail
    //  - Info messages are for coarse tracing, showing events of interest
    //  - Warning messages are to indicate events that may have undesirable or noticeable (side) effects.
    //  - Error messages are to indicate unrecoverable errors, the calling process needs to abort its current action
    //
    // The parameters file, line and function /may/ be used by the log interface but this is not required.
    // The message is already expanded, if necessary.
    virtual void log_message(LogMessageType message_type, const char *file, int line, const char *function, const char *message) = 0;
};

//
// Register/unregister your logger with the rplayer library
//
void registerLogger(ILog &);
void unregisterLogger();

} // namespace rplayer
