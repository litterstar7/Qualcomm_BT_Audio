/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    power_manager
\brief      Sophisticated version of the reboot.

It is a workaround it should be deleted at some point.

*/

#include "power_manager.h"

#include <system_clock.h>
#include <logging.h>

#include <boot.h>
#include <panic.h>

#define APP_POWER_SEC_TO_US(s)    ((rtime_t) ((s) * (rtime_t) US_PER_SEC))

void appPowerReboot(void)
{
    /* Reboot now */
    BootSetMode(BootGetMode());
    DEBUG_LOG("appPowerReboot, post reboot");

    /* BootSetMode returns control on some devices, although should reboot.
       Wait here for 1 seconds and then Panic() to force the reboot. */
    rtime_t start = SystemClockGetTimerTime();

    while (1)
    {
        rtime_t now = SystemClockGetTimerTime();
        rtime_t elapsed = rtime_sub(now, start);
        if (rtime_gt(elapsed, APP_POWER_SEC_TO_US(1)))
        {
            DEBUG_LOG("appPowerReboot, forcing reboot by panicking");
            Panic();
        }
    }
}
