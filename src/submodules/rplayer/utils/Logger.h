///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <rplayer/ILog.h>

namespace rplayer {

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

void log_message(ILog::LogMessageType message_type, const char *file, int line, const char *function, const char *fmt, ...) ATTRIB(5,6);

#undef ATTRIB

} // namespace

#define RPLAYER_LOG_ERROR(...)     rplayer::log_message(ILog::LOG_ERROR, __FILE__, __LINE__, __PRETTY_FUNCTION__, __VA_ARGS__)
#define RPLAYER_LOG_WARNING(...)   rplayer::log_message(ILog::LOG_WARNING, __FILE__, __LINE__, __PRETTY_FUNCTION__, __VA_ARGS__)
#define RPLAYER_LOG_INFO(...)      rplayer::log_message(ILog::LOG_INFO, __FILE__, __LINE__, __PRETTY_FUNCTION__, __VA_ARGS__)
#ifdef DEBUG
#define RPLAYER_LOG_DEBUG(...)     rplayer::log_message(ILog::LOG_DEBUG, __FILE__, __LINE__, __PRETTY_FUNCTION__, __VA_ARGS__)
#else
namespace rplayer {
inline void log_dummy(...) {}
} // namespace
#define RPLAYER_LOG_DEBUG(...) rplayer::log_dummy(__VA_ARGS__)
#endif
