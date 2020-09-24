/*!
\copyright  Copyright (c) 2008 - 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Monitoring of charger status, battery voltage and temperature.

*/

#include "power_manager_monitors.h"
#include "power_manager.h"
#include "power_manager_sm.h"

#include "temperature.h"
#include "battery_monitor.h"
#include "battery_monitor_config.h"
#include "charger_monitor.h"

#include "ui.h"

#include <logging.h>

void appPowerRegisterMonitors(void)
{
    batteryRegistrationForm batteryMonitoringForm;

    appChargerClientRegister(PowerGetTask());

    batteryMonitoringForm.task = PowerGetTask();
    batteryMonitoringForm.representation = battery_level_repres_state;
    batteryMonitoringForm.hysteresis = appConfigSmBatteryHysteresisMargin();
    appBatteryRegister(&batteryMonitoringForm);

    /* Need to power off when temperature is outside battery's operating range */
    if (!appTemperatureClientRegister(PowerGetTask(),
                                      appConfigBatteryDischargingTemperatureMin(),
                                      appConfigBatteryDischargingTemperatureMax()))
    {
        DEBUG_LOG_WARN("appPowerInit no temperature support");
    }
}

void appPowerHandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);

    if (isMessageUiInput(id))
        return;

#ifdef DEBUG_POWER_MANAGER_MESSAGES
    switch(id)
    {
        case CHARGER_MESSAGE_ATTACHED:
            DEBUG_LOG_V_VERBOSE("appPowerHandleMessage CHARGER_MESSAGE_ATTACHED");
            break;
        case CHARGER_MESSAGE_DETACHED:
            DEBUG_LOG_V_VERBOSE("appPowerHandleMessage CHARGER_MESSAGE_DETACHED");
            break;
        case MESSAGE_BATTERY_LEVEL_UPDATE_STATE:
            {
                MESSAGE_BATTERY_LEVEL_UPDATE_STATE_T *msg = (MESSAGE_BATTERY_LEVEL_UPDATE_STATE_T *)message;
                if(msg)
                {
                    DEBUG_LOG_V_VERBOSE("appPowerHandleMessage MESSAGE_BATTERY_LEVEL_UPDATE_STATE state 0x%x", msg->state);
                }
            }
            break;
        case TEMPERATURE_STATE_CHANGED_IND:
            {
                TEMPERATURE_STATE_CHANGED_IND_T *msg = (TEMPERATURE_STATE_CHANGED_IND_T *)message;
                if(msg)
                {
                    DEBUG_LOG_V_VERBOSE("appPowerHandleMessage TEMPERATURE_STATE_CHANGED_IND state 0x%x", msg->state);
                }
            }
            break;
        default:
            break;
    }
#endif

    switch (id)
    {
        case CHARGER_MESSAGE_ATTACHED:
        case CHARGER_MESSAGE_DETACHED:
        case MESSAGE_BATTERY_LEVEL_UPDATE_STATE:
        case TEMPERATURE_STATE_CHANGED_IND:
            appPowerHandlePowerEvent();
            break;
        default:
            break;
    }
}
