/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      USB HID: Consumer Transport Control interface
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

TaskData hid_consumer_task;

#define USB_HID_CONSUMER_TRANSPORT_REPORT_ID 0x01

/* HID Report Descriptor - Consumer Transport Control Device */
static const uint8 report_descriptor_hid_consumer_transport[] =
{
    0x05, 0x0C,                  /* USAGE_PAGE (Consumer Devices) */
    0x09, 0x01,                  /* USAGE (Consumer Control) */
    0xa1, 0x01,                  /* COLLECTION (Application) */

    0x85, USB_HID_CONSUMER_TRANSPORT_REPORT_ID, /*   REPORT_ID (1) */

    0x15, 0x00,                  /*   LOGICAL_MINIMUM (0) */
    0x25, 0x01,                  /*   LOGICAL_MAXIMUM (1) */
    0x09, 0xcd,                  /*   USAGE (Play/Pause - OSC) */
    0x09, 0xb5,                  /*   USAGE (Next Track - OSC) */
    0x09, 0xb6,                  /*   USAGE (Previous Track - OSC) */
    0x09, 0xb7,                  /*   USAGE (Stop - OSC) */
    0x75, 0x01,                  /*   REPORT_SIZE (1) */
    0x95, 0x04,                  /*   REPORT_COUNT (4) */
    0x81, 0x02,                  /*   INPUT (Data,Var,Abs) */

    0x15, 0x00,                  /*   LOGICAL_MINIMUM (0) */
    0x25, 0x01,                  /*   LOGICAL_MAXIMUM (1) */
    0x09, 0xb0,                  /*   USAGE (Play - OOC) */
    0x09, 0xb1,                  /*   USAGE (Pause - OOC) */
    0x09, 0xb3,                  /*   USAGE (Fast Forward -OOC) */
    0x09, 0xb4,                  /*   USAGE (Rewind - OOC) */
    0x75, 0x01,                  /*   REPORT_SIZE (1) */
    0x95, 0x04,                  /*   REPORT_COUNT (4) */
    0x81, 0x22,                  /*   INPUT (Data,Var,Abs,NoPref) */

    0x15, 0x00,                  /*   LOGICAL_MINIMUM (0) */
    0x25, 0x01,                  /*   LOGICAL_MAXIMUM (1) */
    0x09, 0xe9,                  /*   USAGE (Volume Increment - RTC) */
    0x09, 0xea,                  /*   USAGE (Volume Decrement - RTC) */
    0x75, 0x01,                  /*   REPORT_SIZE (1) */
    0x95, 0x02,                  /*   REPORT_COUNT (2) */
    0x81, 0x02,                  /*   INPUT (Data,Var,Abs,Bit Field) */
    0x09, 0xe2,                  /*   USAGE (Mute - OOC) */

    0x95, 0x01,                  /*   REPORT_COUNT (1) */
    0x81, 0x06,                  /*   INPUT (Data,Var,Rel,Bit Field) */

    0x95, 0x05,                  /*   REPORT_COUNT (5) */
    0x81, 0x01,                  /*   INPUT (Const) */

    0xc0                        /* END_COLLECTION */
};

/* USB HID class descriptor - Consumer Transport Control Device*/
static const uint8 interface_descriptor_hid_consumer_transport[] =
{
    HID_DESCRIPTOR_LENGTH,              /* bLength */
    B_DESCRIPTOR_TYPE_HID,              /* bDescriptorType */
    0x11, 0x01,                         /* bcdHID */
    0,                                  /* bCountryCode */
    1,                                  /* bNumDescriptors */
    B_DESCRIPTOR_TYPE_HID_REPORT,       /* bDescriptorType */
    sizeof(report_descriptor_hid_consumer_transport),   /* wDescriptorLength */
    0                                   /* wDescriptorLength */
};

static void usbHid_ConsumerHandler(Task task, MessageId id, Message message)
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
        /* Build the response. It must contain the original request, so copy
           from the source header. */
        memcpy(&resp.original_request, SourceMapHeader(source), sizeof(UsbRequest));

        /* Set the response fields to default values to make the code below simpler */
        resp.success = FALSE;
        resp.data_length = 0;

        switch (resp.original_request.bRequest)
        {
            case HID_GET_REPORT:
                DEBUG_LOG_INFO("UsbHid:CT Get_Report wValue=0x%X wIndex=0x%X wLength=0x%X",
                               resp.original_request.wValue,
                               resp.original_request.wIndex,
                               resp.original_request.wLength);
                break;

            case HID_GET_IDLE:
            {
                uint8 *out;
                if ((out = SinkMapClaim(sink, 1)) != 0)
                {
                     DEBUG_LOG_INFO("UsbHid:CT Get_Idle wValue=0x%X wIndex=0x%X",
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
                DEBUG_LOG_INFO("UsbHid:CT Set_Report wValue=0x%X wIndex=0x%X wLength=0x%X",
                               resp.original_request.wValue,
                               resp.original_request.wIndex,
                               resp.original_request.wLength);

                /* SET_REPORT is not handled for Consumer Transport. */

                resp.success = TRUE;
                break;
            }

            case HID_SET_IDLE:
                DEBUG_LOG_INFO("UsbHid:CT Set_Idle wValue=0x%X wIndex=0x%X",
                               resp.original_request.wValue,
                               resp.original_request.wIndex);
                data->idle_rate = (resp.original_request.wValue >> 8) & 0xff;
                resp.success = TRUE;
                break;

            default:
                DEBUG_LOG_INFO("UsbHid:CT req=0x%X wValue=0x%X wIndex=0x%X wLength=0x%X",
                               resp.original_request.bRequest,
                               resp.original_request.wValue,
                               resp.original_request.wIndex,
                               resp.original_request.wLength);
                break;
        }

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
        /* Discard the original request */
        SourceDrop(source, packet_size);
    }
}

static void consumerTransport_SendKeyEvent(Sink ep_sink, uint16 key, uint16 state)
{
    Sink sink = ep_sink;
    uint8 *ptr;
    uint16 data_size = 3;

    if ((ptr = SinkMapClaim(sink, data_size)) != 0)
    {
        ptr[0] = USB_HID_CONSUMER_TRANSPORT_REPORT_ID; /* REPORT ID */

        if (state)
        {
            ptr[1] = key & 0xff;        /* key on code */
            ptr[2] = (key >> 8) & 0xff;   /* key on code */
        }
        else
        {
            ptr[1] = 0; /* key released */
            ptr[2] = 0; /* key released */
        }

        /* Flush data */
        (void) SinkFlush(sink, data_size);
    }
}

typedef enum
{
    STATE_ON = 1,
    STATE_OFF = 2,
    STATE_TOGGLE = 3
} key_state_t;

typedef struct
{
    uint16 key_code;
    uint16 key_state;
} event_key_map_t;

typedef enum
{
    PLAY_PAUSE = 1,
    STOP = 8,
    NEXT_TRACK = 2,
    PREVIOUS_TRACK = 4,
    PLAY = 16,
    PAUSE = 32,
    VOL_UP = 256,
    VOL_DOWN = 512,
    MUTE = 1024,
    FFWD = 64,
    RWD = 128,
} consumer_transport_keys;

static const event_key_map_t event_key_map[] =
{
        /* USB_HID_CONSUMER_TRANSPORT_PLAY_PAUSE */
        {PLAY_PAUSE, STATE_TOGGLE},

        /*  USB_HID_CONSUMER_TRANSPORT_STOP */
        {STOP, STATE_TOGGLE},

        /* USB_HID_CONSUMER_TRANSPORT_NEXT_TRACK */
        {NEXT_TRACK, STATE_TOGGLE},

        /* USB_HID_CONSUMER_TRANSPORT_PREVIOUS_TRACK */
        {PREVIOUS_TRACK, STATE_TOGGLE},

        /* USB_HID_CONSUMER_TRANSPORT_PLAY */
        {PLAY, STATE_ON},

        /* USB_HID_CONSUMER_TRANSPORT_PAUSE */
        {PAUSE, STATE_ON},

        /* USB_HID_CONSUMER_TRANSPORT_VOL_UP */
        {VOL_UP, STATE_TOGGLE},

        /* USB_HID_CONSUMER_TRANSPORT_VOL_DOWN */
        {VOL_DOWN, STATE_TOGGLE},

        /* USB_HID_CONSUMER_TRANSPORT_MUTE */
        {MUTE, STATE_ON},

        /* USB_HID_CONSUMER_TRANSPORT_FFWD_ON */
        {FFWD, STATE_ON},

        /* USB_HID_CONSUMER_TRANSPORT_REW_ON */
        {RWD, STATE_ON},

        /* USB_HID_CONSUMER_TRANSPORT_FFWD_OFF */
        {FFWD, STATE_OFF},

        /* USB_HID_CONSUMER_TRANSPORT_REW_OFF */
        {RWD, STATE_OFF}
};

usb_result_t UsbHid_ConsumerTransporControl_SendEvent(usb_class_context_t context,
                                                      usb_hid_consumer_transport_event event)
{
    uint16 key_code;
    key_state_t key_state;
    usb_hid_class_data_t *data = hid_class_data;

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

    if (event > USB_HID_CONSUMER_TRANSPORT_MUTE)
    {
        return USB_RESULT_NOT_FOUND;
    }

    DEBUG_LOG_INFO("UsbHid:CT send event %d", event);

    key_code = event_key_map[event].key_code;
    key_state = event_key_map[event].key_state;

    if (key_state & STATE_ON)
    {
        consumerTransport_SendKeyEvent(data->ep_sink, key_code, 1);
    }
    if (key_state & STATE_OFF)
    {
        consumerTransport_SendKeyEvent(data->ep_sink, key_code, 0);
    }

    return USB_RESULT_OK;
}

static usb_class_context_t usbHid_ConsumerTransporControl_Create(usb_device_index_t dev_index,
                                  usb_class_interface_config_data_t config_data)
{
    UsbCodes codes;
    UsbInterface intf;
    EndPointInfo ep_info;
    uint8 endpoint;
    usb_hid_class_data_t *data;
    UNUSED(config_data);

    DEBUG_LOG_INFO("UsbHid:CT Consumer Transport");

    /* HID no boot codes */
    codes.bInterfaceClass    = B_INTERFACE_CLASS_HID;
    codes.bInterfaceSubClass = B_INTERFACE_SUB_CLASS_HID_NO_BOOT;
    codes.bInterfaceProtocol = B_INTERFACE_PROTOCOL_HID_NO_BOOT;
    codes.iInterface         = 0;

    intf = UsbAddInterface(&codes, B_DESCRIPTOR_TYPE_HID,
            interface_descriptor_hid_consumer_transport,
            sizeof(interface_descriptor_hid_consumer_transport));

    if (intf == usb_interface_error)
    {
        DEBUG_LOG_ERROR("UsbHid:CT UsbAddInterface ERROR");
        Panic();
    }

    /* Register HID Consumer Control Device report descriptor with the interface */
    if (!UsbAddDescriptor(intf, B_DESCRIPTOR_TYPE_HID_REPORT,
            report_descriptor_hid_consumer_transport,
            sizeof(report_descriptor_hid_consumer_transport)))
    {
        DEBUG_LOG_ERROR("UsbHid:CT sbAddDescriptor ERROR");
        Panic();
    }

    /* USB HID endpoint information */
    endpoint = UsbDevice_AllocateEndpointAddress(dev_index, TRUE /* = is_to_host */);
    if (!endpoint)
    {
        DEBUG_LOG_ERROR("UsbHid:CT UsbDevice_AllocateEndpointAddress ERROR");
        Panic();
    }

    ep_info.bEndpointAddress = endpoint;
    ep_info.bmAttributes = end_point_attr_int;
    ep_info.wMaxPacketSize = 64;
    ep_info.bInterval = 8;
    ep_info.extended = NULL;
    ep_info.extended_length = 0;

    /* Add required endpoints to the interface */
    if (!UsbAddEndPoints(intf, 1, &ep_info))
    {
        DEBUG_LOG_ERROR("UsbHid:CT UsbAddEndPoints ERROR");
        Panic();
    }

    hid_consumer_task.handler = usbHid_ConsumerHandler;

    data = (usb_hid_class_data_t *)
            PanicUnlessMalloc(sizeof(usb_hid_class_data_t));
    memset(data, 0, sizeof(usb_hid_class_data_t));

    data->dev_index = dev_index;
    data->intf = intf;
    data->class_sink = StreamUsbClassSink(intf);
    data->class_source = StreamSourceFromSink(data->class_sink);
    MessageStreamTaskFromSink(data->class_sink, &hid_consumer_task);
    data->ep_sink = StreamUsbEndPointSink(endpoint);
    data->next = hid_class_data;
    hid_class_data = data;

    return (usb_class_context_t)hid_class_data;
}

static usb_result_t usbHid_ConsumerTransporControl_Destroy(usb_class_context_t context)
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

    DEBUG_LOG_INFO("UsbHid:CT closed");

    return data ? USB_RESULT_OK : USB_RESULT_NOT_FOUND;
}

const usb_class_interface_cb_t UsbHid_ConsumerTransport_Callbacks =
{
        .Create = usbHid_ConsumerTransporControl_Create,
        .Destroy = usbHid_ConsumerTransporControl_Destroy,
        .SetInterface = NULL
};

usb_class_context_t UsbHid_ConsumerTransporControl_GetContext(usb_device_index_t dev_index)
{
    usb_hid_class_data_t *data = hid_class_data;

    while (data)
    {
        if (data->dev_index == dev_index)
        {
            break;
        }
        data = data->next;
    }
    return (usb_class_context_t)data;

}
