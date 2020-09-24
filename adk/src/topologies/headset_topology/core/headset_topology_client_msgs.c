/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Headset Topology utility functions for sending messages to clients.
*/

#include "headset_topology.h"
#include "headset_topology_private.h"
#include "headset_topology_client_msgs.h"

#include <logging.h>
#include <task_list.h>
#include <panic.h>

void HeadsetTopology_SendHandsetDisconnectedIndication(const HANDSET_SERVICE_DISCONNECT_CFM_T *cfm)
{
    DEBUG_LOG_VERBOSE("HeadsetTopology_SendHandsetDisconnectedIndication");
    MESSAGE_MAKE(msg, HEADSET_TOPOLOGY_HANDSET_DISCONNECTED_IND_T);
    msg->addr    = cfm->addr;
    msg->status = cfm->status;
    TaskList_MessageSend(TaskList_GetFlexibleBaseTaskList(HeadsetTopologyGetMessageClientTasks()), HEADSET_TOPOLOGY_HANDSET_DISCONNECTED_IND, msg);
}


void HeadsetTopology_SendStopCfm(headset_topology_status_t status)
{
    headsetTopologyTaskData *headset_taskdata = HeadsetTopologyGetTaskData();
    MAKE_HEADSET_TOPOLOGY_MESSAGE(HEADSET_TOPOLOGY_STOP_CFM);

    DEBUG_LOG_VERBOSE("HeadsetTopology_SendStopCfm status %u", status);

    MessageCancelAll(HeadsetTopologyGetTask(), HSTOP_INTERNAL_TIMEOUT_TOPOLOGY_STOP);

    message->status = status;
    MessageSend(headset_taskdata->app_task, HEADSET_TOPOLOGY_STOP_CFM, message);
}

