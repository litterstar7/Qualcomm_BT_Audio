/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      USB HID: generic code
*/

#include "usb_hid.h"
#include "usb_hid_common.h"

#include <sink.h>

usb_hid_class_data_t *hid_class_data;

usb_hid_class_data_t *UsbHid_GetClassData(Source class_source)
{
    usb_hid_class_data_t *ptr = hid_class_data;
    while (ptr)
    {
        if (class_source == ptr->class_source)
        {
            break;
        }
        ptr = ptr->next;
    }
    return ptr;
}
