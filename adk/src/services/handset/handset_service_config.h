/*!
\copyright  Copyright (c) 2019 - 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   handset_service Handset Service
\ingroup    services
\brief      Configurable values for the Handset Service.
*/

#ifndef HANDSET_SERVICE_CONFIG_H_
#define HANDSET_SERVICE_CONFIG_H_

#include "handset_service.h"

extern handset_service_config_t handset_service_config;

/*! Maximum permitted number of BR/EDR ACL connections
*/
#define HANDSET_SERVICE_MAX_PERMITTED_BREDR_CONNECTIONS 2

/*! Maximum number of handset ACL connect requests to attempt.

    This is the maximum number of times the BR/EDR ACL connection to the
    handset will be attempted for a single client handset connect request.

    After this the connection request will be completed with a fail status.
*/
#define handsetService_BredrAclConnectAttemptLimit() (3)

/*! Time delay between each handset ACL connect retry.

    The delay is in ms.
*/
#define handsetService_BredrAclConnectRetryDelayMs() (500)

/*! Maximum permitted number of BR/EDR ACL connections

    Can be modified using HandsetService_Configure
*/
#define handsetService_BredrAclMaxConnections() handset_service_config.max_bredr_connections

/*! Initialise handset service configuration to defaults
*/
void HandsetServiceConfig_Init(void);

#endif /* HANDSET_SERVICE_CONFIG_H_ */
