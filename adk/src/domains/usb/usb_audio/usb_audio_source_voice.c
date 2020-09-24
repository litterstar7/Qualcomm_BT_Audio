/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file   usb_audio_source_voice.c
\brief Driver for USB Voice source registration and handling.
*/

#include "kymera_adaptation_voice_protected.h"
#include "usb_audio_fd.h"
#include "usb_audio_defines.h"

static bool usbAudio_GetVoiceConnectParameters(voice_source_t source,
                                      source_defined_params_t * source_params);
static void usbAudio_FreeVoiceConnectParameters(voice_source_t source,
                                      source_defined_params_t * source_params);
static bool usbAudio_GetVoiceDisconnectParameters(voice_source_t source,
                                      source_defined_params_t * source_params);
static void usbAudio_FreeVoiceDisconnectParameters(voice_source_t source,
                                      source_defined_params_t * source_params);
static bool usbAudio_IsVoiceAvailable(voice_source_t source);

static const voice_source_audio_interface_t usb_voice_interface =
{
    .GetConnectParameters = usbAudio_GetVoiceConnectParameters,
    .ReleaseConnectParameters = usbAudio_FreeVoiceConnectParameters,
    .GetDisconnectParameters = usbAudio_GetVoiceDisconnectParameters,
    .ReleaseDisconnectParameters = usbAudio_FreeVoiceDisconnectParameters,
    .IsAudioAvailable = usbAudio_IsVoiceAvailable
};

static usb_voice_mode_t usbAudio_GetVoiceBand(uint32 sample_rate)
{
    usb_voice_mode_t ret = usb_voice_mode_wb;
    switch(sample_rate)
    {
        case SAMPLE_RATE_32K:
            ret = usb_voice_mode_uwb;
        break;
        case SAMPLE_RATE_16K:
            ret = usb_voice_mode_wb;
        break;
        case SAMPLE_RATE_8K:
            ret = usb_voice_mode_nb;
        break;
        default:
            DEBUG_LOG("USB Voice: Unsupported Sample rate");
            PanicFalse(FALSE);
        break;
    }
    return ret;
}

static bool usbAudio_GetVoiceConnectParameters(voice_source_t source,
                                       source_defined_params_t * source_params)
{
    bool ret = FALSE;
    usb_audio_info_t *usb_audio = UsbAudioFd_GetHeadsetInfo(source);

    if(usbAudio_IsVoiceAvailable(source))
    {
        PanicNull(source_params);

        usb_voice_connect_parameters_t *connect_params =
                    (usb_voice_connect_parameters_t *)PanicNull(malloc(sizeof(usb_voice_connect_parameters_t)));
        memset(connect_params, 0, sizeof(usb_voice_connect_parameters_t));

        usb_audio_streaming_info_t *streaming_info =
                UsbAudio_GetStreamingInfo(usb_audio,
                                          USB_AUDIO_DEVICE_TYPE_VOICE_SPEAKER);
        if (streaming_info)
        {
            usb_audio->headset->spkr_source = StreamUsbEndPointSource(streaming_info->ep_address);
            connect_params->sample_rate = streaming_info->current_sampling_rate;
        }

        streaming_info = UsbAudio_GetStreamingInfo(usb_audio,
                                                   USB_AUDIO_DEVICE_TYPE_VOICE_MIC);
        if (streaming_info)
        {
            usb_audio->headset->mic_sink = StreamUsbEndPointSink(streaming_info->ep_address);
        }

        connect_params->spkr_src = usb_audio->headset->spkr_source;
        connect_params->mic_sink = usb_audio->headset->mic_sink;

        connect_params->volume =
            Volume_CalculateOutputVolume(VoiceSources_GetVolume(usb_audio->headset->audio_source));

        DEBUG_LOG("USB Voice: sample_rate %x", connect_params->sample_rate);

        usb_audio->headset->sample_rate = connect_params->sample_rate;

        connect_params->mode = usbAudio_GetVoiceBand(connect_params->sample_rate);

        /* Update TTP values for Voice chain */
        connect_params->max_latency_ms = usb_audio->headset->config->max_latency_ms;
        connect_params->min_latency_ms = usb_audio->headset->config->min_latency_ms;
        connect_params->target_latency_ms = usb_audio->headset->config->target_latency_ms;

        /* Audio source has read data for Audio chain creation, it will not
         * do again until it is informed. Lets keep this status and if
         * Host change any of above parameter then need to inform
         * Audio source.
         */
        usb_audio->headset->chain_active = TRUE;


        source_params->data = (void *)connect_params;
        source_params->data_length = sizeof(usb_voice_connect_parameters_t);
        ret = TRUE;
    }

    return ret;
}

static void usbAudio_FreeVoiceConnectParameters(voice_source_t source,
                                      source_defined_params_t * source_params)
{
    PanicNull(source_params);
    PanicFalse(source_params->data_length == sizeof(usb_voice_connect_parameters_t));
    if(source == voice_source_usb && source_params->data_length)
    {
        free(source_params->data);
        source_params->data = (void *)NULL;
        source_params->data_length = 0;
    }
}

static bool usbAudio_GetVoiceDisconnectParameters(voice_source_t source,
                                      source_defined_params_t * source_params)
{
    bool ret = FALSE;
    usb_audio_info_t *usb_audio = UsbAudioFd_GetHeadsetInfo(source);

    if(usbAudio_IsVoiceAvailable(source))
    {
        usb_voice_disconnect_parameters_t *disconnect_params;
        PanicNull(source_params);
        disconnect_params = (usb_voice_disconnect_parameters_t *)PanicNull(malloc(sizeof(usb_voice_disconnect_parameters_t)));

        disconnect_params->spkr_src = usb_audio->headset->spkr_source;
        disconnect_params->mic_sink = usb_audio->headset->mic_sink;


        usb_audio->headset->chain_active = FALSE;

        source_params->data = (void *)disconnect_params;
        source_params->data_length = sizeof(usb_voice_disconnect_parameters_t);
        ret = TRUE;
    }

    return ret;
}

static void usbAudio_FreeVoiceDisconnectParameters(voice_source_t source,
                                       source_defined_params_t * source_params)
{
    PanicNull(source_params);
    usb_audio_info_t *usb_audio = UsbAudioFd_GetHeadsetInfo(source);
    PanicFalse(source_params->data_length == sizeof(usb_voice_disconnect_parameters_t));

    if(source == voice_source_usb && source_params->data_length)
    {
        free(source_params->data);
        source_params->data = (void *)NULL;
        source_params->data_length = 0;
    }

    if(usb_audio != NULL && usb_audio->headset != NULL)
    {
        usb_audio->headset->spkr_active = FALSE;
        usb_audio->headset->mic_active = FALSE;
        usb_audio->headset->spkr_source = NULL;
        usb_audio->headset->mic_sink = NULL;
    }
}

static bool usbAudio_IsVoiceAvailable(voice_source_t source)
{
    bool is_available = FALSE;
    usb_audio_info_t *usb_audio = UsbAudioFd_GetHeadsetInfo(source);

    if (usb_audio)
    {
        DEBUG_LOG("USB Audio: Voice mic and speaker status %x, %x", usb_audio->headset->mic_active, usb_audio->headset->spkr_active);

        if(source == voice_source_usb && usb_audio->headset != NULL &&
                usb_audio->headset->spkr_active)
        {
            is_available = TRUE;
        }
    }

    return is_available;
}

const voice_source_audio_interface_t * UsbAudioFd_GetSourceVoiceInterface(void)
{
    return &usb_voice_interface;
}
