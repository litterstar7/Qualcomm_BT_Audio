/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file   usb_audio_media_control_audio.c
\brief  Driver for USB Audio media control interfaces implementation.
*/

#include "kymera_adaptation_audio_protected.h"
#include "usb_audio_fd.h"

static unsigned usbAudio_GetContext(audio_source_t source);

static const media_control_interface_t usb_audio_media_control_interface =
{
    .Play = NULL,
    .Pause = NULL,
    .PlayPause = NULL,
    .Stop = NULL,
    .Forward = NULL,
    .Back = NULL,
    .FastForward = NULL,
    .FastRewind = NULL,
    .NextGroup = NULL,
    .PreviousGroup = NULL,
    .Shuffle = NULL,
    .Repeat = NULL,
    .Context = usbAudio_GetContext,
    .Device = NULL
};

static unsigned usbAudio_GetContext(audio_source_t source)
{
    /* Get the Usb audio context */
    return UsbAudioFd_GetAudioContext(source);
}

const media_control_interface_t * UsbAudioFd_GetMediaControlAudioInterface(void)
{
    return &usb_audio_media_control_interface;
}


