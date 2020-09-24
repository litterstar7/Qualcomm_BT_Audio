/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Private interface file between USB Audio function driver and class driver.
*/

#ifndef USB_AUDIO_CLASS_H
#define USB_AUDIO_CLASS_H

#include "usb_audio.h"

/* USB Audio message comes from class driver */
typedef enum
{
    /* AUDIO_LEVELS message indicates value specified in usb_audio_class_levels_t*/
    USB_AUDIO_CLASS_MSG_LEVELS,
    USB_AUDIO_CLASS_MSG_SAMPLE_RATE,
    /* message limit */
    USB_AUDIO_MSG_TOP
} uac_message_t;

typedef struct
{
    /*! Left and right gain settings */
    int16     out_l_vol;
    int16     out_r_vol;
    /*! mic mute settings (TRUE/FALSE) */
    bool      out_mute;
} uac_audio_levels_t;

typedef struct
{
    /*! Left and right gain settings */
    int16     out_l_vol;
    int16     out_r_vol;
    /*! Input Gain setting */
    int16     in_vol;
    /*! Speaker/mic mute settings (TRUE/FALSE) */
    bool      out_mute;
    bool      in_mute;
} uac_voice_levels_t;

/*! Structure to hold voulme status. */
typedef struct
{
    /*! Left channel volume gain in dB */
    int16     l_volume;
    /*! Right channel volume gain in dB */
    int16     r_volume;
    /*! Mute status of master channel */
    bool      mute_status;
} uac_volume_status_t;

/*! Structure to hold information regarding streaming interface. */
typedef struct uac_streaming_info_t
{
    Source                source;
    UsbInterface          interface;
    uint8                 ep_address;
    uint8                 feature_unit_id;
    uint32                current_sampling_rate;
    uac_volume_status_t   volume_status;
} usb_audio_streaming_info_t;


/*!
    Structure to hold sampling rate information
*/
typedef struct
{
    uint32 sample_rate;
} usb_audio_sample_rate_t;

/*! Opaque USB audio class context data */
typedef void * uac_ctx_t;

/*!
    USB audio event callback
    
    \param device_index USB device index
    \param id message ID
    \param message message payload, is owned by the caller and is not valid after the handler returns
*/
typedef  void (*uac_event_handler_t)(uac_ctx_t class_ctx,
                                     uint8 interface_index,
                                     uac_message_t uac_message);

/*!
    Below interfaces are implemented in usb_audio_class driver and will
    provide its instance using UsbAudioClassXX_GetFnTbl() api where XX is
    version.
*/
typedef struct
{
    uac_ctx_t (*Create)(usb_device_index_t device_index,
                        const usb_audio_config_params_t *config,
                        usb_audio_streaming_info_t *streaming_info,
                        uac_event_handler_t evt_handler);
    bool (*Delete)(uac_ctx_t class_ctx);
} usb_fn_tbl_uac_if;


#endif /* USB_AUDIO_CLASS_H */
