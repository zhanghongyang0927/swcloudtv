///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include <porting_layer/Keyboard.h>

#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

using namespace ctvc;

namespace {
class TerminalModeSetter
{
public:
    TerminalModeSetter()
    {
        // Save the current terminal settings.
        tcgetattr(0, &m_orig_termios);

        // Create a new terminal mode from the old and set it into raw mode.
        struct termios new_termios;
        memcpy(&new_termios, &m_orig_termios, sizeof(new_termios));
        cfmakeraw(&new_termios);

        // Keep the original output modes.
        new_termios.c_oflag = m_orig_termios.c_oflag;

        // Make sure our inputs still pass signal keys.
        new_termios.c_lflag |= ISIG;

        // And set the new terminal mode.
        tcsetattr(0, TCSANOW, &new_termios);
    }

    ~TerminalModeSetter()
    {
        // Restore the original terminal settings.
        tcsetattr(0, TCSANOW, &m_orig_termios);
    }

private:
    struct termios m_orig_termios;
};

static TerminalModeSetter s_terminal_mode_setter;
} // namespace

int Keyboard::get_key()
{
    fd_set socket_set;
    struct timeval tv;

    FD_ZERO(&socket_set);
    FD_SET(0, &socket_set);
    tv.tv_sec = 0;
    tv.tv_usec = 1000 * TIMEOUT_IN_MS;

    if (select(1,  &socket_set, NULL, NULL, &tv) == 0) {
        return 0;
    }

    unsigned char c;
    if (read(0, &c, sizeof(c)) < 0) {
        return EOF;
    }

    if (c == ESC_KEY) { // Esc
        if (read(0, &c, sizeof(c)) < 0) {
            return EOF;
        }

        if (c == '[') {
            if (read(0, &c, sizeof(c)) < 0) {
                return EOF;
            }

            switch (c) {
            case '3': // DEL
                return DEL_KEY;
            case 'A': // UP
                return UP_KEY;
            case 'B': // DOWN
                return DOWN_KEY;
            case 'C': // RIGHT
                return RIGHT_KEY;
            case 'D': // LEFT
                return LEFT_KEY;
            default:
                return ESC_SEQ | c;
            }
        }
    }

    return c;
}
