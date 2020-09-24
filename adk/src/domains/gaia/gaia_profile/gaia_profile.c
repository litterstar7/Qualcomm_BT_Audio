/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       gaia_profile.c
\brief  Implementation of the profile interface for GAIA
*/

#include <profile_manager.h>
#include <logging.h>
#include <csrtypes.h>
#include <stdlib.h>
#include <panic.h>
#include <task_list.h>

#include "gaia_profile.h"
#include "bt_device.h"

/*
    TODO:

    This is initially a template module for the GAIA team to make use of in fleshing out with full functionality.

    The GaiaProfile_Init function will need to be called once at some point when it is determined that GAIA has
    initialised.

    The GaiaProfile_SendConnectedInd function should be called when it is determined that GAIA has connected.
    The GaiaProfile_SendConnectedInd function can only be used after the GaiaProfile_Init function has been called.

    The GaiaProfile_SendDisconnectedInd function should be called when it is determined that GAIA has disconnected.
    The GaiaProfile_SendDisconnectedInd function can only be used after the GaiaProfile_Init function has been called.
    The GaiaProfile_SendDisconnectedInd function should be used after the GaiaProfile_SendConnectedInd function has
    been used to indicted connection, and sould not be used again until after a subsequent call to the
    GaiaProfile_SendConnectedInd function. (It does not make sense to indicate disconnection is not connected.)
 */
/*
    TODO:

    The following Gaia_DisconnectIfRequired function is a dummy function that needs to be replaced by a real function
    in GAIA, comparable to the AmaTws_DisconnectIfRequired function in AMA, and the GaaService_Disconnect function in
    GAA, that will cause GAIA to be disconnected if required. It returns TRUE if disconnection was required, else FALSE
    if GAIA was already disconnected.
 */
bool Gaia_DisconnectIfRequired(void)
{
    return FALSE;
}

/*! List of tasks requiring confirmation of GAIA disconnect requests */
static task_list_with_data_t disconnect_request_clients;

static bool gaiaProfile_FindClientSendDisconnectCfm(Task task, task_list_data_t *data, void *arg)
{
    bool found_client_task  = FALSE;
    profile_manager_send_client_cfm_params * params = (profile_manager_send_client_cfm_params *) arg;

    if (data && params && (data->ptr == params->device))
    {
        found_client_task = TRUE;
        DEBUG_LOG("gaiaProfile_FindClientSendDisconnectCfm toTask=%x success=%d", task, params->result);
        ProfileManager_GenericDisconnectCfm(profile_manager_gaia_profile, params->device, 
            params->result == profile_manager_success ? TRUE : FALSE);

        TaskList_RemoveTask(TaskList_GetBaseTaskList(&disconnect_request_clients), task);
    }
    else
    {
        if (data == NULL)
            DEBUG_LOG_ERROR("gaiaProfile_FindClientSendDisconnectCfm: NULL data");
        if (params == NULL)
            DEBUG_LOG_ERROR("gaiaProfile_FindClientSendDisconnectCfm: NULL params");
        if (data && params)
            DEBUG_LOG_ERROR("gaiaProfile_FindClientSendDisconnectCfm: data->ptr %p != params->device %p", data->ptr, params->device);
    }
    return !found_client_task;
}

static void gaiaProfile_Disconnect(const Task client_task, bdaddr* bd_addr)
{
    DEBUG_LOG("gaiaProfile_Disconnect");
    PanicNull((bdaddr *)bd_addr);
    device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);
    if (device)
    {
        ProfileManager_AddRequestToTaskList(TaskList_GetBaseTaskList(&disconnect_request_clients), device, client_task);
        if (!Gaia_DisconnectIfRequired())
        {
            /* If already disconnected, send an immediate confirmation */
            ProfileManager_SendConnectCfmToTaskList(TaskList_GetBaseTaskList(&disconnect_request_clients),
                                                    bd_addr, profile_manager_success, gaiaProfile_FindClientSendDisconnectCfm);
        }
    }
}

/*
    It has been assumed that GAIA cannot be told to connect by the profile manager, and hence there is no
    gaiaProfile_Connect callback function defined, and the ProfileManager_RegisterProfile function is called with a
    connect parameter value of NULL.
 */
void GaiaProfile_Init(void)
{
    DEBUG_LOG("GaiaProfile_Init");
    TaskList_WithDataInitialise(&disconnect_request_clients);
    ProfileManager_RegisterProfile(profile_manager_gaia_profile, NULL, gaiaProfile_Disconnect);
}

/*
    The GaiaProfile_SendConnectedInd function needs to be called from within GAIA when it has been determined that
    GAIA has connected.
 */
void GaiaProfile_SendConnectedInd(void)
{
    DEBUG_LOG("GaiaProfile_SendConnectedInd");
    bdaddr bd_addr;
    appDeviceGetHandsetBdAddr(&bd_addr);
    ProfileManager_GenericConnectedInd(profile_manager_gaia_profile, &bd_addr);
}

void GaiaProfile_SendDisconnectedInd(void)
{
    DEBUG_LOG("GaiaProfile_SendDisconnectedInd");
    bdaddr bd_addr;
    appDeviceGetHandsetBdAddr(&bd_addr);
    if (TaskList_Size(TaskList_GetBaseTaskList(&disconnect_request_clients)) != 0)
    {
        ProfileManager_SendConnectCfmToTaskList(TaskList_GetBaseTaskList(&disconnect_request_clients),
                                                &bd_addr,
                                                profile_manager_success,
                                                gaiaProfile_FindClientSendDisconnectCfm);
    }
    ProfileManager_GenericDisconnectedInd(profile_manager_gaia_profile, &bd_addr, 0);
}

