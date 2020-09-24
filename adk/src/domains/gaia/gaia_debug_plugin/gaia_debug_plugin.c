/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for Gaia Debug Feature plugin, which provides some
            debug feature that enables followings:
                - Transfer panic log from the device to the host (e.g. smartphone)
                  over Bluetooth connection (NB: RFCOMM only at the moment).
                  Panic log can contain debug information such as stack trace,
                  core dump, and logs stored to the 'Debug Partition' of the flash
                  memory.

Dependencies:
            This Feature requires the following functionalities provided by the
            Apps-P0 firmware:
             - 'Debug Partition': This is a space of the flash memory allocated
               for storing debug information when the chip code Application crashes.
*/

#if defined(INCLUDE_GAIA_PYDBG_REMOTE_DEBUG) || defined(INCLUDE_GAIA_PANIC_LOG_TRANSFER)

#include "gaia_debug_plugin.h"
#ifdef INCLUDE_GAIA_PYDBG_REMOTE_DEBUG
#include "gaia_debug_plugin_pydbg_remote_debug.h"
#endif

#include <source.h>
#include <stream.h>
#include <panic.h>

/* Enable debug log outputs with per-module debug log levels.
 * The log output level for this module can be changed with the PyDbg command:
 *      >>> apps1.log_level("gaia_debug_plugin", 3)
 * Where the second parameter value means:
 *      0:ERROR, 1:WARN, 2:NORMAL(= INFO), 3:VERBOSE(= DEBUG), 4:V_VERBOSE, 5:V_V_VERBOSE
 * See 'logging.h' and PyDbg 'log_level()' command descriptions for details. */
#define DEBUG_LOG_MODULE_NAME gaia_debug_plugin
#include <logging.h>
DEBUG_LOG_DEFINE_LEVEL_VAR

#include "gaia_framework_feature.h"
#include "gaia_framework_data_channel.h"


#ifdef ENABLE_GAIA_DBG_PLUGIN_PANIC
#define GAIA_DBG_PLUGIN_PANIC() Panic()
#else
#define GAIA_DBG_PLUGIN_PANIC()
#endif

#define DISABLE_DP_CONFIG   (0UL)
#define ENABLE_DP_CONFIG    (1UL)



/*! \brief Gaia Debug Feature command handler function.
           This handler must be registered to Gaia Framework, and it is called
           when a Debug Feature command is received from the host.

    \param t           GAIA transport command was received on.

    \param pdu_id      Command ID (7 bits) of Gaia Debug Feature.

    \param length      Payload size in bytes.

    \param payload     Payload data.
*/
static gaia_framework_command_status_t GaiaDebugPlugin_CommandHandler(GAIA_TRANSPORT *t, uint8 pdu_id, uint16 payload_length, const uint8 *payload);


#ifdef INCLUDE_GAIA_PANIC_LOG_TRANSFER
/*! \brief Data transfer channel message handler function for Gaia Debug Feature.

    \param task     The data transfer channel message handler task.

    \param id       The message ID to handle.

    \param message  The message content (if any).

    This function handles messages of Gaia data transfer channel opened for
    Gaia Debug Feature.
*/
static void GaiaDebugPlugin_DataTransferHandler(Task task, MessageId id, Message message);


/*! \brief Data transfer channel message handler function for Gaia Debug Feature.

    This function initialises the Apps-P0 to save debug info to to the 'Debug
    Partition' at panic. This function must be called to enable the feature,
    because, by default, Apps-P0 is configured *not* to save anything at panic
    to the partition.
    The developers may customise the settings to fit for their debugging purpose.
*/
static void gaiaDebugPlugin_ConfigureDebugPartition(void);


/*! \brief Get the status code for Gaia 'Debug' Feature plugin from the
           result code returned by DebugPartition APIs.

    \param result   A result code returned by one of the Debug Partition APIs.

    This function converts the result returned by the Debug Partition APIs
    listed below to the Gaia 'Debug' Feature status code:
        - DebugPartitionConfig(...)
        - DebugPartitionInfo(...)
        - DebugPartitionErase(...)
*/
static debug_plugin_status_code_t gaiaDebugPlugin_GetDebugPluginStatus(debug_partition_result result);


/*! \brief Handle 'Get Debug Log Info' command.

    \param t                GAIA transport command was received on.

    \param payload_length   Size of the command parameter data in bytes.

    \param payload          Command parameter(s) in uint8 array.

    This function reads the 'Debug Partition' information from the Apps-P0 FW
    and send the response back to the host.
    Supported Debug Log Information:
        - The 'Debug Partition' size.
        - The panic-log size stored in the 'Debug Partition'.
          (The size can be zero if none is stored.)

    This function does not check if the 'Info Key' is valid or not as the VM
    trap returns the result (TRUE on success).

    Command Payload Format:
        (!) This can be omitted. In that case, the size of panic log stored in
            the 'Debug Partition' is always returned.
        0        1     (Byte)
    +--------+--------+         Info Key:
    |    Info Key     |             0x0000: DP_INFO_PARTITION_SIZE
    +--------+--------+             0x0001: DP_INFO_DATA_SIZE

    Response Payload Format:
        0        1        2        3        4        5        6     (Byte)
    +--------+--------+--------+--------+--------+--------+--------+
    |Version |    Info Key     |    (MSB) Size in bytes (LSB)      |
    +--------+--------+--------+--------+--------+--------+--------+
    (*) 'Version' on the octet 0 represents the version of this Debug Log Info
        Response PDU. This must be set to zero.
    (*) 'Size' is either 'Debug Partition' size or panic log size stored in it.
*/
static void gaiaDebugPlugin_GetDebugLogInfo(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload);


/*! \brief Handle 'Configure Debug Logging' command.

    \param t                GAIA transport command was received on.

    \param payload_length   Size of the command parameter data in bytes.

    \param payload          Command parameter(s) in uint8 array.

    This function parses the config setting parameters given in the command
    payload (as shown below), and call the Apps-P0 FW with the config parameters.

    This function does not check if the given parameters are both valid or not
    as the VM trap returns the result (TRUE on success).

    Command Payload Format:
        0        1        2        3        4        5     (Byte)
    +--------+--------+--------+--------+--------+--------+
    |   Config Key    |  (MSB) Config Setting Value (LSB) |
    +--------+--------+--------+--------+--------+--------+
    (*) 'Config key' & 'Config Setting Value':
         See 'debug_partition_config_key' enum defined in 'debug_partition_api.h'.

    Response Payload Format:
        0        1        2        3        4        5     (Byte)
    +--------+--------+--------+--------+--------+--------+
    |   Config Key    |  (MSB) Config Setting Value (LSB) |
    +--------+--------+--------+--------+--------+--------+
    (*) Just respond to the host with the 'Config Key' and its 'Value'.
*/
static void gaiaDebugPlugin_ConfigureDebugLogging(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload);


/*! \brief Handle 'Set up Debug Log Transfer' command.

    \param t                GAIA transport command was received on.

    \param payload_length   Size of the command parameter data in bytes.

    \param payload          Command parameter(s) in uint8 array.

    This function returns a Session ID assigned to this plugin.
    If none is not assigned yet, a data transfer session is created with a new
    Session ID.

    Command Payload Format:
        0        1     (Byte)
    +--------+--------+
    |    (Reserved)   |     This reserved field must be zero.
    +--------+--------+

    Response Payload Format:
        0        1        2        3        4        5     (Byte)
    +--------+--------+--------+--------+--------+--------+
    | Session ID (*1) |   (MSB) Log size in bytes (LSB)   |
    +--------+--------+--------+--------+--------+--------+
    (*1) Data transfer Session ID created (MSB first).
*/
static void gaiaDebugPlugin_SetupDebugLogTransfer(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload);


/*! \brief Handle 'Erase Panic Log' command.

    \param t                GAIA transport command was received on.

    \param payload_length   Size of the command parameter data in bytes.

    \param payload          Command parameter(s) in uint8 array.


    Command Payload Format:
        0        1     (Byte)
    +--------+--------+
    |    (Reserved)   |     This reserved field must be zero.
    +--------+--------+

    Response Payload Format
        None. The Erase command shall be acknowledged by the Response PDU, which has no payload.
*/
static void gaiaDebugPlugin_ErasePanicLog(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload);


/*! \brief Handle SESSION_START_IND data transfer session message, that
           indicates the peer (the mobile app) has connected to this device.

    \param response_task    Task which #GAIA_DATA_TRANSFER_INTERNAL_SESSION_START_CFM
                            is sent to.
    \param ind              Indication message that contains either the sink or
                            the source of the data transfer channel.

    This function starts sending the panic data stored in the 'Debug Partition'
    by connecting the data channel sink with the 'Debug Partition' source.

    This function is for panic log data transfer over separate RFCOMM channel.
    (Transport type: transport_rfcomm_data_channel)
*/
static void gaiaDebugPlugin_HandleDataSessionStartInd(Task response_task, GAIA_DATA_TRANSFER_INTERNAL_SESSION_START_IND_T *ind);


/*! \brief Handle SESSION_CLOSE_IND data transfer session message, that
           indicates the peer (the mobile app) has disconnected.

    \param response_task    Task which #GAIA_DATA_TRANSFER_INTERNAL_SESSION_CLOSE_CFM
                            is sent to.
    \param ind              Indication message that contains the Session ID of
                            which the connection has been closed.

    This function tidies up the parameters used for the data transfer session.

    This function is for panic log data transfer over separate RFCOMM channel.
    (Transport type: transport_rfcomm_data_channel)
*/
static void gaiaDebugPlugin_HandleDataSessionCloseInd(Task response_task, GAIA_DATA_TRANSFER_INTERNAL_SESSION_CLOSE_IND_T *ind);


/*! \brief Handle DATA_TRANSFER_GET_REQ message, with which the mobile app request
           the 'size' of data bytes starting from the 'offset'.

    \param response_task    Task which #GAIA_DATA_TRANSFER_INTERNAL_GET_RSP
                            is sent to.
    \param req              Request message that contains the Session ID, the
                            starting offset, and the size requested.

    This function returns a chunk of the Debug Log data (i.e. post-panic log)
    which starts from the 'offset' specified. This function tries to fill the
    payload up to the size requested, but the actual payload size can be fewer
    than the requested one.
*/
static void gaiaDebugPlugin_GetPanicLogData(Task response_task, GAIA_DATA_TRANSFER_INTERNAL_GET_REQ_T *req);


/*! \brief Handles an unknown message of Gaia Data transfer session.

    \param task     The task that has sent the data transfer session message.

    \param id       The message ID, which is unknown to this plugin.
*/
static void gaiaDebugPlugin_HandleDataSessionUnknown(Task task, MessageId id);



/*! \brief Session ID assigned for Gaia Debug Feature.
           This must be INVALID_DATA_TRANSFER_SESSION_ID, if no sessions are allocated. */
static gaia_data_transfer_session_id_t data_channel_session_id = INVALID_DATA_TRANSFER_SESSION_ID;


/*! \brief Task that receives messages for the data transfer channel. */
static TaskData debug_plugin_data_transfer_handler = { GaiaDebugPlugin_DataTransferHandler };
#endif /* INCLUDE_GAIA_PANIC_LOG_TRANSFER */



bool GaiaDebugPlugin_Init(Task init_task)
{
    static const gaia_framework_plugin_functions_t functions =
    {
        .command_handler = GaiaDebugPlugin_CommandHandler,
        .send_all_notifications = NULL,
        .transport_connect = NULL,
        .transport_disconnect = NULL,
    };

    UNUSED(init_task);

    DEBUG_LOG_DEBUG("GaiaDebugPlugin_Init");
#ifdef INCLUDE_GAIA_PANIC_LOG_TRANSFER
    data_channel_session_id = INVALID_DATA_TRANSFER_SESSION_ID;

    gaiaDebugPlugin_ConfigureDebugPartition(); 
#endif /* INCLUDE_GAIA_PANIC_LOG_TRANSFER */

    /* Even if DebugPartitionConfig() fails, the 'Debug' Feature should be accessible to the developer. */
    GaiaFramework_RegisterFeature(GAIA_DEBUG_FEATURE_ID, GAIA_DEBUG_FEATURE_PLUGIN_VERSION, &functions);

    return TRUE;
}


static gaia_framework_command_status_t GaiaDebugPlugin_CommandHandler(GAIA_TRANSPORT *t, uint8 pdu_id, uint16 payload_length, const uint8 *payload)
{
    DEBUG_LOG_DEBUG("GaiaDebugPlugin_CommandHandler CmdID: %d", pdu_id);

    switch (pdu_id)
    {
#ifdef INCLUDE_GAIA_PANIC_LOG_TRANSFER
        case get_debug_log_info:
            gaiaDebugPlugin_GetDebugLogInfo(t, payload_length, payload);
            break;

        case configure_debug_logging:
            gaiaDebugPlugin_ConfigureDebugLogging(t, payload_length, payload);
            break;

        case setup_debug_log_transfer:
            gaiaDebugPlugin_SetupDebugLogTransfer(t, payload_length, payload);
            break;

        case erase_panic_log:
            gaiaDebugPlugin_ErasePanicLog(t, payload_length, payload);
            break;
#endif /* INCLUDE_GAIA_PANIC_LOG_TRANSFER */

#ifdef INCLUDE_GAIA_PYDBG_REMOTE_DEBUG
        case debug_tunnel_to_chip:
            GaiaDebugPlugin_DebugTunnelToChip(t, payload_length, payload);
            break;
#endif /* INCLUDE_GAIA_PYDBG_REMOTE_DEBUG */

        default:
            DEBUG_LOG_WARN("gaiaDebugPlugin Invalid command ID: 0x%02X", pdu_id);
            return command_not_handled;
    }
    return command_handled;
}

#ifdef INCLUDE_GAIA_PANIC_LOG_TRANSFER
static void GaiaDebugPlugin_DataTransferHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);

    DEBUG_LOG_DEBUG("GaiaDebugPlugin DataTransferHandler(ID:0x%04X)");

   switch (id)
   {
        case GAIA_DATA_TRANSFER_INTERNAL_SESSION_START_IND:
            gaiaDebugPlugin_HandleDataSessionStartInd(task, (GAIA_DATA_TRANSFER_INTERNAL_SESSION_START_IND_T*) message);
            break;
        case GAIA_DATA_TRANSFER_INTERNAL_SESSION_CLOSE_IND:
            gaiaDebugPlugin_HandleDataSessionCloseInd(task, (GAIA_DATA_TRANSFER_INTERNAL_SESSION_CLOSE_IND_T*) message);
            break;

        case GAIA_DATA_TRANSFER_INTERNAL_GET_REQ:
            gaiaDebugPlugin_GetPanicLogData(task, (GAIA_DATA_TRANSFER_INTERNAL_GET_REQ_T*)message);
            break;

        default:
            gaiaDebugPlugin_HandleDataSessionUnknown(task, id);
            break;
   }
}


static void gaiaDebugPlugin_ConfigureDebugPartition(void)
{
    debug_partition_result result;
    debug_partition_config_key config_key = DP_CONFIG_MAX;

    data_channel_session_id = INVALID_DATA_TRANSFER_SESSION_ID;

    /* The following multiple function calls of "DebugPartitionConfig(...)"
     * specify which debug information will be saved to the 'Debug Partition'
     * at the event of panic.
     * The configuration can be adjusted to the developer's needs.
     * See the descriptions for 'debug_partition_config_key' enum defined in
     * 'debug_partition_if.h'.
     */
    do
    {
        config_key = DP_CONFIG_DUMP_P1_BASIC_REGS;
        result= DebugPartitionConfig(config_key, ENABLE_DP_CONFIG);
        if (DP_SUCCESS != result)   break;

        config_key = DP_CONFIG_DUMP_P1_STACK;
        result= DebugPartitionConfig(config_key, ENABLE_DP_CONFIG);
        if (DP_SUCCESS != result)   break;

        config_key = DP_CONFIG_DUMP_P1_HYDRA_LOG;
        result= DebugPartitionConfig(config_key, ENABLE_DP_CONFIG);
        break;
    } while (FALSE);

    switch (result)
    {
        case DP_SUCCESS:
            DEBUG_LOG_DEBUG("GaiaDebugPlugin DebugPartitionConfig: OK! (P1_BASIC_REGS, P1_STACK, P1_HYDRA_LOG");
            break;
        case DP_NOT_ENOUGH_SPACE:
            DEBUG_LOG_DEBUG("GaiaDebugPlugin DebugPartitionConfig: Not enough space left!");
            break;
        case DP_NOT_CONFIGURED:
            /* Probably, 'debug_partition' is not defined in the flash layout config file. */
            DEBUG_LOG_DEBUG("GaiaDebugPlugin DebugPartitionConfig: The Debug Partition is not found!");
            break;

        /* The following errors should not happen. */
        case DP_BUSY:
            DEBUG_LOG_ERROR("GaiaDebugPlugin DebugPartitionConfig: The resource is busy!");
            break;
        case DP_KEY_NOT_SUPPORTED:
            DEBUG_LOG_ERROR("GaiaDebugPlugin DebugPartitionConfig: The config key (%d) not supported!", config_key);
            break;
        case DP_INVALID_PARAMETER:
            DEBUG_LOG_ERROR("GaiaDebugPlugin DebugPartitionConfig: The parameter is invalid: 0x%08X", ENABLE_DP_CONFIG);
            break;
        default:
            DEBUG_LOG_ERROR("GaiaDebugPlugin DebugPartitionConfig: Unknown error code: %d", result);
            GAIA_DBG_PLUGIN_PANIC();
            break;
    }
}


static debug_plugin_status_code_t gaiaDebugPlugin_GetDebugPluginStatus(debug_partition_result result)
{
    debug_plugin_status_code_t status = debug_plugin_status_unknown_error;

    switch (result)
    {
        case DP_SUCCESS:
            status = debug_plugin_status_success;
            break;
        case DP_KEY_NOT_SUPPORTED:
            status = debug_plugin_status_invalid_parameters;
            break;
        case DP_INVALID_PARAMETER:
            status = debug_plugin_status_invalid_parameters;
            break;
        case DP_BUSY:
            status = debug_plugin_status_busy;
            break;
        case DP_NOT_ENOUGH_SPACE:
            status = debug_plugin_status_not_enough_space;
            break;
        case DP_NOT_CONFIGURED:
            /* This is the case if the 'Debug Partition' is not defined in the flash layout config file. */
            status = debug_plugin_status_no_debug_partition;
            break;
        default:
            status = debug_plugin_status_unknown_error;
            break;
    }
    DEBUG_LOG_DEBUG("gaiaDebugPlugin GetDebugPluginStatus Result:0x%04X, Status:0x%04X", result, status);

    return status;
}


static void gaiaDebugPlugin_GetDebugLogInfo(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    if (payload_length == number_of_debug_plugin_get_debug_log_info_cmd_bytes)
    {
        debug_partition_info_key key;
        uint32 value;
        debug_partition_result result;
        debug_plugin_status_code_t status;

        /* Parse the command parameter that specifies which information the host wants. */
        key = (debug_partition_info_key) ((payload[debug_plugin_get_debug_log_info_cmd_log_info_key_msb] << 8) | payload[debug_plugin_get_debug_log_info_cmd_log_info_key_lsb]);
        DEBUG_LOG_DEBUG("gaiaDebugPlugin GetInfo, Key: 0x%04X", key);

        value = 0UL;
        result = DebugPartitionInfo(key, &value);
        status = gaiaDebugPlugin_GetDebugPluginStatus(result);
        if (status == debug_plugin_status_success)
        {
            uint8 rsp_payload[number_of_debug_plugin_get_debug_log_info_rsp_bytes];

            DEBUG_LOG_DEBUG("gaiaDebugPlugin GetInfo, Size: 0x%08X", value);
            rsp_payload[debug_plugin_get_debug_log_info_rsp_pdu_version]            = GAIA_DEBUG_FEATURE_DBG_INFO_CMD_PDU_VERSION;
            rsp_payload[debug_plugin_get_debug_log_info_rsp_info_key_msb]           = (uint8) ((key & 0xFF00) >> 8);
            rsp_payload[debug_plugin_get_debug_log_info_rsp_info_key_lsb]           = (uint8)  (key & 0x00FF);
            rsp_payload[debug_plugin_get_debug_log_info_rsp_panic_log_size_msb]     = (uint8) ((value & 0xFF000000UL) >> 24);
            rsp_payload[debug_plugin_get_debug_log_info_rsp_panic_log_size_2nd_sb]  = (uint8) ((value & 0x00FF0000UL) >> 16);
            rsp_payload[debug_plugin_get_debug_log_info_rsp_panic_log_size_3rd_sb]  = (uint8) ((value & 0x0000FF00UL) >>  8);
            rsp_payload[debug_plugin_get_debug_log_info_rsp_panic_log_size_lsb]     = (uint8)  (value & 0x000000FFUL);
            GaiaFramework_SendResponse(t, GAIA_DEBUG_FEATURE_ID, get_debug_log_info, sizeof(rsp_payload), rsp_payload);
        }
        else
        {
            DEBUG_LOG_ERROR("gaiaDebugPlugin GetInfo, FAILED! (Status: 0x%02X, Result: %d)", status, result);
            GaiaFramework_SendError(t, GAIA_DEBUG_FEATURE_ID, get_debug_log_info, status);
        }
    }
    else
    {
        DEBUG_LOG_WARN("gaiaDebugPlugin Config: ERROR! Invalid PDU Size:%d", payload_length);
        GaiaFramework_SendError(t, GAIA_DEBUG_FEATURE_ID, get_debug_log_info, debug_plugin_status_invalid_parameters);
    }
}


static void gaiaDebugPlugin_ConfigureDebugLogging(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    if (payload_length == number_of_debug_plugin_configure_debug_logging_cmd_bytes)
    {
        debug_partition_info_key key;
        uint32 value;
        debug_partition_result result;
        debug_plugin_status_code_t status;

        /* Parse the command parameter that specifies which information the host wants. */
        key = (debug_partition_info_key) ((payload[debug_plugin_configure_debug_logging_cmd_config_key_msb] << 8) | payload[debug_plugin_configure_debug_logging_cmd_config_key_lsb]);
        value = ((uint32)payload[debug_plugin_configure_debug_logging_cmd_config_val_msb]) << 24 |
                ((uint32)payload[debug_plugin_configure_debug_logging_cmd_config_val_2nd_sb]) << 16 |
                ((uint32)payload[debug_plugin_configure_debug_logging_cmd_config_val_3rd_sb]) << 8 |
                 (uint32)payload[debug_plugin_configure_debug_logging_cmd_config_val_lsb];
        DEBUG_LOG_DEBUG("gaiaDebugPlugin Config, Key:0x%04X, Val:0x%08X", key, value);

        result = DebugPartitionConfig(key, value);
        status = gaiaDebugPlugin_GetDebugPluginStatus(result);
        if (status == debug_plugin_status_success)
        {
            uint8 rsp_payload[number_of_debug_plugin_configure_debug_logging_rsp_bytes];

            DEBUG_LOG_DEBUG("gaiaDebugPlugin Config: OK");
            rsp_payload[debug_plugin_configure_debug_logging_rsp_config_key_msb]    = (uint8) ((key & 0xFF00) >> 8);
            rsp_payload[debug_plugin_configure_debug_logging_rsp_config_key_lsb]    = (uint8)  (key & 0x00FF);
            rsp_payload[debug_plugin_configure_debug_logging_rsp_config_val_msb]    = (uint8) ((value & 0xFF000000UL) >> 24);
            rsp_payload[debug_plugin_configure_debug_logging_rsp_config_val_2nd_sb] = (uint8) ((value & 0x00FF0000UL) >> 16);
            rsp_payload[debug_plugin_configure_debug_logging_rsp_config_val_3rd_sb] = (uint8) ((value & 0x0000FF00UL) >>  8);
            rsp_payload[debug_plugin_configure_debug_logging_rsp_config_val_lsb]    = (uint8)  (value & 0x000000FFUL);
            GaiaFramework_SendResponse(t, GAIA_DEBUG_FEATURE_ID, configure_debug_logging, sizeof(rsp_payload), rsp_payload);
        }
        else
        {
            DEBUG_LOG_ERROR("gaiaDebugPlugin Config: FAILED! (Status:0x%02X, Result:%d)", status, result);
            GaiaFramework_SendError(t, GAIA_DEBUG_FEATURE_ID, configure_debug_logging, status);
        }
    }
    else
    {
        DEBUG_LOG_WARN("gaiaDebugPlugin Config: ERROR! Invalid PDU Size:%d", payload_length);
        GaiaFramework_SendError(t, GAIA_DEBUG_FEATURE_ID, configure_debug_logging, debug_plugin_status_invalid_parameters);
    }
}


static void gaiaDebugPlugin_SetupDebugLogTransfer(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    debug_partition_result result;
    debug_plugin_status_code_t status;
    uint32 log_size = 0;
    UNUSED(payload_length);
    UNUSED(payload);

    /* First, check that there's a panic log in the 'Debug Partition'. */
    result = DebugPartitionInfo(DP_INFO_DATA_SIZE, &log_size);
    status = gaiaDebugPlugin_GetDebugPluginStatus(result);
    if (status == debug_plugin_status_success)
    {
        if (log_size != 0)
        {
            /* Create a new session only when one is not allocated yet. */
            if (data_channel_session_id == INVALID_DATA_TRANSFER_SESSION_ID)
            {
                data_channel_session_id = GaiaFramework_CreateDataTransferSession(t, GAIA_DEBUG_FEATURE_ID, &debug_plugin_data_transfer_handler);
            }

            if (data_channel_session_id != INVALID_DATA_TRANSFER_SESSION_ID)
            {
                uint8 rsp_payload[number_of_get_panic_log_rsp_bytes];

                DEBUG_LOG_DEBUG("gaiaDebugPlugin GetLog: Ready, Session ID:0x%04X", data_channel_session_id);
                rsp_payload[debug_plugin_get_panic_log_rsp_session_id_msb]        = (uint8) ((data_channel_session_id & 0xFF00) >> 8);
                rsp_payload[debug_plugin_get_panic_log_rsp_session_id_lsb]        = (uint8)  (data_channel_session_id & 0x00FF);
                rsp_payload[debug_plugin_get_panic_log_rsp_panic_log_size_msb]    = (uint8) ((log_size & 0xFF000000UL) >> 24);
                rsp_payload[debug_plugin_get_panic_log_rsp_panic_log_size_2nd_sb] = (uint8) ((log_size & 0x00FF0000UL) >> 16);
                rsp_payload[debug_plugin_get_panic_log_rsp_panic_log_size_3rd_sb] = (uint8) ((log_size & 0x0000FF00UL) >>  8);
                rsp_payload[debug_plugin_get_panic_log_rsp_panic_log_size_lsb]    = (uint8)  (log_size & 0x000000FFUL);
                GaiaFramework_SendResponse(t, GAIA_DEBUG_FEATURE_ID, setup_debug_log_transfer, sizeof(rsp_payload), rsp_payload);
            }
            else
            {
                /* Failed to create a Session ID */
                DEBUG_LOG_ERROR("gaiaDebugPlugin GetLog: FAILED to create a Session ID!");
                GaiaFramework_SendError(t, GAIA_DEBUG_FEATURE_ID, setup_debug_log_transfer, debug_plugin_status_unknown_error);
            }
        }
        else    /* log_size == 0 */
        {
            /* The 'Debug Partition' is empty! */
            DEBUG_LOG_WARN("gaiaDebugPlugin GetLog: (!) No panic log is available...");
            GaiaFramework_SendError(t, GAIA_DEBUG_FEATURE_ID, setup_debug_log_transfer, debug_plugin_status_no_data);
        }
    }
    else /* status != debug_plugin_status_success */
    {
        /* Failed to obtain the panic log size in the 'Debug Partition'. */
        DEBUG_LOG_ERROR("gaiaDebugPlugin GetLog: FAILED to get panic log size! (Status: 0x%02X, Result: %d", status, result);
        GaiaFramework_SendError(t, GAIA_DEBUG_FEATURE_ID, setup_debug_log_transfer, status);
    }
}


static void gaiaDebugPlugin_ErasePanicLog(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    uint16 erase_param = ((uint16)payload[debug_plugin_erase_panic_log_cmd_reserved00] << 8) | payload[debug_plugin_erase_panic_log_cmd_reserved01];

    DEBUG_LOG_DEBUG("gaiaDebugPlugin Erase: Started");

    if (payload_length == number_of_debug_plugin_erase_panic_log_cmd_bytes &&
        erase_param == GAIA_DEBUG_FEATURE_ERASE_PANIC_LOG_CMD_PARAM_ERASE_ALL)
    {
        debug_partition_result result;
        debug_plugin_status_code_t status;

        /* Note that this function is a blocking one while erasing the content of
        * the debug partition. It might take 100ms ~ a few seconds to complete!
        * And it has no return values.
        * 
        * The source opend by StreamDebugPartitionSource(...) must be closed
        * before calling DebugPartitionErase().
        */
        result = DebugPartitionErase();

        status = gaiaDebugPlugin_GetDebugPluginStatus(result);
        if (status == debug_plugin_status_success)
        {
            GaiaFramework_SendResponse(t, GAIA_DEBUG_FEATURE_ID, erase_panic_log, 0, NULL);
            DEBUG_LOG_DEBUG("gaiaDebugPlugin Erase: Done!");
        }
        else
        {
            DEBUG_LOG_ERROR("gaiaDebugPlugin Config: FAILED! (Status:0x%02X, Result:%d)", status, result);
            GaiaFramework_SendError(t, GAIA_DEBUG_FEATURE_ID, erase_panic_log, status);
        }
    }
    else
    {
        DEBUG_LOG_WARN("gaiaDebugPlugin Erase: ERROR! Invalid PDU Size:%d", payload_length);
        GaiaFramework_SendError(t, GAIA_DEBUG_FEATURE_ID, erase_panic_log, debug_plugin_status_invalid_parameters);
    }
}


static void gaiaDebugPlugin_HandleDataSessionStartInd(Task response_task, GAIA_DATA_TRANSFER_INTERNAL_SESSION_START_IND_T *ind)
{
    data_transfer_status_code_t status;
    Transform transform = NULL;

    if (ind->sink != NULL)
    {
        Source source;

        source = StreamDebugPartitionSource();
        if (source != NULL)
        {
            transform = StreamConnect(source, ind->sink);
            if (transform != NULL)
            {
                status = data_transfer_status_success;
            }
            else
            {
                DEBUG_LOG_ERROR("GaiaDebugPlugin FAILED: StreamConnect");
                status = data_transfer_status_invalid_stream;
            }
        }
        else
        {
            DEBUG_LOG_ERROR("GaiaDebugPlugin FAILED: StreamDebugPartitionSource");
            status = data_transfer_status_invalid_source;
        }
    }
    else
    {
        DEBUG_LOG_ERROR("GaiaDebugPlugin FAILED: Invalid RFCOMM Sink!");
        status = data_transfer_status_invalid_sink;
    }

    /* Send the CFM message regardless of the result */
    {
        MAKE_GAIA_DATA_TRANSFER_MESSAGE(GAIA_DATA_TRANSFER_INTERNAL_SESSION_START_CFM);
        message->status = status;
        message->session_id = ind->session_id;
        message->transform = transform;
        MessageSend(response_task, GAIA_DATA_TRANSFER_INTERNAL_SESSION_START_CFM, message);
    }

    if (status != data_transfer_status_success)
    {
        GAIA_DBG_PLUGIN_PANIC();
    }
}


static void gaiaDebugPlugin_HandleDataSessionCloseInd(Task response_task, GAIA_DATA_TRANSFER_INTERNAL_SESSION_CLOSE_IND_T *ind)
{
    data_channel_session_id = INVALID_DATA_TRANSFER_SESSION_ID;

    MAKE_GAIA_DATA_TRANSFER_MESSAGE(GAIA_DATA_TRANSFER_INTERNAL_SESSION_CLOSE_CFM);
    message->session_id = ind->session_id;
    MessageSend(response_task, GAIA_DATA_TRANSFER_INTERNAL_SESSION_CLOSE_CFM, message);
}


static void gaiaDebugPlugin_GetPanicLogData(Task response_task, GAIA_DATA_TRANSFER_INTERNAL_GET_REQ_T *req)
{
    data_transfer_status_code_t status = data_transfer_status_failure_with_unspecified_reason;
    Source source = NULL;
    uint32 offset;
    uint16 data_size;
    uint32 log_total_size;
    uint16 source_size;
    
    DEBUG_LOG_DEBUG("gaiaDebugPlugin GetPanicLogData Offset:0x%08X, Size: 0x%08X", req->starting_offset, req->requested_size);
    while (TRUE)
    {
        /* Step 1: Make sure that the requested size is within 0xFFFF bytes. */
        if (req->requested_size > 0xFFFFUL)
        {
            /* The requested size must be less than 0x10000. */
            DEBUG_LOG_WARN("GaiaDebugPlugin DataTransfer_Get, ERROR: Requested size exceeds 64KB! 0x%08X", req->requested_size);
            status = data_transfer_invalid_parameter;
            break;
        }

        /* As the stream supports up to 64KBytes, the data size is set to a 16-bit variable. */
        data_size = (uint16) (req->requested_size & 0xFFFFUL);
        /* But offset can be more than 64KBytes if the partition size is more than 64KB. */
        offset = req->starting_offset;

        /* Step 2: Get the stream source for the 'Debug Partition'. */
        source = StreamDebugPartitionSource();
        if (source == NULL)
        {
            DEBUG_LOG_ERROR("GaiaDebugPlugin FAILED: StreamDebugPartitionSource");
            status = data_transfer_status_invalid_source;
            break;
        }

        /* Step 3: Check if the starting offset is within the data? */
        DebugPartitionInfo(DP_INFO_DATA_SIZE, &log_total_size);
        if (log_total_size <= offset)
        {
            /* The specified starting offset exceeds the panic data size! */
            DEBUG_LOG_WARN("GaiaDebugPlugin (i) Log size:%d <= Offset:%d", log_total_size, offset);
            status = data_transfer_no_more_data;
            break;
        }

        /* Step 4: Move the starting point to read to the offset specified. */
        while (offset)
        {
            uint16 drop_size;

            source_size = SourceSize(source);
            drop_size = (offset < source_size) ? (uint16) offset : source_size;
            if (drop_size == 0)
            {
                break;
            }
            SourceDrop(source, drop_size);
            offset -= drop_size;
        }

        /* Step 5: Ensure that the data size fits the payload length of 'DataTransfer_Get' Response. */
        if (data_size > DATA_TRANSFER_GET_RESPONSE_PAYLOAD_SIZE)
        {
            data_size = DATA_TRANSFER_GET_RESPONSE_PAYLOAD_SIZE;
        }

        /* Step 6: Copy the data bytes read from the stream, and pass it to Gaia Framework (Data Transfer) */
        {
            MAKE_GAIA_DATA_TRANSFER_MESSAGE_WITH_LEN(GAIA_DATA_TRANSFER_INTERNAL_GET_RSP, data_size);

            if (message != NULL)
            {
                const uint8 *data;
                uint16 pos = 0;

                while (data_size)
                {
                    int copy_size;

                    source_size = SourceSize(source);
                    copy_size = (data_size < source_size) ? data_size : source_size;
                    if (copy_size == 0)
                    {
                        break;
                    }
                    data = SourceMap(source);
                    memmove(&message->data[pos], data, copy_size);
                    data_size -= copy_size;
                    pos += copy_size;
                    SourceDrop(source, copy_size);
                }
                DEBUG_LOG_DEBUG("gaiaDebugPlugin_GetPanicLogData: pos: %d", pos);

                message->status = data_transfer_status_success;
                message->session_id = req->session_id;
                message->data_size = pos;
                MessageSend(response_task, GAIA_DATA_TRANSFER_INTERNAL_GET_RSP, message);
                status = data_transfer_status_success;
            }
            else
            {
                DEBUG_LOG_ERROR("GaiaDebugPlugin FAILED: malloc(%d)", sizeof(GAIA_DATA_TRANSFER_INTERNAL_GET_RSP_T) + data_size - 1);
                status = data_transfer_status_insufficient_resource;
            }
        }
        break;
    }

    if (source != NULL)
    {
        SourceClose(source);
    }

    if (status != data_transfer_status_success)
    {
        /* Something goes wrong. Send an error response. */
        MAKE_GAIA_DATA_TRANSFER_MESSAGE(GAIA_DATA_TRANSFER_INTERNAL_GET_RSP);

        if (message != NULL)
        {
            message->status = status;
            message->session_id = req->session_id;
            message->data_size = 0;
            MessageSend(response_task, GAIA_DATA_TRANSFER_INTERNAL_GET_RSP, message);
        }
        else
        {
            DEBUG_LOG_ERROR("GaiaDebugPlugin FAILED: malloc(%d)", sizeof(GAIA_DATA_TRANSFER_INTERNAL_GET_RSP_T));
            GAIA_DBG_PLUGIN_PANIC();
        }
    }
}


static void gaiaDebugPlugin_HandleDataSessionUnknown(Task task, MessageId id)
{
    DEBUG_LOG_DEBUG("GaiaDebugPlugin Unknown Msg: 0x%04X", id);

    MAKE_GAIA_DATA_TRANSFER_MESSAGE(GAIA_DATA_TRANSFER_INTERNAL_UNKNOWN_RESPONSE);
    message->unknown_message_id = id;
    MessageSend(task, GAIA_DATA_TRANSFER_INTERNAL_UNKNOWN_RESPONSE, message);

    GAIA_DBG_PLUGIN_PANIC();
}
#endif /* INCLUDE_GAIA_PANIC_LOG_TRANSFER */
#endif /* INCLUDE_GAIA_PYDBG_REMOTE_DEBUG || INCLUDE_GAIA_PANIC_LOG_TRANSFER */
