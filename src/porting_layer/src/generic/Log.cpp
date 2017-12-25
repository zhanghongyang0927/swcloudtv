/// \file Log.cpp
/// \brief The Porting Layer Library API
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include <porting_layer/Log.h>
#include <porting_layer/ClientContext.h>

#include <stdio.h>
#include <stdarg.h>

using namespace ctvc;

void ctvc::log_message(LogMessageType message_type, const char *file, int line, const char *function, const char *fmt, ...)
{
    char expanded_message[3000];

    va_list valist;
    va_start(valist, fmt);
    vsnprintf(expanded_message, sizeof(expanded_message), fmt, valist);
    va_end(valist);

    // Forward the log message to all objects registered with the ClientContext
    ClientContext::instance().log_message(message_type, file, line, function, expanded_message);
}
