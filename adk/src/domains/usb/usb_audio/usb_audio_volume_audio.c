/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   usb_audio
\brief      The audio source volume interface implementation for USB Audio
*/

#include "usb_audio_fd.h"
#include "audio_sources_list.h"
#include "volume_types.h"

/* amount of dB attenuation USB will report for volume level */
#define USB_AUDIO_VOLUME_CONFIG   { .range = { .min = USB_AUDIO_VOLUME_MIN_STEPS, .max = USB_AUDIO_VOLUME_MAX_STEPS }, .number_of_steps = USB_AUDIO_NUM_VOL_SETTINGS }
#define USB_AUDIO_VOLUME(step)    { .config = USB_AUDIO_VOLUME_CONFIG, .value = step }

static volume_t usbAudio_GetVolume(audio_source_t source);
static void usbAudio_SetVolume(audio_source_t source, volume_t volume);

static const audio_source_volume_interface_t usb_audio_volume_interface =
{
    .GetVolume = usbAudio_GetVolume,
    .SetVolume = usbAudio_SetVolume
};

static volume_t usbAudio_GetVolume(audio_source_t source)
{
    volume_t volume = USB_AUDIO_VOLUME(USB_AUDIO_VOLUME_MIN_STEPS);
    usb_audio_info_t *usb_audio = UsbAudioFd_GetHeadphoneInfo(source);

    if(source == audio_source_usb && usb_audio != NULL &&
                                     usb_audio->headphone->spkr_active)
    {
        volume.value = usb_audio->headphone->spkr_volume;
    }

    return volume;
}

static void usbAudio_SetVolume(audio_source_t source, volume_t volume)
{
    usb_audio_info_t *usb_audio = UsbAudioFd_GetHeadphoneInfo(source);

    if(source == audio_source_usb && usb_audio != NULL &&
                                     usb_audio->headphone->spkr_active)
    {
        DEBUG_LOG("USB Audio: Headphone Spkr=%ddB", volume.value);
        usb_audio->headphone->spkr_volume = volume.value;
    }
}

const audio_source_volume_interface_t * UsbAudioFd_GetAudioSourceVolumeInterface(void)
{
    return &usb_audio_volume_interface;
}

