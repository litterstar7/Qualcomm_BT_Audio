    /*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Headset Topology component core.
*/

#include "headset_topology.h"
#include "headset_topology_private.h"
#include "headset_topology_config.h"
#include "headset_topology_rules.h"
#include "headset_topology_goals.h"
#include "headset_topology_client_msgs.h"
#include "headset_topology_private.h"
#include "headset_topology_procedure_system_stop.h"


#include "core/headset_topology_rules.h"

#include <logging.h>

#include <handset_service.h>
#include <bredr_scan_manager.h>
#include <connection_manager.h>
#include <power_manager.h>
#include <pairing.h>

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_TYPE(headset_topology_message_t)
LOGGING_PRESERVE_MESSAGE_TYPE(hs_topology_internal_message_t)
LOGGING_PRESERVE_MESSAGE_ENUM(headset_topology_goals)

/*! Instance of the headset Topology. */
headsetTopologyTaskData headset_topology = {0};

/*! \brief Generate handset related Connection events */
static void headsetTopology_HandleHandsetServiceConnectedInd(const HANDSET_SERVICE_CONNECTED_IND_T* ind)
{
    DEBUG_LOG_VERBOSE("headsetTopology_HandleHandsetConnectedInd %04x,%02x,%06lx", ind->addr.nap,
                                                                           ind->addr.uap,
                                                                           ind->addr.lap);

    HeadsetTopologyRules_SetEvent(HSTOP_RULE_EVENT_HANDSET_CONNECTED_BREDR);
}

static void headsetTopology_HandlePairingActivity(const PAIRING_ACTIVITY_T *message)
{
    UNUSED(message);
    DEBUG_LOG_VERBOSE("headsetTopology_HandlePairingActivity status=enum:pairingActivityStatus:%d",
                        message->status);
}

/*! \brief Take action following power's indication of imminent shutdown.*/
static void headsetTopology_HandlePowerShutdownPrepareInd(void)
{
    headsetTopologyTaskData *headset_taskdata = HeadsetTopologyGetTaskData();

    DEBUG_LOG_VERBOSE("headsetTopology_HandlePowerShutdownPrepareInd");
    /* Headset should stop being connectable during shutdown. */
    headset_taskdata->shutdown_in_progress = TRUE;
    appPowerShutdownPrepareResponse(HeadsetTopologyGetTask());
}

/*! \brief Generate handset related disconnection events . */
static void headsetTopology_HandleHandsetServiceDisconnectedInd(const HANDSET_SERVICE_DISCONNECTED_IND_T* ind)
{
    DEBUG_LOG_VERBOSE("headsetTopology_HandleHandsetServiceDisconnectedInd %04x,%02x,%06lx status %u", ind->addr.nap,
                                                                                               ind->addr.uap,
                                                                                               ind->addr.lap,
                                                                                               ind->status);

    if(ind->status == handset_service_status_link_loss)
    {
        HeadsetTopologyRules_SetEvent(HSTOP_RULE_EVENT_HANDSET_LINKLOSS);
    }

    else if(ind->status == handset_service_status_disconnected)
    {
        HeadsetTopologyRules_SetEvent(HSTOP_RULE_EVENT_HANDSET_DISCONNECTED_BREDR);
    }
}

/*! \brief Generate LE related disconnection events . */
static void headsetTopology_HandleConnectionInd(const CON_MANAGER_CONNECTION_IND_T* ind)
{

    if(ind->ble) 
    {
        DEBUG_LOG_VERBOSE("headsetTopology_HandleConnectionInd %04x,%02x,%06lx status %u", ind->bd_addr.nap,
                                                                                                   ind->bd_addr.uap,
                                                                                                   ind->bd_addr.lap,
                                                                                                   ind->connected);
    }
}

static void headsetTopology_HandleInternalStop(void)
{
    DEBUG_LOG_FN_ENTRY("headsetTopology_HandleInternalStop");

    HeadsetTopologyRules_SetEvent(HSTOP_RULE_EVENT_STOP);
}

static void headsetTopology_MarkAsStopped(void)
{
    headsetTopologyTaskData *hstop_taskdata = HeadsetTopologyGetTaskData();

    hstop_taskdata->started = FALSE;
    hstop_taskdata->app_task = NULL;
    hstop_taskdata->stopping = hstop_stop_state_stopped;
}

static void headsetTopology_HandleStopTimeout(void)
{
    DEBUG_LOG_FN_ENTRY("headsetTopology_HandleStopTimeout");

    HeadsetTopology_SendStopCfm(hs_topology_status_fail);
    headsetTopology_MarkAsStopped();
}

static void headsetTopology_HandleStopCompletion(void)
{
    if (HeadsetTopologyGetTaskData()->stopping == hstop_stop_state_stopping)
    {
        DEBUG_LOG_FN_ENTRY("headsetTopology_HandleStopCompletion");

        /* Send the stop message before clearing the app task below */
        HeadsetTopology_SendStopCfm(hs_topology_status_success);
        headsetTopology_MarkAsStopped();
    }
}


/*! \brief Headset Topology message handler.
 */
static void headsetTopology_HandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);
    DEBUG_LOG_VERBOSE("headsetTopology_HandleMessage. message id %d(0x%x)",id,id);

    /* handle messages */
    switch (id)
    {
        case PAIRING_ACTIVITY:
            headsetTopology_HandlePairingActivity(message);
            break;

        case HANDSET_SERVICE_CONNECTED_IND:
            headsetTopology_HandleHandsetServiceConnectedInd((HANDSET_SERVICE_CONNECTED_IND_T*)message);
            break;

        case HANDSET_SERVICE_DISCONNECTED_IND:
            DEBUG_LOG_DEBUG("headsetTopology_HandleMessage: HANDSET_SERVICE_DISCONNECTED_IND");
            headsetTopology_HandleHandsetServiceDisconnectedInd((HANDSET_SERVICE_DISCONNECTED_IND_T*)message);
            break;

        case CON_MANAGER_CONNECTION_IND:
            DEBUG_LOG_DEBUG("headsetTopology_HandleMessage. CON_MANAGER_CONNECTION_IND message %d(0x%x)",id,id);
            headsetTopology_HandleConnectionInd((CON_MANAGER_CONNECTION_IND_T*)message);
            break;

        /* Power indications */
        case APP_POWER_SHUTDOWN_PREPARE_IND:
            headsetTopology_HandlePowerShutdownPrepareInd();
            DEBUG_LOG_VERBOSE("headsetTopology_HandleMessage APP_POWER_SHUTDOWN_PREPARE_IND");
            break;

        case HSTOP_INTERNAL_STOP:
            DEBUG_LOG_VERBOSE("headsetTopology_HandleMessage HSTOP_INTERNAL_STOP");
            headsetTopology_HandleInternalStop();
            break;

        case HSTOP_INTERNAL_TIMEOUT_TOPOLOGY_STOP:
            headsetTopology_HandleStopTimeout();
            break;

        case PROC_SEND_HS_TOPOLOGY_MESSAGE_SYSTEM_STOP_FINISHED:
            headsetTopology_HandleStopCompletion();
            break;

        default:
            DEBUG_LOG_VERBOSE("headsetTopology_HandleMessage. Unhandled message %d(0x%x)",id,id);
            break;
      }
}


bool HeadsetTopology_Init(Task init_task)
{
    UNUSED(init_task);

    headsetTopologyTaskData *headset_taskdata = HeadsetTopologyGetTaskData();
    headset_taskdata->task.handler = headsetTopology_HandleMessage;
    headset_taskdata->goal_task.handler = HeadsetTopology_HandleGoalDecision;
    headset_taskdata->prohibit_connect_to_handset = FALSE;
    headset_taskdata->started = FALSE;
    headset_taskdata->shutdown_in_progress = FALSE;
    headset_taskdata->stopping = hstop_stop_state_not_stopping;

    /*Initialize Headset topology's goals and rules */
    
    HeadsetTopologyRules_Init(HeadsetTopologyGetGoalTask());
    HeadsetTopology_GoalsInit();

    /* Register with power to receive shutdown messages. */
    appPowerClientRegister(HeadsetTopologyGetTask());
    /* Allow topology to sleep */
    appPowerClientAllowSleep(HeadsetTopologyGetTask());

    /* register with handset service as we need disconnect and connect notification */
    HandsetService_ClientRegister(HeadsetTopologyGetTask());
    ConManagerRegisterConnectionsClient(HeadsetTopologyGetTask());
    Pairing_ActivityClientRegister(HeadsetTopologyGetTask());
    BredrScanManager_PageScanParametersRegister(&hs_page_scan_params);
    BredrScanManager_InquiryScanParametersRegister(&hs_inquiry_scan_params);

    TaskList_InitialiseWithCapacity(HeadsetTopologyGetMessageClientTasks(), MESSAGE_CLIENT_TASK_LIST_INIT_CAPACITY);

    return TRUE;
}


bool HeadsetTopology_Start(Task requesting_task)
{
    UNUSED(requesting_task);

    headsetTopologyTaskData *hs_topology = HeadsetTopologyGetTaskData();

    if (!hs_topology->started)
    {
        DEBUG_LOG("HeadsetTopology_Start (normal start)");
        hs_topology->started = TRUE;
        hs_topology->stopping = hstop_stop_state_not_stopping;
        hs_topology->prohibit_connect_to_handset = FALSE;
        hs_topology->shutdown_in_progress = FALSE;
        /* Set the rule to get the headset rolling (EnableConnectable, AllowHandsetConnect) */
        HeadsetTopologyRules_SetEvent(HSTOP_RULE_EVENT_START);
    }
    else
    {
        DEBUG_LOG("HeadsetTopology_Start:Topology already started");
    }

    return TRUE;
}


void HeadsetTopology_RegisterMessageClient(Task client_task)
{
   TaskList_AddTask(TaskList_GetFlexibleBaseTaskList(HeadsetTopologyGetMessageClientTasks()), client_task);
}


void HeadsetTopology_UnRegisterMessageClient(Task client_task)
{
   TaskList_RemoveTask(TaskList_GetFlexibleBaseTaskList(HeadsetTopologyGetMessageClientTasks()), client_task);
}


void HeadsetTopology_ProhibitHandsetConnection(bool prohibit)
{
    HeadsetTopologyGetTaskData()->prohibit_connect_to_handset = prohibit;

    if(prohibit)
    {
        HeadsetTopologyRules_SetEvent(HSTOP_RULE_EVENT_PROHIBIT_CONNECT_TO_HANDSET);
    }
    return;
}


bool HeadsetTopology_Stop(Task requesting_task)
{
    headsetTopologyTaskData *headset_taskdata = HeadsetTopologyGetTaskData();
    bool already_started = headset_taskdata->started;

    /* Set started to make sure messages are handled */
    headset_taskdata->app_task = requesting_task;

    if (!already_started)
    {
        DEBUG_LOG_WARN("HeadsetTopology_Stop - already stopped");

        headset_taskdata->started = FALSE;
        headset_taskdata->app_task = requesting_task;

        HeadsetTopology_SendStopCfm(hs_topology_status_success);

    }
    else 
    {
        uint32 timeout_ms = D_SEC(HeadsetTopologyConfig_HeadsetTopologyStopTimeoutS());
        DEBUG_LOG_DEBUG("HeadsetTopology_Stop(). Timeout:%u", timeout_ms);

        headset_taskdata->started = TRUE;

        MessageSend(HeadsetTopologyGetTask(), HSTOP_INTERNAL_STOP, NULL);
        if (timeout_ms)
        {
            MessageSendLater(HeadsetTopologyGetTask(),
                             HSTOP_INTERNAL_TIMEOUT_TOPOLOGY_STOP, NULL,
                             timeout_ms);
        }
    }

    return TRUE;
}


void headsetTopology_StopHasStarted(void)
{
    DEBUG_LOG_FN_ENTRY("headsetTopology_StopHasStarted");

    HeadsetTopologyGetTaskData()->stopping = hstop_stop_state_stopping;
}

bool headsetTopology_IsRunning(void)
{
    headsetTopologyTaskData *headsett = HeadsetTopologyGetTaskData();

    return headsett->app_task && headsett->started;
}


