/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for the gaia framework API
*/

#define DEBUG_LOG_MODULE_NAME gaia_framework
#include <logging.h>

#include "gaia_framework_feature.h"

#include <panic.h>
#include <stdlib.h>


/*! \brief Feature list node */
struct feature_list_item
{
    /*! Feature ID of the plugin to be registered */
    unsigned feature_id:8;

    /*! Version number of the plugin to be registered */
    unsigned version_number:7;

    /*! Flag set by mobile app if it has registered for it's notifications */
    unsigned notifications_active:1;

    /*! Command handler of the plugin to be registered */
    const gaia_framework_plugin_functions_t *functions;

    /*! Next node */
    struct feature_list_item *next_item;
};

static struct feature_list_item *feature_list;
static uint8 number_of_registered_features;


/*! \brief Creates a new item for the feature list

    \param  feature_id          Feature ID of the plugin to be registered

    \param  version_number      Version number of the plugin to be registered

    \param  command_handler     Command handler of the plugin to be registered

    \param  send_notifications  Sends all notifications of the registered feature

    \return New item for the feature list containing the features interface
*/
static struct feature_list_item * gaiaFrameworkFeature_CreateFeatureListItem(gaia_features_t feature_id, uint8 version_number,
                                                                             const gaia_framework_plugin_functions_t *functions);

/*! \brief Checks if a feature is registered

    \param  feature_id  Feautue Id for the plugin

    \return Returns the feature list entry otherwise the next available empty slot
*/
static struct feature_list_item * gaiaFrameworkFeature_FindFeature(gaia_features_t feature_id);

/*! \brief Sets the notification flag of a speceific feature

    \param  feature_id  Feautue Id for the plugin

    \param  set         Sets or resets the notification flag

    \return True if feature is found and the flag is set to the specific value
*/
static bool gaiaFrameworkFeature_SetNotificationFlag(gaia_features_t feature_id, bool set);


void GaiaFrameworkFeature_Init(void)
{
    DEBUG_LOG("GaiaFrameworkFeature_Init");

    /* Globals should be zero initialised, catch case where we are called
     * a second time */
    PanicFalse(!feature_list);
    PanicFalse(number_of_registered_features == 0);
}

bool GaiaFrameworkFeature_AddToList(gaia_features_t feature_id, uint8 version_number,
                                    const gaia_framework_plugin_functions_t *functions)
{
    bool registered = FALSE;
    if (!gaiaFrameworkFeature_FindFeature(feature_id))
    {
        struct feature_list_item *new_feature = gaiaFrameworkFeature_CreateFeatureListItem(feature_id, version_number,
                                                                                           functions);
        if (new_feature)
        {
            new_feature->next_item = feature_list;
            feature_list = new_feature;
            number_of_registered_features++;
            DEBUG_LOG("GaiaFramework_RegisterFeature, feature_id %u registers, %u features", feature_id, number_of_registered_features);
            registered = TRUE;
        }
        else
            DEBUG_LOG("GaiaFramework_RegisterFeature, feature_id %u can't be registered", feature_id);
    }
    else
        DEBUG_LOG_ERROR("GaiaFramework_RegisterFeature, feature_id %u has already been registered", feature_id);

    return registered;
}

gaia_framework_command_status_t GaiaFrameworkFeature_SendToFeature(GAIA_TRANSPORT *transport, gaia_features_t feature_id, uint8 pdu_type, uint8 pdu_specific_id, uint16 payload_size, const uint8 *payload)
{
    UNUSED(pdu_type);
    gaia_framework_command_status_t status = feature_not_handled;

    struct feature_list_item *entry = gaiaFrameworkFeature_FindFeature(feature_id);
    if (entry)
    {
        DEBUG_LOG("GaiaFramework_SendToFeature, feature_id %u", feature_id);
        status = (*entry).functions->command_handler(transport, pdu_specific_id, payload_size, payload);
    }        
    else
        DEBUG_LOG_ERROR("GaiaFramework_SendToFeature, feature_id %u not found", feature_id);

    return status;
}

void GaiaFrameworkFeature_NotifyFeaturesOfConnect(GAIA_TRANSPORT *transport)
{
    struct feature_list_item *entry = feature_list;
    while (entry)
    {
        if (entry->functions->transport_connect)
            entry->functions->transport_connect(transport);

        entry = entry->next_item;
    }
}

void GaiaFrameworkFeature_NotifyFeaturesOfDisconnect(GAIA_TRANSPORT *transport)
{
    struct feature_list_item *entry = feature_list;
    while (entry)
    {
        if (entry->functions->transport_disconnect)
            entry->functions->transport_disconnect(transport);

        entry = entry->next_item;
    }
}

bool GaiaFrameworkFeature_RegisterForNotifications(gaia_features_t feature_id)
{
    DEBUG_LOG("GaiaFrameworkFeature_RegisterForNotifications, feature_id %u", feature_id);
    return gaiaFrameworkFeature_SetNotificationFlag(feature_id, TRUE);
}

bool GaiaFrameworkFeature_UnregisterForNotifications(gaia_features_t feature_id)
{
    DEBUG_LOG("GaiaFrameworkFeature_UnregisterForNotifications, feature_id %u", feature_id);
    return gaiaFrameworkFeature_SetNotificationFlag(feature_id, FALSE);
}

bool GaiaFrameworkFeature_IsNotificationsActive(gaia_features_t feature_id)
{
    struct feature_list_item *entry = gaiaFrameworkFeature_FindFeature(feature_id);
    DEBUG_LOG("GaiaFrameworkFeature_IsNotificationsActive, feature_id %u, active %u",
              feature_id, entry ? entry->notifications_active : FALSE);
    return entry ? entry->notifications_active : FALSE;
}

void GaiaFrameworkFeature_SendAllNotifications(GAIA_TRANSPORT *transport, gaia_features_t feature_id)
{
    struct feature_list_item *entry = gaiaFrameworkFeature_FindFeature(feature_id);
    PanicNull(entry);
    if (entry->functions->send_all_notifications)
    {
        DEBUG_LOG("GaiaFrameworkFeature_SendAllNotifications, feature_id %u, sending notifications", feature_id);
        entry->functions->send_all_notifications(transport);
    }
    else
        DEBUG_LOG_ERROR("GaiaFrameworkFeature_SendAllNotifications, feature_id %u, no notification handler registered", feature_id);

}

uint8 GaiaFrameworkFeature_GetNumberOfRegisteredFeatures(void)
{
    DEBUG_LOG("GaiaFrameworkFeature_GetNumberOfRegisteredFeatures, num_features %u", number_of_registered_features);
    return number_of_registered_features;
}

feature_list_handle_t *GaiaFrameworkFeature_GetNextHandle(feature_list_handle_t *handle)
{
    struct feature_list_item *entry = (struct feature_list_item *)handle;
    DEBUG_LOG("GaiaFrameworkFeature_GetNextHandle");

    if (!entry)
        entry = feature_list;
    else
        entry = entry->next_item;

    return (feature_list_handle_t *)entry;
}

bool GaiaFrameworkFeature_GetFeatureIdAndVersion(feature_list_handle_t *handle, uint8 *feature_id, uint8 *version_number)
{
    if (handle)
    {
        struct feature_list_item *entry = (struct feature_list_item *)handle;
        DEBUG_LOG("GaiaFrameworkFeature_GetFeatureIdAndVersion, feature_id %u, version %u", entry->feature_id, entry->version_number);
        *feature_id = entry->feature_id;
        *version_number = entry->version_number;
    }
    else
        DEBUG_LOG_ERROR("GaiaFrameworkFeature_GetFeatureIdAndVersion, NULL handle");

    return (handle != NULL);
}

static struct feature_list_item *gaiaFrameworkFeature_CreateFeatureListItem(gaia_features_t feature_id, uint8 version_number,
                                                                            const gaia_framework_plugin_functions_t *functions)
{
    struct feature_list_item *new_entry = malloc(sizeof(struct feature_list_item));
    if (new_entry)
    {
        DEBUG_LOG("gaiaFrameworkFeature_CreateFeatureListItem, feature_id %u, version_number %u", feature_id, version_number);
        new_entry->feature_id = feature_id;
        new_entry->version_number = version_number;
        new_entry->notifications_active = FALSE;
        new_entry->functions = functions;
        new_entry->next_item = NULL;
    }
    else
        DEBUG_LOG_ERROR("gaiaFrameworkFeature_CreateFeatureListItem, failed to allocate, feature_id %u, version_number %u", feature_id, version_number);

    return new_entry;
}

static struct feature_list_item * gaiaFrameworkFeature_FindFeature(gaia_features_t feature_id)
{
    struct feature_list_item *entry = feature_list;
    while (entry)
    {
        if (entry->feature_id == feature_id)
            break;

        entry = entry->next_item;
    }

    DEBUG_LOG("gaiaFrameworkFeature_FindFeature, feature_id %u, entry %p", feature_id, entry);
    return entry;
}

static bool gaiaFrameworkFeature_SetNotificationFlag(gaia_features_t feature_id, bool set)
{
    struct feature_list_item * entry = gaiaFrameworkFeature_FindFeature(feature_id);
    DEBUG_LOG("gaiaFrameworkFeature_SetNotificationFlag, feature_id %u, set %u", feature_id, set);
    if (entry)
        entry->notifications_active = set;

    return (entry != NULL);
}
