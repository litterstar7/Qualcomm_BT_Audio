/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   select_focus_domains Focus Select
\ingroup    focus_domains
\brief      This module is an implementation of the focus interface which supports
            selecting the active, focussed device during multipoint use cases.
*/

#include "focus_select.h"

#include "focus_audio_source.h"
#include "focus_device.h"
#include "focus_voice_source.h"

#include <audio_sources.h>
#include <audio_sources_iterator.h>
#include <bt_device.h>
#include <connection_manager.h>
#include <device.h>
#include <device_list.h>
#include <device_properties.h>
#include <handset_service_config.h>
#include <logging.h>
#include <panic.h>
#include <ui.h>
#include <ui_inputs.h>
#include <stdlib.h>

#define CONVERT_AUDIO_SOURCE_TO_ARRAY_INDEX(x) ((x)-1)

static void focusSelect_NotifyVolume(audio_source_t source, event_origin_t origin, volume_t volume);
static void focusSelect_NotifyAudioRouted(audio_source_t source);

/*! \brief Focus Select data structure */
typedef struct
{
    focus_select_audio_priority_t * audio_source_priorities;
    audio_source_t last_routed_audio;

} focus_select_data_t;

/* Used to collect context information from the audio sources available in the
   framework, in a standard form. This data can then be processed to decide what
   should be assigned foreground focus. */
typedef struct
{
    audio_source_provider_context_t context_by_source_array[CONVERT_AUDIO_SOURCE_TO_ARRAY_INDEX(max_audio_sources)];
    audio_source_t highest_priority_audio_source;
    audio_source_provider_context_t highest_priority_context;
    uint8 num_highest_priority_sources;

} audio_source_focus_status_t;

static const audio_source_observer_interface_t focus_select_audio_observer_interface =
{
    .OnVolumeChange = focusSelect_NotifyVolume,
    .OnAudioRouted = focusSelect_NotifyAudioRouted
};

static focus_select_data_t focus_select_data =
{
    .audio_source_priorities = NULL,
    .last_routed_audio = audio_source_none
};

static void focusSelect_NotifyVolume(audio_source_t source, event_origin_t origin, volume_t volume)
{
    UNUSED(source);
    UNUSED(origin);
    UNUSED(volume);
}

static void focusSelect_NotifyAudioRouted(audio_source_t source)
{
    focus_select_data.last_routed_audio = source;
}

static audio_source_provider_context_t focusSelect_GetContext(audio_source_focus_status_t * focus_status, audio_source_t source)
{
    PanicFalse(source != audio_source_none);
    return focus_status->context_by_source_array[CONVERT_AUDIO_SOURCE_TO_ARRAY_INDEX(source)];
}

static bool focusSelect_IsSourceContextConnected(audio_source_focus_status_t * focus_status, audio_source_t source)
{
    return focusSelect_GetContext(focus_status, source) == context_audio_connected;
}

static bool focusSelect_IsSourceContextHighestPriority(audio_source_focus_status_t * focus_status, audio_source_t source)
{
    return focusSelect_GetContext(focus_status, source) == focus_status->highest_priority_context;
}

static bool focusSelect_CompileAudioSourceFocusStatus(audio_source_focus_status_t * focus_status)
{
    audio_source_t curr_source = audio_source_none;
    audio_sources_iterator_t iter = NULL;
    bool source_found = FALSE;

    iter = AudioSourcesIterator_Create(audio_interface_type_media_control);
    curr_source =  AudioSourcesIterator_NextSource(iter);

    while (curr_source != audio_source_none)
    {
        audio_source_provider_context_t context = AudioSources_GetSourceContext(curr_source);

        PanicFalse(context != BAD_CONTEXT);

        if (!source_found || context > focus_status->highest_priority_context)
        {
            focus_status->highest_priority_audio_source = curr_source;
            focus_status->highest_priority_context = context;
            focus_status->num_highest_priority_sources = 1;
            source_found = TRUE;

        }
        else if (context == focus_status->highest_priority_context)
        {
            focus_status->num_highest_priority_sources += 1;
        }

        focus_status->context_by_source_array[CONVERT_AUDIO_SOURCE_TO_ARRAY_INDEX(curr_source)] = context;

        curr_source =  AudioSourcesIterator_NextSource(iter);
    }

    AudioSourcesIterator_Destroy(iter);

    return source_found;
}


static audio_source_t focusSelect_ConvertAudioPrioToSource(focus_select_audio_priority_t prio)
{
    audio_source_t source = audio_source_none;
    switch(prio)
    {
    case FOCUS_SELECT_AUDIO_LINE_IN:
        source = audio_source_line_in;
        break;
    case FOCUS_SELECT_AUDIO_USB:
        source = audio_source_usb;
        break;
    case FOCUS_SELECT_AUDIO_A2DP:
        {
            uint8 is_mru_handset = TRUE;
            device_t device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_mru, &is_mru_handset, sizeof(uint8));
            if (device == NULL)
            {
                deviceType type = DEVICE_TYPE_HANDSET;
                device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_type, &type, sizeof(deviceType));
            }
            if (device != NULL)
            {
                void * ptr_to_source = NULL;
                size_t size = sizeof(audio_source_t);
                if (Device_GetProperty(device, device_property_audio_source, &ptr_to_source, &size))
                {
                    source = *((audio_source_t *)ptr_to_source);
                }
            }
        }
        break;
    default:
        break;
    }
    return source;
}

static void focusSelect_HandleTieBreak(audio_source_focus_status_t * focus_status)
{
    /* Nothing to be done if all audio sources are disconnected or there is no need to tie break */
    if (focus_status->highest_priority_context == context_audio_disconnected ||
        focus_status->num_highest_priority_sources == 1)
    {
        return;
    }

    /* A tie break is needed. Firstly, use the last routed audio source, if one exists */
    if (focus_select_data.last_routed_audio != audio_source_none &&
        focusSelect_IsSourceContextConnected(focus_status, focus_select_data.last_routed_audio))
    {
        focus_status->highest_priority_audio_source = focus_select_data.last_routed_audio;
    }
    /* Otherwise, run through the prioritisation of audio sources and select the highest */
    else
    {
        audio_source_t curr_source = audio_source_none;
        if (focus_select_data.audio_source_priorities != NULL)
        {
            /* Tie break using the Application specified priority. */
            for (int i=0; i<FOCUS_SELECT_AUDIO_MAX_SOURCES; i++)
            {
                curr_source = focusSelect_ConvertAudioPrioToSource(focus_select_data.audio_source_priorities[i]);
                if (focusSelect_IsSourceContextHighestPriority(focus_status, curr_source))
                {
                    focus_status->highest_priority_audio_source = curr_source;
                    break;
                }
            }
        }
        else
        {
            /* Tie break using implicit ordering of audio source. */
            for (curr_source++ ; curr_source < max_audio_sources; curr_source++)
            {
                if (focusSelect_IsSourceContextHighestPriority(focus_status, curr_source))
                {
                    focus_status->highest_priority_audio_source = curr_source;
                    break;
                }
            }
        }
    }

    DEBUG_LOG_DEBUG("focusSelect_HandleTieBreak selected=%d context=%d",
                   focus_status->highest_priority_audio_source,
                   focus_status->highest_priority_context);
}

static bool focusSelect_GetAudioSourceForContext(audio_source_t * audio_source)
{
    bool source_found = FALSE;
    audio_source_focus_status_t focus_status = {0};

    *audio_source = audio_source_none;

    source_found = focusSelect_CompileAudioSourceFocusStatus(&focus_status);
    if (source_found)
    {
        focusSelect_HandleTieBreak(&focus_status);

        /* Assign selected audio source */
        *audio_source = focus_status.highest_priority_audio_source;
    }

    DEBUG_LOG_V_VERBOSE("focusSelect_GetAudioSourceForContext source=%d", *audio_source);

    return source_found;
}

static bool focusSelect_GetAudioSourceForUiInput(ui_input_t ui_input, audio_source_t * audio_source)
{
    bool source_found = FALSE;
    audio_source_focus_status_t focus_status = {0};

    /* For audio sources, we don't need to consider the UI Input type. This is because it
       is effectively prescreened by the UI component, which responds to the context returned
       by this module in the API focusSelect_GetAudioSourceForContext().

       A concrete example being we should only receive ui_input_stop if
       focusSelect_GetAudioSourceForContext() previously provided context_audio_is_streaming.
       In that case there can only be a single streaming source and it shall consume the
       UI Input. All other contentions are handled by focusSelect_HandleTieBreak. */
    UNUSED(ui_input);

    *audio_source = audio_source_none;

    source_found = focusSelect_CompileAudioSourceFocusStatus(&focus_status);
    if (source_found)
    {
        focusSelect_HandleTieBreak(&focus_status);

        /* Assign selected audio source */
        *audio_source = focus_status.highest_priority_audio_source;
    }

    DEBUG_LOG_V_VERBOSE("focusSelect_GetAudioSourceForUiInput source=%d context=%d",
                        focus_status.highest_priority_audio_source,
                        focus_status.highest_priority_context);

    return source_found;
}

static focus_t focusSelect_GetFocusForAudioSource(const audio_source_t audio_source)
{
    focus_t focus = focus_none;
    bool source_found = FALSE;
    audio_source_focus_status_t focus_status = {0};

    source_found = focusSelect_CompileAudioSourceFocusStatus(&focus_status);
    if (source_found)
    {
        if (focusSelect_GetContext(&focus_status, audio_source) != context_audio_disconnected)
        {
            focusSelect_HandleTieBreak(&focus_status);

            if (focus_status.highest_priority_audio_source == ((audio_source_t)audio_source))
            {
                focus = focus_foreground;
            }
            else
            {
                focus = focus_background;
            }
        }
    }

    DEBUG_LOG_V_VERBOSE("focusSelect_GetFocusForAudioSource source=%d focus=%d", audio_source, focus);

    return focus;
}

static bool focusSelect_GetVoiceSourceForContext(ui_providers_t provider, voice_source_t * voice_source)
{
    UNUSED(provider);
    *voice_source = voice_source_hfp_1;
    return TRUE;
}

static bool focusSelect_GetVoiceSourceForUiInput(ui_input_t ui_input, voice_source_t * voice_source)
{
    UNUSED(ui_input);
    *voice_source = voice_source_none;
    return FALSE;
}

static focus_t focusSelect_GetFocusForVoiceSource(const voice_source_t voice_source)
{
    UNUSED(voice_source);
    return focus_none;
}

static bool focusSelect_GetHandsetDevice(device_t *device)
{
    DEBUG_LOG_FN_ENTRY("focusSelect_GetHandsetDevice");
    bool device_found = FALSE;

    PanicNull(device);

    for (uint8 pdl_index = 0; pdl_index < DeviceList_GetMaxTrustedDevices(); pdl_index++)
    {
        if (BtDevice_GetIndexedDevice(pdl_index, device))
        {
            if (BtDevice_GetDeviceType(*device) == DEVICE_TYPE_HANDSET)
            {

                uint8 is_blacklisted = FALSE;
                Device_GetPropertyU8(*device, device_property_blacklist, &is_blacklisted);

                if (!is_blacklisted)
                {
                    device_found = TRUE;
                    break;
                }
            }
        }
    }

    return device_found;
}

static bool focusSelect_GetDeviceForUiInput(ui_input_t ui_input, device_t * device)
{
    bool device_found = FALSE;

    switch (ui_input)
    {
        case ui_input_connect_handset:
            device_found = focusSelect_GetHandsetDevice(device);
            break;
        default:
            DEBUG_LOG_WARN("focusSelect_GetDeviceForUiInput ui_input 0x%x not supported", ui_input);
            break;
    }

    return device_found;
}

static bool focusSelect_GetDeviceForContext(ui_providers_t provider, device_t* device)
{
    UNUSED(provider);
    UNUSED(device);
    return FALSE;
}

static focus_t focusSelect_GetFousForDevice(const device_t device)
{
    UNUSED(device);
    focus_t focus = focus_none;

    return focus;
}


static bool focusSelect_AddDeviceToBlacklist(device_t device)
{
    PanicNull(device);
    DEBUG_LOG_FN_ENTRY("focusSelect_AddDeviceToBlacklist device 0x%p", device);

     return Device_SetPropertyU8(device, device_property_blacklist, TRUE);
 }

static bool focusSelect_RemoveDeviceFromBlacklist(device_t device)
{
    PanicNull(device);
    DEBUG_LOG_FN_ENTRY("focusSelect_RemoveDeviceFromBlacklist device 0x%p", device);

    return Device_SetPropertyU8(device, device_property_blacklist, FALSE);
}

static void focusSelect_ResetDeviceBlacklist(void)
{
    DEBUG_LOG_FN_ENTRY("focusSelcet_ResetDeviceBlacklist");

    device_t* devices = NULL;
    unsigned num_devices = 0;
    deviceType type = DEVICE_TYPE_HANDSET;

    DeviceList_GetAllDevicesWithPropertyValue(device_property_blacklist, &type, sizeof(deviceType), &devices, &num_devices);
    if (devices && num_devices)
    {
        for (unsigned i=0; i< num_devices; i++)
        {
            focusSelect_RemoveDeviceFromBlacklist(devices[i]);
        }
    }
    free(devices);
    devices = NULL;
 }

bool FocusSelect_Init(Task init_task)
{
    audio_source_t source = audio_source_none;

    focus_device_t interface_fns_for_device =
    {
        .for_context = focusSelect_GetDeviceForContext,
        .for_ui_input = focusSelect_GetDeviceForUiInput,
        .focus = focusSelect_GetFousForDevice,
        .add_to_blacklist = focusSelect_AddDeviceToBlacklist,
        .remove_from_blacklist = focusSelect_RemoveDeviceFromBlacklist,
        .reset_blacklist = focusSelect_ResetDeviceBlacklist
    };

    focus_get_audio_source_t interface_fns =
    {
        .for_context = focusSelect_GetAudioSourceForContext,
        .for_ui_input = focusSelect_GetAudioSourceForUiInput,
        .focus = focusSelect_GetFocusForAudioSource
    };

    focus_get_voice_source_t voice_interface_fns =
    {
        .for_context = focusSelect_GetVoiceSourceForContext,
        .for_ui_input = focusSelect_GetVoiceSourceForUiInput,
        .focus = focusSelect_GetFocusForVoiceSource
    };

    UNUSED(init_task);

    DEBUG_LOG_FN_ENTRY("FocusSelect_Init");

    Focus_ConfigureDevice(interface_fns_for_device);
    Focus_ConfigureAudioSource(interface_fns);
    Focus_ConfigureVoiceSource(voice_interface_fns);

    focus_select_data.audio_source_priorities = NULL;
    focus_select_data.last_routed_audio = source;

    /* Register for all audio sources, to obtain indications when audio source is routed */
    while(++source < max_audio_sources)
    {
        AudioSources_RegisterObserver(source, &focus_select_audio_observer_interface);
    }

    return TRUE;
}

void FocusSelect_ConfigureAudioSourcePriorities(const focus_select_audio_priority_t *prioritisation)
{
    focus_select_data.audio_source_priorities = (focus_select_audio_priority_t *)prioritisation;
}
