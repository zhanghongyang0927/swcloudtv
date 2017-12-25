/// \file utils.h
/// \brief Collection of utilities
///
///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include <string>
#include <vector>

namespace ctvc {

/// \brief Split the url into sub parts.
/// \param[in] url URL to split into parts
/// \param[out] proto string to put protocol part
/// \param[out] authorization string to put allocation part
/// \param[out] hostname string to put host part
/// \param[out] port location to put port part
/// \param[out] path string to put path part
/// \result void
void url_split(const std::string &url, std::string &proto/*out*/, std::string &authorization/*out*/, std::string &hostname/*out*/, int &port/*out*/, std::string &path/*out*/);

/// \brief Escape characters that are not allowed in a URL.
///
/// \param[in] src
///
/// \result The URL-encoded string that was specified in \a src.
std::string url_encode(const char *src);

/// \brief Escape characters that are not allowed in XML.
///
/// \param[in] src
///
/// \result The XML-encoded string that was specified in \a src.
std::string xmlencode(const char *src);

#if defined __GNUC__
#define ATTRIB(F,A)     __attribute__ ((format (printf, F, A)))
#else
#define ATTRIB(F,A)
#endif

/// \brief sprintf-like helper function that can directly print into an std::string
void string_printf(std::string &, const char *fmt, ...) ATTRIB(2,3);
void string_printf_append(std::string &, const char *fmt, ...) ATTRIB(2,3);
std::string uint64_to_string(uint64_t value);

#undef ATTRIB

/// \brief helper function that can output a hex dump of arbitrary data
std::string hex_dump(const void *data, size_t size);

/// \brief Parse any character separated list into std::vector of strings.
///
/// \param[in] list string of comma separated values.
/// \param[in] sep character separator.
///
/// \result std::vector of values as strings.
std::vector<std::string> parse_character_separated_list(const std::string &list, char sep);

/// \brief Parse a GUID-formatted string into a 16-byte sequence
///
/// \param[in] string The GUID-formatted string, e.g. "1077EFEC-C0B2-4D02-ACE3-3C1E52E2FB4B"
/// \param[out] id The resulting 16-byte sequence
///
/// \note 1. The string is parsed in a case insensitive manner
///       2. It is allowed to leave out the dashes.
///       3. If the input designates a longer or shorter sequence, the result is undefined.
void parse_guid_formatted_string(const std::string &string, uint8_t (&id)[16]/*out*/);

/// \brief Generate a GUID-formatted string from a 16-byte sequence
///
/// \param[out] id The resulting 16-byte sequence
/// \result string The GUID-formatted string, e.g. "1077EFEC-C0B2-4D02-ACE3-3C1E52E2FB4B"
std::string id_to_guid_string(const uint8_t (&id)[16]);

/// \brief Alternatives to the functions that are provided by the standard C-library, just in case they are not supported by a specific compiler
int strcasecmp(const char *s1, const char *s2);
int strncasecmp(const char *s1, const char *s2, size_t n);

/// \brief Convert an ASCII string with hexadecimal characters into a value
///
/// \param[in] string A non-hex character terminated string. Must be a valid pointer.
/// \note The functionality is equal to calling strtol(string, 0, 16) but this method is more than twice as fast.
/// \note Leading spaces are skipped. Parsing is stopped at the first non-hex character. + or - signs are not interpreted.
uint32_t atox(const char *string);

} // namespace
