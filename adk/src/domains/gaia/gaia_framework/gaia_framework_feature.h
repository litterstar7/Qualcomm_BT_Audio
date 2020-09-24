/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for the gaia framework API
*/

#ifndef GAIA_FRAMEWORK_FEATURE_H_
#define GAIA_FRAMEWORK_FEATURE_H_

#include "gaia_framework.h"

/*! \brief Opaque type for a feature list item */
typedef struct feature_list_item *feature_list_handle_t;

/*! \brief Inialisation function for the feature list
*/
void GaiaFrameworkFeature_Init(void);

/*! \brief Adds a new feature to the feature list

    \param  feature_id          Feature ID of the plugin to be registered
    \param  version_number      Version number of the plugin to be registered
    \param  command_handler     Command handler of the plugin to be registered
    \param  send_notifications  Sends all notifications of the registered feature

    \return True if successful
*/
bool GaiaFrameworkFeature_AddToList(gaia_features_t feature_id, uint8 version_number,
                                    const gaia_framework_plugin_functions_t *functions);

/*! \brief Calls the appropriate command handler based on the feature ID

    \param transport    Transport packet arrived on
    \param feature_id   Feature ID for the plugin
    \param pdu_type     PDU type
    \param length       PDU specific ID
    \param payload_size Payload size
    \param payload      Payload data
    \param handle       Handle for this packet

    \return status of command handling, one of gaia_framework_command_status_t
*/
gaia_framework_command_status_t GaiaFrameworkFeature_SendToFeature(GAIA_TRANSPORT *transport, gaia_features_t feature_id, uint8 pdu_type, uint8 pdu_specific_id, uint16 payload_size, const uint8 *payload);


void GaiaFrameworkFeature_NotifyFeaturesOfConnect(GAIA_TRANSPORT *transport);
void GaiaFrameworkFeature_NotifyFeaturesOfDisconnect(GAIA_TRANSPORT *transport);

/*! \brief Sets the mobile application notification flag

    \param feature_id   Feature ID for the plugin

    \return True if feature is registered
*/
bool GaiaFrameworkFeature_RegisterForNotifications(gaia_features_t feature_id);


/*! \brief Resets the mobile application notification flag

    \param feature_id   Feature ID for the plugin

    \return True if feature is registered
*/
bool GaiaFrameworkFeature_UnregisterForNotifications(gaia_features_t feature_id);

/*! \brief Resets the mobile application notification flag

    \param feature_id   Feature ID for the plugin

    \return True if feature is registered
*/
bool GaiaFrameworkFeature_IsNotificationsActive(gaia_features_t feature_id);

/*! \brief Sends all availabe notifications of a registered feature to the mobile app

    \param feature_id   Feature ID for the plugin
*/
void GaiaFrameworkFeature_SendAllNotifications(GAIA_TRANSPORT *transport, gaia_features_t feature_id);

/*! \brief Gets the number of registered features

    \return The number of registered features
*/
uint8 GaiaFrameworkFeature_GetNumberOfRegisteredFeatures(void);

/*! \brief Gets the next feature handle

    \param handle   Handle to continue from. NULL to start from the begining

    \return New handle
*/
feature_list_handle_t * GaiaFrameworkFeature_GetNextHandle(feature_list_handle_t * handle);

/*! \brief Gets the next feature handle

    \param handle   Handle to continue from. NULL to start from the begining
*/
bool GaiaFrameworkFeature_GetFeatureIdAndVersion(feature_list_handle_t *handle, uint8 *feature_id, uint8 *version_number);

#endif /* GAIA_FRAMEWORK_FEATURE_H_ */
