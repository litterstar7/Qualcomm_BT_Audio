/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file   usb_audio_source_audio.c
\brief  Driver for USB Audio source registration and handling.
*/

#include "kymera_adaptation_audio_protected.h"
#include "usb_audio_fd.h"
#include "usb_audio_defines.h"

static bool usbAudio_GetConnectParameters(audio_source_t source,
                                      source_defined_params_t * source_params);
static void usbAudio_FreeConnectParameters(audio_source_t source,
                                      source_defined_params_t * source_params);
static bool usbAudio_GetDisconnectParameters(audio_source_t source,
                                      source_defined_params_t * source_params);
static void usbAudio_FreeDisconnectParameters(audio_source_t source,
                                      source_defined_params_t * source_params);
static bool usbAudio_IsAudioAvailable(audio_source_t source);
static void usbAudio_SetState(audio_source_t source, audio_source_state_t state);

static const audio_source_audio_interface_t usb_audio_interface =
{
    .GetConnectParameters = usbAudio_GetConnectParameters,
    .ReleaseConnectParameters = usbAudio_FreeConnectParameters,
    .GetDisconnectParameters = usbAudio_GetDisconnectParameters,
    .ReleaseDisconnectParameters = usbAudio_FreeDisconnectParameters,
    .IsAudioAvailable = usbAudio_IsAudioAvailable,
    .SetState = usbAudio_SetState
};

static bool usbAudio_IsAudioAvailable(audio_source_t source)
{
    bool is_available = FALSE;
    usb_audio_info_t *usb_audio = UsbAudioFd_GetHeadphoneInfo(source);

    if((source == audio_source_usb && usb_audio != NULL && 
        (usb_audio->headphone->spkr_active || (UsbAudioFd_GetAudioContext(audio_source_usb) == context_audio_is_streaming))))
    {
        is_available = TRUE;
    }

    return is_available;
}

static bool usbAudio_GetConnectParameters(audio_source_t source,
                                       source_defined_params_t * source_params)
{
    bool ret = FALSE;
    usb_audio_info_t *usb_audio = UsbAudioFd_GetHeadphoneInfo(source);

    if(usbAudio_IsAudioAvailable(source))
    {
        /* USB Audio maybe avialable, but could have lost to priority so disconnected.
            But once it gets the foreground focus, need to set the speaker active flag */
        if(!usb_audio->headphone->spkr_active)
            usb_audio->headphone->spkr_active = TRUE;
        
        PanicNull(source_params);
        usb_audio_connect_parameters_t *connect_params =
                    (usb_audio_connect_parameters_t *)PanicNull(malloc(sizeof(usb_audio_connect_parameters_t)));
        memset(connect_params, 0, sizeof(usb_audio_connect_parameters_t));

        usb_audio_streaming_info_t *streaming_info =
                UsbAudio_GetStreamingInfo(usb_audio,
                                          USB_AUDIO_DEVICE_TYPE_AUDIO_SPEAKER);
        if (streaming_info)
        {
            usb_audio->headphone->spkr_src = StreamUsbEndPointSource(streaming_info->ep_address);
            connect_params->sample_freq = streaming_info->current_sampling_rate;
        }

        connect_params->spkr_src = usb_audio->headphone->spkr_src;

        /* No requirement for Mono Audio, this can be fixed here */
        connect_params->channels = USB_AUDIO_CHANNELS_SPEAKER_STEREO;

        /* At present this is fixed here, in future this can be read from
         * class driver.
         */
        connect_params->frame_size = USB_SAMPLE_SIZE_16_BIT;
        usb_audio->headphone->spkr_sample_rate = connect_params->sample_freq;

        connect_params->volume =
            Volume_CalculateOutputVolume(AudioSources_GetVolume(usb_audio->headphone->audio_source));

        /* Update TTP values for Audio chain */
        connect_params->max_latency_ms = usb_audio->headphone->config->max_latency_ms;
        connect_params->min_latency_ms = usb_audio->headphone->config->min_latency_ms;
        connect_params->target_latency_ms = usb_audio->headphone->config->target_latency_ms;

        /* Audio source has read data for Audio chain creation, it will not
         * do again until it is informed. Lets keep this status and if
         * Host change any of above parameter then need to inform
         * Audio source.
         */
        usb_audio->headphone->chain_active = TRUE;

        DEBUG_LOG("USB Audio channels = %x, frame=%x, Freq=%x",
            connect_params->channels, connect_params->frame_size, connect_params->sample_freq);

        source_params->data = (void *)connect_params;
        source_params->data_length = sizeof(usb_audio_connect_parameters_t);
        ret = TRUE;
    }

    return ret;
}

static void usbAudio_FreeConnectParameters(audio_source_t source,
                                      source_defined_params_t * source_params)
{
    PanicNull(source_params);
    PanicFalse(source_params->data_length == sizeof(usb_audio_connect_parameters_t));
    if(source == audio_source_usb && source_params->data_length)
    {
        free(source_params->data);
        source_params->data = (void *)NULL;
        source_params->data_length = 0;
    }
}

static bool usbAudio_GetDisconnectParameters(audio_source_t source,
                                      source_defined_params_t * source_params)
{
    bool ret = FALSE;
    usb_audio_info_t *usb_audio = UsbAudioFd_GetHeadphoneInfo(source);

    if(source == audio_source_usb && usb_audio != NULL)
    {
        usb_audio_disconnect_parameters_t *disconnect_params;
        PanicNull(source_params);
        disconnect_params = (usb_audio_disconnect_parameters_t *)PanicNull(malloc(sizeof(usb_audio_disconnect_parameters_t)));

        disconnect_params->source = usb_audio->headphone->spkr_src;

        usb_audio->headphone->chain_active = FALSE;

        source_params->data = (void *)disconnect_params;
        source_params->data_length = sizeof(usb_audio_disconnect_parameters_t);
        ret = TRUE;
    }

    return ret;
}

static void usbAudio_FreeDisconnectParameters(audio_source_t source,
                                       source_defined_params_t * source_params)
{
    PanicNull(source_params);
    usb_audio_info_t *usb_audio = UsbAudioFd_GetHeadphoneInfo(source);
    PanicFalse(source_params->data_length == sizeof(usb_audio_disconnect_parameters_t));
    if(source == audio_source_usb && source_params->data_length)
    {
        free(source_params->data);
        source_params->data = (void *)NULL;
        source_params->data_length = 0;
    }

    if(usb_audio != NULL && usb_audio->headphone != NULL)
    {
        usb_audio->headphone->spkr_active = FALSE;
        usb_audio->headphone->spkr_src = NULL;
    }
}

const audio_source_audio_interface_t * UsbAudioFd_GetSourceAudioInterface(void)
{
    return &usb_audio_interface;
}

static void usbAudio_SetState(audio_source_t source, audio_source_state_t state)
{
    DEBUG_LOG("usbAudio_SetState source=%d state=%d", source, state);
    /* Function logic to be implemented */
}
