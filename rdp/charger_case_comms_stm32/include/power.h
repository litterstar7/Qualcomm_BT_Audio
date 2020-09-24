/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Power modes
*/

#ifndef POWER_H_
#define POWER_H_

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include <stdint.h>
#include "cli.h"

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

/*
* Reasons to run.
*/
#define POWER_RR_UART_RX       0x0001
#define POWER_RR_UART_TX       0x0002
#define POWER_RR_CHARGER_COMMS 0x0004
#define POWER_RR_DEBUG         0x0008
#define POWER_RR_CASE_EVENT    0x0010
#define POWER_RR_STATUS_L      0x0020
#define POWER_RR_STATUS_R      0x0040
#define POWER_RR_WATCHDOG      0x0080
#define POWER_RR_DFU           0x0100
#define POWER_RR_BROADCAST     0x0200
#define POWER_RR_USB_RX        0x0400
#define POWER_RR_USB_TX        0x0800
#define POWER_RR_LED           0x1000
#define POWER_RR_FORCE_ON      0x2000

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

void power_enter_standby(void);
void power_enter_sleep(void);
void power_enter_stop(void);
void power_periodic(void);
void power_set_run_reason(uint16_t reason);
void power_clear_run_reason(uint16_t reason);
CLI_RESULT power_cmd_standby(uint8_t cmd_source);
CLI_RESULT power_cmd(uint8_t cmd_source);

#endif /* POWER_H_ */
