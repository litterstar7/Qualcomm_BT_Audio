/*!
\copyright  Copyright (c) 2019-2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Dynamic handover script, which handles handover recommendation other than earbud going in case.
*/

#include "tws_topology_procedure_dynamic_handover.h"
#include "tws_topology_procedure_notify_role_change_clients.h"
#include "script_engine.h"
#include "tws_topology_procedure_handover.h"
#include "tws_topology_procedure_set_role.h"
#include "tws_topology_procedure_enable_le_connectable_handset.h"
#include "tws_topology_procedure_disconnect_le_connections.h"

#define DYNAMIC_HANDOVER_SCRIPT(ENTRY) \
    ENTRY(proc_notify_role_change_clients_fns, PROC_NOTIFY_ROLE_CHANGE_CLIENTS_FORCE_NOTIFICATION), \
    ENTRY(proc_enable_le_connectable_handset_fns, PROC_ENABLE_LE_CONNECTABLE_PARAMS_DISABLE), \
    ENTRY(proc_disconnect_le_connections_fns, NO_DATA), \
    ENTRY(proc_handover_fns, NO_DATA), \
    ENTRY(proc_set_role_fns, PROC_SET_ROLE_TYPE_DATA_SECONDARY),\

/* Define the dynamic handover script */
DEFINE_TOPOLOGY_SCRIPT(dynamic_handover, DYNAMIC_HANDOVER_SCRIPT);

