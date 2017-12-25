///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

namespace ctvc {

/// \brief Generic platform-independent keyboard interface
/// Mainly meant for use with example applications, not intended for
/// use with real clients.
class Keyboard
{
public:
    static const int TIMEOUT_IN_MS = 10; ///< Time to block when no key is pressed.

    /// \brief Special key codes
    /// Common ASCII key codes like 'A' are not defined here.
    static const int ESC_SEQ = 0x1000;     ///< ESC-key followed by a '[' character. The key value returned will be this
                                           ///  value plus an escape-specific key value. For instance, the escape sequence
                                           ///  ESC-'['-'E' will return the key value 0x1045, which equals ESC_SEQ + 'E'.
                                           ///  (This holds for unrecognized escape sequences; Recognized sequences like
                                           ///  ESC-'['-'A' will be returned as UP_KEY, for instance.)
    static const int BACKSPACE_KEY = '\b'; ///< Backspace key, same as '\\b'.
    static const int ENTER_KEY = '\r';     ///< Enter key, same as '\\r'.
    static const int ESC_KEY = 0x1B;       ///< ESC-key (if not followed by a '[' character).
    static const int DEL_KEY = 0x7F;       ///< DEL key.
    static const int UP_KEY = 0x100;       ///< Cursor up key.
    static const int DOWN_KEY = 0x101;     ///< Cursor down key.
    static const int LEFT_KEY = 0x102;     ///< Cursor left key.
    static const int RIGHT_KEY = 0x103;    ///< Cursor right key.

    /// \brief Get a key from the console keyboard.
    /// \returns Key value (typically ASCII), EOF if end-of-file is reached or 0 if no key was pressed.
    /// \note \a get_key() typically blocks for at most \a TIMEOUT_IN_MS milliseconds waiting for a key to be pressed.
    /// \note Escape sequences are returned as specific values. An ESC-'['-\<key> sequence is returned as value
    /// ESC_SEQ + x, where x is the ASCII key value after the escape sequence. For example, the 'UP' arrow key typically
    /// encodes as ESC-'['-'A', which will then be returned as the value ESC_SEQ + 'A'. Other escape sequences are not
    /// translated and typically ignore the first ESC key. And ESC-ESC sequence returns as a single ESC_KEY character, 0x1B.
    static int get_key();
};

} // namespace
