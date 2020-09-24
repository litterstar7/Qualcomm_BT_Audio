/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Charger
*/

#ifndef MEMORY_H_
#define MEMORY_H_

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include "cli.h"

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

#define MEM_RAM_START  0x20000000

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

void mem_init(void);
CLI_RESULT mem_cmd(uint8_t cmd_source);
void mem_stack_dump(uint8_t cmd_source);
void mem_cfg_standby_set(void);
bool mem_cfg_standby(void);

#endif /* MEMORY_H_ */
