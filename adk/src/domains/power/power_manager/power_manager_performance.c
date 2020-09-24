/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      API to change VM performance profile with the reference counter.

*/

#include "power_manager_performance.h"
#include "power_manager.h"

#include <vm.h>
#include <panic.h>
#include <logging.h>

void appPowerPerformanceInit(void)
{
    VmRequestRunTimeProfile(VM_BALANCED);
}

void appPowerPerformanceProfileRequest(void)
{
    powerTaskData *thePower = PowerGetTaskData();

    if (0 == thePower->performance_req_count)
    {
        VmRequestRunTimeProfile(VM_PERFORMANCE);
        DEBUG_LOG("appPowerPerformanceProfileRequest VM_PERFORMANCE");
    }
    thePower->performance_req_count++;
    /* Unsigned overflowed request count */
    PanicZero(thePower->performance_req_count);
}

void appPowerPerformanceProfileRelinquish(void)
{
    powerTaskData *thePower = PowerGetTaskData();

    if (thePower->performance_req_count > 0)
    {
        thePower->performance_req_count--;
        if (0 == thePower->performance_req_count)
        {
            VmRequestRunTimeProfile(VM_BALANCED);
            DEBUG_LOG("appPowerPerformanceProfileRelinquish VM_BALANCED");
        }
    }
    else
    {
        DEBUG_LOG("appPowerPerformanceProfileRelinquish ignoring double relinquish");
    }
}
