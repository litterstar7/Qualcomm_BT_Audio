/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for USB HID Consumer Transport Control class driver
*/

#ifndef USB_HID_CONSUMER_TRANSPORT_CONTROL_H_
#define USB_HID_CONSUMER_TRANSPORT_CONTROL_H_

#include <usb_device.h>

extern const usb_class_interface_cb_t UsbHid_ConsumerTransport_Callbacks;

typedef enum
{
    /*! Send a HID consumer play/pause event over USB */
    USB_HID_CONSUMER_TRANSPORT_PLAY_PAUSE,
    /*! Send a HID consumer stop event over USB */
    USB_HID_CONSUMER_TRANSPORT_STOP,
    /*! Send a HID consumer next track event over USB */
    USB_HID_CONSUMER_TRANSPORT_NEXT_TRACK,
    /*! Send a HID consumer previous track event over USB */
    USB_HID_CONSUMER_TRANSPORT_PREVIOUS_TRACK,
    /*! Send a HID consumer play event over USB */
    USB_HID_CONSUMER_TRANSPORT_PLAY,
    /*! Send a HID consumer pause event over USB */
    USB_HID_CONSUMER_TRANSPORT_PAUSE,
    /*! Send a HID consumer Fast Forward ON event over USB */
    USB_HID_CONSUMER_TRANSPORT_FFWD_ON,
    /*! Send a HID consumer Fast Forward OFF event over USB */
    USB_HID_CONSUMER_TRANSPORT_FFWD_OFF,
    /*! Send a HID consumer Rewind ON event over USB */
    USB_HID_CONSUMER_TRANSPORT_REW_ON,
    /*! Send a HID consumer Rewind OFF event over USB */
    USB_HID_CONSUMER_TRANSPORT_REW_OFF,
    /*! Send a HID consumer Volume Up event over USB */
    USB_HID_CONSUMER_TRANSPORT_VOL_UP,
    /*! Send a HID consumer Volume Down event over USB */
    USB_HID_CONSUMER_TRANSPORT_VOL_DOWN,
    /*! Send a HID consumer Mute event over USB */
    USB_HID_CONSUMER_TRANSPORT_MUTE
} usb_hid_consumer_transport_event;

usb_result_t UsbHid_ConsumerTransporControl_SendEvent(usb_class_context_t context,
                                                      usb_hid_consumer_transport_event event);
usb_class_context_t UsbHid_ConsumerTransporControl_GetContext(usb_device_index_t dev_index);

#endif /* USB_HID_CONSUMER_TRANSPORT_CONTROL_H_ */
