/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   focus_domains Focus
\ingroup    domains
\brief      Focus interface definition for instantiating a module which shall
            return the focussed remote device.
*/
#ifndef FOCUS_DEVICE_H
#define FOCUS_DEVICE_H

#include "focus_types.h"

#include <device.h>
#include <ui_inputs.h>
#include <bdaddr.h>

/*! \brief Focus interface callback used by Focus_GetDeviceForContext API */
typedef bool (*focus_device_for_context_t)(ui_providers_t provider, device_t* device);

/*! \brief Focus interface callback used by Focus_GetDeviceForUiInput API */
typedef bool (*focus_device_for_ui_input_t)(ui_input_t ui_input, device_t* device);

/*! \brief Focus interface callback used by Focus_GetFocusForDevice API */
typedef focus_t (*focus_for_device_t)(const device_t device);

/*! \brief Focus interface callback used by Focus_AddDeviceToBlacklist API */
typedef bool (*focus_device_add_to_blacklist_t)(device_t device);

/*! \brief Focus interface callback used by Focus_RemoveDeviceFromBlacklist API */
typedef bool (*focus_device_remove_from_blacklist_t)(device_t device);

/*! \brief Focus interface callback used by Focus_ResetDeviceBlacklist API */
typedef void (*focus_device_reset_blacklist_t)(void);

/*! \brief Structure used to configure the focus interface callbacks to be used
           to access the focussed device. And also to add/remove device to/from blacklist. */
typedef struct
{
    focus_device_for_context_t for_context;
    focus_device_for_ui_input_t for_ui_input;
    focus_for_device_t focus;
    focus_device_add_to_blacklist_t add_to_blacklist;
    focus_device_remove_from_blacklist_t remove_from_blacklist;
    focus_device_reset_blacklist_t reset_blacklist;
} focus_device_t;

/*! \brief Configure a set of function pointers to use for retrieving the focussed device

    \param a structure containing the functions implementing the focus interface for retrieving
           the focussed device.
*/
void Focus_ConfigureDevice(focus_device_t focus_device);

/*! \brief Get the focussed device for which to query the context of the specified UI Provider

    \param provider - a UI Provider
    \param device - a pointer to the focussed device_t handle
    \return a bool indicating whether or not a focussed device was returned in the
            device parameter
*/
bool Focus_GetDeviceForContext(ui_providers_t provider, device_t* device);

/*! \brief Get the focussed device that should consume the specified UI Input

    \param ui_input - the UI Input that shall be consumed
    \param device - a pointer to the focussed device_t handle
    \return a bool indicating whether or not a focussed device was returned in the
            device parameter
*/
bool Focus_GetDeviceForUiInput(ui_input_t ui_input, device_t* device);

/*! \brief Get the current focus status for the specified device

    \param device - the device_t handle
    \return the focus status of the specified device
*/
focus_t Focus_GetFocusForDevice(const device_t device);


/*! \brief Set the BT device as blacklisted once ACL connection fails.

    \param device - a pointer to the focussed device_t handle to be blacklisted.

    \return a bool indicating whether or not a device is added to blacklist.
*/
bool Focus_AddDeviceToBlacklist(device_t device);

/*! \brief Remove the Bluetooth address from blacklist.

    \param device - a pointer to the focussed device_t handle to be removed from blacklist.

    \return a bool indicating whether or not a device is removed from blacklist.
*/
bool Focus_RemoveDeviceFromBlacklist(device_t device);

/*! \brief Reset the blacklist.
*/
void Focus_ResetDeviceBlacklist(void);

#endif /* FOCUS_DEVICE_H */
