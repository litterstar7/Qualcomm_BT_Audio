/* Copyright (c) 2014 - 2020 Qualcomm Technologies International, Ltd. */
/* Part of 6.2 */

#include <string.h>
#include <stdio.h>
#include <logging.h>

#include "gatt_ama_server_private.h"

#include "gatt_ama_server_msg_handler.h"
#include "gatt_ama_server_db.h"

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_TYPE(gatt_ama_server_message_id_t)

/****************************************************************************/
bool GattAmaServerInit(GAMASS *ama_server,
                           Task app_task,
                           uint16 start_handle,
                           uint16 end_handle)
{
    gatt_manager_server_registration_params_t registration_params;

    if ((app_task == NULL) || (ama_server == NULL))
    {
        GATT_AMA_SERVER_PANIC(("GAMASS: Invalid Initialisation parameters"));
        return FALSE;
    }
    
    /* Set up library handler for external messages */
    ama_server->lib_task.handler = GattAmaServerMsgHandler;
        
    /* Store the Task function parameter.
       All library messages need to be sent here */
    ama_server->app_task = app_task;

    ama_server->start_handle = start_handle;

    /* Setup data required for Ama Service to be registered with the GATT Manager */
    registration_params.task = &ama_server->lib_task;
    registration_params.start_handle = start_handle;
    registration_params.end_handle = end_handle;
        
    /* Register with the GATT Manager and verify the result */
    return (GattManagerRegisterServer(&registration_params) == gatt_manager_status_success);
}
