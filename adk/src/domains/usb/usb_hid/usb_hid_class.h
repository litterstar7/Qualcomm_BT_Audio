/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Class specific definitions for USB HID
*/

#ifndef USB_HID_CLASS_H_
#define USB_HID_CLASS_H_

#define B_INTERFACE_CLASS_HID 0x03
#define B_INTERFACE_SUB_CLASS_HID_NO_BOOT 0x00
#define B_INTERFACE_PROTOCOL_HID_NO_BOOT 0x00

#define HID_DESCRIPTOR_LENGTH 9
#define B_DESCRIPTOR_TYPE_HID 0x21
#define B_DESCRIPTOR_TYPE_HID_REPORT 0x22

#define USB_REPORT_TYPE_INPUT (1 << 8)

/** HID 1.11 spec, 7.2 Class-Specific Requests */
typedef enum {
    HID_GET_REPORT = 0x01,
    HID_GET_IDLE = 0x02,
    HID_GET_PROTOCOL = 0x03,
    HID_SET_REPORT = 0x09,
    HID_SET_IDLE = 0x0A,
    HID_SET_PROTOCOL = 0x0B
} b_request_hid_t;

#endif /* USB_HID_CLASS_H_ */
