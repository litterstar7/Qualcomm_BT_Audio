/*!
\copyright  Copyright (c) 2019-2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of the Telephony Service
*/

#include "telephony_service.h"

#include "kymera_adaptation.h"
#include "ui.h"
#include "ui_inputs.h"
#include "telephony_messages.h"
#include "voice_sources.h"
#include "voice_sources_list.h"
#include "usb_audio.h"

#include <focus_voice_source.h>
#include <panic.h>
#include <logging.h>

static void telephonyService_CallStateNotificationMessageHandler(Task task, MessageId id, Message message);
static void telephonyService_HandleUiInput(Task task, MessageId ui_input, Message message);

static TaskData telephony_message_handler_task = { telephonyService_CallStateNotificationMessageHandler };
static TaskData ui_handler_task = { telephonyService_HandleUiInput };

static const message_group_t ui_inputs[] =
{
    UI_INPUTS_TELEPHONY_MESSAGE_GROUP,
};

static const telephony_routing_ind_t *routing_observer = NULL;

static voice_source_t telephonyService_GetVoiceSourceInFocus(void)
{
    /* Default voice source focus should be set to hfp when no other source is
     * routed.
     */
    voice_source_t source = VoiceSources_GetRoutedSource();
    return (source == voice_source_none) ? voice_source_hfp_1 : source;
}

static void telephonyService_SendRoutingIndication(void)
{
    if (routing_observer && (routing_observer->RoutingInd))
    {
        routing_observer->RoutingInd();
    }
}

static void telephonyService_SendUnRoutedIndication(void)
{
    if (routing_observer && (routing_observer->UnroutedInd))
    {
        routing_observer->UnroutedInd();
    }
}

static void telephonyService_ConnectAudio(voice_source_t source)
{
    source_defined_params_t source_params;
    if(VoiceSources_GetConnectParameters(source, &source_params))
    {
        connect_parameters_t connect_params = 
        { .source = { .type = source_type_voice, .sourceu = voice_source_none },
           .source_params = source_params};
        /* update the voice source */
        connect_params.source.sourceu.voice = source;

        telephonyService_SendRoutingIndication();
        KymeraAdaptation_Connect(&connect_params);
        VoiceSources_ReleaseConnectParameters(source, &source_params);
    }
}

static void telephonyService_DisconnectAudio(voice_source_t source)
{
    source_defined_params_t source_params;
    if(VoiceSources_GetDisconnectParameters(source, &source_params))
    {
        disconnect_parameters_t disconnect_params = { 
           .source = { .type = source_type_voice, .sourceu = voice_source_none },
           .source_params = source_params};
        /* update the voice source */
        disconnect_params.source.sourceu.voice = source;
        
        KymeraAdaptation_Disconnect(&disconnect_params);
        VoiceSources_ReleaseDisconnectParameters(source, &source_params);
        telephonyService_SendUnRoutedIndication();
    }
}

static void telephonyService_CallStateNotificationMessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);
    switch(id)
    {
        case TELEPHONY_AUDIO_CONNECTED:
            if(message && telephonyService_GetVoiceSourceInFocus() == ((TELEPHONY_AUDIO_CONNECTED_T *)message)->voice_source)
            {
                telephonyService_ConnectAudio(((TELEPHONY_AUDIO_CONNECTED_T *)message)->voice_source);
            }
            break;
        case TELEPHONY_AUDIO_DISCONNECTED:
            if(message && telephonyService_GetVoiceSourceInFocus() == ((TELEPHONY_AUDIO_DISCONNECTED_T *)message)->voice_source)
            {
                telephonyService_DisconnectAudio(((TELEPHONY_AUDIO_DISCONNECTED_T *)message)->voice_source);
            }
            break;
        default:
            DEBUG_LOG("telephonyService_CallStateNotificationMessageHandler: Unhandled event 0x%x", id);
            break;
    }
}

static void telephonyService_HandleUiInput(Task task, MessageId ui_input, Message message)
{
    voice_source_t source = telephonyService_GetVoiceSourceInFocus();
    UNUSED(task);
    UNUSED(message);
    switch(ui_input)
    {
        case ui_input_voice_call_hang_up:
            VoiceSources_TerminateOngoingCall(source);
            break;

        case ui_input_voice_call_accept:
            VoiceSources_AcceptIncomingCall(source);
            break;

        case ui_input_voice_call_reject:
            VoiceSources_RejectIncomingCall(source);
            break;

        case ui_input_hfp_transfer_to_ag:
            VoiceSources_TransferOngoingCallAudioToAg(source);
            break;

        case ui_input_hfp_transfer_to_headset:
            VoiceSources_TransferOngoingCallAudioToSelf(source);
            break;

        case ui_input_hfp_voice_dial:
            VoiceSources_InitiateVoiceDial(source);
            break;

        default:
            break;
    }
}

static unsigned telephonyService_GetContext(void)
{
    unsigned context = context_voice_disconnected;
    voice_source_t focused_source = voice_source_none;

    if (Focus_GetVoiceSourceForContext(ui_provider_telephony, &focused_source))
    {
        context = VoiceSources_GetSourceContext(focused_source);
    }

    return context;
}

bool TelephonyService_Init(Task init_task)
{
    UNUSED(init_task);
    Telephony_RegisterForMessages(&telephony_message_handler_task);

    Ui_RegisterUiProvider(ui_provider_telephony, telephonyService_GetContext);

    Ui_RegisterUiInputConsumer(&ui_handler_task, (uint16*)ui_inputs, sizeof(ui_inputs)/sizeof(uint16));
    UsbAudio_ClientRegister(&telephony_message_handler_task,
                              USB_AUDIO_REGISTERED_CLIENT_TELEPHONY);
    return TRUE;
}

void TelephonyService_RegisterForRoutingInd(const telephony_routing_ind_t *interface)
{
    if (routing_observer)
    {
        DEBUG_LOG_ERROR("TelephonyService_RegisterForRoutingInd: Only one observer is supported");
        Panic();
    }

    routing_observer = interface;
}
