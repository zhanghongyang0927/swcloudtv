/// \file Log.h
/// \brief Defines the log functions
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

namespace ctvc {

/// \brief Defines the log levels
enum LogMessageType
{
    LOG_ERROR, LOG_WARNING, LOG_INFO, LOG_DEBUG
};

/**
 * type-checking for printf-style functions
 * F = number of the argument containing the formatting std::string
 * A = nunmber of the varargs argument
 * Example:
 * my_printf(int fd, const char *fmt, ...)  ATTRIB(2, 3);
 *
 * NOTE:
 *  When used in a class, g++ adds 'this' to the argument list.
 *  Hence, the numbers must be incremented by 1:
 *
 *  class Test
 *  {
 *      public:
 *          void my_printf(int fd, const char *fmt, ...) ATTRIB(3, 4);
 *  };
 */
#if defined __GNUC__
#define ATTRIB(F,A)     __attribute__ ((format (printf, F, A)))
#else
#define ATTRIB(F,A)
#endif

/// \brief Core function to output log messages
void log_message(LogMessageType message_type, const char *file, int line, const char *function, const char *fmt, ...) ATTRIB(5,6);

#undef ATTRIB

} // namespace

#ifdef _WINLIB_ /* Windows CE compiler cannot deal with variadic arguments on macro's */

namespace ctvc {
void log_error(const char *fmt, ...);
void log_warning(const char *fmt, ...);
void log_info(const char *fmt, ...);
void log_debug(const char *fmt, ...);
}

#define CTVC_LOG_ERROR     ctvc::log_error
#define CTVC_LOG_WARNING   ctvc::log_warning
#define CTVC_LOG_INFO      ctvc::log_info
#ifdef DEBUG
#define CTVC_LOG_DEBUG     ctvc::log_debug
#else
namespace ctvc {
inline void log_dummy(...) {}
}
#define CTVC_LOG_DEBUG     ctvc::log_dummy
#endif

#else /* Normal platforms */

#define CTVC_LOG_ERROR(...)     ctvc::log_message(ctvc::LOG_ERROR, __FILE__, __LINE__, __PRETTY_FUNCTION__, __VA_ARGS__)
#define CTVC_LOG_WARNING(...)   ctvc::log_message(ctvc::LOG_WARNING, __FILE__, __LINE__, __PRETTY_FUNCTION__, __VA_ARGS__)
#define CTVC_LOG_INFO(...)      ctvc::log_message(ctvc::LOG_INFO, __FILE__, __LINE__, __PRETTY_FUNCTION__, __VA_ARGS__)
#ifdef DEBUG
#define CTVC_LOG_DEBUG(...)     ctvc::log_message(ctvc::LOG_DEBUG, __FILE__, __LINE__, __PRETTY_FUNCTION__, __VA_ARGS__)
#else
namespace ctvc {
/// \brief Empty dummy log function to be used in release builds
///
/// \note Its definition and use prevents 'unused variable' warnings in case variables are used in debug logs only.
inline void log_dummy(...) {}
} // namespace
#define CTVC_LOG_DEBUG(...) ctvc::log_dummy(__VA_ARGS__)
#endif
#endif
