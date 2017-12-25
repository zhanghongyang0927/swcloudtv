///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include <porting_layer/Keyboard.h>
#include <porting_layer/Thread.h>

#include <conio.h>

using namespace ctvc;

int Keyboard::get_key()
{
    if (!kbhit()) {
        Thread::sleep(TIMEOUT_IN_MS);
        if (!kbhit()) {
            return 0;
        }
    }
    return _getch();
}
