/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for the gaia framework API
*/

#define DEBUG_LOG_MODULE_NAME gaia_framework
#include <logging.h>
DEBUG_LOG_DEFINE_LEVEL_VAR

#include <panic.h>

#include "gaia_framework.h"
#include "gaia_framework_command.h"
#include "gaia_framework_feature.h"
#include "gaia_core_plugin.h"



#define GAIA_STATUS_NONE (0xFE)


bool GaiaFramework_Init(Task init_task)
{
    DEBUG_LOG("GaiaFramework_Init");

    GaiaFrameworkCommand_ResetVendorCommandHandler();
    GaiaFrameworkFeature_Init();
    GaiaCorePlugin_Init();
    return GaiaFrameworkInternal_Init(init_task);
}

void GaiaFramework_RegisterFeature(gaia_features_t feature_id, uint8 version_number,
                                   const gaia_framework_plugin_functions_t *functions)
{
    DEBUG_LOG("GaiaFramework_RegisterFeature");
    PanicFalse(GaiaFrameworkFeature_AddToList(feature_id, version_number, functions));
}


void GaiaFramework_RegisterVendorCommandHandler(gaia_framework_vendor_command_handler_fn_t command_handler)
{
    DEBUG_LOG("GaiaFramework_RegisterVendorCommandHandler");
    PanicFalse(GaiaFrameworkCommand_RegisterVendorCommandHandler(command_handler));
}

void GaiaFramework_SendVendorResponse(GAIA_TRANSPORT *t, uint16 vendor_id, uint8 feature_id, uint8 pdu_id, uint16 length, const uint8 *payload)
{
    DEBUG_LOG("GaiaFramework_SendVendorResponse, vendor_id %u, feature_id %u, pdu_id %u", vendor_id, feature_id, pdu_id);

    uint16 command_id = gaiaFramework_BuildCommandId(feature_id, pdu_type_response, pdu_id);
    GaiaSendPacket(t, vendor_id, command_id, GAIA_STATUS_NONE, length, payload);
}

void GaiaFramework_SendVendorError(GAIA_TRANSPORT *t, uint16 vendor_id, uint8 feature_id, uint8 pdu_id, uint8 status_code)
{
    DEBUG_LOG("GaiaFramework_SendVendorError, vendor_id %u, feature_id %u, pdu_id %u, status_code %u", vendor_id, feature_id, pdu_id, status_code);

    uint16 command_id = gaiaFramework_BuildCommandId(feature_id, pdu_type_error, pdu_id);
    GaiaSendPacket(t, vendor_id, command_id, GAIA_STATUS_NONE, sizeof(status_code), &status_code);
}

void GaiaFramework_SendVendorNotification(GAIA_TRANSPORT *t, uint16 vendor_id, uint8 feature_id, uint8 notification_id, uint16 length, const uint8 *payload)
{
    DEBUG_LOG("GaiaFramework_SendVendorNotification, vendor_id %u, feature_id %u, notification_id %u", feature_id, notification_id);
    uint16 command_id = gaiaFramework_BuildCommandId(feature_id, pdu_type_notification, notification_id);
    GaiaSendPacket(t, vendor_id, command_id, GAIA_STATUS_NONE, length, payload);
}

void GaiaFramework_SendResponse(GAIA_TRANSPORT *t, gaia_features_t feature_id, uint8 pdu_id, uint16 length, const uint8 *payload)
{
    DEBUG_LOG("GaiaFramework_SendResponse, feature_id %u, pdu_id %u", feature_id, pdu_id);

    uint16 command_id = gaiaFramework_BuildCommandId(feature_id, pdu_type_response, pdu_id);
    GaiaSendPacket(t, GAIA_V3_VENDOR_ID, command_id, GAIA_STATUS_NONE, length, payload);
}

void GaiaFramework_SendError(GAIA_TRANSPORT *t, gaia_features_t feature_id, uint8 pdu_id, uint8 status_code)
{
    DEBUG_LOG("GaiaFramework_SendError, feature_id %u, pdu_id %u, status_code %u", feature_id, pdu_id, status_code);

    uint16 command_id = gaiaFramework_BuildCommandId(feature_id, pdu_type_error, pdu_id);
    GaiaSendPacket(t, GAIA_V3_VENDOR_ID, command_id, GAIA_STATUS_NONE, sizeof(status_code), &status_code);
}

static void gaiaFramework_SendNotificationToEndpoint(gaia_features_t feature_id, uint8 notification_id, uint16 length, const uint8 *payload, bool is_data)
{
    if (GaiaFrameworkFeature_IsNotificationsActive(feature_id))
    {
        /* Find transports that have notifications enabled for this feature */
        uint8_t index = 0;
        GAIA_TRANSPORT *t = Gaia_TransportIterate(&index);
        while (t)
        {
            uint32_t notifications = Gaia_TransportGetClientData(t);
            if (notifications & 1UL << feature_id)
            {
                DEBUG_LOG("GaiaFramework_SendNotification, feature_id %u, notification_id %u", feature_id, notification_id);
                uint16 command_id = gaiaFramework_BuildCommandId(feature_id, pdu_type_notification, notification_id);
                if (is_data)
                    Gaia_SendDataPacket(t, GAIA_V3_VENDOR_ID, command_id, GAIA_STATUS_NONE, length, payload);
                else
                    GaiaSendPacket(t, GAIA_V3_VENDOR_ID, command_id, GAIA_STATUS_NONE, length, payload);
            }
            t = Gaia_TransportIterate(&index);
        }
    }
    else
        DEBUG_LOG("GaiaFramework_SendNotification, feature_id %u, notification_id %u not active", feature_id, notification_id);
}

void GaiaFramework_SendNotification(gaia_features_t feature_id, uint8 notification_id, uint16 length, const uint8 *payload)
{
    gaiaFramework_SendNotificationToEndpoint(feature_id, notification_id, length, payload, FALSE);
}

void GaiaFramework_SendDataNotification(gaia_features_t feature_id, uint8 notification_id, uint16 length, const uint8 *payload)
{
    gaiaFramework_SendNotificationToEndpoint(feature_id, notification_id, length, payload, TRUE);
}
