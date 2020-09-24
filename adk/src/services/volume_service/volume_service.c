/*!
\copyright  Copyright (c) 2018-2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of the Volume Service.
*/

#include "volume_service.h"

#include "audio_sources.h"
#include "kymera_adaptation.h"
#include "volume_system.h"
#include "voice_sources.h"
#include "volume_messages.h"
#include "volume_mute.h"
#include "volume_utils.h"

#include <av.h>
#include <focus_audio_source.h>
#include <panic.h>
#include <task_list.h>
#include <message.h>
#include <message_broker.h>
#include <logging.h>
#include <stdio.h>
#include <ui.h>

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_ENUM(volume_service_messages)

#define VOLUME_SERVICE_CLIENT_TASK_LIST_INIT_CAPACITY 1

/*! Macro for creating messages */
#define MAKE_VOLUME_SERVICE_MESSAGE(TYPE) \
    TYPE##_T *message = PanicUnlessNew(TYPE##_T);

/*! \brief Internal message IDs */
enum
{
    INTERNAL_MSG_APPLY_AUDIO_VOLUME,
    INTERNAL_MSG_VOLUME_UP_REPEAT,
    INTERNAL_MSG_VOLUME_DOWN_REPEAT,
};

/*! Internal message for a volume repeat */
typedef struct
{
    audio_source_t source;      /*!< Audio Source volume ramp is applied to */
    int16 step;                 /*!< Step to adjust volume by +ve or -ve */
} INTERNAL_MSG_VOLUME_REPEAT_T;

/*! Number of volume steps to use per Audio UI volume change event. The full volume range is 0-127 */
#define volumeService_GetAudioVolumeStep()  (8)

/*! \brief Time between volume changes (in milliseconds) */
#define VOLUME_SERVICE_CONFIG_VOLUME_RAMP_REPEAT_TIME               (300)

/* Ui Inputs in which volume service is interested*/
static const message_group_t ui_inputs[] =
{
    UI_INPUTS_VOLUME_MESSAGE_GROUP
};

typedef struct
{
    TASK_LIST_WITH_INITIAL_CAPACITY(VOLUME_SERVICE_CLIENT_TASK_LIST_INIT_CAPACITY)  client_list;
    TaskData volume_message_handler_task;
} volume_service_data;

static volume_service_data the_volume_service;

#define VolumeServiceGetClientLIst() (task_list_flexible_t *)(&the_volume_service.client_list)

static void volumeService_InternalMessageHandler( Task task, MessageId id, Message message );
static void volumeService_RefreshAudioVolume(event_origin_t origin);

static TaskData internal_message_task = { volumeService_InternalMessageHandler };

static int16 volumeService_CalculateAudioVolumeStep(ui_input_t ui_input)
{
    uint16 num_pending_msgs = 0;
    uint16 num_steps = 1;
    int16 av_change = 0;

    if(ui_input == ui_input_av_volume_up_start)
    {
        num_pending_msgs = MessageCancelAll(&internal_message_task, ui_input_av_volume_up_start);
        num_steps += num_pending_msgs;
        av_change = volumeService_GetAudioVolumeStep() * num_steps;
    }
    else if(ui_input == ui_input_av_volume_down_start)
    {
        num_pending_msgs = MessageCancelAll(&internal_message_task, ui_input_av_volume_down_start);
        num_steps += num_pending_msgs;
        av_change = -(volumeService_GetAudioVolumeStep() * num_steps);
    }
    return av_change;
}

/*! \brief Start a volume ramp change

    \param step Relative amount to adjust the volume by. This can be negative.
*/
static void volumeService_StartAudioVolumeRamp(audio_source_t source, int16 step)
{
    DEBUG_LOG_INFO("volumeService_StartAudioVolumeRamp source=%d, step=%d", source, step);

    VolumeService_ChangeAudioSourceVolume(source, step);

    MAKE_VOLUME_SERVICE_MESSAGE(INTERNAL_MSG_VOLUME_REPEAT);
    message->step = step;
    message->source = source;
    MessageSendLater(&internal_message_task,
                     step > 0 ? INTERNAL_MSG_VOLUME_UP_REPEAT : INTERNAL_MSG_VOLUME_DOWN_REPEAT,
                     message, VOLUME_SERVICE_CONFIG_VOLUME_RAMP_REPEAT_TIME);
}

static void volumeService_StopAudioVolumeRamp(void)
{
    MessageCancelFirst(&internal_message_task, INTERNAL_MSG_VOLUME_UP_REPEAT);
    MessageCancelFirst(&internal_message_task, INTERNAL_MSG_VOLUME_DOWN_REPEAT);
}

/*! \brief handles av module specific ui inputs

    Invokes routines based on ui input received from ui module.

    \param[in] id - ui input

    \returns void
 */
static void volumeService_HandleUiInput(MessageId ui_input)
{
    audio_source_t source = audio_source_none;

    DEBUG_LOG_FN_ENTRY("volumeService_HandleUiInput");

    if (Focus_GetAudioSourceForUiInput(ui_input, &source))
    {
        switch (ui_input)
        {
            case ui_input_av_volume_up_start:
            case ui_input_av_volume_down_start:
                volumeService_StartAudioVolumeRamp(source, volumeService_CalculateAudioVolumeStep(ui_input));
                break;
            case ui_input_av_volume_up_stop:
            case ui_input_av_volume_down_stop:
                volumeService_StopAudioVolumeRamp();
                break;
            case ui_input_av_volume_up:
                VolumeService_ChangeAudioSourceVolume(source, volumeService_GetAudioVolumeStep());
                break;
            case ui_input_av_volume_down:
                VolumeService_ChangeAudioSourceVolume(source, -volumeService_GetAudioVolumeStep());
                break;
            default:
                break;
        }
    }
}

void VolumeService_ChangeAudioSourceVolume(audio_source_t source, int16 step)
{
    int step_size = VolumeUtils_GetStepSize(AudioSources_GetVolume(source).config);

    DEBUG_LOG_FN_ENTRY("VolumeService_ChangeAudioSourceVolume");

    if(step == step_size)
    {
        Volume_SendAudioSourceVolumeIncrementRequest(source, event_origin_local);
    }
    else if(step == -step_size)
    {
        Volume_SendAudioSourceVolumeDecrementRequest(source, event_origin_local);
    }
    else
    {
        int new_volume = VolumeUtils_LimitVolumeToRange((AudioSources_GetVolume(source).value + step),
                                                         AudioSources_GetVolume(source).config.range);
        Volume_SendAudioSourceVolumeUpdateRequest(source, event_origin_local, new_volume);
    }
}

static void volumeService_DoVolumeRampRepeat(MessageId id, INTERNAL_MSG_VOLUME_REPEAT_T * msg)
{
    volume_t new_volume = AudioSources_GetVolume(msg->source);
    new_volume.value += msg->step;
    VolumeService_SetAudioSourceVolume(msg->source, event_origin_local, new_volume);

    MAKE_VOLUME_SERVICE_MESSAGE(INTERNAL_MSG_VOLUME_REPEAT);
    memcpy(message, msg, sizeof(INTERNAL_MSG_VOLUME_REPEAT_T));
    MessageSendLater(&internal_message_task, id, message, VOLUME_SERVICE_CONFIG_VOLUME_RAMP_REPEAT_TIME);
}

static void volumeService_InternalMessageHandler( Task task, MessageId id, Message msg )
{
    UNUSED(task);

    if (isMessageUiInput(id))
    {
        volumeService_HandleUiInput(id);
    }
    else
    {
        switch(id)
        {
        case INTERNAL_MSG_APPLY_AUDIO_VOLUME:
            volumeService_RefreshAudioVolume(event_origin_local);
            break;

        case INTERNAL_MSG_VOLUME_UP_REPEAT:
        case INTERNAL_MSG_VOLUME_DOWN_REPEAT:
            volumeService_DoVolumeRampRepeat(id, (INTERNAL_MSG_VOLUME_REPEAT_T *) msg);
            break;

        default:
            Panic();
            break;
        }
    }
}

static void volumeService_NotifyMinOrMaxVolume(volume_t volume)
{
    if(volume.value >= volume.config.range.max)
    {
        TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(VolumeServiceGetClientLIst()), VOLUME_SERVICE_MAX_VOLUME);
    }
    if(volume.value <= volume.config.range.min)
    {
        TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(VolumeServiceGetClientLIst()), VOLUME_SERVICE_MIN_VOLUME);
    }
}

static void volumeService_RefreshVoiceVolume(void)
{
    volume_t volume = Volume_CalculateOutputVolume(VoiceSources_GetVolume(VoiceSources_GetRoutedSource()));
    volume_parameters_t volume_params = { .source_type = source_type_voice, .volume = volume };
    KymeraAdaptation_SetVolume(&volume_params);
}

static bool isVolumeToBeSynchronised(void)
{
    /* Doesn't exist yet */
    return FALSE;
}

static uint16 getSynchronisedVolumeDelay(void)
{
    return 0;
}

static void volumeService_RefreshAudioVolume(event_origin_t origin)
{
    if ((origin == event_origin_local) && isVolumeToBeSynchronised())
    {
        MessageSendLater(&internal_message_task, INTERNAL_MSG_APPLY_AUDIO_VOLUME, 0, getSynchronisedVolumeDelay());
    }
    else
    {
        volume_t volume = Volume_CalculateOutputVolume(AudioSources_GetVolume(AudioSources_GetRoutedSource()));
        volume_parameters_t volume_params = { .source_type = source_type_audio, .volume = volume };
        KymeraAdaptation_SetVolume(&volume_params);
    }
}

static void volumeService_RefreshCurrentVolume(event_origin_t origin)
{
    if(VoiceSources_GetRoutedSource() != voice_source_none)
    {
        volumeService_RefreshVoiceVolume();
    }
    else
    {
        if(AudioSources_GetRoutedSource() != audio_source_none)
        {
            volumeService_RefreshAudioVolume(origin);
        }
    }
}

static void volumeService_UpdateAudioSourceVolume(audio_source_t source, volume_t new_volume, event_origin_t origin)
{
    AudioSources_SetVolume(source, new_volume);
    AudioSources_OnVolumeChange(source, origin, new_volume);
    if(source == AudioSources_GetRoutedSource())
    {
        volumeService_RefreshAudioVolume(origin);
    }
}

static void volumeService_UpdateSystemVolume(volume_t new_volume, event_origin_t origin)
{
    Volume_SetSystemVolume(new_volume);
    volumeService_RefreshCurrentVolume(origin);
}

static void volumeService_UpdateVoiceSourceLocalVolume(voice_source_t source, volume_t new_volume, event_origin_t origin)
{
    VoiceSources_SetVolume(source, new_volume);
    VoiceSources_OnVolumeChange(source, origin, new_volume);
    if(source == VoiceSources_GetRoutedSource())
    {
        volumeService_RefreshVoiceVolume();
    }
}

void VolumeService_SetAudioSourceVolume(audio_source_t source, event_origin_t origin, volume_t new_volume)
{
    volume_t source_volume = AudioSources_GetVolume(source);
    DEBUG_LOG("VolumeService_SetAudioSourceVolume, source %u, origin %u, volume %u", source, origin, new_volume.value);
    source_volume.value = VolumeUtils_ConvertToVolumeConfig(new_volume, source_volume.config);

    if(AudioSources_IsVolumeControlRegistered(source) && (origin == event_origin_local))
    {
        AudioSources_VolumeSetAbsolute(source, source_volume);
    }
    else
    {
        volumeService_UpdateAudioSourceVolume(source, source_volume, origin);
    }
    volumeService_NotifyMinOrMaxVolume(source_volume);
}

void VolumeService_IncrementAudioSourceVolume(audio_source_t source, event_origin_t origin)
{
    DEBUG_LOG("VolumeService_IncrementAudioSourceVolume, source %u, origin %u", source, origin);
    if (AudioSources_IsVolumeControlRegistered(source) && (origin == event_origin_local))
    {
        AudioSources_VolumeUp(source);
    }
    else
    {
        volume_t source_volume = AudioSources_GetVolume(source);
        source_volume.value = VolumeUtils_IncrementVolume(source_volume);
        volumeService_UpdateAudioSourceVolume(source, source_volume, origin);
        volumeService_NotifyMinOrMaxVolume(source_volume);
    }
}

void VolumeService_DecrementAudioSourceVolume(audio_source_t source, event_origin_t origin)
{
    DEBUG_LOG("VolumeService_DecrementAudioSourceVolume, source %u, origin %u", source, origin);
    if (AudioSources_IsVolumeControlRegistered(source) && (origin == event_origin_local))
    {
        AudioSources_VolumeDown(source);
    }
    else
    {
        volume_t source_volume = AudioSources_GetVolume(source);
        source_volume.value = VolumeUtils_DecrementVolume(source_volume);
        volumeService_UpdateAudioSourceVolume(source, source_volume, origin);
        volumeService_NotifyMinOrMaxVolume(source_volume);
    }
}

void VolumeService_SetSystemVolume(event_origin_t origin, volume_t new_volume)
{
    volume_t system_volume = Volume_GetSystemVolume();
    system_volume.value = VolumeUtils_ConvertToVolumeConfig(new_volume, system_volume.config);
    volumeService_UpdateSystemVolume(system_volume, origin);
}

void VolumeService_IncrementSystemVolume(event_origin_t origin)
{
    volume_t system_volume = Volume_GetSystemVolume();
    system_volume.value = VolumeUtils_IncrementVolume(system_volume);
    volumeService_UpdateSystemVolume(system_volume, origin);
}

void VolumeService_DecrementSystemVolume(event_origin_t origin)
{
    volume_t system_volume = Volume_GetSystemVolume();
    system_volume.value = VolumeUtils_DecrementVolume(system_volume);
    volumeService_UpdateSystemVolume(system_volume, origin);
}

void VolumeService_SetVoiceSourceVolume(voice_source_t source, event_origin_t origin, volume_t new_volume)
{
    volume_t source_volume = VoiceSources_GetVolume(source);
    DEBUG_LOG("VolumeService_SetVoiceSourceVolume, source %u, origin %u, volume %u", source, origin, new_volume.value);
    source_volume.value = VolumeUtils_ConvertToVolumeConfig(new_volume, source_volume.config);

    if(VoiceSources_IsVolumeControlRegistered(source) && (origin == event_origin_local))
    {
        VoiceSources_VolumeSetAbsolute(source, source_volume);
    }
    else
    {
        volumeService_UpdateVoiceSourceLocalVolume(source, source_volume, origin);
    }
    volumeService_NotifyMinOrMaxVolume(source_volume);
}

void VolumeService_IncrementVoiceSourceVolume(voice_source_t source, event_origin_t origin)
{
    DEBUG_LOG("VolumeService_IncrementVoiceSourceVolume, source %u, origin %u", source, origin);
    if (VoiceSources_IsVolumeControlRegistered(source) && (origin == event_origin_local))
    {
        VoiceSources_VolumeUp(source);
    }
    else
    {
        volume_t source_volume = VoiceSources_GetVolume(source);
        source_volume.value = VolumeUtils_IncrementVolume(source_volume);
        volumeService_UpdateVoiceSourceLocalVolume(source, source_volume, origin);
        volumeService_NotifyMinOrMaxVolume(source_volume);
    }
}

void VolumeService_DecrementVoiceSourceVolume(voice_source_t source, event_origin_t origin)
{
    DEBUG_LOG("VolumeService_DecrementVoiceSourceVolume, source %u, origin %u", source, origin);
    if (VoiceSources_IsVolumeControlRegistered(source) && (origin == event_origin_local))
    {
        VoiceSources_VolumeDown(source);
    }
    else
    {
        volume_t source_volume = VoiceSources_GetVolume(source);
        source_volume.value = VolumeUtils_DecrementVolume(source_volume);
        volumeService_UpdateVoiceSourceLocalVolume(source, source_volume, origin);
        volumeService_NotifyMinOrMaxVolume(source_volume);
    }
}

void VolumeService_Mute(void)
{
    Volume_SetMuteState(TRUE);
    volumeService_RefreshCurrentVolume(event_origin_local);
}

void VolumeService_Unmute(void)
{
    Volume_SetMuteState(FALSE);
    volumeService_RefreshCurrentVolume(event_origin_local);
}

static void volumeMessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch(id)
    {
        case VOICE_SOURCE_VOLUME_UPDATE_REQUEST:
            {
                voice_source_volume_update_request_message_t *msg = (voice_source_volume_update_request_message_t *)message;
                VolumeService_SetVoiceSourceVolume(msg->voice_source, msg->origin, msg->volume);
            }
            break;
        case VOICE_SOURCE_VOLUME_INCREMENT_REQUEST:
            {
                voice_source_volume_increment_request_message_t *msg = (voice_source_volume_increment_request_message_t *)message;
                VolumeService_IncrementVoiceSourceVolume(msg->voice_source, msg->origin);
            }
            break;
        case VOICE_SOURCE_VOLUME_DECREMENT_REQUEST:
            {
                voice_source_volume_decrement_request_message_t *msg = (voice_source_volume_decrement_request_message_t *)message;
                VolumeService_DecrementVoiceSourceVolume(msg->voice_source, msg->origin);
            }
            break;
        case AUDIO_SOURCE_VOLUME_UPDATE_REQUEST:
            {
                audio_source_volume_update_request_message_t *msg = (audio_source_volume_update_request_message_t *)message;
                VolumeService_SetAudioSourceVolume(msg->audio_source, msg->origin, msg->volume);
            }
            break;
        case AUDIO_SOURCE_VOLUME_INCREMENT_REQUEST:
            {
                audio_source_volume_increment_request_message_t *msg = (audio_source_volume_increment_request_message_t *)message;
                VolumeService_IncrementAudioSourceVolume(msg->audio_source, msg->origin);
            }
            break;
        case AUDIO_SOURCE_VOLUME_DECREMENT_REQUEST:
            {
                audio_source_volume_decrement_request_message_t *msg = (audio_source_volume_decrement_request_message_t *)message;
                VolumeService_DecrementAudioSourceVolume(msg->audio_source, msg->origin);
            }
            break;
        case MUTE_VOLUME_REQUEST:
            {
                mute_volume_request_message_t *msg = (mute_volume_request_message_t *)message;
                if(msg->mute_state)
                {
                    VolumeService_Mute();
                }
                else
                {
                    VolumeService_Unmute();
                }
            }
            break;
        default:
            break;
    }
}

bool VolumeService_Init(Task init_task)
{
    UNUSED(init_task);
    TaskList_InitialiseWithCapacity(VolumeServiceGetClientLIst(), VOLUME_SERVICE_CLIENT_TASK_LIST_INIT_CAPACITY);

    the_volume_service.volume_message_handler_task.handler = volumeMessageHandler;
    Volume_RegisterForMessages(&the_volume_service.volume_message_handler_task);

    Ui_RegisterUiInputConsumer(&internal_message_task, ui_inputs, ARRAY_DIM(ui_inputs));

    return TRUE;
}

static void volumeService_RegisterMessageGroup(Task task, message_group_t group)
{
    PanicFalse(group == VOLUME_SERVICE_MESSAGE_GROUP);
    TaskList_AddTask(TaskList_GetFlexibleBaseTaskList(VolumeServiceGetClientLIst()), task);
}

MESSAGE_BROKER_GROUP_REGISTRATION_MAKE(VOLUME_SERVICE, volumeService_RegisterMessageGroup, NULL);
