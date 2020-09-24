/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Functions for setting up and shutting down Gaia data transfer channel.
*/

#include "gaia_core_plugin.h"
#include "gaia_framework_data_channel.h"

#include <panic.h>
#include <stdlib.h>

/* Enable debug log outputs with per-module debug log levels.
 * The log output level for this module can be changed with the PyDbg command:
 *      >>> apps1.log_level("gaia_fw_data_ch", 3)
 * Where the second parameter value means:
 *      0:ERROR, 1:WARN, 2:NORMAL(= INFO), 3:VERBOSE(= DEBUG), 4:V_VERBOSE, 5:V_V_VERBOSE
 * See 'logging.h' and PyDbg 'log_level()' command descriptions for details.
 */
#define DEBUG_LOG_MODULE_NAME gaia_fw_data_ch
#include <logging.h>
DEBUG_LOG_DEFINE_LEVEL_VAR

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_ENUM(gaia_data_ch_internal_messages)

#ifdef ENABLE_GAIA_FRAMEWORK_DATA_CH_PANIC
#define GAIA_FRAMEWORK_DATA_CH_PANIC() Panic()
#else
#define GAIA_FRAMEWORK_DATA_CH_PANIC()
#endif


/*! \brief Types of transport
*/
typedef enum
{
    /*! No data channel is set up yet. */
    data_ch_status_none = 0,
    /*! Allocation of data channel has been requested. */
    data_ch_status_allocating,
    /*! A data chanel is listening for incoming connect_req. */
    data_ch_status_listening,
    /*! The SDP record of the data channel is visible to the peer. */
    data_ch_status_listening_and_visible,
    /*! The data channel is established but the SDP record is not withdrawn yet. */
    data_ch_status_connected,
    /*! The data channel is established and the SDP record is being withdrawn. */
    data_ch_status_connected_removing_sdp,
    /*! Ready for data transfer. */
    data_ch_status_data_transfer_ready,
    /*! Deallocation of data channel has been requested. */
    data_ch_status_deallocating,

    /*! Total number of the data channel states. */
    number_of_data_ch_state,
} data_transfer_channel_states_t;


/*! \brief Session instance that binds a Session ID to a Gaia Feature.
*/
typedef struct __session_instance_t
{
    /*! A unique 16-bit ID that distinguishes each data transfer session. */
    gaia_data_transfer_session_id_t session_id;
    /*! Feature ID, which tells a Gaia Feature being bound with this session. */
    uint8                           feature_id;

    /*! Task of the Gaia Feature, where Gaia data transfer channel messages are forwarded to. */
    TaskData                        handler_task;

    /*! Transport type specified by Gaia 'Data Transfer Setup' command. */
    core_plugin_transport_type_t    transport_type;

    GAIA_TRANSPORT *                transport;

    /*! Transform of a stream if in use, otherwise this is NULL. */
    Transform                       data_channel_transform;

    struct __session_instance_t *next;
} session_instance_t;

/*! \brief Session queue for serialising requests and their responsees.
*/
typedef struct __session_queue_t
{
    /*! A unique 16-bit ID that distinguishes each data transfer session. */
    gaia_data_transfer_session_id_t session_id;

    /*! Pointer to the next element. */
    struct __session_queue_t    *next;
} session_queue_t;


/*! \brief Get the Gaia Framework status code from a data transfer status.

    \param status_code  Status code of a data transfer session.

    \return             Gaia Framework status code.
*/
static uint8 gaiaFrameworkDataChannel_GetGaiaStatusFromDataTransferStatus(data_transfer_status_code_t status_code);


/*! \brief Find the session instance for the Gaia Feature (with a Session ID as the key).

    \param feature_id   Feautue ID that represents a Gaia Feature, which a data
                        transfer session is bound to.

    \return Returns the the pointer to the session instance if exists, otherwise NULL.
*/
static session_instance_t* gaiaFrameworkDataChannel_FindSessionInstance(gaia_data_transfer_session_id_t session_id);


/*! \brief Return the first Session Instance of the linked list.
*/
static session_instance_t* gaiaFrameworkDataChannel_GetFirstSessionInstance(void);


/*! \brief Add a session instance to the list.

    \param session_id       A 16-bit ID that represents a data transfer session.

    \param feature_id       Feautue ID that represents a Gaia Feature, which will
                            be bound with the data transfer session.

    \param handler_task     Task to forward Gaia data transfer channel messages to.

    \return Returns TRUE on success, otherwise FALSE.
*/
static bool gaiaFrameworkDataChannel_AddSessionInstance(GAIA_TRANSPORT *t, gaia_data_transfer_session_id_t session_id, uint8 feature_id, Task handler_task);


/*! \brief Delete the specified session instance from the linked list.

    \param session_id   A 16-bit ID that represents a data transfer session.

    \return Returns TRUE on success, otherwise FALSE.
*/
static bool gaiaFrameworkDataChannel_DeleteSessionInstance(gaia_data_transfer_session_id_t session_id);


/*! \brief Handle #GAIA_DATA_TRANSFER_INTERNAL_SESSION_START_CFM message from a Gaia
           Feature's data transfer handler.

    \param cfm  #GAIA_DATA_TRANSFER_INTERNAL_SESSION_START_CFM message.

    \return TRUE on success, otherwise FALSE.

    This function handles the acknowledgement to #GAIA_DATA_TRANSFER_INTERNAL_SESSION_START_IND
    and just does some sanity checks.
*/
static bool gaiaFrameworkDataChannel_HandleGaiaDataTransferSessionStartCfm(GAIA_DATA_TRANSFER_INTERNAL_SESSION_START_CFM_T *cfm);


/*! \brief Handle #GAIA_DATA_TRANSFER_INTERNAL_GET_RSP message from a Gaia
           Feature's data transfer handler.

    \param rsp  #GAIA_DATA_TRANSFER_INTERNAL_GET_RSP message.

    \return TRUE on success, otherwise FALSE.

    This function receives the data bytes requested by Gaia 'Data Transfer Get'
    command, and forward them to the mobile app.
*/
static bool gaiaFrameworkDataChannel_HandleGaiaDataTransferGetRsp(GAIA_DATA_TRANSFER_INTERNAL_GET_RSP_T* rsp);


/*! \brief Handle #GAIA_DATA_TRANSFER_INTERNAL_SET_RSP message from a Gaia
           Feature's data transfer handler.

    \param rsp  #GAIA_DATA_TRANSFER_INTERNAL_SET_RSP message.

    \return TRUE on success, otherwise FALSE.

    This function receives the acknowledgement to #GAIA_DATA_CH_INTERNAL_DATA_SET_REQ
    from a Gaia Feature's data transfer, and forward it to the mobile app.
*/
static bool gaiaFrameworkDataChannel_HandleGaiaDataTransferSetRsp(GAIA_DATA_TRANSFER_INTERNAL_SET_RSP_T* rsp);


/*! \brief Send 'Data Transfer Setup' Response to the mobile app.

    \param session_id   A 16-bit ID that represents a data transfer session.

    This function sends a Response to the 'Data Transfer Setup' command to the
    mobile app.
*/
static void gaiaFrameworkDataChannel_SendDataTransferSetupResponse(gaia_data_transfer_session_id_t session_id);



/*! \brief Task that receives Gaia Data Transfer internal messages. */
static TaskData gaia_framework_data_transfer_internal_message_handler = { GaiaFramework_DataTransferInternalMessageHandler };

/*! \brief The root of the Session Instance linked list. */
static session_instance_t *session_instance_linked_list = NULL;



static uint8 gaiaFrameworkDataChannel_GetGaiaStatusFromDataTransferStatus(data_transfer_status_code_t status_code)
{
    uint8 gaia_fw_status = GAIA_STATUS_INCORRECT_STATE;

    switch (status_code)
    {
        case data_transfer_status_success:
            gaia_fw_status = GAIA_STATUS_SUCCESS;
            break;
        case data_transfer_no_more_data:
            gaia_fw_status = GAIA_STATUS_SUCCESS;   /* NB: 'No more data' is a normal case. */
            break;
        case data_transfer_invalid_parameter:
            gaia_fw_status = GAIA_STATUS_INVALID_PARAMETER;
            break;
        case data_transfer_status_insufficient_resource:
            gaia_fw_status = GAIA_STATUS_INSUFFICIENT_RESOURCES;
            break;

        case data_transfer_status_failure_with_unspecified_reason:
            /* Fall through */
        case data_transfer_status_failed_to_get_data_info:
            /* Fall through */
        case data_transfer_status_invalid_sink:
            /* Fall through */
        case data_transfer_status_invalid_source:
            /* Fall through */
        case data_transfer_status_invalid_stream:
            gaia_fw_status = GAIA_STATUS_INCORRECT_STATE;
            break;

        default:
            gaia_fw_status = GAIA_STATUS_INCORRECT_STATE;
            DEBUG_LOG_ERROR("GaiaFW DataTransfer: PANIC Unknown DataTransfer Status code: %d", status_code);
            GAIA_FRAMEWORK_DATA_CH_PANIC();
            break;
    }

    return gaia_fw_status;
}


static session_instance_t* gaiaFrameworkDataChannel_FindSessionInstance(gaia_data_transfer_session_id_t session_id)
{
    session_instance_t *instance = session_instance_linked_list;

    while (instance != NULL)
    {
        if (instance->session_id == session_id)
        {
            return instance;
        }
        instance = instance->next;
    }
    return NULL;
}


static session_instance_t* gaiaFrameworkDataChannel_GetFirstSessionInstance(void)
{
    return session_instance_linked_list;
}


static bool gaiaFrameworkDataChannel_AddSessionInstance(GAIA_TRANSPORT *t, gaia_data_transfer_session_id_t session_id, uint8 feature_id, Task handler_task)
{
    session_instance_t *instance = session_instance_linked_list;

    /* Look for the end of the linked list. */
    while (instance != NULL)
    {
        if (instance->next == NULL)
        {
            break;
        }
        instance = instance->next;
    }

    {
        session_instance_t *sr;

        sr = (session_instance_t*) PanicUnlessMalloc(sizeof(session_instance_t));
        sr->session_id = session_id;
        sr->feature_id = feature_id;
        sr->handler_task = *handler_task;
        sr->transport = t;
        sr->next = NULL;

        if (session_instance_linked_list == NULL)
        {
            session_instance_linked_list = sr;
        }
        else
        {
            instance->next = sr;
        }
    }
    return TRUE;
}


static bool gaiaFrameworkDataChannel_DeleteSessionInstance(gaia_data_transfer_session_id_t session_id)
{
    session_instance_t *instance = session_instance_linked_list;
    session_instance_t *prev = NULL;

    while (instance != NULL)
    {
        if (instance->session_id == session_id)
        {
            if (prev == NULL)
            {
                session_instance_linked_list = NULL;
            }
            else
            {
                prev->next = instance->next;
            }
            free(instance);
            return TRUE;
        }
        prev = instance;
        instance = instance->next;
    }
    return FALSE;
}


static bool gaiaFrameworkDataChannel_HandleGaiaDataTransferSessionStartCfm(GAIA_DATA_TRANSFER_INTERNAL_SESSION_START_CFM_T *cfm)
{
    bool result = FALSE;

    if (cfm->status == data_transfer_status_success)
    {
        session_instance_t  *instance;

        instance = gaiaFrameworkDataChannel_FindSessionInstance(cfm->session_id);
        if (instance != NULL)
        {
            DEBUG_LOG_DEBUG("GaiaFW DataTransfer: SESSION_START_CFM OK, Session ID: 0x%04X", cfm->session_id);
            instance->data_channel_transform = cfm->transform;

            /* 
             *  SESSION_START_CFM does not result in sending any messages to the peer device.
             */

            result = TRUE;
        }
        else
        {
            DEBUG_LOG_ERROR("GaiaFW DataTransfer: PANIC SESSION_START_CFM, Session ID: 0x%04X has no instance!", cfm->session_id);
            GAIA_FRAMEWORK_DATA_CH_PANIC();
        }
    }
    else
    {
        DEBUG_LOG_ERROR("GaiaFW DataTransfer: PANIC SESSION_START_CFM->status:%d", cfm->status);
        GAIA_FRAMEWORK_DATA_CH_PANIC();
    }
    return result;
}


static bool gaiaFrameworkDataChannel_HandleGaiaDataTransferGetRsp(GAIA_DATA_TRANSFER_INTERNAL_GET_RSP_T* rsp)
{
    uint8 gaia_status = GAIA_STATUS_INCORRECT_STATE;

    if (rsp->status == data_transfer_status_success || rsp->status == data_transfer_no_more_data)
    {
        session_instance_t  *instance;

        instance = gaiaFrameworkDataChannel_FindSessionInstance(rsp->session_id);
        if (instance != NULL)
        {
            /* This code re-uses the 'rsp' message body rather than copying the data bytes.
             * Thus, the code assumes that 'rsp->data_size' is 16-bit and followed by 'rsp->data'.
             * And 'rsp->data_size' value is overwritten by the Session ID. */
            uint8 *rsp_payload = (uint8*) &rsp->data_size;
            uint8 length = sizeof(rsp->data_size) + rsp->data_size;

            /* Send Response to Gaia 'Data Transfer Get' command. */
            DEBUG_LOG_DEBUG("GaiaFW DataTransfer: DATA_TRANSFER_GET OK, Session ID: 0x%04X", rsp->session_id);
            rsp_payload[0] = (uint8) ((rsp->session_id & 0xFF00) >> 8);
            rsp_payload[1] = (uint8)  (rsp->session_id & 0x00FF);
            GaiaFramework_SendResponse(instance->transport, GAIA_CORE_FEATURE_ID, data_transfer_get, length, rsp_payload);
            gaia_status = GAIA_STATUS_SUCCESS;
        }
        else
        {
            /* This should never happen as the existance of the instance has
             * been already checked by 'GaiaFramework_DataTransferGet(...)'. */
            DEBUG_LOG_ERROR("GaiaFW DataTransfer: PANIC DATA_TRANSFER_GET, Session ID: 0x%04X has no instance!", rsp->session_id);
            GAIA_FRAMEWORK_DATA_CH_PANIC();
        }
    }

    if (gaia_status != GAIA_STATUS_SUCCESS)
    {
        session_instance_t  *instance;
        instance = gaiaFrameworkDataChannel_FindSessionInstance(rsp->session_id);
        if (instance != NULL)
        {
            gaia_status = gaiaFrameworkDataChannel_GetGaiaStatusFromDataTransferStatus(rsp->status);
            GaiaFramework_SendError(instance->transport, GAIA_CORE_FEATURE_ID, data_transfer_get, gaia_status);
            DEBUG_LOG_ERROR("GaiaFW DataTransfer: ERROR! DATA_TRANSFER_GET->status:%d, GaiaStatus:%d", rsp->status, gaia_status);
        }
        else
        {
            /* This should never happen as the existance of the instance has
             * been already checked by 'GaiaFramework_DataTransferGet(...)'. */
            DEBUG_LOG_ERROR("GaiaFW DataTransfer: PANIC DATA_TRANSFER_GET, Session ID: 0x%04X has no instance!", rsp->session_id);
            GAIA_FRAMEWORK_DATA_CH_PANIC();
        }

        return FALSE;
    }

    return TRUE;
}


static bool gaiaFrameworkDataChannel_HandleGaiaDataTransferSetRsp(GAIA_DATA_TRANSFER_INTERNAL_SET_RSP_T* rsp)
{
    bool result = FALSE;

    if (rsp->status == data_transfer_status_success)
    {
        session_instance_t  *instance;

        instance = gaiaFrameworkDataChannel_FindSessionInstance(rsp->session_id);
        if (instance != NULL)
        {
            uint8 rsp_payload[2];

            /* Send Response to Gaia 'Data Transfer Set' command. */
            DEBUG_LOG_DEBUG("GaiaFW DataTransfer: DATA_TRANSFER_SET OK, Session ID: 0x%04X", rsp->session_id);
            rsp_payload[0] = (uint8) ((rsp->session_id & 0xFF00) >> 8);
            rsp_payload[1] = (uint8)  (rsp->session_id & 0x00FF);
            GaiaFramework_SendResponse(instance->transport, GAIA_CORE_FEATURE_ID, data_transfer_set, sizeof(rsp_payload), rsp_payload);
            result = TRUE;
        }
        else
        {
            DEBUG_LOG_ERROR("GaiaFW DataTransfer: PANIC DATA_TRANSFER_SET, Session ID: 0x%04X has no instance!", rsp->session_id);
            GAIA_FRAMEWORK_DATA_CH_PANIC();
        }
    }
    else
    {
        DEBUG_LOG_ERROR("GaiaFW DataTransfer: PANIC DATA_TRANSFER_SET->status:%d", rsp->status);
        GAIA_FRAMEWORK_DATA_CH_PANIC();
    }
    return result;
}


static void gaiaFrameworkDataChannel_SendDataTransferSetupResponse(gaia_data_transfer_session_id_t session_id)
{
    session_instance_t  *instance;
    instance = gaiaFrameworkDataChannel_FindSessionInstance(session_id);
    if (instance != NULL)
    {
        uint8 rsp_payload[2];

        rsp_payload[0] = (uint8) ((session_id & 0xFF00) >> 8);
        rsp_payload[1] = (uint8)  (session_id & 0x00FF);

        GaiaFramework_SendResponse(instance->transport, GAIA_CORE_FEATURE_ID, data_transfer_setup, sizeof(rsp_payload), rsp_payload);
    }
    else
    {
        DEBUG_LOG_ERROR("GaiaFW DataTransfer: Data Transfer Setup Rsp, SessionID: 0x%04X has no instance!", session_id);
        GAIA_FRAMEWORK_DATA_CH_PANIC();
    }

    DEBUG_LOG_DEBUG("GaiaFW DataTransfer: Data Transfer Setup Rsp, SessionID: 0x%04X", session_id);
}


Task GaiaFramework_GetDataTransferHandler(gaia_data_transfer_session_id_t session_id)
{
    session_instance_t *instance = session_instance_linked_list;

    while (instance != NULL)
    {
        if (instance->session_id == session_id)
        { 
            return &(instance->handler_task);
        }
        instance = instance->next;
    }
    return NULL;
}


gaia_data_transfer_session_id_t GaiaFramework_CreateDataTransferSession(GAIA_TRANSPORT *t, uint8 feature_id, Task handler_task)
{
    static gaia_data_transfer_session_id_t session_id_counter = 0x0000;

    /* Create a new Session ID */
    session_id_counter = (session_id_counter + 1) & 0xFFFF;
    if (session_id_counter == INVALID_DATA_TRANSFER_SESSION_ID)
    {
        session_id_counter += 1;
    }

    /* Create a new Session Instance */
    gaiaFrameworkDataChannel_AddSessionInstance(t, session_id_counter, feature_id, handler_task);

    return session_id_counter;
}


void GaiaFramework_DeleteDataTransferSession(gaia_data_transfer_session_id_t session_id)
{
    bool result;

    result = gaiaFrameworkDataChannel_DeleteSessionInstance(session_id);
    if (result != TRUE)
    {
        DEBUG_LOG_ERROR("GaiaFW DataTransfer: DeleteSession failed to remove Session ID:0x%04X", session_id);
        GAIA_FRAMEWORK_DATA_CH_PANIC();
    }
}


bool GaiaFramework_DataTransferSetup(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    UNUSED(payload_length);
    bool result = FALSE;
    core_plugin_transport_type_t transport_type = payload[2];
    gaia_data_transfer_session_id_t session_id = (uint16)payload[0] << 8 | payload[1];
    session_instance_t *instance = NULL;

    DEBUG_LOG_DEBUG("GaiaFramework_DataTransferSetup");

    /* Check if the Session ID is valid (= registered) or not. */
    instance = gaiaFrameworkDataChannel_FindSessionInstance(session_id);
    if (instance == NULL)
    {
        GaiaFramework_SendError(t, GAIA_CORE_FEATURE_ID, data_transfer_setup, GAIA_STATUS_INVALID_PARAMETER);
    }
    else
    {
        instance->transport_type = transport_type;

        switch (transport_type)
        {
            case transport_gaia_command_response:
                /* No need to open a new link.
                * As this transport uses existing Gaia link for data transfer with
                * the payloads of 'Data Transfer Get/Set' Commands. */
                gaiaFrameworkDataChannel_SendDataTransferSetupResponse(session_id);
                result = TRUE;
                break;

            default:
                DEBUG_LOG_WARN("GaiaFramework_DataTransferSetup: Invalid transport type:%d", transport_type);
                GaiaFramework_SendError(t, GAIA_CORE_FEATURE_ID, data_transfer_setup, GAIA_STATUS_INVALID_PARAMETER);
                break;
        }
    }

    return result;
}


bool GaiaFramework_DataTransferGet(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    UNUSED(payload_length);
    bool result = FALSE;
    gaia_data_transfer_session_id_t session_id = (uint16)payload[0] << 8 | payload[1];
    session_instance_t *instance = NULL;

    DEBUG_LOG_DEBUG("GaiaFramework_DataTransferGet");

    /* Check if the Session ID is valid (= registered) or not. */
    instance = gaiaFrameworkDataChannel_FindSessionInstance(session_id);
    if (instance == NULL)
    {
        GaiaFramework_SendError(t, GAIA_CORE_FEATURE_ID, data_transfer_get, GAIA_STATUS_INVALID_PARAMETER);
    }
    else
    {
        /* Forward the 'Data Transfer Get' request to the registered Feature's data handler. */
        Task data_handler;
        MAKE_GAIA_DATA_TRANSFER_MESSAGE(GAIA_DATA_TRANSFER_INTERNAL_GET_REQ);

        message->session_id = session_id;
        message->starting_offset = (uint32)payload[2] << 24 | (uint32)payload[3] << 16 | (uint32)payload[4] << 8 | (uint32)payload[5];
        message->requested_size = (uint32)payload[6] << 24 | (uint32)payload[7] << 16 | (uint32)payload[8] << 8 | (uint32)payload[9];
        data_handler = GaiaFramework_GetDataTransferHandler(instance->session_id);
        (*data_handler).handler(&gaia_framework_data_transfer_internal_message_handler, GAIA_DATA_TRANSFER_INTERNAL_GET_REQ, message);
        free(message);
    }

    return result;
}


bool GaiaFramework_DataTransferSet(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    UNUSED(payload_length);
    bool result = FALSE;
    gaia_data_transfer_session_id_t session_id = (uint16)payload[0] << 8 | payload[1];
    session_instance_t *instance = NULL;
    uint16 data_size = payload_length - (2 + 4);  /* Session ID: 2 bytes + Start Offset: 4 bytes */

    DEBUG_LOG_DEBUG("GaiaFramework_DataTransferSet");

    /* Check if the Session ID is valid (= registered) or not. */
    instance = gaiaFrameworkDataChannel_FindSessionInstance(session_id);
    if (instance == NULL)
    {
        GaiaFramework_SendError(t, GAIA_CORE_FEATURE_ID, data_transfer_set, GAIA_STATUS_INVALID_PARAMETER);
    }
    else
    {
        /* Forward the 'Data Transfer Set' request to the registered Feature's data handler. */
        Task data_handler;
        MAKE_GAIA_DATA_TRANSFER_MESSAGE_WITH_LEN(GAIA_DATA_TRANSFER_INTERNAL_SET_REQ, data_size);

        message->session_id = session_id;
        message->starting_offset = (uint32)payload[2] << 24 | (uint32)payload[3] << 16 | (uint32)payload[4] << 8 | (uint32)payload[5];
        message->data_size = data_size;
        memmove(message->data, &payload[6], data_size);
        data_handler = GaiaFramework_GetDataTransferHandler(instance->session_id);
        (*data_handler).handler(&gaia_framework_data_transfer_internal_message_handler, GAIA_DATA_TRANSFER_INTERNAL_SET_REQ, message);
        free(message);
    }

    return result;
}


void GaiaFramework_ShutDownDataChannels(void)
{
    session_instance_t *instance;
    bool result;
    
    while (TRUE)
    {
        instance = gaiaFrameworkDataChannel_GetFirstSessionInstance();
        if (instance == NULL)
        {
            break;
        }

        result = gaiaFrameworkDataChannel_DeleteSessionInstance(instance->session_id);
        if (result == FALSE)
        {
            DEBUG_LOG_ERROR("GaiaFW DataTransfer: PANIC Broken instance linked list: SessionID: 0x%04X", instance->session_id);
            GAIA_FRAMEWORK_DATA_CH_PANIC();
        }
    }
}



void GaiaFramework_DataTransferInternalMessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch (id)
    {
        case GAIA_DATA_TRANSFER_INTERNAL_SESSION_START_CFM:
            gaiaFrameworkDataChannel_HandleGaiaDataTransferSessionStartCfm((GAIA_DATA_TRANSFER_INTERNAL_SESSION_START_CFM_T*) message);
            break;

        case GAIA_DATA_TRANSFER_INTERNAL_GET_RSP:
            gaiaFrameworkDataChannel_HandleGaiaDataTransferGetRsp((GAIA_DATA_TRANSFER_INTERNAL_GET_RSP_T*) message);
            break;

        case GAIA_DATA_TRANSFER_INTERNAL_SET_RSP:
            gaiaFrameworkDataChannel_HandleGaiaDataTransferSetRsp((GAIA_DATA_TRANSFER_INTERNAL_SET_RSP_T*) message);
            break;

        default:
            DEBUG_LOG_ERROR("GaiaFW DataTransfer: Internal Msg Handler, UNHANDLED Msg MESSAGE:gaia_data_ch_internal_messages:0x%04X",
                            id);
            break;
    }
}

