/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of audio router functions.
*/

#include "audio_router.h"
#include "logging.h"
#include "audio_sources.h"
#include "voice_sources.h"
#include "kymera_adaptation.h"
#include "panic.h"

static audio_router_t* router_instance_handlers = NULL;

static void audioRouter_PanicIfSourceInvalid(audio_router_source_t source)
{
    switch(source.type)
    {
        case source_type_voice:
            PanicFalse(source.u.voice < max_voice_sources);
            break;
        case source_type_audio:
            PanicFalse(source.u.audio < max_audio_sources);
            break;
        default:
            Panic();
            break;
    }
}

void AudioRouter_ConfigureHandlers(const audio_router_t* handlers)
{
    DEBUG_LOG_FN_ENTRY("AudioRouter_ConfigureHandlers");

    PanicNull((void*)handlers);

    router_instance_handlers = (audio_router_t*)handlers;
}
 
void AudioRouter_ConfigurePriorities(audio_source_type_t* priorities, unsigned num_priorities)
{
    DEBUG_LOG_FN_ENTRY("AudioRouter_ConfigurePriorities");

    PanicNull(router_instance_handlers);

    router_instance_handlers->configure_priorities(priorities, num_priorities);
}
 
void AudioRouter_AddSource(audio_router_source_t source)
{
    DEBUG_LOG_FN_ENTRY("AudioRouter_AddSource");

    audioRouter_PanicIfSourceInvalid(source);

    PanicNull(router_instance_handlers);

    router_instance_handlers->add_source(source);
}
 
void AudioRouter_RemoveSource(audio_router_source_t source)
{
    DEBUG_LOG_FN_ENTRY("AudioRouter_RemoveSource");

    audioRouter_PanicIfSourceInvalid(source);

    PanicNull(router_instance_handlers);

    router_instance_handlers->remove_source(source);
}

void AudioRouter_RefreshRouting(void)
{
    DEBUG_LOG_FN_ENTRY("AudioRouter_RefreshRouting");

    PanicNull(router_instance_handlers);

    router_instance_handlers->refresh_routing();
}

static bool audioRouter_CommonConnectAudioSource(audio_router_source_t source)
{
    bool success = FALSE;
    connect_parameters_t connect_parameters;
    DEBUG_LOG_FN_ENTRY("audioRouter_CommonConnectAudioSource");

    if(AudioSources_GetConnectParameters(source.u.audio, 
                                            &connect_parameters.source_params))
    {
        connect_parameters.source.type = source_type_audio;
        connect_parameters.source.sourceu.audio = source.u.audio;

        AudioSources_SetState(source.u.audio, audio_source_connecting);
        KymeraAdaptation_Connect(&connect_parameters);
        AudioSources_ReleaseConnectParameters(source.u.audio,
                                                &connect_parameters.source_params);
        AudioSources_SetState(source.u.audio, audio_source_connected);
        AudioSources_OnAudioRouted(source.u.audio);
        success = TRUE;
    }
    return success;
}

static bool audioRouter_CommonConnectVoiceSource(audio_router_source_t source)
{
    bool success = FALSE;
    connect_parameters_t connect_parameters;

    DEBUG_LOG_FN_ENTRY("audioRouter_CommonConnectVoiceSource");

    if(VoiceSources_GetConnectParameters(source.u.voice,
                                            &connect_parameters.source_params))
    {
        connect_parameters.source.type = source_type_voice;
        connect_parameters.source.sourceu.voice = source.u.voice;

        KymeraAdaptation_Connect(&connect_parameters);
        VoiceSources_ReleaseConnectParameters(source.u.voice,
                                                &connect_parameters.source_params);
        success = TRUE;
    }
    return success;
}

bool AudioRouter_CommonConnectSource(audio_router_source_t source)
{
    bool success = FALSE;

    DEBUG_LOG_FN_ENTRY("AudioRouter_CommonConnectSource type=%d, source=%d", source.type, (unsigned)source.u.audio);

    audioRouter_PanicIfSourceInvalid(source);

    switch(source.type)
    {
        case source_type_voice:
            success = audioRouter_CommonConnectVoiceSource(source);
            break;

        case source_type_audio:
            success = audioRouter_CommonConnectAudioSource(source);
            break;

        default:
            break;
    }

    return success;
}

static bool audioRouter_CommonDisconnectAudioSource(audio_router_source_t source)
{
    bool success = FALSE;
    disconnect_parameters_t disconnect_parameters;

    DEBUG_LOG_FN_ENTRY("audioRouter_CommonDisconnectAudioSource");

    if(AudioSources_GetDisconnectParameters(source.u.audio, 
                                            &disconnect_parameters.source_params))
    {
        disconnect_parameters.source.type = source_type_audio;
        disconnect_parameters.source.sourceu.audio = source.u.audio;

        AudioSources_SetState(source.u.audio, audio_source_disconnecting);
        KymeraAdaptation_Disconnect(&disconnect_parameters);
        AudioSources_ReleaseDisconnectParameters(source.u.audio,
                                                    &disconnect_parameters.source_params);
        AudioSources_SetState(source.u.audio, audio_source_disconnected);
        success = TRUE;
    }
    return success;
}

static bool audioRouter_CommonDisconnectVoiceSource(audio_router_source_t source)
{
    bool success = FALSE;
    disconnect_parameters_t disconnect_parameters;

    DEBUG_LOG_FN_ENTRY("audioRouter_CommonDisconnectVoiceSource");

    if(VoiceSources_GetDisconnectParameters(source.u.voice, 
                                            &disconnect_parameters.source_params))
    {
        disconnect_parameters.source.type = source_type_voice;
        disconnect_parameters.source.sourceu.voice = source.u.voice;

        KymeraAdaptation_Disconnect(&disconnect_parameters);
        VoiceSources_ReleaseDisconnectParameters(source.u.voice,
                                                    &disconnect_parameters.source_params);

        success = TRUE;
    }
    return success;
}

bool AudioRouter_CommonDisconnectSource(audio_router_source_t source)
{
    bool success = FALSE;

    DEBUG_LOG_FN_ENTRY("AudioRouter_CommonDisconnectSource type=%d, source=%d", source.type, (unsigned)source.u.audio);

    audioRouter_PanicIfSourceInvalid(source);

    switch(source.type)
    {
        case source_type_voice:
            success = audioRouter_CommonDisconnectVoiceSource(source);
            break;

        case source_type_audio:
            success =   audioRouter_CommonDisconnectAudioSource(source);
            break;

        default:
            break;
    }
    return success;
}
