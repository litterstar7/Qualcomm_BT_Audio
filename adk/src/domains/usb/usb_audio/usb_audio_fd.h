/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Private header file for USB Audio Function driver.
*/

#ifndef USB_AUDIO_FD_H
#define USB_AUDIO_FD_H

#include "logging.h"

#include "volume_system.h"

#include <logging.h>
#include <panic.h>

#include <stdlib.h>
#include <stream.h>
#include <sink.h>
#include <source.h>
#include <string.h>
#include <usb.h>

#include "audio_sources_volume_interface.h"
#include "audio_sources_media_control_interface.h"

#include "usb_audio_class.h"

#define USB_AUDIO_CHANNELS_SPEAKER_STEREO   USB_AUDIO_CHANNELS_STEREO

#define USB_AUDIO_NUM_VOL_SETTINGS    ((USB_AUDIO_VOLUME_MAX_STEPS - USB_AUDIO_VOLUME_MIN_STEPS) + 1)
#define USB_AUDIO_DB_TO_STEPS(x)    ((USB_AUDIO_NUM_VOL_SETTINGS * (USB_AUDIO_MIN_VOLUME_DB + x))/USB_AUDIO_MIN_VOLUME_DB)


typedef struct
{
    bool                        mic_active:1;
    bool                        spkr_active:1;
    bool                        chain_active:1;
    int16                       mic_volume;
    int16                       spkr_volume;
    voice_source_t              audio_source;
    Source                      spkr_source;
    Sink                        mic_sink;
    uint32                      sample_rate;

    const usb_audio_config_params_t *config;
    usb_audio_streaming_info_t  *streaming_info;
    void                        *class_ctx;
} usb_audio_headset_info_t;


typedef struct
{
    bool                        spkr_active:1;
    bool                        chain_active:1;
    int16                       spkr_volume;
    audio_source_t              audio_source;
    Source                      spkr_src;
    uint32                      spkr_sample_rate;
    audio_source_provider_context_t audio_ctx;

    const usb_audio_config_params_t *config;
    usb_audio_streaming_info_t  *streaming_info;
    void                        *class_ctx;
} usb_audio_headphone_info_t;

typedef struct usb_audio_info_t
{

    uint8                           device_index;
    uint8                           num_interfaces;
    usb_audio_headset_info_t        *headset;
    usb_audio_headphone_info_t      *headphone;
    const usb_fn_tbl_uac_if         *usb_fn_uac;
    struct usb_audio_info_t         *next;
} usb_audio_info_t;

/*! \brief Get USB Audio Info for headphone device

    \return USB Audio Info.
 */
usb_audio_info_t *UsbAudioFd_GetHeadphoneInfo(audio_source_t source);

usb_audio_streaming_info_t *UsbAudio_GetStreamingInfo(usb_audio_info_t *usb_audio,
                                                      usb_audio_device_type_t type);

/*! \brief Get USB Audio Info for headset device

    \return USB Audio Info.
 */

usb_audio_info_t *UsbAudioFd_GetHeadsetInfo(audio_source_t source);

/*! \brief Gets the USB Audio volume interface.

    \return The audio source volume interface for an USB Audio
 */
const audio_source_volume_interface_t * UsbAudioFd_GetAudioSourceVolumeInterface(void);

/*! \brief Get USB Audio source interface for registration.

    \return USB Audio source interface.
 */
const audio_source_audio_interface_t * UsbAudioFd_GetSourceAudioInterface(void);

/*! \brief Get USB Audio media control interface for registration

    \return USB Audio media control interface.
 */
const media_control_interface_t * UsbAudioFd_GetMediaControlAudioInterface(void);

/*! \brief Get USB Voice source interface for registration.

    \return USB Voice source interface.
 */
const voice_source_audio_interface_t * UsbAudioFd_GetSourceVoiceInterface(void);

/*! \brief Gets the USB Voice volume interface.

    \return The voice source volume interface for an USB Voice
 */
const voice_source_volume_interface_t * UsbAudioFd_GetVoiceSourceVolumeInterface(void);

/*! \brief Get USB new volume gain in steps.
    \param audio context of USB audio levels.

    \return USB Volume in steps.
 */
int16 UsbAudio_GetAudioVolumeGain(uac_audio_levels_t *audio);

/*! \brief Get USB new volume gain in steps.
    \param voice context of USB voice levels.

    \return USB Volume in steps.
 */
int16 UsbAudio_GetVoiceVolumeGain(uac_voice_levels_t *voice);

/*! \brief Get USB Audio context

    \return USB Audio context
 */
unsigned UsbAudioFd_GetAudioContext(audio_source_t source);

#endif // USB_AUDIO_FD_H
