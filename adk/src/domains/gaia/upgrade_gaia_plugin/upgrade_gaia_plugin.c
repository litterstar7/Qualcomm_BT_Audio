/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for the  upgrade gaia framework plugin
*/

#define DEBUG_LOG_MODULE_NAME upgrade_gaia_plugin
#include <logging.h>
DEBUG_LOG_DEFINE_LEVEL_VAR

#include "upgrade_gaia_plugin.h"

#include <gaia.h>
#include <panic.h>


static void upgradeGaiaPlugin_MessageHandler(Task task, MessageId id, Message message);

static TaskData upgradeGaiaPlugin_task;
static GAIA_TRANSPORT *upgradeGaiaPlugin_transport;


static void upgradeGaiaPlugin_TransportDisconnect(GAIA_TRANSPORT *t)
{
    if (upgradeGaiaPlugin_transport == t)
    {
        DEBUG_LOG_INFO("upgradeGaiaPlugin_TransportDisconnect");
        UpgradeTransportDisconnectRequest();
        upgradeGaiaPlugin_transport = NULL;
    }
    else if (upgradeGaiaPlugin_transport)
        DEBUG_LOG_ERROR("upgradeGaiaPlugin_TransportDisconnect for wrong transport");
}

static void upgradeGaiaPlugin_UpgradeConnect(GAIA_TRANSPORT *t)
{

    /* Only allow connecting upgrade if not already connected */
    if (!upgradeGaiaPlugin_transport)
    {
        DEBUG_LOG_INFO("upgradeGaiaPlugin_UpgradeConnect");
        upgradeGaiaPlugin_task.handler = upgradeGaiaPlugin_MessageHandler;
        upgradeGaiaPlugin_transport = t;

        /* Connect transport task and request UPGRADE_TRANSPORT_DATA_CFM
        * messages and request several blocks at a time */
        UpgradeTransportConnectRequest(&upgradeGaiaPlugin_task, UPGRADE_DATA_CFM_ALL, FALSE);
    }
    else
    {
        DEBUG_LOG_ERROR("upgradeGaiaPlugin_UpgradeConnect, already connected");
        GaiaFramework_SendError(t, GAIA_DFU_FEATURE_ID, upgrade_connect, incorrect_state);
    }
}

static void upgradeGaiaPlugin_UpgradeConnectCfm(UPGRADE_TRANSPORT_CONNECT_CFM_T *cfm)
{
    DEBUG_LOG_INFO("upgradeGaiaPlugin_UpgradeConnectCfm, status %u", cfm->status);
    if (cfm->status == upgrade_status_success)
    {
        GaiaFramework_SendResponse(upgradeGaiaPlugin_transport, GAIA_DFU_FEATURE_ID, upgrade_connect, 0, NULL);
        TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(GaiaGetClientList()), APP_GAIA_UPGRADE_CONNECTED);
    }
    else
    {
        GaiaFramework_SendError(upgradeGaiaPlugin_transport, GAIA_DFU_FEATURE_ID, upgrade_connect, incorrect_state);
        upgradeGaiaPlugin_transport = NULL;
    }
}

static void upgradeGaiaPlugin_UpgradeDisconnect(GAIA_TRANSPORT *transport)
{
    /* Disconnect upgrade if command was on correct transport */
    if (upgradeGaiaPlugin_transport == transport)
    {
        DEBUG_LOG_INFO("upgradeGaiaPlugin_UpgradeDisconnect");

        UpgradeTransportDisconnectRequest();
        TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(GaiaGetClientList()), APP_GAIA_UPGRADE_DISCONNECTED);
    }
    else
    {
        DEBUG_LOG_ERROR("upgradeGaiaPlugin_UpgradeDisconnect, from different transport");
        GaiaFramework_SendError(transport, GAIA_DFU_FEATURE_ID, upgrade_disconnect, incorrect_state);
    }
}

static void upgradeGaiaPlugin_UpgradeDisconnectCfm(UPGRADE_TRANSPORT_DISCONNECT_CFM_T *cfm)
{
    DEBUG_LOG_INFO("upgradeGaiaPlugin_UpgradeDisconnectCfm, status %u", cfm->status);
    if (upgradeGaiaPlugin_transport)
    {
        if (cfm->status == upgrade_status_success)
        {
            GaiaFramework_SendResponse(upgradeGaiaPlugin_transport, GAIA_DFU_FEATURE_ID,
                                       upgrade_disconnect, 0, NULL);
            upgradeGaiaPlugin_transport = NULL;
        }
        else
            GaiaFramework_SendError(upgradeGaiaPlugin_transport, GAIA_DFU_FEATURE_ID,
                                    upgrade_disconnect, incorrect_state);
    }
    else
        DEBUG_LOG_ERROR("upgradeGaiaPlugin_UpgradeDisconnectCfm, no transport");
}

static gaia_framework_command_status_t upgradeGaiaPlugin_UpgradeControl(GAIA_TRANSPORT *transport, uint16 payload_length, const uint8 *payload)
{
    /* Disconnect upgrade if command was on correct transport */
    if (upgradeGaiaPlugin_transport == transport)
    {
        DEBUG_LOG_VERBOSE("upgradeGaiaPlugin_UpgradeControl");
        UpgradeProcessDataRequest(payload_length, (uint8 *)payload);
        return command_pending;
    }
    else
    {
        DEBUG_LOG_ERROR("upgradeGaiaPlugin_UpgradeControl, from different transport");
        return command_not_handled;
    }
}

static void upgradeGaiaPlugin_UpgradeDataInd(UPGRADE_TRANSPORT_DATA_IND_T *ind)
{
    DEBUG_LOG_VERBOSE("upgradeGaiaPlugin_UpgradeDataInd");
    DEBUG_LOG_DATA_V_VERBOSE(ind->data, ind->size_data);

    /* Send notifcation over data endpoint if available otherwise over normal response endpoint */
    GaiaFramework_SendDataNotification(GAIA_DFU_FEATURE_ID,
                                       upgrade_data_indication, ind->size_data, ind->data);
}

static void upgradeGaiaPlugin_UpgradeDataCfm(UPGRADE_TRANSPORT_DATA_CFM_T *cfm)
{
    uint8 status = cfm->status;

    if (upgradeGaiaPlugin_transport)
    {
        DEBUG_LOG_VERBOSE("upgradeGaiaPlugin_UpgradeDataCfm, status %u", status);

        /* Check if response should be sent or not */
//        const bool send_response = (cfm->status != upgrade_status_success) ||  /* Always send response if there has been an error */
//                                   (cfm->packet_type != 0x04) ||               /* TODO: Get this number from somewhere */
//                                   (upgradeGaiaPlugin_SendControlResponse == 1);

        /* Only send response if packet wasn't received over data endpoint */
        const gaia_data_endpoint_mode_t mode = Gaia_GetPayloadDataEndpointMode(upgradeGaiaPlugin_transport, cfm->size_data, cfm->data);
        if (mode == GAIA_DATA_ENDPOINT_MODE_NONE)
        {
            GaiaFramework_SendResponse(upgradeGaiaPlugin_transport, GAIA_DFU_FEATURE_ID,
                                       upgrade_control, sizeof(status), &status);
        }

        Gaia_CommandResponse(upgradeGaiaPlugin_transport, cfm->size_data, cfm->data);
    }
    else
        DEBUG_LOG_INFO("upgradeGaiaPlugin_UpgradeDataCfm, no transport");
}


static void upgradeGaiaPlugin_GetDataEndpoint(GAIA_TRANSPORT *t)
{
    DEBUG_LOG("upgradeGaiaPlugin_GetDataEndpoint");

    uint8 data_endpoint_mode = Gaia_GetDataEndpointMode(t);
    GaiaFramework_SendResponse(upgradeGaiaPlugin_transport, GAIA_DFU_FEATURE_ID,
                               get_data_endpoint, 1, &data_endpoint_mode);
}

static void upgradeGaiaPlugin_SetDataEndpoint(GAIA_TRANSPORT *t,  uint16 payload_length, const uint8 *payload)
{
    if (payload_length >= 1)
    {
        DEBUG_LOG("upgradeGaiaPlugin_SetDataEndpoint, mode %u", payload[0]);

        if (Gaia_SetDataEndpointMode(t, payload[0]))
        {
            GaiaFramework_SendResponse(t, GAIA_DFU_FEATURE_ID,
                                       set_data_endpoint, 0, NULL);
        }
        else
        {
            DEBUG_LOG("upgradeGaiaPlugin_SetDataEndpoint, failed to set mode");
            GaiaFramework_SendError(t, GAIA_DFU_FEATURE_ID,
                                    set_data_endpoint, invalid_parameter);
        }
    }
    else
    {
        DEBUG_LOG("upgradeGaiaPlugin_SetDataEndpoint, no payload");
        GaiaFramework_SendError(t, GAIA_DFU_FEATURE_ID,
                                set_data_endpoint, invalid_parameter);
    }
}

static void upgradeGaiaPlugin_SendAllNotifications(GAIA_TRANSPORT *t)
{
    UNUSED(t);
    DEBUG_LOG("upgradeGaiaPlugin_SendAllNotifications");
}

static void upgradeGaiaPlugin_SendDataIndicationNotification(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    DEBUG_LOG("upgradeGaiaPlugin_SendAllNotifications");
    UNUSED(t);
    /* TODO: Check t is expected transport */

    GaiaFramework_SendNotification(GAIA_DFU_FEATURE_ID,
                                   upgrade_data_indication, payload_length, payload);
}


static gaia_framework_command_status_t upgradeGaiaPlugin_MainHandler(GAIA_TRANSPORT *t, uint8 pdu_id, uint16 payload_length, const uint8 *payload)
{
    DEBUG_LOG("upgradeGaiaPlugin_MainHandler, called for %d", pdu_id);

    switch (pdu_id)
    {
        case upgrade_connect:
            upgradeGaiaPlugin_UpgradeConnect(t);
            return command_handled;

        case upgrade_disconnect:
            upgradeGaiaPlugin_UpgradeDisconnect(t);
            return command_handled;

        case upgrade_control:
            return upgradeGaiaPlugin_UpgradeControl(t, payload_length, payload);

        case get_data_endpoint:
            upgradeGaiaPlugin_GetDataEndpoint(t);
            return command_handled;

        case set_data_endpoint:
            upgradeGaiaPlugin_SetDataEndpoint(t, payload_length, payload);
            return command_handled;

        case send_upgrade_data_indication_notification:
            upgradeGaiaPlugin_SendDataIndicationNotification(t, payload_length, payload);
            return command_handled;

        default:
            DEBUG_LOG_ERROR("upgradeGaiaPlugin_MainHandler, unhandled call for %d", pdu_id);
            return command_not_handled;
    }
}


static void upgradeGaiaPlugin_MessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);
    switch(id)
    {
        /* Response from call to UpgradeTransportConnectRequest() */
        case UPGRADE_TRANSPORT_CONNECT_CFM:
            upgradeGaiaPlugin_UpgradeConnectCfm((UPGRADE_TRANSPORT_CONNECT_CFM_T *)message);
            break;

        /* Response from call to UpgradeTransportDisconnectRequest() */
        case UPGRADE_TRANSPORT_DISCONNECT_CFM:
            upgradeGaiaPlugin_UpgradeDisconnectCfm((UPGRADE_TRANSPORT_DISCONNECT_CFM_T *)message);
            break;

        /* Request from upgrade library to send a data packet to the host */
        case UPGRADE_TRANSPORT_DATA_IND:
            upgradeGaiaPlugin_UpgradeDataInd((UPGRADE_TRANSPORT_DATA_IND_T *)message);
            break;

        case UPGRADE_TRANSPORT_DATA_CFM:
            upgradeGaiaPlugin_UpgradeDataCfm((UPGRADE_TRANSPORT_DATA_CFM_T *)message);
            break;

        default:
            DEBUG_LOG_ERROR("upgradeGaiaPlugin_MessageHandler, unhandled message 0x%04x", id);
            break;
    }
}


void UpgradeGaiaPlugin_Init(void)
{
    static const gaia_framework_plugin_functions_t functions =
    {
        .command_handler = upgradeGaiaPlugin_MainHandler,
        .send_all_notifications = upgradeGaiaPlugin_SendAllNotifications,
        .transport_connect = NULL,
        .transport_disconnect = upgradeGaiaPlugin_TransportDisconnect,
    };

    DEBUG_LOG("UpgradeGaiaPlugin_Init");

    GaiaFramework_RegisterFeature(GAIA_DFU_FEATURE_ID, UPGRADE_GAIA_PLUGIN_VERSION, &functions);
}
