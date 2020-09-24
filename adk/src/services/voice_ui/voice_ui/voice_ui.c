/*!
\copyright  Copyright (c) 2019 - 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       voice_ui.c
\brief      Implementation of voice ui service.
*/

#include "logging.h"
#include <task_list.h>
#include <csrtypes.h>
#include <panic.h>
#include <power_manager.h>
#include <vmtypes.h>
#include <handset_service.h>
#include <feature.h>

#include "ui.h"

#include "voice_ui.h"
#include "voice_ui_config.h"
#include "voice_ui_container.h"
#include "voice_ui_audio.h"
#include "voice_ui_gaia_plugin.h"
#include "voice_ui_peer_sig.h"
#include "telephony_service.h"
#include <device_properties.h>

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_TYPE(voice_ui_msg_id_t)

#define VOICE_UI_REBOOT_DELAY_MILLISECONDS ((Delay) 250)

static void voiceUi_HandleMessage(Task task, MessageId id, Message message);
static const TaskData msg_handler = {voiceUi_HandleMessage};

static const telephony_routing_ind_t routing_ind =
{
    .RoutingInd = VoiceUi_SuspendAudio,
    .UnroutedInd = VoiceUi_ResumeAudio,
};

/* Ui Inputs in which voice ui service is interested*/
static const uint16 voice_ui_inputs[] =
{
    ID_TO_MSG_GRP(UI_INPUTS_VOICE_UI_MESSAGE_BASE),
};

static task_list_t * voice_ui_client_list;

static Task voiceUi_GetTask(void)
{
    return (Task)&msg_handler;
}

static void voiceUi_HandleUiInput(MessageId ui_input)
{
    voice_ui_handle_t* handle = VoiceUi_GetActiveVa();

    if(handle)
    {
        VoiceUi_EventHandler(handle, ui_input);
    }
}

static unsigned voiceUi_GetUiContext(void)
{
    return (unsigned)context_voice_ui_default;
}

static void voiceUi_HandleHandsetConnected(HANDSET_SERVICE_CONNECTED_IND_T *ind)
{
    device_t device;
    uint16 flags;

    DEBUG_LOG_DEBUG("voiceUi_HandleHandsetConnected: addr %04x %02x %06x",
                    ind->addr.nap, ind->addr.uap, ind->addr.lap);

    device = BtDevice_GetDeviceForBdAddr(&ind->addr);
    if (device && Device_GetPropertyU16(device, device_property_flags, &flags))
    {
        if (flags & DEVICE_FLAGS_NOT_PAIRED)
        {
            VoiceUi_SetSelectedAssistant(VOICE_UI_PROVIDER_DEFAULT);
            DEBUG_LOG_DEBUG("voiceUi_HandleHandsetConnected: set VA provider enum:voice_ui_provider_t:%d", VOICE_UI_PROVIDER_DEFAULT);
        }
    }
    else
    {
        DEBUG_LOG_ERROR("voiceUi_HandleHandsetConnected: no device flags");
    }
}

static void voiceUi_HandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);

    if (isMessageUiInput(id))
    {
        voiceUi_HandleUiInput(id);
        return;
    }

    switch (id)
    {
    case VOICE_UI_INTERNAL_REBOOT:
        appPowerReboot();
        break;

    case HANDSET_SERVICE_CONNECTED_IND:
        voiceUi_HandleHandsetConnected((HANDSET_SERVICE_CONNECTED_IND_T *) message);
        break;

    default:
        DEBUG_LOG_DEBUG("voiceUi_HandleMessage: unhandled 0x%04X", id);
        break;
    }
}

static void voiceUi_licenseCheck(void)
{
    if (FeatureVerifyLicense(TDFBC))
    {
        DEBUG_LOG_VERBOSE("voiceUi_licenseCheck: TDFBC is licensed");
    }
    else
    {
        DEBUG_LOG_WARN("voiceUi_licenseCheck: TDFBC not licensed");
    }

    if (FeatureVerifyLicense(CVC_SEND_HS_2MIC_MO))
    {
        DEBUG_LOG_VERBOSE("voiceUi_licenseCheck: cVc Send 2-MIC is licensed");
    }
    else
    {
        DEBUG_LOG_WARN("voiceUi_licenseCheck: cVc Send 2-MIC not licensed");
    }

#ifdef INCLUDE_WUW
    if (FeatureVerifyLicense(VAD))
    {
        DEBUG_LOG_VERBOSE("voiceUi_licenseCheck: VAD is licensed");
    }
    else
    {
        DEBUG_LOG_WARN("voiceUi_licenseCheck: VAD not licensed");
        if (LOG_LEVEL_CURRENT_SYMBOL >= DEBUG_LOG_LEVEL_VERBOSE)
        {
            Panic();
        }
    }
#endif /* INCLUDE_WUW */
}

bool VoiceUi_Init(Task init_task)
{
    DEBUG_LOG("VoiceUi_Init()");

    UNUSED(init_task);

    voice_ui_client_list = TaskList_Create();
    VoiceUiGaiaPlugin_Init();
    VoiceUi_PeerSignallingInit();

    HandsetService_ClientRegister(voiceUi_GetTask());
    TelephonyService_RegisterForRoutingInd(&routing_ind);
    
    /* Register av task call back as ui provider*/
    Ui_RegisterUiProvider(ui_provider_voice_ui, voiceUi_GetUiContext);

    Ui_RegisterUiInputConsumer(voiceUi_GetTask(), (uint16*)voice_ui_inputs, sizeof(voice_ui_inputs)/sizeof(uint16));

    voiceUi_licenseCheck();

    return TRUE;
}

void VoiceUi_Notify(voice_ui_msg_id_t msg)
{
    TaskList_MessageSendId(voice_ui_client_list, msg);
}

static void voiceAssistant_RegisterMessageGroup(Task task, message_group_t group)
{
    PanicFalse(group == VOICE_UI_SERVICE_MESSAGE_GROUP);
    TaskList_AddTask(voice_ui_client_list, task);
}

void VoiceUi_RebootLater(void)
{
    MessageSendLater(voiceUi_GetTask(), VOICE_UI_INTERNAL_REBOOT, NULL, VOICE_UI_REBOOT_DELAY_MILLISECONDS);
}

void VoiceUi_AssistantConnected(void)
{
    VoiceUi_UpdateHfpState();
}

MESSAGE_BROKER_GROUP_REGISTRATION_MAKE(VOICE_UI_SERVICE, voiceAssistant_RegisterMessageGroup, NULL);
