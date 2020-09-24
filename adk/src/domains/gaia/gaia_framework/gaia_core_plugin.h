/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for the gaia framework core plugin
*/

#ifndef GAIA_CORE_PLUGIN_H_
#define GAIA_CORE_PLUGIN_H_

#include <gaia_features.h>

#include "gaia_framework.h"


/*! \brief Gaia core plugin version
*/
#define GAIA_CORE_PLUGIN_VERSION 2


/*! \brief These are the built-in commands provided by the GAIA framework
*/
typedef enum
{
    /*! Get the Gaia protocol version number */
    get_api_version = 0,
    /*! Get the list of features the device supports */
    get_supported_features = 1,
    /*! Get the list continuation of features the device supports */
    get_supported_features_next = 2,
    /*! Get the customer provided serial number for this device */
    get_serial_number = 3,
    /*! Get the customer provided variant name */
    get_variant = 4,
    /*! Get the customer provided application version number */
    get_application_version = 5,
    /*! The mobile app can cause a device to warm reset using this command */
    device_reset = 6,
    /*! The mobile application can register to receive all the notifications from a Feature */
    register_notification = 7,
    /*! The mobile application can unregister to stop receiving feature notifications */
    unregister_notification = 8,
    /*! Set up a data transfer over one of several transports.*/
    data_transfer_setup,
    /*! The mobile app can get data bytes from the device as a command response. */
    data_transfer_get,
    /*! The mobile app can send data bytes to the device on the command payloads. */
    data_transfer_set,
    /*! Get transport information */
    get_transport_info = 12,
    /*! Set transport parameter */
    set_transport_parameter = 13,
    /*! Total number of commands */
    number_of_core_commands,
} core_plugin_pdu_ids_t;

/*! \brief These are the core notifications provided by the GAIA framework
*/
typedef enum
{
    /*! The device can generate a Notification when the charger is plugged in or unplugged */
    charger_status_notification = 0,
    /*! Total number of notifications */
    number_of_core_notifications,
} core_plugin_notifications_t;


/*! \brief Gaia core plugin init function
*/
void GaiaCorePlugin_Init(void);


#endif /* GAIA_CORE_PLUGIN_H_ */
