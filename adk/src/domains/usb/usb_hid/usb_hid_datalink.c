/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      USB HID: Datalink interface
*/

#include "usb_hid.h"
#include "usb_hid_class.h"
#include "usb_hid_common.h"

#include "logging.h"

#include <csrtypes.h>

#include <usb.h>
#include <message.h>

#include <panic.h>
#include <sink.h>
#include <source.h>
#include <string.h>
#include <stream.h>
#include <stdlib.h>

#define HID_REPORTID_DATA_TRANSFER          1   /* data from host for AHI */
#define HID_REPORTID_DATA_TRANSFER_SIZE     62

#define HID_REPORTID_RESPONSE               2   /* device response for AHI */
#define HID_REPORTID_RESPONSE_SIZE          16

#define HID_REPORTID_COMMAND                3   /* command channel */
#define HID_REPORTID_COMMAND_SIZE          62

#define HID_REPORTID_CONTROL                4   /* control channel dedicated to HID library */
#define HID_REPORTID_CONTROL_SIZE          62

#define HID_REPORTID_UPGRADE_DATA_TRANSFER  5   /* data from host for Upgrade */
#define HID_REPORTID_UPGRADE_DATA_TRANSFER_SIZE 512

#define HID_REPORTID_UPGRADE_RESPONSE       6   /* device response for Upgrade */
#define HID_REPORTID_UPGRADE_RESPONSE_SIZE  12

typedef struct
{
    uint8 report_id;
    uint16 report_size;
} hid_datalink_report_info_t;

const hid_datalink_report_info_t hid_datalink_reports[] =
{
        {HID_REPORTID_DATA_TRANSFER,         HID_REPORTID_DATA_TRANSFER_SIZE},
        {HID_REPORTID_RESPONSE,              HID_REPORTID_RESPONSE_SIZE},
        {HID_REPORTID_COMMAND,               HID_REPORTID_COMMAND_SIZE},
        {HID_REPORTID_CONTROL,               HID_REPORTID_CONTROL_SIZE},
        {HID_REPORTID_UPGRADE_DATA_TRANSFER, HID_REPORTID_UPGRADE_DATA_TRANSFER_SIZE},
        {HID_REPORTID_UPGRADE_RESPONSE,      HID_REPORTID_UPGRADE_RESPONSE_SIZE}
};

TaskData hid_datalink_task;
usb_hid_handler_list_t *hid_datalink_handlers;

static const uint8 report_descriptor_hid_datalink[] =
{
    0x06, 0x00, 0xFF,                   /* Vendor Defined Usage Page 1 */

    0x09, 0x01,                         /* Vendor Usage 1*/
    0xA1, 0x01,                         /* Collection */
    0x15, 0x00,                         /* Logical Minimum */
    0x26, 0xFF, 0x00,                   /* Logical Maximum */
    0x75, 0x08,                         /* Report size (8 bits) */

    0x09, 0x02,                         /* Vendor Usage 2 */
    0x96,                               /* Report count */
    (HID_REPORTID_DATA_TRANSFER_SIZE&0xff),
    (HID_REPORTID_DATA_TRANSFER_SIZE>>8),
    0x85, HID_REPORTID_DATA_TRANSFER,   /* Report ID */
    0x91, 0x02,                         /* OUTPUT Report */

    0x09, 0x02,                         /* Vendor Usage 2 */
    0x96,                               /* Report count */
    (HID_REPORTID_UPGRADE_DATA_TRANSFER_SIZE&0xff),
    (HID_REPORTID_UPGRADE_DATA_TRANSFER_SIZE>>8),
    0x85, HID_REPORTID_UPGRADE_DATA_TRANSFER, /* Report ID */
    0x91, 0x02,                         /* OUTPUT Report */

    0x09, 0x02,                         /* Vendor Usage 2 */
    0x95,                               /* Report count */
    (HID_REPORTID_RESPONSE_SIZE&0xff),
    0x85, HID_REPORTID_RESPONSE,        /* Report ID */
    0x81, 0x02,                         /* INPUT (Data,Var,Abs) */

    0x09, 0x02,                         /* Vendor Usage 2 */
    0x95,                               /* Report count */
    (HID_REPORTID_UPGRADE_RESPONSE_SIZE&0xff),
    0x85, HID_REPORTID_UPGRADE_RESPONSE,/* Report ID */
    0x81, 0x02,                         /* INPUT (Data,Var,Abs) */

    0x09, 0x02,                         /* Vendor Usage 2 */
    0x95,                               /* Report count */
    (HID_REPORTID_COMMAND_SIZE&0xff),
    0x85, HID_REPORTID_COMMAND,         /* Report ID */
    0xB1, 0x02,                         /* Feature Report */

    0x09, 0x02,                         /* Vendor Usage 2 */
    0x95,                               /* Report count */
    (HID_REPORTID_CONTROL_SIZE&0xff),
    0x85, HID_REPORTID_CONTROL,         /* Report ID */
    0xB1, 0x02,                         /* Feature Report */

    0xC0                                /* End of Collection */
};

/* See the USB HID 1.11 spec section 6.2.1 for description */
static const uint8 interface_descriptor_hid_datalink[] =
{
    HID_DESCRIPTOR_LENGTH,                  /* bLength */
    B_DESCRIPTOR_TYPE_HID,                  /* bDescriptorType */
    0x11, 0x01,                             /* HID class release number (1.00).
                                             * The 1st and the 2nd byte denotes
                                             * the minor & major Nos respectively
                                             */
    0x00,                                   /* Country code (None) */
    0x01,                                   /* Only one class descriptor to follow */
    B_DESCRIPTOR_TYPE_HID_REPORT,           /* Class descriptor type (HID Report) */
    sizeof(report_descriptor_hid_datalink), /* Report descriptor length. LSB first */
    0x00                                    /* followed by MSB */
};

static const hid_datalink_report_info_t *find_report_info(uint8 report_id)
{
    const hid_datalink_report_info_t *r = NULL;
    int i;

    for (i = 0; i < ARRAY_DIM(hid_datalink_reports); i++)
    {
        if (hid_datalink_reports[i].report_id == report_id)
        {
            r = &hid_datalink_reports[i];
            break;
        }
    }
    return r;
}

usb_result_t UsbHid_Datalink_SendReport(usb_class_context_t context,
                                        uint8 report_id,
                                        const uint8 * report_data, uint16 size)
{
    /* send data from client to USB */
    Sink sink;
    uint8 *out;
    const hid_datalink_report_info_t *r = find_report_info(report_id);
    uint16 report_data_size;
    usb_hid_class_data_t *data = hid_class_data;

    if (!r)
    {
        DEBUG_LOG_ERROR("UsbHid:DL SendReport - report %u not found",
                        report_id);
        Panic();
    }

    while (data)
    {
        if ((usb_class_context_t)data == context)
        {
            break;
        }
        data = data->next;
    }

    if (!data)
    {
        return USB_RESULT_NOT_FOUND;
    }

    sink = data->class_sink;

    out = SinkMapClaim(sink, r->report_size);

    if (!out)
    {
        DEBUG_LOG_ERROR("UsbHid:DL SendReport - cannot claim sink space\n");
        return USB_RESULT_NO_SPACE;
    }

    report_data_size = r->report_size - 2;

    if (!report_data)
    {
        size = 0;
    }
    else
    {
        size = MIN(size, report_data_size);
    }

    out[0] = report_id;
    out[1] = size;

    memcpy(&out[2], report_data, size);
    memset(&out[2 + size], 0, report_data_size - size);

    if(!SinkFlush(sink, r->report_size))
    {
        DEBUG_LOG_ERROR("UsbHid:DL SendReport - failed to send data\n");
        Panic();
    }

    return USB_RESULT_OK;
}

static void usbHid_DatalinkHandler(Task task, MessageId id, Message message)
{
    Source source;
    Sink sink;
    uint16 packet_size;
    usb_hid_class_data_t *data;

    UNUSED(task);

    if (id != MESSAGE_MORE_DATA)
    {
        return;
    }

    source = ((MessageMoreData *)message)->source;

    data = UsbHid_GetClassData(source);

    if (!data)
    {
        return;
    }

    sink = data->class_sink;

    while ((packet_size = SourceBoundary(source)) != 0)
    {
        UsbResponse resp;
        bool response_sent = FALSE;
        /* Build the response. It must contain the original request, so copy
           from the source header. */
        memcpy(&resp.original_request, SourceMapHeader(source), sizeof(UsbRequest));

        /* Set the response fields to default values to make the code below simpler */
        resp.success = FALSE;
        resp.data_length = 0;

        switch (resp.original_request.bRequest)
        {
            case HID_GET_REPORT:
                 DEBUG_LOG_INFO("UsbHid:DL Get_Report wValue=0x%X wIndex=0x%X wLength=0x%X",
                                resp.original_request.wValue,
                                resp.original_request.wIndex,
                                resp.original_request.wLength);
                break;

            case HID_GET_IDLE:
            {
                uint8 *out;
                if ((out = SinkMapClaim(sink, 1)) != 0)
                {
                     DEBUG_LOG_INFO("UsbHid:DL Get_Idle wValue=0x%X wIndex=0x%X",
                                    resp.original_request.wValue,
                                    resp.original_request.wIndex);
                    out[0] = data->idle_rate;
                    resp.success = TRUE;
                    resp.data_length = 1;
                }
                break;
            }

            case HID_SET_REPORT:
            {
                uint16 size_data = resp.original_request.wLength;
                uint8 report_id = resp.original_request.wValue & 0xff;
                DEBUG_LOG_INFO("UsbHid:DL Set_Report wValue=0x%X wIndex=0x%X wLength=0x%X",
                               resp.original_request.wValue,
                               resp.original_request.wIndex,
                               resp.original_request.wLength);

                resp.success = TRUE;

#if 0
                if (size_data)
                {
                    const uint8 *report_data;
                    usb_hid_handler_list_t *handler_list = hid_datalink_handlers;

                    /* That's a special case as we want to complete
                     * control transfer as soon as possible.
                     * Careful: need to make sure Source buffer is big enough
                     * for 2x largest requests. */
                    (void) SinkClaim(sink, 1);
                    (void) SinkFlushHeader(sink, 1,(uint16 *) &resp, sizeof(UsbResponse));
                    response_sent = TRUE;

                    report_data = SourceMap(source);
                    size_data = MIN(packet_size, size_data);

                    while (handler_list)
                    {
                        handler_list->handler(report_id, report_data, size_data);
                        handler_list = handler_list->next;
                    }
                }
#endif
                break;
            }

            case HID_SET_IDLE:
                 DEBUG_LOG_INFO("UsbHid:DL Set_Idle wValue=0x%X wIndex=0x%X",
                                resp.original_request.wValue,
                                resp.original_request.wIndex);
                data->idle_rate = (resp.original_request.wValue >> 8) & 0xff;
                resp.success = TRUE;
                break;

            default:
            {
                 DEBUG_LOG_ERROR("UsbHid:DL req=0x%X wValue=0x%X HID wIndex=0x%X wLength=0x%X\n",
                        resp.original_request.bRequest,
                        resp.original_request.wValue,
                        resp.original_request.wIndex,
                        resp.original_request.wLength);
                break;
            }
        }

        if (!response_sent)
        {
            /* Send response */
            if (resp.data_length)
            {
                (void)SinkFlushHeader(sink, resp.data_length, (uint16 *)&resp, sizeof(UsbResponse));
            }
            else
            {
                   /* Sink packets can never be zero-length, so flush a dummy byte */
                (void) SinkClaim(sink, 1);
                (void) SinkFlushHeader(sink, 1, (uint16 *) &resp, sizeof(UsbResponse));
            }
        }
        /* Discard the original request */
        SourceDrop(source, packet_size);
    }
}

static usb_class_context_t usbHid_Datalink_Create(usb_device_index_t dev_index,
                                  usb_class_interface_config_data_t config_data)
{
    UsbCodes codes;
    UsbInterface intf;
    EndPointInfo ep_info;
    uint8 endpoint;
    usb_hid_class_data_t *data;
    UNUSED(config_data);

    DEBUG_LOG_INFO("UsbHid:DL Datalink");

    /* HID no boot codes */
    codes.bInterfaceClass    = B_INTERFACE_CLASS_HID;
    codes.bInterfaceSubClass = B_INTERFACE_SUB_CLASS_HID_NO_BOOT;
    codes.bInterfaceProtocol = B_INTERFACE_PROTOCOL_HID_NO_BOOT;
    codes.iInterface         = 0;

    intf = UsbAddInterface(&codes, B_DESCRIPTOR_TYPE_HID,
                           interface_descriptor_hid_datalink,
                           sizeof(interface_descriptor_hid_datalink));

    if (intf == usb_interface_error)
    {
        DEBUG_LOG_ERROR("UsbHid:DL UsbAddInterface ERROR");
        Panic();
    }

    /* Register HID Datalink report descriptor with the interface */
    if (!UsbAddDescriptor(intf, B_DESCRIPTOR_TYPE_HID_REPORT,
            report_descriptor_hid_datalink,
            sizeof(report_descriptor_hid_datalink)))
    {
        DEBUG_LOG_ERROR("UsbHid:DL UsbAddDescriptor ERROR");
        Panic();
    }

    /* USB HID endpoint information */
    endpoint = UsbDevice_AllocateEndpointAddress(dev_index, TRUE /* = is_to_host */);
    if (!endpoint)
    {
        DEBUG_LOG_ERROR("UsbHid:DL UsbDevice_AllocateEndpointAddress ERROR");
        Panic();
    }

    ep_info.bEndpointAddress = endpoint;
    ep_info.bmAttributes = end_point_attr_int;
    ep_info.wMaxPacketSize = 64;
    ep_info.bInterval = 1;
    ep_info.extended = NULL;
    ep_info.extended_length = 0;

    /* Add required endpoints to the interface */
    if (!UsbAddEndPoints(intf, 1, &ep_info))
    {
        DEBUG_LOG_ERROR("UsbHid:DL UsbAddEndPoints ERROR");
        Panic();
    }

    hid_datalink_task.handler = usbHid_DatalinkHandler;

    data = (usb_hid_class_data_t *)
            PanicUnlessMalloc(sizeof(usb_hid_class_data_t));
    memset(data, 0, sizeof(usb_hid_class_data_t));

    data->dev_index = dev_index;
    data->intf = intf;

    data->class_sink = StreamUsbClassSink(intf);
    data->class_source = StreamSourceFromSink(data->class_sink);
    MessageStreamTaskFromSink(data->class_sink, &hid_datalink_task);

    data->ep_sink = StreamUsbEndPointSink(endpoint);
    data->next = hid_class_data;
    hid_class_data = data;

    return (usb_class_context_t)hid_class_data;
}

static usb_result_t usbHid_Datalink_Destroy(usb_class_context_t context)
{
    usb_hid_class_data_t **pp = &hid_class_data, *data = NULL;

    while (*pp)
    {
        if ((usb_class_context_t)(*pp) == context)
        {
            data = *pp;
            *pp = (*pp)->next;
            free(data);
            break;
        }
        pp = &((*pp)->next);
    }

    DEBUG_LOG_INFO("UsbHid:DL closed");

    return data ? USB_RESULT_OK : USB_RESULT_NOT_FOUND;
}


const usb_class_interface_cb_t UsbHid_Datalink_Callbacks =
{
        .Create = usbHid_Datalink_Create,
        .Destroy = usbHid_Datalink_Destroy,
        .SetInterface = NULL
};

void UsbHid_Datalink_RegisterSetReportHandler(usb_hid_handler_t handler)
{
    UNUSED(handler);
    /* Sink app had 2x clients:
     * AhiUsbHostHandleMessage -> AhiTransportProcessData((TaskData*)&ahiTask, (uint8*)msg->report);
     * HidUpgradeSetReportHandler(msg->report_id, msg->size_report, msg->report);
     */
}

void UsbHid_Datalink_UnregisterSetReportHandler(usb_hid_handler_t handler)
{
    UNUSED(handler);
    /* todo */
}
