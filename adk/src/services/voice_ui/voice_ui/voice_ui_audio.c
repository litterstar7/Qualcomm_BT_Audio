/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of the voice ui audio related interface.
*/

#include <mirror_profile_protected.h>
#include "voice_ui_audio.h"
#include "voice_ui_va_client_if.h"
#include "voice_audio_manager.h"
#include "voice_ui_container.h"
#include "hfp_profile.h"
#include <panic.h>
#include <logging.h>

static struct
{
    unsigned hfp_routed:1;
    unsigned capture_active:1;
    unsigned detection_active:1;
    unsigned detection_suspended:1;
} state = {FALSE};

static va_audio_wuw_detection_params_t detection_config;

static unsigned voiceUi_CaptureDataReceived(Source source)
{
    voice_ui_handle_t *va_handle = VoiceUi_GetActiveVa();
    PanicFalse(va_handle->voice_assistant->audio_if.CaptureDataReceived != NULL);
    return va_handle->voice_assistant->audio_if.CaptureDataReceived(source);
}

static va_audio_wuw_detected_response_t voiceUi_WakeUpWordDetected(const va_audio_wuw_detection_info_t *wuw_info)
{
    va_audio_wuw_detected_response_t response = {0};
    voice_ui_handle_t *va_handle = VoiceUi_GetActiveVa();
    UNUSED(MirrorProfile_PeerLinkPolicyLowLatencyKick());
    PanicFalse(va_handle->voice_assistant->audio_if.WakeUpWordDetected != NULL);
    response.start_capture = va_handle->voice_assistant->audio_if.WakeUpWordDetected(&response.capture_params, wuw_info);
    response.capture_callback = voiceUi_CaptureDataReceived;

    if (response.start_capture)
        state.capture_active = TRUE;

    return response;
}

bool VoiceUi_IsActiveAssistant(voice_ui_handle_t *va_handle)
{
    return (va_handle != NULL) && (va_handle == VoiceUi_GetActiveVa());
}

static bool voiceUi_StartCapture(const va_audio_voice_capture_params_t *capture_config)
{
    bool status = VoiceAudioManager_StartCapture(voiceUi_CaptureDataReceived, capture_config);

    if (status)
        state.capture_active = TRUE;

    return status;
}

static void voiceUi_StopCapture(void)
{
    VoiceAudioManager_StopCapture();
    state.capture_active = FALSE;
}

static bool voiceUi_StartDetection(const va_audio_wuw_detection_params_t *wuw_config)
{
    bool status = VoiceAudioManager_StartDetection(voiceUi_WakeUpWordDetected, wuw_config);

    if (status)
    {
        state.detection_active = TRUE;
        state.detection_suspended = FALSE;
    }

    return status;
}

static void voiceUi_StopDetection(void)
{
    VoiceAudioManager_StopDetection();
    state.detection_active = FALSE;
    state.detection_suspended = FALSE;
}

static bool voiceUi_IsHfpRouted(void)
{
    return state.hfp_routed;
}

static bool voiceUi_IsCaptureActive(void)
{
    return state.capture_active;
}

static bool voiceUi_IsDetectionActive(void)
{
    return state.detection_active;
}

static bool voiceUi_IsDetectionSuspended(void)
{
    return state.detection_suspended;
}

static void voiceUi_NotifyCaptureSuspended(void)
{
    voice_ui_handle_t *va_handle = VoiceUi_GetActiveVa();
    if (va_handle->voice_assistant->audio_if.CaptureSuspended)
        va_handle->voice_assistant->audio_if.CaptureSuspended();
}

void VoiceUi_SuspendAudio(void)
{
    if (voiceUi_IsCaptureActive())
    {
        voiceUi_StopCapture();
        voiceUi_NotifyCaptureSuspended();
    }

    if (voiceUi_IsDetectionActive())
    {
        voiceUi_StopDetection();
        state.detection_suspended = TRUE;
    }

    state.hfp_routed = TRUE;
}

void VoiceUi_ResumeAudio(void)
{
    if (voiceUi_IsDetectionSuspended())
        voiceUi_StartDetection(&detection_config);

    state.hfp_routed = FALSE;
}

void VoiceUi_UnrouteAudio(void)
{
    if (voiceUi_IsCaptureActive())
        voiceUi_NotifyCaptureSuspended();

    voiceUi_StopCapture();
    voiceUi_StopDetection();
}

voice_ui_audio_status_t VoiceUi_StartAudioCapture(voice_ui_handle_t *va_handle, const va_audio_voice_capture_params_t *audio_config)
{
    if (VoiceUi_IsActiveAssistant(va_handle) == FALSE)
        return voice_ui_audio_not_active;

    if (voiceUi_IsHfpRouted())
        return voice_ui_audio_suspended;

    if (voiceUi_StartCapture(audio_config))
        return voice_ui_audio_success;
    else
        return voice_ui_audio_failed;
}

void VoiceUi_StopAudioCapture(voice_ui_handle_t *va_handle)
{
    if (VoiceUi_IsActiveAssistant(va_handle))
        voiceUi_StopCapture();
}

voice_ui_audio_status_t VoiceUi_StartWakeUpWordDetection(voice_ui_handle_t *va_handle, const va_audio_wuw_detection_params_t *audio_config)
{
    if (VoiceUi_IsActiveAssistant(va_handle) == FALSE)
        return voice_ui_audio_not_active;

    if (voiceUi_IsHfpRouted())
    {
        detection_config = *audio_config;
        state.detection_suspended = TRUE;
        return voice_ui_audio_success;
    }

    if (voiceUi_IsDetectionActive())
    {
        if (VA_AUDIO_DETECTION_PARAMS_EQUAL(&detection_config, audio_config))
        {
            return voice_ui_audio_already_started;
        }
        else
        {
            DEBUG_LOG("VoiceUi_StartWakeUpWordDetection: stopping to apply new paramenters");
            voiceUi_StopDetection();
        }
    }

    DEBUG_LOG("VoiceUi_StartWakeUpWordDetection: starting");
    if (voiceUi_StartDetection(audio_config))
    {
        detection_config = *audio_config;
        return voice_ui_audio_success;
    }
    else
        return voice_ui_audio_failed;
}

void VoiceUi_StopWakeUpWordDetection(voice_ui_handle_t *va_handle)
{
    if (VoiceUi_IsActiveAssistant(va_handle))
        voiceUi_StopDetection();
}

bool VoiceUi_IsHfpIsActive(void)
{
    hfp_call_state hfp_state;
    HfpLinkGetCallState(hfp_primary_link, &hfp_state) ;
    bool mic_in_use = hfp_state != hfp_call_state_idle;
    DEBUG_LOG("VoiceUi_IsHfpIsActive hfp_active %d", mic_in_use);

    if (!mic_in_use)
    {
        /* If HFP itself is not active, with iPhone, Siri might have SCO active to use the mic. */
        mic_in_use = appHfpIsScoActive();
        DEBUG_LOG("VoiceUi_IsHfpIsActive sco_active %d", mic_in_use);
    }

    if (!mic_in_use)
    {
        mic_in_use = VoiceSources_IsVoiceRouted();
        DEBUG_LOG("VoiceUi_IsHfpIsActive voice_routed %d", mic_in_use);
    }

    return mic_in_use;
}

void VoiceUi_UpdateHfpState(void)
{
    state.hfp_routed = VoiceUi_IsHfpIsActive();
}

#ifdef HOSTED_TEST_ENVIRONMENT
unsigned VoiceUi_CaptureDataReceived(Source source)
{
    return voiceUi_CaptureDataReceived(source);
}
void VoiceUi_TestResetAudio(void)
{
    memset(&state, FALSE, sizeof(state));
    memset(&detection_config, 0, sizeof(detection_config));
}
va_audio_wuw_detected_response_t VoiceUi_WakeUpWordDetected(const va_audio_wuw_detection_info_t *wuw_info)
{
    return voiceUi_WakeUpWordDetected(wuw_info);
}
#endif
