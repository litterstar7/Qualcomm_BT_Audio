/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for USB HID datalink class driver
*/

#ifndef USB_HID_DATALINK_H_
#define USB_HID_DATALINK_H_

#include <usb_device.h>

extern const usb_class_interface_cb_t UsbHid_Datalink_Callbacks;

void UsbHid_Datalink_RegisterSetReportHandler(usb_hid_handler_t handler);
void UsbHid_Datalink_UnregisterSetReportHandler(usb_hid_handler_t handler);

usb_result_t UsbHid_Datalink_SendReport(usb_class_context_t context,
                                        uint8 report_id,
                                        const uint8 * data, uint16 size);

usb_class_context_t UsbHid_Datalink_GetContext(usb_device_index_t dev_index);

#endif /* USB_HID_DATALINK_H_ */
