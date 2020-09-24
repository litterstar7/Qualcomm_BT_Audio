/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Handset service config
*/

#include "handset_service_config.h"
#include "handset_service.h"

handset_service_config_t handset_service_config;

void HandsetServiceConfig_Init(void)
{
    handset_service_config.max_bredr_connections = 1;
}

bool HandsetService_Configure(handset_service_config_t config)
{
    if(config.max_bredr_connections > HANDSET_SERVICE_MAX_PERMITTED_BREDR_CONNECTIONS)
        return FALSE;
    
    if(!config.max_bredr_connections)
        return FALSE;
    
    handset_service_config = config;
    return TRUE;
}
