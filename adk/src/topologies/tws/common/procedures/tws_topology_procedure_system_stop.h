/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      
*/

#ifndef TWS_TOPOLOGY_PROC_SYSTEM_STOP_H
#define TWS_TOPOLOGY_PROC_SYSTEM_STOP_H

#include "script_engine.h"
#include "tws_topology_private.h"
#include "tws_topology_procedures.h"

typedef enum
{
    PROC_SEND_TOPOLOGY_MESSAGE_SYSTEM_STOP_FINISHED = TWSTOP_INTERNAL_PROCEDURE_RESULTS_MSG_BASE | tws_topology_procedure_system_stop,
} procSystemStopMessages;


extern const procedure_script_t system_stop_script;

#endif /* TWS_TOPOLOGY_PROC_SYSTEM_STOP_H */
