/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   focus_domains Focus
\ingroup    domains
\brief      Module implementing the interface by which the application framework can
            call a concrete focus select implementation, either focus select or a
            customer module.
*/

#include "focus_audio_source.h"
#include "focus_device.h"
#include "focus_voice_source.h"

static focus_device_t select_focused_device_fns = {0};
static focus_get_audio_source_t select_focused_audio_source_fns = {0};
static focus_get_voice_source_t select_focused_voice_source_fns = {0};

void Focus_ConfigureDevice(focus_device_t focus_device)
{
    select_focused_device_fns = focus_device;
}

bool Focus_GetDeviceForContext(ui_providers_t provider, device_t* device)
{
    if (select_focused_device_fns.for_context)
    {
        return select_focused_device_fns.for_context(provider, device);
    }
    return FALSE;
}

bool Focus_GetDeviceForUiInput(ui_input_t ui_input, device_t* device)
{
    if (select_focused_device_fns.for_ui_input)
    {
        return select_focused_device_fns.for_ui_input(ui_input, device);
    }
    return FALSE;
}

focus_t Focus_GetFocusForDevice(const device_t device)
{
    focus_t device_focus = focus_none;
    if (select_focused_device_fns.focus)
    {
        device_focus = select_focused_device_fns.focus(device);
    }
    return device_focus;
}

bool Focus_AddDeviceToBlacklist(device_t device)
{
    if (select_focused_device_fns.add_to_blacklist)
    {
        return select_focused_device_fns.add_to_blacklist(device);
    }

    return FALSE;
}

bool Focus_RemoveDeviceFromBlacklist(device_t device)
{
    if (select_focused_device_fns.remove_from_blacklist)
    {
        return select_focused_device_fns.remove_from_blacklist(device);
    }

    return FALSE;
}

void Focus_ResetDeviceBlacklist(void)
{
    if (select_focused_device_fns.reset_blacklist)
    {
        select_focused_device_fns.reset_blacklist();
    }
}

void Focus_ConfigureAudioSource(focus_get_audio_source_t focus_audio_source)
{
    select_focused_audio_source_fns = focus_audio_source;
}

bool Focus_GetAudioSourceForContext(audio_source_t* audio_source)
{
    if (select_focused_audio_source_fns.for_context)
    {
        return select_focused_audio_source_fns.for_context(audio_source);
    }
    return FALSE;
}

bool Focus_GetAudioSourceForUiInput(ui_input_t ui_input, audio_source_t* audio_source)
{
    if (select_focused_audio_source_fns.for_ui_input)
    {
        return select_focused_audio_source_fns.for_ui_input(ui_input, audio_source);
    }
    return FALSE;
}

focus_t Focus_GetFocusForAudioSource(const audio_source_t audio_source)
{
    focus_t audio_source_focus = focus_none;
    if (select_focused_audio_source_fns.focus)
    {
        audio_source_focus = select_focused_audio_source_fns.focus(audio_source);
    }
    return audio_source_focus;
}

void Focus_ConfigureVoiceSource(focus_get_voice_source_t focus_voice_source)
{
    select_focused_voice_source_fns = focus_voice_source;
}

bool Focus_GetVoiceSourceForContext(ui_providers_t provider, voice_source_t* voice_source)
{
    if (select_focused_voice_source_fns.for_context)
    {
        return select_focused_voice_source_fns.for_context(provider, voice_source);
    }
    return FALSE;
}

bool Focus_GetVoiceSourceForUiInput(ui_input_t ui_input, voice_source_t* voice_source)
{
    if (select_focused_voice_source_fns.for_ui_input)
    {
        return select_focused_voice_source_fns.for_ui_input(ui_input, voice_source);
    }
    return FALSE;
}

focus_t Focus_GetFocusForVoiceSource(const voice_source_t voice_source)
{
    focus_t voice_source_focus = focus_none;
    if (select_focused_voice_source_fns.focus)
    {
        voice_source_focus = select_focused_voice_source_fns.focus(voice_source);
    }
    return voice_source_focus;
}
