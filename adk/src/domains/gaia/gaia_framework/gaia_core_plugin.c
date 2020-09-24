/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for the gaia framework core plugin
*/

#define DEBUG_LOG_MODULE_NAME gaia_core
#include <logging.h>
DEBUG_LOG_DEFINE_LEVEL_VAR

#include <panic.h>
#include <byte_utils.h>
#include <stdlib.h>

#include "gaia_core_plugin.h"
#include "gaia_framework_feature.h"
#include "device_info.h"
#include "power_manager.h"
#include "charger_monitor.h"
#include "gaia_framework_data_channel.h"



/*! \brief Function pointer definition for the command handler

    \param pdu_id      PDU specific ID for the message

    \param length      Length of the payload

    \param payload     Payload data
*/
static gaia_framework_command_status_t gaiaCorePlugin_MainHandler(GAIA_TRANSPORT *t, uint8 pdu_id, uint16 payload_length, const uint8 *payload);

static void gaiaCorePlugin_GetApiVersion(GAIA_TRANSPORT *t);
static void gaiaCorePlugin_GetSupportedFeatures(GAIA_TRANSPORT *t);
static void gaiaCorePlugin_GetSupportedFeaturesNext(GAIA_TRANSPORT *t);
static void gaiaCorePlugin_GetSerialNumber(GAIA_TRANSPORT *t);
static void gaiaCorePlugin_GetVariant(GAIA_TRANSPORT *t);
static void gaiaCorePlugin_GetApplicationVersion(GAIA_TRANSPORT *t);
static void gaiaCorePlugin_DeviceReset(GAIA_TRANSPORT *t);
static void gaiaCorePlugin_RegisterNotification(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload);
static void gaiaCorePlugin_UnregisterNotification(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload);
static void gaiaCorePlugin_GetTransportInfo(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload);
static void gaiaCorePlugin_SetTransportParameter(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload);

/*! \brief Function that hendles 'Data Transfer Setup' command.
*/
static void gaiaCorePlugin_DataTransferSetup(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload);

/*! \brief Function that handles 'Data Transfer Get' command.
*/
static void gaiaCorePlugin_DataTransferGet(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload);

/*! \brief Function that handles 'Data Transfer Set' command.
*/
static void gaiaCorePlugin_DataTransferSet(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload);

static bool gaiaCorePlugin_GetSupportedFeaturesPayload(uint16 payload_length, uint8 *payload);

/*! \brief Function that sends all available notifications
*/
static void gaiaCorePlugin_SendAllNotifications(GAIA_TRANSPORT *t);

/*! \brief Function that sends all available notifications
*/
static void gaiaCorePlugin_SendChargerStatusNotification(GAIA_TRANSPORT *t, bool plugged);

/*! \brief Gaia core plugin function to be registered as an observer of charger messages

    \param Task     Task passed to the registration function

    \param id       Messages id sent over from the charger

    \param message  Message
*/
static void gaiaCorePlugin_ChargerTask(Task task, MessageId message_id, Message message);

static bool charger_client_is_registered = FALSE;

static TaskData gaia_core_plugin_task;

static bool current_charger_plugged_in_state = FALSE;


void GaiaCorePlugin_Init(void)
{
    static const gaia_framework_plugin_functions_t functions =
    {
        .command_handler = gaiaCorePlugin_MainHandler,
        .send_all_notifications = gaiaCorePlugin_SendAllNotifications,
        .transport_connect = NULL,
        .transport_disconnect = NULL,
    };

    DEBUG_LOG("GaiaCorePlugin_Init");

    GaiaFramework_RegisterFeature(GAIA_CORE_FEATURE_ID, GAIA_CORE_PLUGIN_VERSION, &functions);

    /* Register the core gaia plugin as an observer for charger messages */
    gaia_core_plugin_task.handler = gaiaCorePlugin_ChargerTask;
    charger_client_is_registered = appChargerClientRegister((Task)&gaia_core_plugin_task);
}

static gaia_framework_command_status_t gaiaCorePlugin_MainHandler(GAIA_TRANSPORT *t, uint8 pdu_id, uint16 payload_length, const uint8 *payload)
{
    DEBUG_LOG("gaiaCorePlugin_MainHandler, transport %p, pdu_id %u", t, pdu_id);

    switch (pdu_id)
    {
        case get_api_version:
            gaiaCorePlugin_GetApiVersion(t);
            break;

        case get_supported_features:
            gaiaCorePlugin_GetSupportedFeatures(t);
            break;

        case get_supported_features_next:
            gaiaCorePlugin_GetSupportedFeaturesNext(t);
            break;

        case get_serial_number:
            gaiaCorePlugin_GetSerialNumber(t);
            break;

        case get_variant:
            gaiaCorePlugin_GetVariant(t);
            break;

        case get_application_version:
            gaiaCorePlugin_GetApplicationVersion(t);
            break;

        case device_reset:
            gaiaCorePlugin_DeviceReset(t);
            break;

        case register_notification:
            gaiaCorePlugin_RegisterNotification(t, payload_length, payload);
            break;

        case unregister_notification:
            gaiaCorePlugin_UnregisterNotification(t, payload_length, payload);
            break;

        case data_transfer_setup:
            gaiaCorePlugin_DataTransferSetup(t, payload_length, payload);
            break;

        case data_transfer_get:
            gaiaCorePlugin_DataTransferGet(t, payload_length, payload);
            break;

        case data_transfer_set:
            gaiaCorePlugin_DataTransferSet(t, payload_length, payload);
            break;

        case get_transport_info:
            gaiaCorePlugin_GetTransportInfo(t, payload_length, payload);
            break;

        case set_transport_parameter:
            gaiaCorePlugin_SetTransportParameter(t, payload_length, payload);
            break;

        default:
            DEBUG_LOG("gaiaCorePlugin_MainHandler, unhandled call for %u", pdu_id);
            return command_not_handled;
    }

    return command_handled;
}

static void gaiaCorePlugin_GetApiVersion(GAIA_TRANSPORT *t)
{
    static const uint8 value[2] = { GAIA_V3_VERSION_MAJOR, GAIA_V3_VERSION_MINOR };
    DEBUG_LOG_INFO("gaiaCorePlugin_GetApiVersion");

    GaiaFramework_SendResponse(t, GAIA_CORE_FEATURE_ID, get_api_version, sizeof(value), value);
}

static void gaiaCorePlugin_GetSupportedFeatures(GAIA_TRANSPORT *t)
{
    DEBUG_LOG("gaiaCorePlugin_GetSupportedFeatures");

    uint8 more_to_come = 0;
    uint8 number_of_registered_features = GaiaFrameworkFeature_GetNumberOfRegisteredFeatures();
    uint16 response_payload_length = (number_of_registered_features * 2) + 1;
    uint8 *response_payload = malloc(response_payload_length * sizeof(uint8));
    if (response_payload && gaiaCorePlugin_GetSupportedFeaturesPayload((response_payload_length - 1), &response_payload[1]))
    {
        response_payload[0] = more_to_come;
        GaiaFramework_SendResponse(t, GAIA_CORE_FEATURE_ID, get_supported_features, response_payload_length, response_payload);
    }
    else
    {
        DEBUG_LOG_ERROR("gaiaCorePlugin_GetSupportedFeatures, failed");
        GaiaFramework_SendError(t, GAIA_CORE_FEATURE_ID, get_supported_features, 0);
    }
    free(response_payload);
}


static void gaiaCorePlugin_GetSupportedFeaturesNext(GAIA_TRANSPORT *t)
{
    DEBUG_LOG("gaiaCorePlugin_GetSupportedFeaturesNext");
    GaiaFramework_SendError(t, GAIA_CORE_FEATURE_ID, get_supported_features_next, command_not_supported);
}

static void gaiaCorePlugin_GetSerialNumber(GAIA_TRANSPORT *t)
{
    const char * response_payload = DeviceInfo_GetSerialNumber();
    uint8 response_payload_length = strlen(response_payload);

    DEBUG_LOG("gaiaCorePlugin_GetSerialNumber");

    GaiaFramework_SendResponse(t, GAIA_CORE_FEATURE_ID, get_serial_number, response_payload_length, (uint8 *)response_payload);
}

static void gaiaCorePlugin_GetVariant(GAIA_TRANSPORT *t)
{
    const char * response_payload = DeviceInfo_GetName();
    uint8 response_payload_length = strlen(response_payload);

    DEBUG_LOG("gaiaCorePlugin_GetVariant");

    GaiaFramework_SendResponse(t, GAIA_CORE_FEATURE_ID, get_variant, response_payload_length, (uint8 *)response_payload);
}

static void gaiaCorePlugin_GetApplicationVersion(GAIA_TRANSPORT *t)
{
    const char * response_payload = DeviceInfo_GetHardwareVersion();
    uint8 response_payload_length = strlen(response_payload);

    DEBUG_LOG("gaiaCorePlugin_GetApplicationVersion");

    GaiaFramework_SendResponse(t, GAIA_CORE_FEATURE_ID, get_application_version, response_payload_length, (uint8 *)response_payload);
}

static void gaiaCorePlugin_DeviceReset(GAIA_TRANSPORT *t)
{
    DEBUG_LOG("gaiaCorePlugin_DeviceReset");

    GaiaFramework_SendResponse(t, GAIA_CORE_FEATURE_ID, device_reset, 0, NULL);

    appPowerReboot();
}

static void gaiaCorePlugin_RegisterNotification(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    bool error = TRUE;
    if (payload_length > 0)
    {
        const gaia_features_t feature = payload[0];
        DEBUG_LOG_INFO("gaiaCorePlugin_RegisterNotification, feature_id %u", feature);
        if (GaiaFrameworkFeature_RegisterForNotifications(feature))
        {
            GaiaFramework_SendResponse(t, GAIA_CORE_FEATURE_ID, register_notification, 0, NULL);
            GaiaFrameworkFeature_SendAllNotifications(t, feature);

            /* Record that feature has notifications enabled on this transport */
            uint32_t notifications = Gaia_TransportGetClientData(t);
            notifications |= 1UL << feature;
            Gaia_TransportSetClientData(t, notifications);

            error = FALSE;
        }
        else
            DEBUG_LOG_ERROR("gaiaCorePlugin_RegisterNotification, failed to register feature_id %u", payload[0]);

    }
    else
        DEBUG_LOG_ERROR("gaiaCorePlugin_RegisterNotification, no feature in packet");

    if (error)
        GaiaFramework_SendError(t, GAIA_CORE_FEATURE_ID, register_notification, 0);
}

static void gaiaCorePlugin_UnregisterNotification(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    bool error = TRUE;
    if (payload_length > 0)
    {
        const gaia_features_t feature = payload[0];
        DEBUG_LOG_INFO("gaiaCorePlugin_UnregisterNotification, feature_id %u", feature);
        if (GaiaFrameworkFeature_UnregisterForNotifications(feature))
        {
            GaiaFramework_SendResponse(t, GAIA_CORE_FEATURE_ID, unregister_notification, 0, NULL);

            /* Record that feature has notifications disabled on this transport */
            uint32_t notifications = Gaia_TransportGetClientData(t);
            notifications &= ~(1UL << feature);
            Gaia_TransportSetClientData(t, notifications);

            error = FALSE;
        }
        else
            DEBUG_LOG_ERROR("gaiaCorePlugin_UnregisterNotification, failed to unregister feature_id %u", payload[0]);

    }
    else
        DEBUG_LOG_ERROR("gaiaCorePlugin_UnregisterNotification, no feature in packet");

    if (error)
        GaiaFramework_SendError(t, GAIA_CORE_FEATURE_ID, unregister_notification, 0);
}

static void gaiaCorePlugin_GetTransportInfo(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    bool error = TRUE;
    if (payload_length > 0)
    {
        uint32 value;
        const gaia_transport_info_key_t key = payload[0];
        if (Gaia_TransportGetInfo(t, key, &value))
        {
            uint8 response[1 + sizeof(value)];
            response[0] = key;
            ByteUtilsSet4Bytes(response, 1, value);
            GaiaFramework_SendResponse(t, GAIA_CORE_FEATURE_ID, get_transport_info, sizeof(response), response);
            DEBUG_LOG_INFO("gaiaCorePlugin_GetTransportInfo, key %u, value %u", key, value);
            error = FALSE;
        }
        else
            DEBUG_LOG_ERROR("gaiaCorePlugin_GetTransportInfo, key %u not accepted", key);

    }
    else
        DEBUG_LOG_ERROR("gaiaCorePlugin_GetTransportInfo, no key in packet");

    if (error)
        GaiaFramework_SendError(t, GAIA_CORE_FEATURE_ID, get_transport_info, invalid_parameter);
}

static void gaiaCorePlugin_SetTransportParameter(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    bool error = TRUE;
    if (payload_length >= 5)
    {
        const gaia_transport_info_key_t key = payload[0];
        uint32 value = ByteUtilsGet4BytesFromStream(payload + 1) ;
        DEBUG_LOG_INFO("gaiaCorePlugin_SetTransportInfo, key %u, requested value %u", key, value);
        if (Gaia_TransportSetParameter(t, key, &value))
        {
            uint8 response[1 + sizeof(value)];
            response[0] = key;
            ByteUtilsSet4Bytes(response, 1, value);
            GaiaFramework_SendResponse(t, GAIA_CORE_FEATURE_ID, set_transport_parameter, sizeof(response), response);
            DEBUG_LOG_INFO("gaiaCorePlugin_SetTransportInfo, actual value %u", value);
            error = FALSE;
        }
    }
    else
        DEBUG_LOG_ERROR("gaiaCorePlugin_SetTransportInfo, no key and/or value in packet");

    if (error)
        GaiaFramework_SendError(t, GAIA_CORE_FEATURE_ID, set_transport_parameter, invalid_parameter);
}

static void gaiaCorePlugin_DataTransferSetup(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    DEBUG_LOG("gaiaCorePlugin_DataTransferSetup");
    if (payload_length == GAIA_DATA_TRANSFER_SETUP_CMD_PAYLOAD_SIZE)
    {
        GaiaFramework_DataTransferSetup(t, payload_length, payload);
    }
    else
    {
        DEBUG_LOG("gaiaCorePlugin_DataTransferSetup, Invalid payload length: %d", payload_length);
        GaiaFramework_SendError(t, GAIA_CORE_FEATURE_ID, data_transfer_setup, GAIA_STATUS_INVALID_PARAMETER);
    }
}

static void gaiaCorePlugin_DataTransferGet(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    DEBUG_LOG("gaiaCorePlugin_DataTransferGet");
    if (payload_length == GAIA_DATA_TRANSFER_GET_CMD_PAYLOAD_SIZE)
    {
        GaiaFramework_DataTransferGet(t, payload_length, payload);
    }
    else
    {
        DEBUG_LOG("gaiaCorePlugin_DataTransferGet, Invalid payload length: %d", payload_length);
        GaiaFramework_SendError(t, GAIA_CORE_FEATURE_ID, data_transfer_get, GAIA_STATUS_INVALID_PARAMETER);
    }
}

static void gaiaCorePlugin_DataTransferSet(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    DEBUG_LOG("gaiaCorePlugin_DataTransferSet");
    if (GAIA_DATA_TRANSFER_SET_CMD_HEADER_SIZE < payload_length)
    {
        GaiaFramework_DataTransferSet(t, payload_length, payload);
    }
    else
    {
        DEBUG_LOG("gaiaCorePlugin_DataTransferSet, Invalid payload length: %d", payload_length);
        GaiaFramework_SendError(t, GAIA_CORE_FEATURE_ID, data_transfer_set, GAIA_STATUS_INVALID_PARAMETER);
    }
}

static void gaiaCorePlugin_SendAllNotifications(GAIA_TRANSPORT *t)
{
    DEBUG_LOG("gaiaCorePlugin_SendAllNotifications");

    if (charger_client_is_registered)
    {
        switch(appChargerIsConnected())
        {
            case CHARGER_CONNECTED:
            case CHARGER_CONNECTED_NO_ERROR:
                current_charger_plugged_in_state = TRUE;
                break;

            case CHARGER_DISCONNECTED:
            default:
                current_charger_plugged_in_state = FALSE;
        }

        gaiaCorePlugin_SendChargerStatusNotification(t, current_charger_plugged_in_state);
    }
}

static void gaiaCorePlugin_SendChargerStatusNotification(GAIA_TRANSPORT *t, bool plugged)
{
    UNUSED(t);
    uint8 payload = (uint8)plugged;

    DEBUG_LOG("gaiaCorePlugin_SendChargerStatusNotification");
    GaiaFramework_SendNotification(GAIA_CORE_FEATURE_ID, charger_status_notification, sizeof(payload), &payload);
}

static void gaiaCorePlugin_ChargerTask(Task task, MessageId message_id, Message message)
{
    UNUSED(task);
    UNUSED(message);

    bool charger_plugged = current_charger_plugged_in_state;

    switch(message_id)
    {
        case CHARGER_MESSAGE_DETACHED:
            charger_plugged = FALSE;
            break;

        case CHARGER_MESSAGE_ATTACHED:
        case CHARGER_MESSAGE_COMPLETED:
        case CHARGER_MESSAGE_CHARGING_OK:
        case CHARGER_MESSAGE_CHARGING_LOW:
            charger_plugged = TRUE;
            break;

        default:
            DEBUG_LOG("gaiaCorePlugin_ChargerTask, Unknown charger message");
            break;
    }

    if (charger_plugged != current_charger_plugged_in_state)
    {
        current_charger_plugged_in_state = charger_plugged;
        gaiaCorePlugin_SendChargerStatusNotification(0, current_charger_plugged_in_state);
    }
}

static bool gaiaCorePlugin_GetSupportedFeaturesPayload(uint16 payload_length, uint8 *payload)
{
    uint16 payload_index;
    feature_list_handle_t *handle = NULL;
    bool status = TRUE;

    DEBUG_LOG_INFO("gaiaCorePlugin_GetSupportedFeaturesPayload");

    for (payload_index = 0; payload_index < payload_length; payload_index+=2 )
    {
        handle = GaiaFrameworkFeature_GetNextHandle(handle);
        if (handle)
        {
            status = GaiaFrameworkFeature_GetFeatureIdAndVersion(handle, &payload[payload_index], &payload[payload_index + 1]);
        }
        else
        {
            DEBUG_LOG_ERROR("gaiaCorePlugin_GetSupportedFeaturesPayload, FAILED");
            status = FALSE;
        }

        if (!status)
        {
            break;
        }
    }

    return status;
}
