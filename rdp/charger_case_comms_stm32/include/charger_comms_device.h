/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Charger
*/

#ifndef CHARGER_COMMS_DEVICE_H_
#define CHARGER_COMMS_DEVICE_H_

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include "cli.h"

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

extern void charger_comms_device_init(void);
extern void charger_comms_periodic(void);
extern CLI_RESULT debug_cmd(uint8_t cmd_source);

#endif /* CHARGER_COMMS_DEVICE_H_ */
