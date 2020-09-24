/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      USB Audio Function Driver
*/

#include "usb_audio_fd.h"
#include "usb_audio_class_10.h"

#include <panic.h>
#include "kymera.h"
#include "volume_messages.h"
#include "telephony_messages.h"
#include "ui.h"

#define USB_AUDIO_MSG_SEND(task, id, msg)       if(task != NULL) \
                                                   task->handler(task, id, msg)

usb_audio_info_t *usbaudio_globaldata = NULL;

static void UsbAudio_HandleMessage(usb_device_index_t device_index,
                                     MessageId id, Message message);
static void UsbAudio_ClassEvent(uac_ctx_t class_ctx,
                                uint8 interface_index,
                                uac_message_t uac_message);
static void usbAudioUpdateHeadphoneVolume(usb_audio_info_t *usb_audio);
static void usbAudioUpdateHeadsetVolume(usb_audio_info_t *usb_audio);
static int16 usbAudio_GetVolumeGain(int16 volume_in_db);
static void usbAudio_SetAudioCxt(const usb_audio_info_t* usb_audio, audio_source_provider_context_t cxt);

#define USB_AUDIO_GET_DATA()         usbaudio_globaldata

Task usb_audio_client_cb[USB_AUDIO_REGISTERED_CLIENT_COUNT];

/****************************************************************************
    Initialize and add USB Voice Class Driver
*/
static void usbAudioAddHeadset(usb_audio_info_t *usb_audio,
                               const usb_audio_config_params_t *config)
{
    usb_audio->headset = PanicUnlessNew(usb_audio_headset_info_t);
    memset(usb_audio->headset, 0, sizeof(usb_audio_headset_info_t));

    usb_audio->headset->streaming_info = (usb_audio_streaming_info_t *)
            PanicUnlessMalloc(sizeof(usb_audio_streaming_info_t) *
                              usb_audio->num_interfaces);
    memset(usb_audio->headset->streaming_info, 0,
           sizeof(usb_audio_streaming_info_t) * usb_audio->num_interfaces);

    usb_audio->headset->config = config;
    /* create USB audio class instance */
    usb_audio->headset->class_ctx =
            usb_audio->usb_fn_uac->Create(usb_audio->device_index,
                                          config,
                                          usb_audio->headset->streaming_info,
                                          UsbAudio_ClassEvent);

    usb_audio->headset->audio_source = voice_source_usb;
    /* Register with audio source for Audio use case */
    VoiceSources_RegisterAudioInterface(voice_source_usb,
                              UsbAudioFd_GetSourceVoiceInterface());

    /* Register with volume source for Audio use case */
    VoiceSources_RegisterVolume(voice_source_usb,
        UsbAudioFd_GetVoiceSourceVolumeInterface());

    /* Init default volume for USB Voice */
    usb_audio->headset->spkr_volume = usbAudio_GetVolumeGain(config->volume_config.target_db);
}

/****************************************************************************
    Initialize and add USB Audio Class Driver
*/
static void usbAudioAddHeadphone(usb_audio_info_t *usb_audio,
                                 const usb_audio_config_params_t *config)
{
    usb_audio->headphone = PanicUnlessNew(usb_audio_headphone_info_t);
    memset(usb_audio->headphone, 0, sizeof(usb_audio_headphone_info_t));

    usb_audio->headphone->streaming_info = (usb_audio_streaming_info_t *)
            PanicUnlessMalloc(sizeof(usb_audio_streaming_info_t) *
                              usb_audio->num_interfaces);
    memset(usb_audio->headphone->streaming_info, 0,
           sizeof(usb_audio_streaming_info_t) * usb_audio->num_interfaces);

    usb_audio->headphone->config = config;
    /* create USB audio class instance */
    usb_audio->headphone->class_ctx =
            usb_audio->usb_fn_uac->Create(usb_audio->device_index,
                                          config,
                                          usb_audio->headphone->streaming_info,
                                          UsbAudio_ClassEvent);

    usb_audio->headphone->audio_source = audio_source_usb;
    /* Register with audio source for Audio use case */
    AudioSources_RegisterAudioInterface(audio_source_usb,
                              UsbAudioFd_GetSourceAudioInterface());

    /* Register with volume source for Audio use case */
    AudioSources_RegisterVolume(audio_source_usb,
        UsbAudioFd_GetAudioSourceVolumeInterface());

    AudioSources_RegisterMediaControlInterface(audio_source_usb,
                UsbAudioFd_GetMediaControlAudioInterface());

    usb_audio->headphone->spkr_volume = usbAudio_GetVolumeGain(config->volume_config.target_db);
}

/****************************************************************************
    Return function table for supported USB class driver
*/
static const usb_fn_tbl_uac_if * usbAudioGetFnTbl(usb_audio_class_rev_t rev)
{
    const usb_fn_tbl_uac_if *tbl = NULL;

    switch(rev)
    {
        case USB_AUDIO_CLASS_REV_1:
            tbl = UsbAudioClass10_GetFnTbl();
            break;

        default:
            DEBUG_LOG("Unsupported USB Class Revision 0x%x\n", rev);
            break;
    }

    return tbl;
}

/****************************************************************************
    Set the audio context
*/
static void usbAudio_SetAudioCxt(const usb_audio_info_t* usb_audio, audio_source_provider_context_t cxt)
{
    DEBUG_LOG_STATE("usbAudio_SetAudioCxt: %d\n",cxt);
    if(usb_audio && usb_audio->headphone)
    {
        usb_audio->headphone->audio_ctx = cxt;
    }
}

unsigned UsbAudioFd_GetAudioContext(audio_source_t source)
{
    audio_source_provider_context_t context = BAD_CONTEXT;
    usb_audio_info_t *usb_audio = UsbAudioFd_GetHeadphoneInfo(source);

    if(usb_audio && usb_audio->headphone)
    {
        context = usb_audio->headphone->audio_ctx;
    }

    return (unsigned)context;
}

/****************************************************************************
    Inform Audio source for USB Audio disconnection.
*/
static void usbAudioDisconnectAudioMsg(usb_audio_info_t *usb_audio)
{
    /* This is error case, if we dont have Media Player client and Audio
     * cant be played, it should have not happend.
     */
    PanicNull(usb_audio_client_cb[USB_AUDIO_REGISTERED_CLIENT_MEDIA]);

    usb_audio->headphone->spkr_active = FALSE;
    /* with respect to audio context, host has just connected but not actively streaming */
    usbAudio_SetAudioCxt(usb_audio, context_audio_connected);
    DEBUG_LOG_ALWAYS("USB Audio: Audio Disconnected");
    /* Inform Media player as speaker is in placed */
    USB_AUDIO_MSG_SEND(usb_audio_client_cb[USB_AUDIO_REGISTERED_CLIENT_MEDIA],
                       USB_AUDIO_DISCONNECTED_IND,
                       (Message)(&usb_audio->headphone->audio_source));
}

/****************************************************************************
    Inform Audio source for USb Audio connection.
*/
static void usbAudioConnectAudioMsg(usb_audio_info_t *usb_audio)
{
    /* This is error case, if we dont have Media Player client and Audio
     * cant be played, it should have not happend.
     */
    PanicNull(usb_audio_client_cb[USB_AUDIO_REGISTERED_CLIENT_MEDIA]);

    usb_audio->headphone->spkr_active = TRUE;
    /* with respect to audio context, this means host is actively streaming audio */
    usbAudio_SetAudioCxt(usb_audio, context_audio_is_streaming);
    DEBUG_LOG_ALWAYS("USB Audio: Audio Connected");
    /* Inform Media player as speaker is in placed */
    USB_AUDIO_MSG_SEND(usb_audio_client_cb[USB_AUDIO_REGISTERED_CLIENT_MEDIA],
                       USB_AUDIO_CONNECTED_IND,
                       (Message)(&usb_audio->headphone->audio_source));
}

/****************************************************************************
    Inform Voice source for USB Voice disconnection.
*/
static void usbAudioDisconnectVoiceMsg(usb_audio_info_t *usb_audio)
{
    /* This is error case, if we dont have Media Player client and Audio
     * cant be played, it should have not happend.
     */
    PanicNull(usb_audio_client_cb[USB_AUDIO_REGISTERED_CLIENT_TELEPHONY]);

    DEBUG_LOG("USB Audio: Voice Disconnected");
    /* Inform Media player as speaker is in placed */
    USB_AUDIO_MSG_SEND(usb_audio_client_cb[USB_AUDIO_REGISTERED_CLIENT_TELEPHONY],
                       TELEPHONY_AUDIO_DISCONNECTED,
                       (Message)(&usb_audio->headset->audio_source));
}

/****************************************************************************
    Inform Voice source for USB Voice connection.
*/
static void usbAudioConnectVoiceMsg(usb_audio_info_t *usb_audio)
{
    /* This is error case, if we dont have Media Player client and Audio
     * cant be played, it should have not happend.
     */
    PanicNull(usb_audio_client_cb[USB_AUDIO_REGISTERED_CLIENT_TELEPHONY]);

    usb_audio->headset->spkr_active = TRUE;
    DEBUG_LOG("USB Audio: Voice Enumerated");
    /* Inform Media player as speaker is in placed */
    USB_AUDIO_MSG_SEND(usb_audio_client_cb[USB_AUDIO_REGISTERED_CLIENT_TELEPHONY],
                       TELEPHONY_AUDIO_CONNECTED,
                       (Message)(&usb_audio->headset->audio_source));
}

/****************************************************************************
    Update status for MIC/Speaker for Headset.
*/
static void usbAudioUpdateHeadset(usb_audio_info_t *usb_audio,
                                  uint16 interface, uint16 altsetting)
{
    const usb_audio_interface_config_list_t *intf_list = usb_audio->headset->config->intf_list;
    usb_audio_streaming_info_t  *streaming_info = usb_audio->headset->streaming_info;

    for (uint8 i=0; i < usb_audio->num_interfaces; i++)
    {
        if (streaming_info[i].interface != interface)
        {
            continue;
        }

        if (intf_list->intf[i].type == USB_AUDIO_DEVICE_TYPE_VOICE_MIC)
        {
            DEBUG_LOG("USB Voice Mic %x, %x",  interface, altsetting);

            if(altsetting)
            {
                /* For voice both mic and speakers are present. And when speaker
                * is active, Voice chain can be created. So no action in required
                * here except keeping track mic is active.
                * This will be set to FALSE when chain is destroyed.
                */
                usb_audio->headset->mic_active = TRUE;
            }
        }
        else if (intf_list->intf[i].type == USB_AUDIO_DEVICE_TYPE_VOICE_SPEAKER)
        {
            DEBUG_LOG("USB Voice Spkr %x, %x", interface, altsetting);

            if(usb_audio->headset->spkr_active)
            {
                /* USB Speaker is already connected, check if Host does not
                 * want to use anymore. */
                if(!altsetting)
                {
                    usbAudioDisconnectVoiceMsg(usb_audio);
                }
            }
            else
            {
                if(altsetting)
                {
                    usbAudioConnectVoiceMsg(usb_audio);
                    usbAudioUpdateHeadsetVolume(usb_audio);
                }
            }
        }
    }
}

/****************************************************************************
    Update status for Speaker for Headphone.
*/
static void usbAudioUpdateHeadphone(usb_audio_info_t *usb_audio,
                                    uint16 interface, uint16 altsetting)
{
    const usb_audio_interface_config_list_t *intf_list = usb_audio->headphone->config->intf_list;
    usb_audio_streaming_info_t  *streaming_info = usb_audio->headphone->streaming_info;

    for (uint8 i=0; i < usb_audio->num_interfaces; i++)
    {
        if (streaming_info[i].interface != interface)
        {
            continue;
        }

        if (intf_list->intf[i].type == USB_AUDIO_DEVICE_TYPE_AUDIO_SPEAKER)
        {
            DEBUG_LOG("USB Audio: Spkr %x, %x", interface, altsetting);

            if(usb_audio->headphone->spkr_active)
            {
                /* USB Speaker is already connected, check if Host does not
                 * want to use anymore. */
                if(!altsetting)
                {
                    usbAudioDisconnectAudioMsg(usb_audio);
                }
            }
            else
            {
                if(altsetting)
                {
                    usbAudioConnectAudioMsg(usb_audio);
                    usbAudioUpdateHeadphoneVolume(usb_audio);
                }
            }
        }
    }
}

/****************************************************************************
    Converts volume DB in steps.
*/
static int16 usbAudio_GetVolumeGain(int16 volume_in_db)
{
    int16 new_gain = USB_AUDIO_DB_TO_STEPS(volume_in_db);

    /* limit check */
    if(new_gain > USB_AUDIO_VOLUME_MAX_STEPS)
    {
        new_gain = USB_AUDIO_VOLUME_MAX_STEPS;
    }
    if(new_gain < USB_AUDIO_VOLUME_MIN_STEPS)
    {
        new_gain = USB_AUDIO_VOLUME_MIN_STEPS;
    }

    return new_gain;
}

/****************************************************************************
    Update USB audio volume for active Voice chain
*/
static void usbAudioUpdateHeadsetVolume(usb_audio_info_t *usb_audio)
{
    if(usb_audio->headset != NULL && usb_audio->headset->spkr_active)
    {
        int16 new_volume = 0;
        uac_voice_levels_t levels;
        const usb_audio_interface_config_list_t *intf_list = usb_audio->headset->config->intf_list;
        usb_audio_streaming_info_t  *streaming_info = usb_audio->headset->streaming_info;;

        memset(&levels, 0, sizeof(levels));

        for (uint8 i=0; i < usb_audio->num_interfaces; i++)
        {
            if (intf_list->intf[i].type == USB_AUDIO_DEVICE_TYPE_VOICE_MIC)
            {
                levels.in_vol =  streaming_info[i].volume_status.l_volume;
                levels.in_mute = streaming_info[i].volume_status.mute_status;
            }
            else if (intf_list->intf[i].type == USB_AUDIO_DEVICE_TYPE_VOICE_SPEAKER)
            {
                levels.out_l_vol = streaming_info[i].volume_status.l_volume;
                levels.out_r_vol = streaming_info[i].volume_status.r_volume;
                levels.out_mute =  streaming_info[i].volume_status.mute_status;
            }
        }

        DEBUG_LOG("USB Audio headset: Scaled Gain L=%ddB, R=%ddB\n",
                                     levels.out_l_vol, levels.out_r_vol);
        DEBUG_LOG("USB Audio headset: Mute %X\n", levels.out_mute);

        new_volume = usbAudio_GetVolumeGain(levels.out_l_vol);

        /* check for mute state */
        if(levels.out_mute)
        {
            /* set to mute */
            new_volume = USB_AUDIO_VOLUME_MIN_STEPS;
        }

        if(new_volume != usb_audio->headset->spkr_volume)
        {
            DEBUG_LOG("USB Audio headset: Gain Level = %d\n", new_volume);
            usb_audio->headset->spkr_volume = new_volume;
            /* Update volume structure */
            Volume_SendVoiceSourceVolumeUpdateRequest(
                                  usb_audio->headset->audio_source,
                                  event_origin_external, new_volume);
        }

        /* Re-configure audio chain */
        appKymeraUsbVoiceMicMute(levels.in_mute);
    }
}

/****************************************************************************
    Update USB audio volume for active Audio chain
*/
static void usbAudioUpdateHeadphoneVolume(usb_audio_info_t *usb_audio)
{
    if(usb_audio->headphone != NULL && usb_audio->headphone->spkr_active)
    {
        int16 new_volume = 0;

        uac_audio_levels_t levels;

        const usb_audio_interface_config_list_t *intf_list = usb_audio->headphone->config->intf_list;
        usb_audio_streaming_info_t  *streaming_info = usb_audio->headphone->streaming_info;;

        memset(&levels, 0, sizeof(levels));

        for (uint8 i=0; i < usb_audio->num_interfaces; i++)
        {
            if (intf_list->intf[i].type == USB_AUDIO_DEVICE_TYPE_AUDIO_SPEAKER)
            {
                levels.out_l_vol = streaming_info[i].volume_status.l_volume;
                levels.out_r_vol = streaming_info[i].volume_status.r_volume;
                levels.out_mute =  streaming_info[i].volume_status.mute_status;
            }
        }

        DEBUG_LOG("USB Audio headphone: Scaled Gain L=%ddB, R=%ddB\n",
                                     levels.out_l_vol, levels.out_r_vol);
        DEBUG_LOG("USB Audio headphone: Mute %X\n", levels.out_mute);

        new_volume = usbAudio_GetVolumeGain(levels.out_l_vol);

        /* check for mute state */
        if(levels.out_mute)
        {
            /* set to mute */
            new_volume = USB_AUDIO_VOLUME_MIN_STEPS;
        }

        if(new_volume != usb_audio->headphone->spkr_volume)
        {
            DEBUG_LOG("USB Audio headphone: Gain Level = %d\n", new_volume);
            usb_audio->headphone->spkr_volume = new_volume;
            /* Update volume structure */
            Volume_SendAudioSourceVolumeUpdateRequest(
                                  usb_audio->headphone->audio_source,
                                  event_origin_external, new_volume);
        }
    }
}

/****************************************************************************
    Update sample rate for Headphone
*/
static void usbAudioSetHeadphoneSampleRate(usb_audio_info_t *usb_audio)
{
    uint32 old_sample_rate;

    if(usb_audio->headphone != NULL && usb_audio->headphone->spkr_active)
    {
        old_sample_rate = usb_audio->headphone->spkr_sample_rate;

        uint32 new_sample_rate = 0;

        const usb_audio_interface_config_list_t *intf_list = usb_audio->headphone->config->intf_list;
        usb_audio_streaming_info_t  *streaming_info = usb_audio->headphone->streaming_info;;

        for (uint8 i=0; i < usb_audio->num_interfaces; i++)
        {
            if (intf_list->intf[i].type == USB_AUDIO_DEVICE_TYPE_AUDIO_SPEAKER)
            {
                new_sample_rate = streaming_info[i].current_sampling_rate;
            }
        }

        /* If sample rate is read by audio source then first
         * disconnect existing chain and connect with new sample rate.
         * Otherwise just update context as it will read later.
         */
        if(usb_audio->headphone->chain_active &&
                             old_sample_rate != new_sample_rate)
        {
            usb_audio->headphone->spkr_sample_rate = new_sample_rate;
            DEBUG_LOG("USB Audio: New sample rate %x", new_sample_rate);
            usbAudioDisconnectAudioMsg(usb_audio);
            usbAudioConnectAudioMsg(usb_audio);
        }
    }
}

/****************************************************************************
    Update sample rate for Headset
*/
static void usbAudioSetHeadsetSampleRate(usb_audio_info_t *usb_audio)
{
    if(usb_audio->headset != NULL && usb_audio->headset->spkr_active)
    {
        uint32 old_sample_rate = usb_audio->headset->sample_rate;
        uint32 new_sample_rate = 0;

        const usb_audio_interface_config_list_t *intf_list = usb_audio->headset->config->intf_list;
        usb_audio_streaming_info_t  *streaming_info = usb_audio->headset->streaming_info;;

        for (uint8 i=0; i < usb_audio->num_interfaces; i++)
        {
            if (intf_list->intf[i].type == USB_AUDIO_DEVICE_TYPE_VOICE_SPEAKER)
            {
                new_sample_rate = streaming_info[i].current_sampling_rate;
            }
        }

        /* If sample rate is read by audio source then first
         * disconnect existing chain and connect with new sample rate.
         * Otherwise just update context as it will read later.
         */
        if(usb_audio->headset->chain_active &&
                             old_sample_rate != new_sample_rate)
        {
            usb_audio->headset->sample_rate = new_sample_rate;
            DEBUG_LOG("USB Audio: Headset New sample rate %x", new_sample_rate);
            usbAudioDisconnectVoiceMsg(usb_audio);
            usbAudioConnectVoiceMsg(usb_audio);
        }
    }
}

usb_audio_streaming_info_t *UsbAudio_GetStreamingInfo(usb_audio_info_t *usb_audio,
                                                      usb_audio_device_type_t type)
{
    const usb_audio_interface_config_list_t *intf_list;
    usb_audio_streaming_info_t *streaming_info;

    if (type == USB_AUDIO_DEVICE_TYPE_AUDIO_SPEAKER)
    {
        intf_list = usb_audio->headphone->config->intf_list;
        streaming_info = usb_audio->headphone->streaming_info;
    }
    else
    {
        intf_list = usb_audio->headset->config->intf_list;
        streaming_info = usb_audio->headset->streaming_info;
    }

     for (uint8 i=0; i < usb_audio->num_interfaces; i++)
     {
         if (intf_list->intf[i].type == type)
         {
             return &streaming_info[i];
         }
     }

     return NULL;
}


usb_audio_info_t *UsbAudioFd_GetHeadphoneInfo(audio_source_t source)
{
    usb_audio_info_t *usb_audio = USB_AUDIO_GET_DATA();
    while (usb_audio)
    {
        if(usb_audio->headphone != NULL &&
            usb_audio->headphone->audio_source == source)
        {
            break;
        }
        usb_audio = usb_audio->next;
    }

    return usb_audio;
}

usb_audio_info_t *UsbAudioFd_GetHeadsetInfo(voice_source_t source)
{
    usb_audio_info_t *usb_audio = USB_AUDIO_GET_DATA();
    while (usb_audio)
    {
        if(usb_audio->headset != NULL &&
            usb_audio->headset->audio_source == source)
        {
            break;
        }
        usb_audio = usb_audio->next;
    }

    return usb_audio;
}

void UsbAudio_ClientRegister(Task client_task,
                             usb_audio_registered_client_t name)
{
    if (usb_audio_client_cb[name] == NULL)
    {
        usb_audio_client_cb[name]= client_task;
    }
}

void UsbAudio_ClientUnRegister(Task client_task,
                               usb_audio_registered_client_t name)
{
    if (usb_audio_client_cb[name] == client_task)
    {
        usb_audio_client_cb[name]= NULL;
    }
}

/****************************************************************************
    Handle USB device and audio class messages.
*/
static void UsbAudio_HandleMessage(usb_device_index_t device_index,
                                     MessageId id, Message message)
{
    usb_audio_info_t *usb_audio = USB_AUDIO_GET_DATA();

    DEBUG_LOG_DEBUG("USB Audio device %d event 0x%x", device_index, id);

    while (usb_audio)
    {
        if(device_index == usb_audio->device_index)
        {
            switch(id)
            {
                case MESSAGE_USB_DETACHED:
                    if(usb_audio->headphone != NULL && usb_audio->headphone->spkr_active)
                    {
                        usbAudioDisconnectAudioMsg(usb_audio);
                        /* with respect to usb audio, this means the host as disconnected audio */
                        usbAudio_SetAudioCxt(usb_audio, context_audio_disconnected);
                    }
                    if(usb_audio->headset != NULL && usb_audio->headset->spkr_active)
                    {
                        usbAudioDisconnectVoiceMsg(usb_audio);
                    }
                    break;
                case MESSAGE_USB_ALT_INTERFACE:
                {
                    const MessageUsbAltInterface* ind = (const MessageUsbAltInterface*)message;

                    if(usb_audio->headset != NULL)
                    {
                        usbAudioUpdateHeadset(usb_audio, ind->interface, ind->altsetting);
                    }
                    if(usb_audio->headphone != NULL)
                    {
                        usbAudioUpdateHeadphone(usb_audio, ind->interface, ind->altsetting);
                    }
                    break;
                }
                case MESSAGE_USB_ENUMERATED:
                    {
                        /* with respect to usb audio, this means that host has just connected and not streaming */
                        usbAudio_SetAudioCxt(usb_audio, context_audio_connected);
                    }
                    break;
                default:
                    DEBUG_LOG("Unhandled USB message 0x%x\n", id);
                    break;
            }
        }
        usb_audio = usb_audio->next;
    }
}

/* Handle event from USB audio class driver */
static void UsbAudio_ClassEvent(uac_ctx_t class_ctx,
                                uint8 interface_index,
                                uac_message_t uac_message)
{
    usb_audio_info_t *usb_audio = USB_AUDIO_GET_DATA();
    UNUSED(interface_index);

    while (usb_audio)
    {
        if (usb_audio->headset && usb_audio->headset->class_ctx == class_ctx)
        {
            switch(uac_message)
            {
                case USB_AUDIO_CLASS_MSG_LEVELS:
                    usbAudioUpdateHeadsetVolume(usb_audio);
                    break;

                case USB_AUDIO_CLASS_MSG_SAMPLE_RATE:
                    usbAudioSetHeadsetSampleRate(usb_audio);
                    break;

                default:
                    DEBUG_LOG("Unhandled USB message 0x%x\n", uac_message);
                    break;
            }
            break;
        }
        else if (usb_audio->headphone && usb_audio->headphone->class_ctx == class_ctx)
        {
            switch(uac_message)
            {
                case USB_AUDIO_CLASS_MSG_LEVELS:
                    usbAudioUpdateHeadphoneVolume(usb_audio);
                    break;

                case USB_AUDIO_CLASS_MSG_SAMPLE_RATE:
                    usbAudioSetHeadphoneSampleRate(usb_audio);
                    break;

                default:
                    DEBUG_LOG("Unhandled USB message 0x%x\n", uac_message);
                    break;
            }
            break;
        }
        usb_audio = usb_audio->next;
    }
}
/****************************************************************************
    Create context for USB Audio function driver.
*/
static usb_class_context_t UsbAudio_Create(usb_device_index_t device_index,
                                usb_class_interface_config_data_t config_data)
{
    const usb_audio_config_params_t *config = (const usb_audio_config_params_t *)config_data;
    usb_audio_info_t *usb_audio;
    usb_audio_device_type_t audio_device_types;

    /* Configuration data is required */
    PanicZero(config);
    PanicZero(config->intf_list);
    PanicZero(config->intf_list->num_interfaces);

    usb_audio = (usb_audio_info_t *)PanicUnlessMalloc(sizeof(usb_audio_info_t));
    memset(usb_audio, 0, sizeof(usb_audio_info_t));

    usb_audio->usb_fn_uac = usbAudioGetFnTbl(config->rev);
    /* Requested device class revision must be supported */
    PanicZero(usb_audio->usb_fn_uac);

    usb_audio->num_interfaces = config->intf_list->num_interfaces;

    /* Check only supported types requested */
    audio_device_types = 0;
    for (uint8 i=0; i < usb_audio->num_interfaces; i++)
    {
        audio_device_types |= config->intf_list->intf[i].type;
    }
    /* At least one supported device class must be requested */
    PanicZero(audio_device_types & USB_AUDIO_SUPPORTED_DEVICE_TYPES);

    usb_audio->device_index = device_index;

    if ((audio_device_types & USB_AUDIO_DEVICE_TYPE_VOICE_SPEAKER) &&
        (audio_device_types & USB_AUDIO_DEVICE_TYPE_VOICE_MIC))
    {
        usbAudioAddHeadset(usb_audio, config);
        audio_device_types &= ~(USB_AUDIO_DEVICE_TYPE_VOICE_SPEAKER |
                                USB_AUDIO_DEVICE_TYPE_VOICE_MIC);
    }

    if (audio_device_types & USB_AUDIO_DEVICE_TYPE_AUDIO_SPEAKER)
    {
        usbAudioAddHeadphone(usb_audio, config);
        audio_device_types &= ~(USB_AUDIO_DEVICE_TYPE_AUDIO_SPEAKER);
    }

    /* All requested devices handled */
    PanicNotZero(audio_device_types);

    UsbDevice_RegisterEventHandler(device_index, UsbAudio_HandleMessage);

    usb_audio->next = usbaudio_globaldata;
    usbaudio_globaldata = usb_audio;

    return usb_audio;
}

static usb_result_t UsbAudio_Destroy(usb_class_context_t context)
{
    usb_result_t ret = USB_RESULT_NOT_FOUND;
    usb_audio_info_t **dp = &usbaudio_globaldata;
    usb_audio_info_t *usb_audio = NULL;

    if(context == NULL)
    {
        return ret;
    }

    while(*dp)
    {
        if ((usb_class_context_t)(*dp) == context)
        {
            usb_audio = *dp;
            *dp = (*dp)->next;
            if(usb_audio->headset != NULL)
            {
                usbAudioDisconnectVoiceMsg(usb_audio);
                usb_audio->usb_fn_uac->Delete(usb_audio->headset->class_ctx);
                free(usb_audio->headset->streaming_info);
                free(usb_audio->headset);
                usb_audio->headset = NULL;
            }

            if(usb_audio->headphone != NULL)
            {
                usbAudioDisconnectAudioMsg(usb_audio);
                usb_audio->usb_fn_uac->Delete(usb_audio->headphone->class_ctx);
                free(usb_audio->headphone->streaming_info);
                free(usb_audio->headphone);
                usb_audio->headphone = NULL;
            }
            free(usb_audio);
            ret = USB_RESULT_OK;
            break;
        }
        dp = &((*dp)->next);
    }

    return ret;
}

const usb_class_interface_cb_t UsbAudio_Callbacks =
{
    .Create = UsbAudio_Create,
    .Destroy = UsbAudio_Destroy,
    .SetInterface = NULL
};
