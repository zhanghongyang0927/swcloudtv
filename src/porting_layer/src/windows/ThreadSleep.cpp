///
/// \copyright Copyright © 2017 ActiveVideo, Inc. and/or its affiliates.
/// Reproduction in whole or in part without written permission is prohibited.
/// All rights reserved. U.S. Patents listed at http://www.activevideo.com/patents
///

#include <porting_layer/Thread.h>

#include <windows.h>

void ctvc::Thread::sleep(uint32_t time_in_milliseconds)
{
    Sleep(time_in_milliseconds);
}
