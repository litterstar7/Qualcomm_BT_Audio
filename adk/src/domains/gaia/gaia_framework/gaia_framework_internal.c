/*!
\copyright  Copyright (c) 2017 - 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version
\file
\brief      Handling of the GAIA transport interface

This is a minimal implementation that only supports upgrade.
*/

/*! @todo VMCSA-3689 remove #ifdef */
#ifdef INCLUDE_DFU

#define DEBUG_LOG_MODULE_NAME gaia_framework
#include <logging.h>

#include "gaia_framework_internal.h"

#include "system_state.h"
#include "adk_log.h"
#include "gaia_framework_command.h"
#include "gaia_framework_feature.h"
#include "gatt_handler.h"
#include "gaia_transport.h"
#include <panic.h>


/*!< Task information for GAIA support */
gaiaTaskData    app_gaia;

/*! Enumerated type of internal message IDs used by this module */
typedef enum gaia_handler_internal_messages
{
        /*! Disconnect GAIA */
    APP_GAIA_INTERNAL_DISCONNECT = INTERNAL_MESSAGE_BASE,
} gaia_handler_internal_messages_t;

LOGGING_PRESERVE_MESSAGE_TYPE(gaia_handler_internal_messages_t)
LOGGING_PRESERVE_MESSAGE_TYPE(av_headet_gaia_messages)

typedef struct
{
    GAIA_TRANSPORT *transport;
} APP_GAIA_INTERNAL_DISCONNECT_T;



#define appGaiaGetDisconnectResponseCallbackFn()    GaiaGetTaskData()->disconnect.response
#define appGaiaGetDisconnectResponseCallbackCid()   GaiaGetTaskData()->disconnect.cid


static void gaiaFrameworkInternal_MessageHandler(Task task, MessageId id, Message message);




/********************  PUBLIC FUNCTIONS  **************************/

/*! Initialise the GAIA Module */
bool GaiaFrameworkInternal_Init(Task init_task)
{
    gaiaTaskData *this = GaiaGetTaskData();

    UNUSED(init_task);
    this->gaia_task.handler = gaiaFrameworkInternal_MessageHandler;
    TaskList_InitialiseWithCapacity(GaiaGetClientList(), GAIA_CLIENT_TASK_LIST_INIT_CAPACITY);
    this->connections_allowed = TRUE;

    GaiaInit(GaiaGetTask(), 1);
    return TRUE;
}


void GaiaFrameworkInternal_ClientRegister(Task task)
{
    TaskList_AddTask(TaskList_GetFlexibleBaseTaskList(GaiaGetClientList()), task);
}


static void gaiaFrameworkInternal_SendInitCfm(void)
{
    MessageSend(SystemState_GetTransitionTask(), APP_GAIA_INIT_CFM, NULL);
}

static void gaiaFrameworkInternal_SetDisconnectResponseCallback(uint16 cid, gatt_connect_disconnect_req_response response)
{
    GaiaGetTaskData()->disconnect.response = response;
    GaiaGetTaskData()->disconnect.cid = cid;
}

static void gaiaFrameworkInternal_NotifyGaiaConnected(GAIA_TRANSPORT *transport)
{
    DEBUG_LOG_INFO("gaiaFrameworkInternal_NotifyGaiaConnected");

    /* Tell all registered features that transport has connected */
    GaiaFrameworkFeature_NotifyFeaturesOfConnect(transport);
    TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(GaiaGetClientList()), APP_GAIA_CONNECTED);
}


static void gaiaFrameworkInternal_NotifyGaiaDisconnected(GAIA_TRANSPORT *transport)
{
    DEBUG_LOG_INFO("gaiaFrameworkInternal_NotifyGaiaDisconnected");

    /* Tell all registered features that transport has disconnected */
    GaiaFrameworkFeature_NotifyFeaturesOfDisconnect(transport);
    TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(GaiaGetClientList()), APP_GAIA_DISCONNECTED);

    gatt_connect_disconnect_req_response response = appGaiaGetDisconnectResponseCallbackFn();
    if (response)
    {
        response(appGaiaGetDisconnectResponseCallbackCid());
        gaiaFrameworkInternal_SetDisconnectResponseCallback(0, NULL);
    }
}


static void gaiaFrameworkInternal_HandleInitConfirm(const GAIA_INIT_CFM_T *init_cfm)
{
    DEBUG_LOG("gaiaFrameworkInternal_HandleInitConfirm GAIA_INIT_CFM: %d (succ)",init_cfm->success);

    PanicFalse(init_cfm->success);

    GaiaSetApiMinorVersion(GAIA_API_MINOR_VERSION);
    GaiaSetAppWillHandleCommand(GAIA_COMMAND_DEVICE_RESET, TRUE);

    GaiaSetAppVersion(gaia_app_version_3);

    /* Initialise transporta */
    GaiaTransport_TestInit();
    GaiaTransport_RfcommInit();
    GaiaTransport_GattInit();
#ifdef INCLUDE_ACCESSORY
    GaiaTransport_Iap2Init();
#endif

    /* Start the GAIA transports */
    Gaia_TransportStartService(gaia_transport_spp);
    Gaia_TransportStartService(gaia_transport_test);
    Gaia_TransportStartService(gaia_transport_gatt);
#ifdef INCLUDE_ACCESSORY
    Gaia_TransportStartService(gaia_transport_iap2);
#endif


    /* Successful initialisation of the library. The application needs
     * this to unblock.
     */
    gaiaFrameworkInternal_SendInitCfm();
}


/*  Accept the GAIA connection if they are allowed, and inform any clients.
 */
static void gaiaFrameworkInternal_HandleConnectInd(const GAIA_CONNECT_IND_T *ind)
{
    GAIA_TRANSPORT *transport;

    if (!ind || !ind->success)
    {
        DEBUG_LOG("gaiaFrameworkInternal_HandleConnectInd Success = FAILED");
        return;
    }

    transport = ind->transport;

    if (!transport)
    {
        DEBUG_LOG("gaiaFrameworkInternal_HandleConnectInd No transport");

        /* Can't disconnect nothing so just return */
        return;
    }

    if (!GaiaGetTaskData()->connections_allowed)
    {
        DEBUG_LOG("gaiaFrameworkInternal_HandleConnectInd GAIA not allowed");
        GaiaDisconnectRequest(transport);
        return;
    }

    DEBUG_LOG("gaiaFrameworkInternal_HandleConnectInd Success. Transport:%p",transport);

    //GaiaSetSessionEnable(transport, TRUE);

    gaiaFrameworkInternal_NotifyGaiaConnected(ind->transport);
}


static void gaiaFrameworkInternal_HandleDisconnectInd(const GAIA_DISCONNECT_IND_T *ind)
{
    DEBUG_LOG("gaiaFrameworkInternal_HandleDisconnectInd, transport %p", ind->transport);

    /* GAIA can send IND with a NULL transport. Seemingly after we
       requested a disconnect (?) */
    if (ind->transport)
        GaiaDisconnectResponse(ind->transport);

    gaiaFrameworkInternal_NotifyGaiaDisconnected(ind->transport);
}


static void gaiaFrameworkInternal_HandleDisconnectCfm(const GAIA_DISCONNECT_CFM_T *cfm)
{
    DEBUG_LOG("gaiaFrameworkInternal_HandleDisconnectInd, transport %p", cfm->transport);

    /* TODO: Check success field */
    gaiaFrameworkInternal_NotifyGaiaDisconnected(cfm->transport);
}


static void gaiaFrameworkInternal_InternalDisconnect(const APP_GAIA_INTERNAL_DISCONNECT_T *msg)
{
    /* Check if particular transport should be disconnected */
    if (msg->transport)
        GaiaDisconnectRequest(msg->transport);
    else
    {
        /* Iterate through all transports disconnecting them */
        uint8_t index = 0;
        GAIA_TRANSPORT *t = Gaia_TransportIterate(&index);
        while (t)
        {
            if (Gaia_TransportIsConnected(t))
                GaiaDisconnectRequest(t);
            t = Gaia_TransportIterate(&index);
        }
    }
}


static void gaiaFrameworkInternal_MessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);

    DEBUG_LOG("gaiaFrameworkInternal_MessageHandler MESSAGE:gaia_handler_internal_messages_t:0x%X", id);

    switch (id)
    {
        case APP_GAIA_INTERNAL_DISCONNECT:
            gaiaFrameworkInternal_InternalDisconnect((const APP_GAIA_INTERNAL_DISCONNECT_T *)message);
            break;

        case GAIA_INIT_CFM:
            gaiaFrameworkInternal_HandleInitConfirm((const GAIA_INIT_CFM_T*)message);
            break;

        case GAIA_CONNECT_IND:                   /* Indication of an inbound connection */
            gaiaFrameworkInternal_HandleConnectInd((const GAIA_CONNECT_IND_T *)message);
            break;

        case GAIA_DISCONNECT_IND:                /* Indication that the connection has closed */
            gaiaFrameworkInternal_HandleDisconnectInd((const GAIA_DISCONNECT_IND_T *)message);
            break;

        case GAIA_DISCONNECT_CFM:                /* Confirmation that a requested disconnection has completed */
            gaiaFrameworkInternal_HandleDisconnectCfm((const GAIA_DISCONNECT_CFM_T *)message);
            break;

        case GAIA_START_SERVICE_CFM:             /* Confirmation that a Gaia server has started */
            DEBUG_LOG("gaiaFrameworkInternal_MessageHandler GAIA_START_SERVICE_CFM (nothing to do)");
            break;


        case GAIA_COMMAND_IND:            /* Indication that an unhandled command has been received */
        {
            const GAIA_COMMAND_IND_T *ind =(const GAIA_COMMAND_IND_T *)message;
            GaiaFrameworkCommand_CommandHandler(ind);
        }
        break;

        case GAIA_CONNECT_CFM:                   /* Confirmation of an outbound connection request */
            DEBUG_LOG("gaiaFrameworkInternal_MessageHandler GAIA_CONNECT_CFM Unexpected");
            break;

        default:
            DEBUG_LOG("gaiaFrameworkInternal_MessageHandler Unknown GAIA message MESSAGE:gaia_handler_internal_messages_t:0x%x",
                      id);
            break;
    }
}



/*! \brief Disconnect any active gaia connection
 */
void gaiaFrameworkInternal_GaiaDisconnect(GAIA_TRANSPORT *t)
{
    MESSAGE_MAKE(msg, APP_GAIA_INTERNAL_DISCONNECT_T);
    msg->transport = t;
    MessageSend(GaiaGetTask(), APP_GAIA_INTERNAL_DISCONNECT, msg);
}


void gaiaFrameworkInternal_AllowNewConnections(bool allow)
{
    GaiaGetTaskData()->connections_allowed = allow;
}


#endif /* INCLUDE_DFU */
