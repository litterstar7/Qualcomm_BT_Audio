/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for USB HID common code
*/

#ifndef USB_HID_COMMON_H_
#define USB_HID_COMMON_H_

typedef struct usb_hid_handler_list_t
{
    usb_hid_handler_t handler;
    struct usb_hid_handler_list_t *next;
} usb_hid_handler_list_t;

typedef struct usb_hid_class_data_t
{
    usb_device_index_t dev_index;
    UsbInterface intf;

    Sink class_sink;
    Source class_source;
    Sink ep_sink;

    uint8 idle_rate;

    struct usb_hid_class_data_t *next;
} usb_hid_class_data_t;

extern usb_hid_class_data_t *hid_class_data;

usb_hid_class_data_t *UsbHid_GetClassData(Source class_source);

#endif /* USB_HID_COMMON_H_ */
