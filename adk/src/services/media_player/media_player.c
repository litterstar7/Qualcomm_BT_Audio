/*!
\copyright  Copyright (c) 2019-2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of the media player service.
*/

#include "media_player.h"
#include <logging.h>
#include <panic.h>

#include "ui.h"
#include "av.h"
#include "audio_sources.h"
#include "kymera_adaptation.h"
#include "wired_audio_source.h"
#include "usb_audio.h"
#include "audio_router.h"

#include <focus_audio_source.h>

static void mediaPlayer_HandleMessage(Task task, MessageId id, Message message);
static void mediaPlayer_HandleMediaMessage(Task task, MessageId id, Message message);

static const TaskData ui_task = {mediaPlayer_HandleMessage};
static const TaskData media_task = {mediaPlayer_HandleMediaMessage};

/* Ui Inputs in which media player service is interested*/
static const message_group_t ui_inputs[] =
{
    UI_INPUTS_MEDIA_PLAYER_MESSAGE_GROUP
};


static Task mediaPlayer_UiTask(void)
{
  return (Task)&ui_task;
}

static Task mediaPlayer_MediaTask(void)
{
  return (Task)&media_task;
}

static void mediaPlayer_HandleUiInput(MessageId ui_input)
{
    audio_source_t routed_source = audio_source_none;

    if (Focus_GetAudioSourceForUiInput(ui_input, &routed_source))
    {
        switch(ui_input)
        {
            case ui_input_toggle_play_pause:
                AudioSources_PlayPause(routed_source);
                break;

            case ui_input_play:
                AudioSources_Play(routed_source);
                break;

            case ui_input_pause:
                AudioSources_Pause(routed_source);
                break;

            case ui_input_stop_av_connection:
                AudioSources_Stop(routed_source);
                break;

            case ui_input_av_forward:
                AudioSources_Forward(routed_source);
                break;

            case ui_input_av_backward:
                AudioSources_Back(routed_source);
                break;

            case ui_input_av_fast_forward_start:
                AudioSources_FastForward(routed_source, TRUE);
                break;

            case ui_input_fast_forward_stop:
                AudioSources_FastForward(routed_source, FALSE);
                break;

            case ui_input_av_rewind_start:
                AudioSources_FastRewind(routed_source, TRUE);
                break;

            case ui_input_rewind_stop:
                AudioSources_FastRewind(routed_source, FALSE);
                break;

            default:
                break;
        }
    }
}

static void mediaPlayer_ConnectAudio(audio_source_t source)
{
    audio_router_source_t audio_source;
    audio_source.type = source_type_audio;
    audio_source.u.audio = source;

    DEBUG_LOG("mediaPlayer_ConnectAudio source=%d", source);

    AudioRouter_AddSource(audio_source);
}

static void mediaPlayer_DisconnectAudio(audio_source_t source)
{
    audio_router_source_t audio_source;
    audio_source.type = source_type_audio;
    audio_source.u.audio = source;

    DEBUG_LOG("mediaPlayer_DisconnectAudio source=%d)", source);

    AudioRouter_RemoveSource(audio_source);
}


static void mediaPlayer_HandleMediaMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);
    DEBUG_LOG("mediaPlayer_HandleMediaMessage message id, id=%d", id);

    switch(id)
    {
        case AV_A2DP_AUDIO_CONNECTED:
            mediaPlayer_ConnectAudio(((AV_A2DP_AUDIO_CONNECT_MESSAGE_T *)message)->audio_source);
            break;

        case AV_A2DP_AUDIO_DISCONNECTED:
            mediaPlayer_DisconnectAudio(((AV_A2DP_AUDIO_DISCONNECT_MESSAGE_T *)message)->audio_source);
            break;

        case WIRED_AUDIO_DEVICE_CONNECT_IND:
            mediaPlayer_ConnectAudio(((WIRED_AUDIO_DEVICE_CONNECT_IND_T*)message)->audio_source);
            break;

        case WIRED_AUDIO_DEVICE_DISCONNECT_IND:
            mediaPlayer_DisconnectAudio(((WIRED_AUDIO_DEVICE_DISCONNECT_IND_T*)message)->audio_source);
            break;

        case USB_AUDIO_CONNECTED_IND:
            mediaPlayer_ConnectAudio(((USB_AUDIO_CONNECT_MESSAGE_T *)message)->audio_source);
            break;

        case USB_AUDIO_DISCONNECTED_IND:
            mediaPlayer_DisconnectAudio(((USB_AUDIO_DISCONNECT_MESSAGE_T *)message)->audio_source);
            break;

        default:
            DEBUG_LOG("mediaPlayer_HandleMediaMessage unknown message id, id=%d", id);
            break;
    }
}

static unsigned mediaPlayer_ConvertAudioSourceToMediaPlayerContext(unsigned audio_context)
{
    unsigned media_context = BAD_CONTEXT;
    switch (audio_context)
    {
    case context_audio_disconnected:
        media_context = context_media_player_no_media;
        break;
    case context_audio_connected:
        media_context = context_media_player_idle;
        break;
    case context_audio_is_streaming:
        media_context = context_media_player_streaming;
        break;
    default:
        Panic();
        break;
    }
    return media_context;
}

static unsigned mediaPlayer_GetFocusedContext(void)
{
    unsigned context = context_audio_disconnected;
    audio_source_t focused_source = audio_source_none;

    DEBUG_LOG_FN_ENTRY("mediaPlayer_GetFocusedContext");

    if (Focus_GetAudioSourceForContext(&focused_source))
    {
        context = AudioSources_GetSourceContext(focused_source);
        if (context >= BAD_CONTEXT)
        {
            context = context_audio_disconnected;
        }
    }

    DEBUG_LOG_INFO("mediaPlayer_GetFocusedContext source=%d context=%d", focused_source, context);

    return mediaPlayer_ConvertAudioSourceToMediaPlayerContext(context);
}

static void mediaPlayer_HandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);

    if (isMessageUiInput(id))
    {
        mediaPlayer_HandleUiInput(id);
    }
}


/*! \brief Initialise the media player service
*/
bool MediaPlayer_Init(Task init_task)
{
    UNUSED(init_task);

    DEBUG_LOG_FN_ENTRY("MediaPlayer_Init");

    /* Register av task call back as ui provider*/
    Ui_RegisterUiProvider(ui_provider_media_player, mediaPlayer_GetFocusedContext);

    Ui_RegisterUiInputConsumer(mediaPlayer_UiTask(), ui_inputs, ARRAY_DIM(ui_inputs));

    appAvStatusClientRegister(mediaPlayer_MediaTask());

    WiredAudioSource_ClientRegister(mediaPlayer_MediaTask());

    UsbAudio_ClientRegister(mediaPlayer_MediaTask(),
                             USB_AUDIO_REGISTERED_CLIENT_MEDIA);

    return TRUE;
}

