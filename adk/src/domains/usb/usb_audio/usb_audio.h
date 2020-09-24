/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for USB Audio support
*/

#ifndef USB_AUDIO_H_
#define USB_AUDIO_H_

#include <message.h>
#include <library.h>
#include <stream.h>

#include "chain.h"
#include "usb_device.h"
#include "volume_types.h"
#include "audio_sources.h"
#include "voice_sources.h"

#include "domain_message.h"

/*!
    USB Audio Class revision. This is passed in USB Audio create API.
    Note: At present only USB Audio Class Specification 1.0 is supported.
*/
typedef enum 
{
    USB_AUDIO_CLASS_REV_1    = 0x1
} usb_audio_class_rev_t;

/*!
    Supported USB Audio classes. These values can be passed into the
    UsbAudio_Create API.
*/
typedef enum
{
    /*! USB audio mic device class */
    USB_AUDIO_DEVICE_TYPE_VOICE_MIC        = 0x10,
    /*! USB audio speaker device class */
    USB_AUDIO_DEVICE_TYPE_VOICE_SPEAKER    = 0x20,
    /*! USB audio speaker and mic device class */
    USB_AUDIO_DEVICE_TYPE_AUDIO_SPEAKER    = 0x40
} usb_audio_device_type_t;

/*! Bitmask with all supported audio device types */
#define USB_AUDIO_SUPPORTED_DEVICE_TYPES (USB_AUDIO_DEVICE_TYPE_VOICE_MIC | \
                                          USB_AUDIO_DEVICE_TYPE_VOICE_SPEAKER | \
                                          USB_AUDIO_DEVICE_TYPE_AUDIO_SPEAKER)

/*! Supported USB Audio clients */
typedef enum
{
    /*! Client for music playback */
    USB_AUDIO_REGISTERED_CLIENT_MEDIA,
    /*! Client for voice calls */
    USB_AUDIO_REGISTERED_CLIENT_TELEPHONY,
    /*! Number of clients */
    USB_AUDIO_REGISTERED_CLIENT_COUNT
} usb_audio_registered_client_t;

/*! \brief Message IDs from USB AUDIO to registered status clients  */
typedef enum
{
    USB_AUDIO_DISCONNECTED_IND = USB_AUDIO_MESSAGE_BASE,
    USB_AUDIO_CONNECTED_IND,
} usb_audio_status_message_t;

typedef enum
{
    usb_voice_no_mode,
    /*! 8 kHz */
    usb_voice_mode_nb,
    /*! 16 kHz */
    usb_voice_mode_wb,
    /*! 32 kHz - not supported */
    usb_voice_mode_uwb,
    /*! 64 kHz - not supported */
    usb_voice_mode_swb
} usb_voice_mode_t;

/*!
    Audio Gain Limits - We support analogue gain from -45dB to 0dB.
    Digital gain goes up to +21.5 but distorts audio so don't report it.
    Firmware settings for analogue gain are:
    0   1     2   3     4   5     6   7     8   9   10   11 12 12 14 15
    Which correspond to gains (in dB) of:
    -45 -41.5 -39 -35.5 -33 -29.5 -27 -23.5 -21 -18 -15 -12 -9 -6 -3 0
    We report resolution of 3dB so we're always within .5dB of the truth:
    -45 -42   -39 -36   -33 -30   -27 -24   -21 -18 -15 -12 -9 -6 -3 0
*/
#define USB_AUDIO_MIN_VOLUME_DB         45
#define USB_AUDIO_VOLUME_MIN_STEPS      0
#define USB_AUDIO_VOLUME_MAX_STEPS      15

typedef struct
{
    /* Value in DB */
    int min_db;
    int max_db;
    int target_db;
    uint16 res_db;
} usb_audio_volume_config_t;

/*! AudioStreaming interface descriptor configuration */
typedef struct
{
    const uint8*        descriptor;
    uint16              size_descriptor;
} uac_streaming_config_t;

/*! AudioControl interface descriptor configuration */
typedef struct
{
    const uint8*        descriptor;
    uint16              size_descriptor;
} uac_control_config_t;

/*! Streaming endpoint descriptor configuration */
typedef struct
{
    /*! Direction - "1": to_host or "0": from_host */
    uint8 is_to_host;

    /*! Maximum packet size in bytes or "0" to calculate value automatically
     * from the class descriptor */
    uint16 wMaxPacketSize;

    /*! Polling interval */
    uint8 bInterval;
} uac_endpoint_config_t;

/*! Audio interface configuration */
typedef struct
{
    /*! Audio interface type */
    usb_audio_device_type_t type;

    /*! Descriptors to be appended to the Class-Specific AudioControl
     * interface descriptors */
    const uac_control_config_t   *control_desc;

    /*! Class-Specific AudioStreaming interface descriptors */
    const uac_streaming_config_t *streaming_desc;

    /*! Configuration for AudioStreaming Endpoint descriptor */
    const uac_endpoint_config_t  *endpoint;
} usb_audio_interface_config_t;

/*! Configuration for one or more Audio interfaces */
typedef struct
{
    /*! Array with interface configuration */
    const usb_audio_interface_config_t *intf;
    /*! Number of interfaces in the array */
    uint8 num_interfaces;
} usb_audio_interface_config_list_t;

/*! Audio function configuration parameters */
typedef struct
{
    usb_audio_class_rev_t      rev;
    usb_audio_volume_config_t  volume_config;
    uint32                     min_latency_ms;
    uint32                     max_latency_ms;
    uint32                     target_latency_ms;

    const usb_audio_interface_config_list_t *intf_list;
} usb_audio_config_params_t;

typedef struct
{
    audio_source_t audio_source;
} USB_AUDIO_CONNECT_MESSAGE_T;

typedef struct
{
    audio_source_t audio_source;
} USB_AUDIO_DISCONNECT_MESSAGE_T;

extern const usb_class_interface_cb_t UsbAudio_Callbacks;

/*! \brief Register a task to receive USB Audio messages

    \param  client_task to be added to the list of registered clients
    \param name Name of the regsitered client in usb_audio_registered_client_t
 */
void UsbAudio_ClientRegister(Task client_task,
                              usb_audio_registered_client_t name);

/*! \brief Unregister a task to receive USB Audio messages

    \param  client_task Task to be removed to the list of registered clients
    \param name Name of the regsitered client in usb_audio_registered_client_t
 */
void UsbAudio_ClientUnRegister(Task client_task,
                              usb_audio_registered_client_t name);

#endif /* USB_AUDIO_H_ */
