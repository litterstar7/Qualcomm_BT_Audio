/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for the  gaia anc framework plugin
*/

#include "anc_gaia_plugin.h"
#include "anc_gaia_plugin_private.h"
#include "anc_state_manager.h"
#include "ui.h"
#include "phy_state.h"

#include <gaia.h>
#include <logging.h>
#include <panic.h>
#include <stdlib.h>

#define ANC_GAIA_AANC_DISABLE_MSG_ID ANC_UPDATE_AANC_DISABLE_IND
#define ANC_GAIA_AANC_ENABLE_MSG_ID ANC_UPDATE_AANC_ENABLE_IND
#define ANC_GAIA_DEFAULT_AANC_FF_GAIN 0

/*! \brief Function pointer definition for the command handler

    \param pdu_id      PDU specific ID for the message

    \param length      Length of the payload

    \param payload     Payload data
*/
static gaia_framework_command_status_t ancGaiaPlugin_MainHandler(GAIA_TRANSPORT *t, uint8 pdu_id, uint16 payload_length, const uint8 *payload);

/*! \brief Function that sends all available notifications
*/
static void ancGaiaPlugin_SendAllNotifications(GAIA_TRANSPORT *t);

/*! GAIA ANC Plugin Message Handler. */
static void ancGaiaPlugin_HandleMessage(Task task, MessageId id, Message message);

static void ancGaiaPlugin_GetAncState(GAIA_TRANSPORT *t);
static void ancGaiaPlugin_SetAncState(GAIA_TRANSPORT *t,uint16 payload_length, const uint8 *payload);
static void ancGaiaPlugin_GetNumOfModes(GAIA_TRANSPORT *t);
static void ancGaiaPlugin_GetCurrentMode(GAIA_TRANSPORT *t);
static void ancGaiaPlugin_SetAncMode(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload);
static void ancGaiaPlugin_GetAncLeakthroughGain(GAIA_TRANSPORT *t);
static void ancGaiaPlugin_SetAncLeakthroughGain(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload);

static void ancGaiaPlugin_AancStateUpdateNotify(MessageId id);
static void ancGaiaPlugin_AancFFGainUpdateNotify(Message msg);

#ifdef ENABLE_ADAPTIVE_ANC
static void ancGaiaPlugin_SendAancStateNotification(void);
static void ancGaiaPlugin_SendAancFFGainNotification(void);
#endif

static bool ancGaiaPlugin_CanInjectUiInput(void)
{
    bool can_inject = TRUE;

#ifndef INCLUDE_STEREO
    can_inject = appPhyStateIsOutOfCase(); /* Verify if device is 'out of case' incase of earbud application*/
#endif

    return can_inject;
}

static void ancGaiaPlugin_SetReceivedCommand(GAIA_TRANSPORT *t, uint8 received_command)
{
    anc_gaia_plugin_task_data_t *anc_gaia_data = ancGaiaPlugin_GetTaskData();

    anc_gaia_data -> command_received_transport = t;
    anc_gaia_data -> received_command = received_command;
}

static uint8 ancGaiaPlugin_GetReceivedCommand(void)
{
    anc_gaia_plugin_task_data_t *anc_gaia_data = ancGaiaPlugin_GetTaskData();

    return anc_gaia_data -> received_command;
}

static void ancGaiaPlugin_ResetReceivedCommand(void)
{
    anc_gaia_plugin_task_data_t *anc_gaia_data = ancGaiaPlugin_GetTaskData();

    anc_gaia_data -> command_received_transport = 0;
}

static bool ancGaiaPlugin_IsCommandReceived(void)
{
    anc_gaia_plugin_task_data_t *anc_gaia_data = ancGaiaPlugin_GetTaskData();

    return anc_gaia_data -> command_received_transport ? TRUE : FALSE;
}

static void ancGaiaPlugin_SetAncEnable(void)
{
    if (ancGaiaPlugin_CanInjectUiInput())
    {
        DEBUG_LOG_ALWAYS("ancGaiaPlugin_SetAncEnable");
        Ui_InjectUiInput(ui_input_anc_on);
    }
}

static void ancGaiaPlugin_SetAncDisable(void)
{
    if (ancGaiaPlugin_CanInjectUiInput())
    {
        DEBUG_LOG_ALWAYS("ancGaiaPlugin_SetAncDisable");
        Ui_InjectUiInput(ui_input_anc_off);
    }
}

static void ancGaiaPlugin_SetMode(anc_mode_t mode)
{
    if (ancGaiaPlugin_CanInjectUiInput())
    {
        DEBUG_LOG_ALWAYS("ancGaiaPlugin_SetMode");
        switch(mode)
        {
            case anc_mode_1:
                Ui_InjectUiInput(ui_input_anc_set_mode_1);
                break;
            case anc_mode_2:
                Ui_InjectUiInput(ui_input_anc_set_mode_2);
                break;
            case anc_mode_3:
                Ui_InjectUiInput(ui_input_anc_set_mode_3);
                break;
            case anc_mode_4:
                Ui_InjectUiInput(ui_input_anc_set_mode_4);
                break;
            case anc_mode_5:
                Ui_InjectUiInput(ui_input_anc_set_mode_5);
                break;
            case anc_mode_6:
                Ui_InjectUiInput(ui_input_anc_set_mode_6);
                break;
            case anc_mode_7:
                Ui_InjectUiInput(ui_input_anc_set_mode_7);
                break;
            case anc_mode_8:
                Ui_InjectUiInput(ui_input_anc_set_mode_8);
                break;
            case anc_mode_9:
                Ui_InjectUiInput(ui_input_anc_set_mode_9);
                break;
            case anc_mode_10:
                Ui_InjectUiInput(ui_input_anc_set_mode_10);
                break;
            default:
                Ui_InjectUiInput(ui_input_anc_set_mode_1);
                break;
        }
    }
}

static void ancGaiaPlugin_SetGain(uint8 gain)
{
    UNUSED(gain);
    if (ancGaiaPlugin_CanInjectUiInput())
    {
        DEBUG_LOG_ALWAYS("ancGaiaPlugin_SetGain");
        AncStateManager_StoreAncLeakthroughGain(gain);
        Ui_InjectUiInput(ui_input_anc_set_leakthrough_gain);
    }
}

void AncGaiaPlugin_Init(void)
{
    static const gaia_framework_plugin_functions_t functions =
    {
        .command_handler = ancGaiaPlugin_MainHandler,
        .send_all_notifications = ancGaiaPlugin_SendAllNotifications,
        .transport_connect = NULL,
        .transport_disconnect = NULL,
    };

    DEBUG_LOG("AncGaiaPlugin_Init");

    anc_gaia_plugin_task_data_t *anc_gaia_data = ancGaiaPlugin_GetTaskData();

    /* Initialise plugin framework task data */
    memset(anc_gaia_data, 0, sizeof(*anc_gaia_data));
    anc_gaia_data->task.handler = ancGaiaPlugin_HandleMessage;

    AncStateManager_ClientRegister(ancGaiaPlugin_GetTask());

    GaiaFramework_RegisterFeature(GAIA_AUDIO_CURATION_FEATURE_ID, ANC_GAIA_PLUGIN_VERSION, &functions);
}

static gaia_framework_command_status_t ancGaiaPlugin_MainHandler(GAIA_TRANSPORT *t, uint8 pdu_id, uint16 payload_length, const uint8 *payload)
{
    DEBUG_LOG("ancGaiaPlugin_MainHandler, called for %d", pdu_id);

    switch (pdu_id)
    {
        case anc_gaia_get_anc_state:
            ancGaiaPlugin_GetAncState(t);
            break;

        case anc_gaia_set_anc_state:
            ancGaiaPlugin_SetAncState(t, payload_length, payload);
            break;

        case anc_gaia_get_num_anc_modes:
            ancGaiaPlugin_GetNumOfModes(t);
            break;

        case anc_gaia_get_current_anc_mode:
            ancGaiaPlugin_GetCurrentMode(t);
            break;

        case anc_gaia_set_anc_mode:
            ancGaiaPlugin_SetAncMode(t, payload_length, payload);
            break;

        case anc_gaia_get_configured_leakthrough_gain:
            ancGaiaPlugin_GetAncLeakthroughGain(t);
            break;

        case anc_gaia_set_configured_leakthrough_gain:
            ancGaiaPlugin_SetAncLeakthroughGain(t, payload_length, payload);
            break;

        default:
            DEBUG_LOG_ERROR("ancGaiaPlugin_MainHandler, unhandled call for %u", pdu_id);
            return command_not_handled;
    }

    return command_handled;
}

static void ancGaiaPlugin_SendAllNotifications(GAIA_TRANSPORT *t)
{
    UNUSED(t);
    DEBUG_LOG("ancGaiaPlugin_SendAllNotifications");

    uint8 anc_state_payload = AncStateManager_IsEnabled() ? ANC_GAIA_STATE_ENABLE : ANC_GAIA_STATE_DISABLE;
    GaiaFramework_SendNotification(GAIA_AUDIO_CURATION_FEATURE_ID, anc_gaia_anc_state_notification, ANC_GAIA_ANC_STATE_NOTIFICATION_PAYLOAD_LENGTH, &anc_state_payload);

    uint8 anc_mode_payload = AncStateManager_GetCurrentMode();
    GaiaFramework_SendNotification(GAIA_AUDIO_CURATION_FEATURE_ID, anc_gaia_anc_mode_change_notification, ANC_GAIA_ANC_MODE_CHANGE_NOTIFICATION_PAYLOAD_LENGTH, &anc_mode_payload);

    if(AncStateManager_GetCurrentMode() != anc_mode_1)
	{
        uint8 anc_gain_payload = AncStateManager_GetAncLeakthroughGain();
        GaiaFramework_SendNotification(GAIA_AUDIO_CURATION_FEATURE_ID, anc_gaia_anc_gain_change_notification, ANC_GAIA_ANC_GAIN_CHANGE_NOTIFICATION_PAYLOAD_LENGTH, &anc_gain_payload);
	}
#ifdef ENABLE_ADAPTIVE_ANC
    else
    {
        ancGaiaPlugin_SendAancStateNotification();
        ancGaiaPlugin_SendAancFFGainNotification();
    }
#endif
}

static void ancGaiaPlugin_GetAncState(GAIA_TRANSPORT *t)
{
    DEBUG_LOG("ancGaiaPlugin_GetAncState");

    uint8 payload = AncStateManager_IsEnabled() ? ANC_GAIA_STATE_ENABLE : ANC_GAIA_STATE_DISABLE;

    DEBUG_LOG("ancGaiaPlugin_GetAncState, ANC State is %d", payload);
    GaiaFramework_SendResponse(t, GAIA_AUDIO_CURATION_FEATURE_ID, anc_gaia_get_anc_state, ANC_GAIA_GET_ANC_STATE_RESPONSE_PAYLOAD_LENGTH, &payload);
}

static void ancGaiaPlugin_SetAncState(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    UNUSED(t);
    DEBUG_LOG("ancGaiaPlugin_SetAncState");

    if(payload_length == ANC_GAIA_SET_ANC_STATE_PAYLOAD_LENGTH)
    {
        if(*payload == ANC_GAIA_SET_ANC_STATE_ENABLE)
        {
            ancGaiaPlugin_SetAncEnable();
        }
        else if(*payload == ANC_GAIA_SET_ANC_STATE_DISABLE)
        {
            ancGaiaPlugin_SetAncDisable();
        }
    }
    ancGaiaPlugin_SetReceivedCommand(t, anc_gaia_set_anc_state);
}

static void ancGaiaPlugin_GetNumOfModes(GAIA_TRANSPORT *t)
{
    DEBUG_LOG("ancGaiaPlugin_GetNumOfModes");

    uint8 payload= AncStateManager_GetNumberOfModes();

    DEBUG_LOG("ancGaiaPlugin_GetNumOfModes, Number of modes = %d", payload);
    GaiaFramework_SendResponse(t, GAIA_AUDIO_CURATION_FEATURE_ID, anc_gaia_get_num_anc_modes, ANC_GAIA_GET_ANC_NUM_OF_MODES_RESPONSE_PAYLOAD_LENGTH, &payload);
}

static void ancGaiaPlugin_GetCurrentMode(GAIA_TRANSPORT *t)
{
    DEBUG_LOG("ancGaiaPlugin_GetCurrentMode");

    uint8 payload= AncStateManager_GetCurrentMode();

    DEBUG_LOG("ancGaiaPlugin_GetCurrentMode, Current mode = %d", payload);
    GaiaFramework_SendResponse(t, GAIA_AUDIO_CURATION_FEATURE_ID, anc_gaia_get_current_anc_mode, ANC_GAIA_GET_ANC_CURRENT_MODE_RESPONSE_PAYLOAD_LENGTH, &payload);
}


static void ancGaiaPlugin_SetAncMode(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    UNUSED(t);
    DEBUG_LOG("ancGaiaPlugin_SetAncMode");

    if(payload_length == ANC_GAIA_SET_ANC_STATE_PAYLOAD_LENGTH)
    {
        anc_mode_t mode = (anc_mode_t)*payload;
        ancGaiaPlugin_SetMode(mode);
    }
    ancGaiaPlugin_SetReceivedCommand(t, anc_gaia_set_anc_mode);
}

static void ancGaiaPlugin_GetAncLeakthroughGain(GAIA_TRANSPORT *t)
{
    DEBUG_LOG("ancGaiaPlugin_GetAncLeakthroughGain");

    if(AncStateManager_GetCurrentMode() != anc_mode_1)
    {
        uint8 payload = AncStateManager_GetAncLeakthroughGain();

        DEBUG_LOG("ancGaiaPlugin_GetAncLeakthroughGain, ANC Leakthrough gain = %d", payload);
        GaiaFramework_SendResponse(t, GAIA_AUDIO_CURATION_FEATURE_ID, anc_gaia_get_configured_leakthrough_gain, ANC_GAIA_GET_ANC_LEAKTHROUGH_GAIN_RESPONSE_PAYLOAD_LENGTH, &payload);
    }
}

static void ancGaiaPlugin_SetAncLeakthroughGain(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    UNUSED(t);
    DEBUG_LOG("ancGaiaPlugin_SetAncLeakthroughGain");

    if(AncStateManager_GetCurrentMode() != anc_mode_1 && payload_length == ANC_GAIA_SET_ANC_STATE_PAYLOAD_LENGTH)
    {
        ancGaiaPlugin_SetGain(*payload);
        ancGaiaPlugin_SetReceivedCommand(t, anc_gaia_set_configured_leakthrough_gain);
    }
}

/*! \brief Handle local events for ANC data update.and Send response */
static void ancGaiaPlugin_SendResponse(GAIA_TRANSPORT *t)
{
    GaiaFramework_SendResponse(t, GAIA_AUDIO_CURATION_FEATURE_ID, ancGaiaPlugin_GetReceivedCommand(), 0, NULL);

    ancGaiaPlugin_ResetReceivedCommand();
}

#ifdef ENABLE_ADAPTIVE_ANC
/*! \brief Handles Adaptive ANC state notification when mobile registers for notifications */
static void ancGaiaPlugin_SendAancStateNotification(void)
{
    MessageId id;
    id = AncStateManager_IsEnabled() ? ANC_GAIA_AANC_ENABLE_MSG_ID : ANC_GAIA_AANC_DISABLE_MSG_ID;

    ancGaiaPlugin_AancStateUpdateNotify(id);
}

/*! \brief Handles Adaptive ANC FF gain notification when mobile registers for notifications */
static void ancGaiaPlugin_SendAancFFGainNotification(void)
{
    /*Send gain notification only if Adaptive ANC is enabled */
    if(AncStateManager_IsEnabled())
    {
        AANC_FF_GAIN_NOTIFY_T* msg;
        msg = PanicUnlessMalloc(sizeof(AANC_FF_GAIN_NOTIFY_T));

        msg->left_aanc_ff_gain = ANC_GAIA_DEFAULT_AANC_FF_GAIN;
        msg->right_aanc_ff_gain = ANC_GAIA_DEFAULT_AANC_FF_GAIN;
        ancGaiaPlugin_AancFFGainUpdateNotify(msg);

        free(msg);
    }
}
#endif

/*! \brief Handle local events for ANC data update.and Sends Notification */
static void ancGaiaPlugin_HandleAncUpdateInd(MessageId id, Message msg)
{
    uint8 notification_id = 0;
    uint8 payload_length = 0;
    uint8 payload = 0;

    switch (id)
    {
        case ANC_UPDATE_STATE_DISABLE_IND:
            {
                notification_id = anc_gaia_anc_state_notification;
                payload_length = ANC_GAIA_ANC_STATE_NOTIFICATION_PAYLOAD_LENGTH;
                payload = ANC_GAIA_STATE_DISABLE;
            }
            break;

        case ANC_UPDATE_STATE_ENABLE_IND:
            {
                notification_id = anc_gaia_anc_state_notification;
                payload_length = ANC_GAIA_ANC_STATE_NOTIFICATION_PAYLOAD_LENGTH;
                payload = ANC_GAIA_STATE_ENABLE;
            }
            break;

        case ANC_UPDATE_MODE_CHANGED_IND:
            {
                ANC_UPDATE_MODE_CHANGED_IND_T *anc_data = (ANC_UPDATE_MODE_CHANGED_IND_T *)msg;

                notification_id = anc_gaia_anc_mode_change_notification;
                payload_length = ANC_GAIA_ANC_MODE_CHANGE_NOTIFICATION_PAYLOAD_LENGTH;
                payload = anc_data->mode;
            }
            break;

        case ANC_UPDATE_LEAKTHROUGH_GAIN_IND:
            {
                ANC_UPDATE_LEAKTHROUGH_GAIN_IND_T *anc_data = (ANC_UPDATE_LEAKTHROUGH_GAIN_IND_T *)msg;

                notification_id = anc_gaia_anc_gain_change_notification;
                payload_length = ANC_GAIA_ANC_GAIN_CHANGE_NOTIFICATION_PAYLOAD_LENGTH;
                payload = anc_data->anc_leakthrough_gain;
            }
            break;
    }

    if(ancGaiaPlugin_IsCommandReceived())
    {
        anc_gaia_plugin_task_data_t *anc_gaia_data = ancGaiaPlugin_GetTaskData();
        ancGaiaPlugin_SendResponse(anc_gaia_data->command_received_transport);
    }
    GaiaFramework_SendNotification(GAIA_AUDIO_CURATION_FEATURE_ID, notification_id, payload_length, &payload);

}

/*! \brief Handles Adaptive ANC state updates from ANC domain and Sends notification accordingly */
static void ancGaiaPlugin_AancStateUpdateNotify(MessageId id)
{
    uint8 notification_payload_length;
    uint8 *left_device_payload = NULL;
    uint8 *right_device_payload = NULL;
    uint8 aanc_state_payload;

    bool aanc_enable = (id == ANC_UPDATE_AANC_ENABLE_IND);

    notification_payload_length = ANC_GAIA_ADAPTIVE_ANC_STATE_NOTIFICATION_PAYLOAD_LENGTH;
    aanc_state_payload = aanc_enable ? ANC_GAIA_AANC_ENABLE : ANC_GAIA_AANC_DISABLE;

    left_device_payload = PanicUnlessMalloc(notification_payload_length * sizeof(uint8));
    right_device_payload = PanicUnlessMalloc(notification_payload_length * sizeof(uint8));

    left_device_payload[ANC_GAIA_DEVICE_OFFSET] = ANC_GAIA_LEFT_DEVICE;
    left_device_payload[ANC_GAIA_AANC_STATE_OFFSET] =aanc_state_payload;

    right_device_payload[ANC_GAIA_DEVICE_OFFSET] = ANC_GAIA_RIGHT_DEVICE;
    right_device_payload[ANC_GAIA_AANC_STATE_OFFSET] =aanc_state_payload;

    GaiaFramework_SendNotification(GAIA_AUDIO_CURATION_FEATURE_ID, anc_gaia_adaptive_anc_state_notification, notification_payload_length, left_device_payload);
    GaiaFramework_SendNotification(GAIA_AUDIO_CURATION_FEATURE_ID, anc_gaia_adaptive_anc_state_notification, notification_payload_length, right_device_payload);

    free(left_device_payload);
    free(right_device_payload);
}

/*! \brief Handles AANC FF Gain updates from ANC domain and Sends notification */
static void ancGaiaPlugin_AancFFGainUpdateNotify(Message msg)
{
    const AANC_FF_GAIN_NOTIFY_T* aanc_ff_gains = (AANC_FF_GAIN_NOTIFY_T*)msg;

    uint8 notification_payload_length;
    uint8 *left_device_payload = NULL;
    uint8 *right_device_payload = NULL;

    notification_payload_length = ANC_GAIA_ADAPTIVE_ANC_FF_GAIN_NOTIFICATION_PAYLOAD_LENGTH;
    left_device_payload = PanicUnlessMalloc(notification_payload_length * sizeof(uint8));
    right_device_payload = PanicUnlessMalloc(notification_payload_length * sizeof(uint8));

    left_device_payload[ANC_GAIA_DEVICE_OFFSET] = ANC_GAIA_LEFT_DEVICE;
    left_device_payload[ANC_GAIA_AANC_FF_GAIN_OFFSET] = aanc_ff_gains->left_aanc_ff_gain;

    right_device_payload[ANC_GAIA_DEVICE_OFFSET] = ANC_GAIA_RIGHT_DEVICE;
    right_device_payload[ANC_GAIA_AANC_FF_GAIN_OFFSET] = aanc_ff_gains->right_aanc_ff_gain;

    GaiaFramework_SendNotification(GAIA_AUDIO_CURATION_FEATURE_ID, anc_gaia_adapted_ff_gain_notification, notification_payload_length, left_device_payload);
    GaiaFramework_SendNotification(GAIA_AUDIO_CURATION_FEATURE_ID, anc_gaia_adapted_ff_gain_notification, notification_payload_length, right_device_payload);

    free(left_device_payload);
    free(right_device_payload);
}

static void ancGaiaPlugin_HandleMessage(Task task, MessageId id, Message msg)
{
    UNUSED(task);

    switch (id)
    {
        /* ANC update indication */
        case ANC_UPDATE_STATE_ENABLE_IND:
        case ANC_UPDATE_STATE_DISABLE_IND:
        case ANC_UPDATE_MODE_CHANGED_IND:
        case ANC_UPDATE_LEAKTHROUGH_GAIN_IND:
            ancGaiaPlugin_HandleAncUpdateInd(id, msg);
            break;

        /* Adaptive ANC State notification */
        case ANC_UPDATE_AANC_ENABLE_IND:
        case ANC_UPDATE_AANC_DISABLE_IND:
                ancGaiaPlugin_AancStateUpdateNotify(id);
                break;

        /* AANC FF Gain notification */
        case AANC_FF_GAIN_NOTIFY:
                ancGaiaPlugin_AancFFGainUpdateNotify(msg);
                break;

        default:
            break;
    }
}

