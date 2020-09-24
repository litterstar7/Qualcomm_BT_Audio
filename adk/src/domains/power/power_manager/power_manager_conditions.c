/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Functions used to decide if system should go to sleep or power off.

*/

#include "power_manager_conditions.h"
#include "power_manager.h"

#include "charger_monitor.h"
#include "battery_monitor.h"
#include "temperature.h"

bool appPowerCanPowerOff(void)
{
    return appChargerCanPowerOff();
}

bool appPowerCanSleep(void)
{
    return    appChargerCanPowerOff()
           && PowerGetTaskData()->allow_dormant
           && PowerGetTaskData()->init_complete;
}

bool appPowerNeedsToPowerOff(void)
{
    bool battery_too_low = (battery_level_too_low == appBatteryGetState());
    bool user_initiated = PowerGetTaskData()->user_initiated_shutdown;
    bool temperature_extreme = (appTemperatureClientGetState(PowerGetTask()) !=
                                TEMPERATURE_STATE_WITHIN_LIMITS);
    return appPowerCanPowerOff() && (temperature_extreme || user_initiated || battery_too_low);
}
