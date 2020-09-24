/*!
\copyright  Copyright (c) 2019-2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Interface to media control for audio sources - provides a mechanism for controlling
            media playback.
*/

#include "audio_sources.h"

#include <panic.h>

#include "ui.h"
#include "audio_sources_interface_registry.h"
#include "logging.h"


/*! \brief Interface obtaining function

    \param source Audio source type

    \return Pointer to the interface
*/
static media_control_interface_t *audioSources_GetMediaControlInterface(audio_source_t source)
{
    interface_list_t interface_list = AudioInterface_Get(source, audio_interface_type_media_control);

    DEBUG_LOG_FN_ENTRY("audioSources_GetMediaControlInterface source=%d", source);

    return (((media_control_interface_t **)interface_list.interfaces)[0]);
}


unsigned AudioSources_GetSourceContext(audio_source_t source)
{
    unsigned context = BAD_CONTEXT;
    media_control_interface_t * interface = audioSources_GetMediaControlInterface(source);

    if ((interface != NULL) && (interface->Context))
    {
        context = interface->Context(source);
    }

    DEBUG_LOG_VERBOSE("AudioSources_GetSourceContext source=%d, context=%d", source, context);
    return context;
}

device_t AudioSources_GetSourceDevice(audio_source_t source)
{
    device_t device = NULL;
    media_control_interface_t * interface = audioSources_GetMediaControlInterface(source);

    DEBUG_LOG_VERBOSE("AudioSources_GetSourceDevice source=%d", source);

    if ((interface != NULL) && (interface->Device))
    {
        device = interface->Device(source);
    }
    return device;
}

void AudioSources_RegisterMediaControlInterface(audio_source_t source, const media_control_interface_t *media_control_if)
{
    /* Retrieve the registered inteface list */
    interface_list_t interface_list = AudioInterface_Get(source, audio_interface_type_media_control);
    media_control_interface_t **media_control_interface_list = (media_control_interface_t **)interface_list.interfaces;

    DEBUG_LOG_VERBOSE("AudioSources_RegisterMediaControlInterface source=%d", source);

    /* Check if the interface is already registered */
    if(interface_list.number_of_interfaces)
    {
        if(media_control_if == media_control_interface_list[0])
            return;

        /* If the interface does not match, unregister the old interface */
        AudioInterface_UnRegister(source, audio_interface_type_media_control, media_control_interface_list[0]);
    }

    /* Register the interface*/
    AudioInterface_Register(source, audio_interface_type_media_control, media_control_if);
}

void AudioSources_Play(audio_source_t source)
{
    media_control_interface_t * interface = audioSources_GetMediaControlInterface(source);

    DEBUG_LOG_FN_ENTRY("AudioSources_Play source=%d", source);

    if ((interface != NULL) && (interface->Play))
    {
        interface->Play(source);
    }
}

void AudioSources_Pause(audio_source_t source)
{
    media_control_interface_t * interface = audioSources_GetMediaControlInterface(source);

    DEBUG_LOG_FN_ENTRY("AudioSources_Pause source=%d", source);

    if ((interface != NULL) && (interface->Pause))
    {
        interface->Pause(source);
    }
}

void AudioSources_PlayPause(audio_source_t source)
{
    media_control_interface_t * interface = audioSources_GetMediaControlInterface(source);

    DEBUG_LOG_FN_ENTRY("AudioSources_PlayPause source=%d", source);

    if ((interface != NULL) && (interface->PlayPause))
    {
        interface->PlayPause(source);
    }
}

void AudioSources_Stop(audio_source_t source)
{
    media_control_interface_t * interface = audioSources_GetMediaControlInterface(source);

    DEBUG_LOG_FN_ENTRY("AudioSources_Stop source=%d", source);

    if ((interface != NULL) && (interface->Stop))
    {
        interface->Stop(source);
    }
}

void AudioSources_Forward(audio_source_t source)
{
    media_control_interface_t * interface = audioSources_GetMediaControlInterface(source);

    DEBUG_LOG_FN_ENTRY("AudioSources_Forward source=%d", source);

    if ((interface != NULL) && (interface->Forward))
    {
        interface->Forward(source);
    }
}

void AudioSources_Back(audio_source_t source)
{
    media_control_interface_t * interface = audioSources_GetMediaControlInterface(source);

    DEBUG_LOG_FN_ENTRY("AudioSources_Back source=%d", source);

    if ((interface != NULL) && (interface->Back))
    {
        interface->Back(source);
    }
}

void AudioSources_FastForward(audio_source_t source, bool state)
{
    media_control_interface_t * interface = audioSources_GetMediaControlInterface(source);

    DEBUG_LOG_FN_ENTRY("AudioSources_FastForward source=%d, state=%d", source, state);

    if ((interface != NULL) && (interface->FastForward))
    {
        interface->FastForward(source, state);
    }
}

void AudioSources_FastRewind(audio_source_t source, bool state)
{
    media_control_interface_t * interface = audioSources_GetMediaControlInterface(source);

    DEBUG_LOG_FN_ENTRY("AudioSources_FastRewind source=%d, state=%d", source, state);

    if ((interface != NULL) && (interface->FastRewind))
    {
        interface->FastRewind(source, state);
    }
}

void AudioSources_NextGroup(audio_source_t source)
{
    media_control_interface_t * interface = audioSources_GetMediaControlInterface(source);

    DEBUG_LOG_FN_ENTRY("AudioSources_NextGroup source=%d", source);

    if ((interface != NULL) && (interface->NextGroup))
    {
        interface->NextGroup(source);
    }
}

void AudioSources_PreviousGroup(audio_source_t source)
{
    media_control_interface_t * interface = audioSources_GetMediaControlInterface(source);

    DEBUG_LOG_FN_ENTRY("AudioSources_PreviousGroup source=%d", source);

    if ((interface != NULL) && (interface->PreviousGroup))
    {
        interface->PreviousGroup(source);
    }
}

void AudioSources_Shuffle(audio_source_t source, shuffle_state_t state)
{
    media_control_interface_t * interface = audioSources_GetMediaControlInterface(source);

    DEBUG_LOG_FN_ENTRY("AudioSources_Shuffle source=%d, state=%d", source, state);

    if ((interface != NULL) && (interface->Shuffle))
    {
        interface->Shuffle(source, state);
    }
}

void AudioSources_Repeat(audio_source_t source, repeat_state_t state)
{
    media_control_interface_t * interface = audioSources_GetMediaControlInterface(source);

    DEBUG_LOG_FN_ENTRY("AudioSources_Repeat source=%d, state=%d", source, state);

    if ((interface != NULL) && (interface->Repeat))
    {
        interface->Repeat(source, state);
    }
}
