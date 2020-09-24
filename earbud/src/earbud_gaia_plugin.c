/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for the gaia framework earbud plugin
*/

#include "earbud_gaia_plugin.h"

#include <logging.h>
#include <panic.h>

#include <gaia_framework_feature.h>


/*! \brief Function pointer definition for the command handler

    \param pdu_id      PDU specific ID for the message
    \param length      Length of the payload
    \param payload     Payload data
*/
static gaia_framework_command_status_t earbudGaiaPlugin_MainHandler(GAIA_TRANSPORT *t, uint8 pdu_id, uint16 payload_length, const uint8 *payload);

/*! \brief Function that sends all available notifications
*/
static void earbudGaiaPlugin_SendAllNotifications(GAIA_TRANSPORT *t);


void EarbudGaiaPlugin_Init(void)
{
    static const gaia_framework_plugin_functions_t functions =
    {
        .command_handler = earbudGaiaPlugin_MainHandler,
        .send_all_notifications = earbudGaiaPlugin_SendAllNotifications,
        .transport_connect = NULL,
        .transport_disconnect = NULL,
    };

    DEBUG_LOG("EarbydGaiaPlugin_Init");

    GaiaFramework_RegisterFeature(GAIA_EARBUD_FEATURE_ID, EARBUD_GAIA_PLUGIN_VERSION, &functions);
}

static gaia_framework_command_status_t earbudGaiaPlugin_MainHandler(GAIA_TRANSPORT *t, uint8 pdu_id, uint16 payload_length, const uint8 *payload)
{
    DEBUG_LOG("earbudGaiaPlugin_MainHandler, called for %d", pdu_id);

    UNUSED(payload_length);
    UNUSED(payload);
    UNUSED(t);

    switch (pdu_id)
    {
        /* This is to be expanded with earbud specific PDU IDs */
        default:
            DEBUG_LOG_ERROR("earbudGaiaPlugin_MainHandler, unhandled call for %d", pdu_id);
    }

    return command_not_handled;
}


static void earbudGaiaPlugin_SendAllNotifications(GAIA_TRANSPORT *t)
{
    DEBUG_LOG("earbudGaiaPlugin_SendAllNotifications");
    UNUSED(t);
}

void EarbudGaiaPlugin_PrimaryAboutToChange(earbud_plugin_handover_types_t handover_type, uint8 delay)
{
    DEBUG_LOG("EarbudGaiaPlugin_PrimaryHasChanged, handover_type %u, delay %u", handover_type, delay);

    uint8 payload[2] = {handover_type, delay};
    GaiaFramework_SendNotification(GAIA_EARBUD_FEATURE_ID, primary_earbud_about_to_change, sizeof(payload), payload);
}
