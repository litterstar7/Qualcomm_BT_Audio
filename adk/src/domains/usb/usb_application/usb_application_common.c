/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      USB application framework - switch between application.
*/

#include "usb_application.h"
#include "usb_device_utils.h"

#include "logging.h"

#include <panic.h>
#include <stdlib.h>

/*! Store context for the currently active application */
typedef struct usb_app_context_t
{
    usb_device_index_t dev_index;
    const usb_app_interface_t *active_app;
} usb_app_context_t;

static usb_app_context_t usb_app_context;

void UsbApplication_Open(const usb_app_interface_t *app)
{
    usb_result_t result;

    DEBUG_LOG_WARN("UsbApplication_Open: 0x%x (active: 0x%x)",
                   app, usb_app_context.active_app);

    if (usb_app_context.active_app == app)
    {
        /* Already active application */
        return;
    }

    if (usb_app_context.active_app)
    {
        /* Detach and close previous application */
        usb_app_context.active_app->Detach(usb_app_context.dev_index);
        usb_app_context.active_app->Destroy(usb_app_context.dev_index);
        /* Delete USB device */
        result = UsbDevice_Delete(usb_app_context.dev_index);
        assert(result == USB_RESULT_OK);
    }

    /* Create USB device instance */
    result = UsbDevice_Create(&usb_app_context.dev_index);
    assert(result == USB_RESULT_OK);

    /* Remember the new active application */
    usb_app_context.active_app = app;

    usb_app_context.active_app->Create(usb_app_context.dev_index);
    usb_app_context.active_app->Attach(usb_app_context.dev_index);
}

void UsbApplication_Close(void)
{
    DEBUG_LOG_WARN("UsbApplication_Close: active 0x%x",
                   usb_app_context.active_app);

    if (usb_app_context.active_app)
    {
        usb_result_t result;

        usb_app_context.active_app->Detach(usb_app_context.dev_index);
        usb_app_context.active_app->Destroy(usb_app_context.dev_index);
        usb_app_context.active_app = NULL;

        /* Last device - destroy USB device instance */
        result = UsbDevice_Delete(usb_app_context.dev_index);
        assert(result == USB_RESULT_OK);
    }
}
