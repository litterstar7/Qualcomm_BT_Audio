/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for USB HID support
*/

#ifndef USB_HID_H_
#define USB_HID_H_

#include <usb_device.h>
#include <usb_device_utils.h>

typedef void (*usb_hid_handler_t)(usb_device_index_t dev_index,
                                  uint8 report_id, const uint8 *data, uint16 size);

#include "usb_hid_consumer_transport_control.h"
#include "usb_hid_datalink.h"

#endif /* USB_HID_H_ */
