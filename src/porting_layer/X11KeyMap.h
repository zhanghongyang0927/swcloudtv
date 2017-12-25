///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#pragma once

#include <map>

namespace ctvc {

/// \brief X11 key codes
///
/// \see 'CloudTV H5 Keycode Mapping Specification' document.
///
enum X11KeyCode
{
    X11_BACK           = 0x10000001,      ///< VK_BACK_SPACE
    X11_LAST           = 0x10000024,      ///< VK_BACK_SPACE
    X11_CLEAR          = 0x0000FF0B,      ///< VK_CLEAR
    X11_OK             = 0x10000000,      ///< VK_ENTER
    X11_PAUSE          = 0x10000012,      ///< VK_PAUSE
    X11_PAGE_UP        = 0x0000FF55,      ///< VK_PAGE_UP
    X11_PAGE_DOWN      = 0x0000FF56,      ///< VK_PAGE_DOWN
    X11_END            = 0x10000022,      ///< VK_END
    X11_HOME           = 0x0000FF95,      ///< VK_HOME
    X11_LEFT           = 0x0000FF51,      ///< VK_LEFT
    X11_UP             = 0x0000FF52,      ///< VK_UP
    X11_RIGHT          = 0x0000FF53,      ///< VK_RIGHT
    X11_DOWN           = 0x0000FF54,      ///< VK_DOWN
    X11_SELECT         = 0x0000FF60,      ///< VK_SELECT
    X11_0              = 0x00000030,      ///< VK_0
    X11_1              = 0x00000031,      ///< VK_1
    X11_2              = 0x00000032,      ///< VK_2
    X11_3              = 0x00000033,      ///< VK_3
    X11_4              = 0x00000034,      ///< VK_4
    X11_5              = 0x00000035,      ///< VK_5
    X11_6              = 0x00000036,      ///< VK_6
    X11_7              = 0x00000037,      ///< VK_7
    X11_8              = 0x00000038,      ///< VK_8
    X11_9              = 0x00000039,      ///< VK_9
    X11_A              = 0x00000041,      ///< VK_A
    X11_B              = 0x00000042,      ///< VK_B
    X11_C              = 0x00000043,      ///< VK_C
    X11_D              = 0x00000044,      ///< VK_D
    X11_HELP           = 0x10000061,      ///< VK_HELP
    X11_HASH           = 0x00000023,      ///< VK_HASH
    X11_ASTERISK       = 0x0000002A,      ///< VK_ASTERISK
    X11_FAVORITES      = 0x10000081,      ///< VK_FAVORITES
    X11_MUTE           = 0x10000090,      ///< VK_MUTE
    X11_VOL_DOWN       = 0x10000091,      ///< VK_VOL_DOWN
    X11_VOL_UP         = 0x10000092,      ///< VK_VOL_UP
    X11_SKIP_BACK      = 0x10000018,      ///< VK_SKIP_BACK
    X11_RED            = 0x10000050,      ///< VK_RED
    X11_GREEN          = 0x10000051,      ///< VK_GREEN
    X11_YELLOW         = 0x10000052,      ///< VK_YELLOW
    X11_BLUE           = 0x10000053,      ///< VK_BLUE
    X11_RW             = 0x10000014,      ///< VK_REWIND
    X11_STOP           = 0x10000011,      ///< VK_STOP
    X11_PLAY           = 0x10000010,      ///< VK_PLAY
    X11_RECORD         = 0x10000070,      ///< VK_RECORD
    X11_FF             = 0x10000013,      ///< VK_FAST_FWD
    X11_PREV           = 0x10000021,      ///< VK_PREV
    X11_NEXT           = 0x10000020,      ///< VK_NEXT
    X11_CHANNEL_UP     = 0x10000040,      ///< VK_CHANNEL_UP
    X11_CHANNEL_DOWN   = 0x10000041,      ///< VK_CHANNEL_DOWN
    X11_INFO           = 0x10000060,      ///< VK_INFO
    X11_GUIDE          = 0x10000080,      ///< VK_GUIDE
    X11_TTX            = 0x1000003C,      ///< VK_TTX
    X11_MENU           = 0x10000032,      ///< VK_MENU
    X11_LIVE           = 0x10000037,      ///< VK_LIVE
    X11_EXIT           = 0x10000031,      ///< VK_EXIT
    X11_DVR            = 0x10000036,      ///< VK_MYDVR
    X11_SETUP          = 0x10000030,      ///< VK_SETUP
    X11_TOP_MENU       = 0x10000034,      ///< VK_TOPMENU
    X11_NETTV          = 0x10000033,      ///< VK_NETTV
    X11_MEDIA          = 0x10000038,      ///< VK_MEDIA
    X11_PPV            = 0x10000035,      ///< VK_ONDEMAND
    X11_SKIP           = 0x10000015,      ///< VK_SKIP_FWD
    X11_REPLAY         = 0x10000016,      ///< VK_REPLAY
    X11_LIST           = 0x10000023,      ///< VK_LIST
    X11_DAY_UP         = 0x10000082,      ///< VK_DAYUP
    X11_DAY_DOWN       = 0x10000083,      ///< VK_DAYDOWN
    X11_PLAYPAUSE      = 0x10000017,      ///< VK_PLAYPAUSE
    X11_LANGUAGE       = 0x10000102,      ///< VK_LANGUAGE
    X11_SETTINGS       = 0x10000039,      ///< VK_SETTINGS
    X11_OEMA           = 0x10000054,      ///< VK_OEMA
    X11_OEMB           = 0x10000055,      ///< VK_OEMB
    X11_OEMC           = 0x10000056,      ///< VK_OEMC
    X11_OEMD           = 0x10000057,      ///< VK_OEMD
    X11_MOVIE          = 0x1000003B,      ///< VK_MOVIE
    X11_DIGITALTV      = 0x1000003D,      ///< VK_DIGITALTV
    X11_TRIANGLE       = 0x10000100,      ///< VK_TRIANGLE
    X11_HEXAGON        = 0x10000101,      ///< VK_HEXAGON
    X11_RADIO          = 0x1000003A,      ///< VK_RADIO
    X11_SPORTS         = 0x10000120,      ///< VK_SPORTS
    X11_KIDS           = 0x10000121,      ///< VK_KIDS
    X11_NEWS           = 0x10000122,      ///< VK_NEWS
    X11_OPTIONS        = 0x10000003,      ///< VK_OPTIONS
    X11_SEARCH         = 0x10000002,      ///< VK_SEARCH
    X11_OK_SELECT      = 0x10000130,      ///< VK_OK_SELECT
    X11_INVALID        = 0xFFFFFFFF
};

/// \brief Class for the key mapping between the platform keycodes and X11 keycodes
class X11KeyMap
{
public:
    X11KeyMap();
    ~X11KeyMap();

    /// \brief Add a new mapping of a native key code to an X11 key code.
    /// \param[in] from_key The native (remote control) key code.
    /// \param[in] to_key The X11KeyCode it maps to.
    void add_mapping(int from_key, X11KeyCode to_key);

    /// \brief A single key map entry for use in add_mapping()
    struct KeyMap
    {
        /// \brief Platform key code to be mapped from
        int from_key;

        /// \brief X11 key code to be generated
        X11KeyCode to_key;
    };

    /// \brief Add a new set of translations from native key code to X11 key code.
    /// \param[in] map The map structure array pointer.
    /// \param[in] n_entries The number of entries in the array.
    void add_mapping(const KeyMap map[], unsigned int n_entries);

    /// \brief Translate a native key code to an X11 key code.
    /// \param[in] native_key The native (remote control) key code.
    /// \return X11 translated code for \a native_key or the native key
    ///         if no mapping exists at all, or X11_INVALID if a keymap
    ///         was set, but no mapping exists for \a native_key.
    X11KeyCode translate(int native_key) const;

private:
    std::map<int, X11KeyCode> m_keymap;
};

} // namespace
