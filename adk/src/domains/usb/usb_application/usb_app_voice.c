/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      USB Voice application - enumerates HID consumer transport,
            HID datalink and USB Audio (for voice calls) classes.
*/

#include "usb_app_voice.h"

/* USB trap API */
#include <usb_device.h>
#include <usb_hub.h>

/* device classes */
#include <usb_hid.h>
#include <usb_audio.h>
#include <usb_audio_class_10_descriptors.h>

#include "logging.h"

#include <panic.h>
#include <stdlib.h>

static const uint16 serial_number_string[] =
{ 0x0048, /* H */
  0x0065, /* e */
  0x0061, /* a */
  0x0064, /* d */
  0x0073, /* s */
  0x0065, /* e */
  0x0074, /* t */
  0x0000
};

const usb_audio_config_params_t usb_voice_config =
{
    .rev                     = USB_AUDIO_CLASS_REV_1,
    .volume_config.min_db    = -45,
    .volume_config.max_db    = 0,
    .volume_config.target_db = -10,
    .volume_config.res_db    = 3,
    .min_latency_ms          = 10,
    .max_latency_ms          = 40,
    .target_latency_ms       = 30,

    .intf_list = &uac1_voice_interfaces
};

static void usbAppVoice_ConfigDevice(usb_device_index_t dev_index)
{
    usb_result_t usb_result;
    bool result;
    uint8 i_string;

    /* Set USB PID */
    result = UsbHubConfigKey(USB_DEVICE_CFG_PRODUCT_ID, 0x4007);
    assert(result);

    /* Set serial number, in two steps:
     * 1. add a string descriptor with the serial number;
     * 2. configure hub to use the string index returned for serial number */
    usb_result = UsbDevice_AddStringDescriptor(dev_index,
                                           serial_number_string,
                                           &i_string);
    assert(usb_result == USB_RESULT_OK);

    result = UsbHubConfigKey(USB_DEVICE_CFG_SERIAL_NUMBER_STRING, i_string);
    assert(result);
}


static const usb_class_interface_t consumer_transport_if =
{
    .cb = &UsbHid_ConsumerTransport_Callbacks,
    .config_data = 0
};

static const usb_class_interface_t datalink_if =
{
    .cb = &UsbHid_Datalink_Callbacks,
    .config_data = 0
};

static const usb_class_interface_t usb_voice =
{
    .cb = &UsbAudio_Callbacks,
    .config_data = (usb_class_interface_config_data_t)&usb_voice_config
};

/* ****************************************************************************
 * Declare class interface structures above, these can be passed into
 * UsbDevice_RegisterClass() to add class interfaces to USB device.
 *
 * Class interface callbacks are mandatory, they are provided by a class driver
 * and declared in its public header.
 *
 * Context data is optional and can be either provided by a class driver
 * or defined here. Context data format is specific to the class driver and
 * is opaque to the USB device framework.
 * ****************************************************************************/


static void usbAppVoice_Create(usb_device_index_t dev_index)
{
    usb_result_t result;

    DEBUG_LOG_INFO("UsbAppVoice: Create");

    /* Configuration callback is called to configure device parameters, like
     * VID, PID, serial number, etc right before device is attached to
     * the hub. */
    result = UsbDevice_RegisterConfig(dev_index,
                                      usbAppVoice_ConfigDevice);
    assert(result == USB_RESULT_OK);

    /* Register required USB classes with the framework */

    /* HID Consumer Transport class interface */
    result = UsbDevice_RegisterClass(dev_index,
                                     &consumer_transport_if);
    assert(result == USB_RESULT_OK);

    /* HID Datalink class interface */
    result = UsbDevice_RegisterClass(dev_index,
                                     &datalink_if);
    assert(result == USB_RESULT_OK);

    result = UsbDevice_RegisterClass(dev_index,
                                     &usb_voice);
    assert(result == USB_RESULT_OK);

    /* **********************************************************************
     * Register class interfaces above by calling UsbDevice_RegisterClass().
     *
     * USB device framework preserves the order of interfaces so those
     * registered earlier get lower interface numbers.
     *
     * Class interface structures are normally defined as static consts
     * before the usbAppVoice_Create() code.
     * **********************************************************************/
}

static void usbAppVoice_Attach(usb_device_index_t dev_index)
{
    usb_result_t result;

    /* Attach device to the hub to make it visible to the host */
    result = UsbDevice_Attach(dev_index);
    assert(result == USB_RESULT_OK);
}

static void usbAppVoice_Detach(usb_device_index_t dev_index)
{
    usb_result_t result;

    result = UsbDevice_Detach(dev_index);
    assert(result == USB_RESULT_OK);
}

static void usbAppVoice_Close(usb_device_index_t dev_index)
{
    UNUSED(dev_index);

    DEBUG_LOG_INFO("UsbAppVoice: Close");

    /* nothing to clear */
}

const usb_app_interface_t usb_app_voice =
{
    .Create = usbAppVoice_Create,
    .Attach = usbAppVoice_Attach,
    .Detach = usbAppVoice_Detach,
    .Destroy = usbAppVoice_Close
};
