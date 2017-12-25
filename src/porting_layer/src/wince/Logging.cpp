/// \file Logging.cpp
/// \brief The Porting Layer Library API
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

// This implementation is specifically for WinCE, because its compiler (cl.exe) does not 'understand' variadic arguments on the log macros's.
// The filename cannot be 'Log.cpp' because that would clash with the 'Log.cpp' in the generic part of the code tree when adding it to the SDK's library file.

#include <porting_layer/Log.h>
#include <porting_layer/ClientContext.h>

#include <stdio.h>
#include <stdarg.h>

using namespace ctvc;


static void log_msg(LogMessageType message_type, const char *file, int line, const char *function, const char *fmt, va_list valist)
{
    char expanded_message[3000];

    vsnprintf(expanded_message, sizeof(expanded_message), fmt, valist);

    // Forward the log message to all objects registered with the ClientContext
    ClientContext::instance().log_message(message_type, file, line, function, expanded_message);
}

void ctvc::log_error(const char *fmt, ...)
{
    va_list valist;
    va_start(valist, fmt);
    log_msg(LOG_ERROR, NULL, 0, "", fmt, valist);
    va_end(valist);
}

void ctvc::log_warning(const char *fmt, ...)
{
    va_list valist;
    va_start(valist, fmt);
    log_msg(LOG_WARNING, NULL, 0, "", fmt, valist);
    va_end(valist);
}

void ctvc::log_info(const char *fmt, ...)
{
    va_list valist;
    va_start(valist, fmt);
    log_msg(LOG_INFO, NULL, 0, "", fmt, valist);
    va_end(valist);
}

void ctvc::log_debug(const char *fmt, ...)
{
    va_list valist;
    va_start(valist, fmt);
    log_msg(LOG_DEBUG, NULL, 0, "", fmt, valist);
    va_end(valist);
}
