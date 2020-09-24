/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Default USB Audio application - enumerates only HID datalink class.
*/

#include "usb_app_default.h"

/* USB trap API */
#include <usb_device.h>
#include <usb_hub.h>

/* device classes */
#include <usb_hid.h>

#include "logging.h"

#include <panic.h>
#include <stdlib.h>

static const uint16 serial_number_string[] =
{ 0x0044, /* D */
  0x0065, /* e */
  0x0066, /* f */
  0x0061, /* a */
  0x0075, /* u */
  0x006c, /* l */
  0x0074, /* t */
  0x0000
};

static void usbAppDefault_ConfigDevice(usb_device_index_t dev_index)
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

static const usb_class_interface_t datalink_if =
{
    .cb = &UsbHid_Datalink_Callbacks,
    .config_data = 0
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


static void usbAppDefault_Create(usb_device_index_t dev_index)
{
    usb_result_t result;

    DEBUG_LOG_INFO("UsbAppDefault: Create");

    /* Configuration callback is called to configure device parameters, like
     * VID, PID, serial number, etc right before device is attached to
     * the hub. */
    result = UsbDevice_RegisterConfig(dev_index,
                                      usbAppDefault_ConfigDevice);
    assert(result == USB_RESULT_OK);

    /* Register required USB classes with the framework */

    /* HID Datalink class interface */
    result = UsbDevice_RegisterClass(dev_index,
                                     &datalink_if);
    assert(result == USB_RESULT_OK);

    /* **********************************************************************
     * Register class interfaces above by calling UsbDevice_RegisterClass().
     *
     * USB device framework preserves the order of interfaces so those
     * registered earlier get lower interface numbers.
     *
     * Class interface structures are normally defined as static consts
     * before the usbAppDefault_Create() code.
     * **********************************************************************/
}

static void usbAppDefault_Attach(usb_device_index_t dev_index)
{
    usb_result_t result;

    /* Attach device to the hub to make it visible to the host */
    result = UsbDevice_Attach(dev_index);
    assert(result == USB_RESULT_OK);
}

static void usbAppDefault_Detach(usb_device_index_t dev_index)
{
    usb_result_t result;

    result = UsbDevice_Detach(dev_index);
    assert(result == USB_RESULT_OK);
}

static void usbAppDefault_Close(usb_device_index_t dev_index)
{
    UNUSED(dev_index);

    DEBUG_LOG_INFO("UsbAppDefault: Close");

    /* nothing to clear */
}

const usb_app_interface_t usb_app_default =
{
    .Create = usbAppDefault_Create,
    .Attach = usbAppDefault_Attach,
    .Detach = usbAppDefault_Detach,
    .Destroy = usbAppDefault_Close
};
