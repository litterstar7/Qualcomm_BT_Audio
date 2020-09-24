/*!
\copyright  Copyright (c) 2019 - 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of the voice assistants.
*/
#include <panic.h>
#include <stdlib.h>
#include <logging.h>
#include "voice_ui_config.h"
#include "voice_ui_container.h"
#include "voice_ui_gaia_plugin.h"
#include "voice_ui_peer_sig.h"
#include "voice_ui_audio.h"
#include "voice_ui.h"

#include <bt_device.h>
#include <device_db_serialiser.h>
#include <device_properties.h>
#include <logging.h>

static voice_ui_handle_t *active_va = NULL;

/*! \brief Container that holds the handle to created voice assistants */
static voice_ui_handle_t* voice_assistant_list[MAX_NO_VA_SUPPORTED] = {0};

static voice_ui_handle_t* voiceUi_FindHandleForProvider(voice_ui_provider_t provider_name);
static voice_ui_handle_t* voiceUi_GetHandleFromProvider(voice_ui_provider_t provider_name);
static bool voiceUi_ProviderIsValid(voice_ui_provider_t provider_name);
static voice_ui_provider_t voiceUi_GetActiveVaProvider(void);
static void voiceUi_SetA2dpStreamState(voice_ui_a2dp_state_t a2dp_stream_state);

static voice_ui_protected_if_t voice_assistant_protected_if =
{
    voiceUi_GetActiveVaProvider,
    voiceUi_SetA2dpStreamState,
};

static voice_ui_provider_t voiceUi_GetActiveVaProvider(void)
{
    voice_ui_provider_t active_va_provider = voice_ui_provider_none;

    if (active_va && active_va->voice_assistant)
    {
        active_va_provider = active_va->voice_assistant->va_provider;
    }

    DEBUG_LOG_DEBUG("voiceUi_GetActiveVaProvider: %d", active_va_provider);
    return active_va_provider;
}

void voiceUi_SetA2dpStreamState(voice_ui_a2dp_state_t a2dp_stream_state)
{
    if (active_va)
    {
        active_va->va_a2dp_state = a2dp_stream_state;
        DEBUG_LOG_DEBUG("voiceUi_SetA2dpStreamState: %d", active_va->va_a2dp_state);
    }
}

static voice_ui_handle_t* voiceUi_FindHandleForProvider(voice_ui_provider_t provider_name)
{
    /* Find corresponding handle in the container */
    voice_ui_handle_t *va_provider_handle = NULL;
    for (uint8 va_index = 0; va_index < MAX_NO_VA_SUPPORTED; va_index++)
    {
        voice_ui_handle_t* va_handle = voice_assistant_list[va_index];
        if (va_handle && va_handle->voice_assistant)
        {
            if (va_handle->voice_assistant->va_provider == provider_name)
            {
                va_provider_handle = va_handle;
                break;
            }
        }
    }
	
    return va_provider_handle;
}

static voice_ui_handle_t* voiceUi_GetHandleFromProvider(voice_ui_provider_t provider_name)
{
    return PanicNull(voiceUi_FindHandleForProvider(provider_name));
}

static bool voiceUi_ProviderIsValid(voice_ui_provider_t provider_name)
{
    return voiceUi_FindHandleForProvider(provider_name) != NULL;
}

void VoiceUi_SetSelectedAssistant(uint8 voice_ui_provider)
{
    DEBUG_LOG_DEBUG("VoiceUi_SetSelectedAssistant(%d)", voice_ui_provider);
    bdaddr bd_addr;
    appDeviceGetMyBdAddr(&bd_addr);
    device_t device = BtDevice_GetDeviceForBdAddr(&bd_addr);
    if(device)
    {
        Device_SetPropertyU8(device, device_property_voice_assistant, voice_ui_provider);
        DeviceDbSerialiser_Serialise();
        DEBUG_LOG_DEBUG("VoiceUi_SetSelectedAssistant: set property %d", voice_ui_provider);
    }
    VoiceUiGaiaPlugin_NotifyAssistantChanged(voice_ui_provider);
}

static void voiceUi_DeselectCurrentAssistant(void)
{
    if (active_va)
    {
        DEBUG_LOG_DEBUG("voiceUi_DeselectCurrentAssistant");
        VoiceUi_UnrouteAudio();
        active_va->voice_assistant->DeselectVoiceAssistant();
        active_va = NULL;
    }
}

voice_ui_handle_t * VoiceUi_Register(voice_ui_if_t *va_table)
{
    int va_index;
    voice_ui_handle_t* va_handle = NULL;

    /* Find a free slot in the container */
    for(va_index=0;va_index<MAX_NO_VA_SUPPORTED;va_index++)
    {
        if(!voice_assistant_list[va_index])
            break;
    }
    PanicFalse(va_index < MAX_NO_VA_SUPPORTED);

    va_handle = (voice_ui_handle_t*)PanicUnlessMalloc(sizeof(voice_ui_handle_t));
    va_handle->va_a2dp_state = voice_ui_a2dp_state_suspended;
    va_handle->voice_assistant = va_table;
    va_handle->va_protected_if = &voice_assistant_protected_if;
    voice_assistant_list[va_index] = va_handle;

    voice_ui_provider_t registering_va_provider = va_handle->voice_assistant->va_provider;
    DEBUG_LOG_DEBUG("VoiceUi_Register: %d", registering_va_provider);
    if (VoiceUi_GetSelectedAssistant() == registering_va_provider)
    {
        active_va = va_handle;
    }

    return va_handle;
}

voice_ui_handle_t* VoiceUi_GetActiveVa(void)
{
    return active_va;
}

voice_ui_provider_t VoiceUi_GetSelectedAssistant(void)
{
    uint8 selected_va = VOICE_UI_PROVIDER_DEFAULT;
    bdaddr bd_addr;
    device_t device;

    appDeviceGetMyBdAddr(&bd_addr);
    device = BtDevice_GetDeviceForBdAddr(&bd_addr);

    if (device)
    {
        Device_GetPropertyU8(device, device_property_voice_assistant, &selected_va);
        DEBUG_LOG_DEBUG("VoiceUi_GetSelectedAssistant: got property %d", selected_va);
    }

    DEBUG_LOG_DEBUG("VoiceUi_GetSelectedAssistant: selected %d", selected_va);
    return selected_va;
}

uint16 VoiceUi_GetSupportedAssistants(uint8 *assistants)
{
    uint16 count = 0;
    uint16 va_index;

    PanicNull(assistants);

    /* Explicit support for 'none' */
    assistants[count++] = voice_ui_provider_none;

    for (va_index = 0; va_index < MAX_NO_VA_SUPPORTED; ++va_index)
    {
        if (voice_assistant_list[va_index] && voice_assistant_list[va_index]->voice_assistant)
        {
            DEBUG_LOG_DEBUG("VoiceUi_GetSupportedAssistants: voice assistant %d",
                voice_assistant_list[va_index]->voice_assistant->va_provider);
            assistants[count++] = voice_assistant_list[va_index]->voice_assistant->va_provider;
        }
    }

    DEBUG_LOG_DEBUG("VoiceUi_GetSupportedAssistants: count %d", count);
    return count;
}

bool VoiceUi_SelectVoiceAssistant(voice_ui_provider_t va_provider, voice_ui_reboot_permission_t reboot_permission)
{
    bool status = FALSE;
    bool reboot = FALSE;

    DEBUG_LOG_DEBUG("VoiceUi_SelectVoiceAssistant(va_provider %d, reboot_permission %d)", va_provider, reboot_permission);
    if (va_provider == VoiceUi_GetSelectedAssistant())
    {
    /*  Nothing to do  */
        status = TRUE;
    }
    else
    {
        if (va_provider == voice_ui_provider_none)
        {
            voiceUi_DeselectCurrentAssistant();
            status = TRUE;
        }
        else if (voiceUi_ProviderIsValid(va_provider))
        {
            voiceUi_DeselectCurrentAssistant();
            active_va = voiceUi_GetHandleFromProvider(va_provider);
            active_va->voice_assistant->SelectVoiceAssistant();
            status = TRUE;
            if (reboot_permission == voice_ui_reboot_allowed)
            {
                reboot = va_provider == voice_ui_provider_gaa;
            }
        }
        else
        {
            DEBUG_LOG_ERROR("VoiceUi_SelectVoiceAssistant:va_provider %d not valid", va_provider);
        }
    }

    DEBUG_LOG_DEBUG("VoiceUi_SelectVoiceAssistant:va_provider %d, status %d, reboot %d", va_provider, status, reboot);
    if (status)
    {
        VoiceUi_SetSelectedAssistant(va_provider);
#ifdef INCLUDE_TWS
        VoiceUi_UpdateSelectedPeerVaProvider(reboot);
#else
        if (reboot)
        {
            VoiceUi_RebootLater();
        }
#endif  /* INCLUDE_TWS */
    }

    return status;
}

void VoiceUi_EventHandler(voice_ui_handle_t* va_handle, ui_input_t event_id)
{
   if (va_handle)
   {
        if(va_handle->voice_assistant->EventHandler)
        {
            va_handle->voice_assistant->EventHandler(event_id);
        }
   }
}

bool VoiceUi_IsVoiceAssistantA2dpStreamActive(void)
{
    bool voice_ui_a2dp_stream_state = FALSE;
    if(active_va)
    {
        voice_ui_a2dp_stream_state = (active_va->va_a2dp_state == voice_ui_a2dp_state_streaming);
    }
    return voice_ui_a2dp_stream_state;
}

DataFileID VoiceUi_LoadHotwordModelFile(FILE_INDEX file_index)
{
    return active_va->voice_assistant->audio_if.LoadHotwordModelFile(file_index);
}

#ifdef HOSTED_TEST_ENVIRONMENT
void VoiceUi_UnRegister(voice_ui_handle_t *va_handle)
{
    if (va_handle)
    {
        int i;
        for(i=0; i < MAX_NO_VA_SUPPORTED; i++)
        {
            if(voice_assistant_list[i] && (va_handle == voice_assistant_list[i]))
            {
                free(voice_assistant_list[i]);
                voice_assistant_list[i] = NULL;
                break;
            }
        }
    }
}
#endif
