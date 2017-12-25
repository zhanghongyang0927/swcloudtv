///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include <rplayer/ILog.h>
#include <rplayer/utils/Logger.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

using namespace rplayer;

static ILog *s_logger = 0;

void rplayer::registerLogger(ILog &logger)
{
    s_logger = &logger;
}

void rplayer::unregisterLogger()
{
    s_logger = 0;
}

// According to vsnprintf documentation, the va_list is modified when it's
// called, so "arg1" cannot be reused. From C+11 onwards it is possible to use
// va_copy to duplicate arg1 and use it in the second vsnprintf. However, this
// is not possible with C++03, so passing "arg2" is a workaround.
void rplayer::log_message(ILog::LogMessageType message_type, const char *file, int line, const char *function, const char *fmt, ...)
{
    if (!s_logger) {
        return;
    }

    va_list ap1;
    va_list ap2;
    va_start(ap1, fmt);
    va_start(ap2, fmt);

    char text[512];

    int n = vsnprintf(text, sizeof(text), fmt, ap1);

    if (n >= 0 && n < static_cast<int>(sizeof(text))) {
        s_logger->log_message(message_type, file, line, function, text);
    } else {
        char *p = new char[n + 1];

        vsnprintf(p, n + 1, fmt, ap2);
        s_logger->log_message(message_type, file, line, function, p);
        delete[] p;
    }

    va_end(ap1);
    va_end(ap2);
}
