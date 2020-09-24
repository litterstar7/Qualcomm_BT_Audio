/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       peer_profile.c
\brief  Implementation of the profile interface for PEER
*/

#include <profile_manager.h>
#include <logging.h>
#include <csrtypes.h>
#include <stdlib.h>
#include <panic.h>
#include <task_list.h>

#include "peer_profile.h"
#include "bt_device.h"

/*
    TODO:

    This is initially a template module for the PEER team to make use of in fleshing out with full functionality.

    The PeerProfile_Init function will need to be called once at some point when it is determined that PEER has
    initialised.

    The PeerProfile_SendConnectedInd function should be called when it is determined that PEER has connected.
    The PeerProfile_SendConnectedInd function can only be used after the PeerProfile_Init function has been called.

    The PeerProfile_SendDisconnectedInd function should be called when it is determined that PEER has disconnected.
    The PeerProfile_SendDisconnectedInd function can only be used after the PeerProfile_Init function has been called.
    The PeerProfile_SendDisconnectedInd function should be used after the PeerProfile_SendConnectedInd function has
    been used to indicted connection, and sould not be used again until after a subsequent call to the
    PeerProfile_SendConnectedInd function. (It does not make sense to indicate disconnection is not connected.)
 */
/*
    TODO:

    The following Peer_ConnectIfRequired function is a dummy function that needs to be replaced by a real function
    in PEER, comparable to the appHfpConnectWithBdAddr function in HFP, the appAvA2dpConnectRequest function in A2DP,
    and the appAvAvrcpConnectRequest in AVRCP, that will cause PEER to be connected if required. It returns TRUE if
    connection was required, else FALSE if PEER was already connected.
 */
bool Peer_ConnectIfRequired(void)
{
    return FALSE;
}

/*
    TODO:

    The following Peer_DisconnectIfRequired function is a dummy function that needs to be replaced by a real function
    in PEER, comparable to the AmaTws_DisconnectIfRequired function in AMA, and the GaaService_Disconnect function in
    GAA, that will cause PEER to be disconnected if required. It returns TRUE if disconnection was required, else FALSE
    if PEER was already disconnected.
 */
bool Peer_DisconnectIfRequired(void)
{
    return FALSE;
}

/*! List of tasks requiring confirmation of PEER connect requests */
static task_list_with_data_t connect_request_clients;
/*! List of tasks requiring confirmation of PEER disconnect requests */
static task_list_with_data_t disconnect_request_clients;

static bool peerProfile_FindClientSendConnectCfm(Task task, task_list_data_t *data, void *arg)
{
    bool found_client_task  = FALSE;
    profile_manager_send_client_cfm_params * params = (profile_manager_send_client_cfm_params *) arg;

    if (data && params && (data->ptr == params->device))
    {
        found_client_task = TRUE;
        DEBUG_LOG("peerProfile_FindClientSendConnectCfm toTask=%x success=%d", task, params->result);
        ProfileManager_GenericConnectCfm(profile_manager_peer_profile, params->device, 
            params->result == profile_manager_success ? TRUE : FALSE);

        TaskList_RemoveTask(TaskList_GetBaseTaskList(&connect_request_clients), task);
    }
    else
    {
        if (data == NULL)
            DEBUG_LOG_ERROR("peerProfile_FindClientSendConnectCfm: NULL data");
        if (params == NULL)
            DEBUG_LOG_ERROR("peerProfile_FindClientSendConnectCfm: NULL params");
        if (data && params)
            DEBUG_LOG_ERROR("peerProfile_FindClientSendConnectCfm: data->ptr %p != params->device %p", data->ptr, params->device);
    }
    return !found_client_task;
}

static bool peerProfile_FindClientSendDisconnectCfm(Task task, task_list_data_t *data, void *arg)
{
    bool found_client_task  = FALSE;
    profile_manager_send_client_cfm_params * params = (profile_manager_send_client_cfm_params *) arg;

    if (data && params && (data->ptr == params->device))
    {
        found_client_task = TRUE;
        DEBUG_LOG("peerProfile_FindClientSendDisconnectCfm toTask=%x success=%d", task, params->result);
        ProfileManager_GenericDisconnectCfm(profile_manager_peer_profile, params->device, 
            params->result == profile_manager_success ? TRUE : FALSE);

        TaskList_RemoveTask(TaskList_GetBaseTaskList(&disconnect_request_clients), task);
    }
    else
    {
        if (data == NULL)
            DEBUG_LOG_ERROR("peerProfile_FindClientSendDisconnectCfm: NULL data");
        if (params == NULL)
            DEBUG_LOG_ERROR("peerProfile_FindClientSendDisconnectCfm: NULL params");
        if (data && params)
            DEBUG_LOG_ERROR("peerProfile_FindClientSendDisconnectCfm: data->ptr %p != params->device %p", data->ptr, params->device);
    }
    return !found_client_task;
}

static void peerProfile_Connect(const Task client_task, bdaddr* bd_addr)
{
    DEBUG_LOG("peerProfile_Connect");
    PanicNull((bdaddr *)bd_addr);
    device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);
    if (device)
    {
        ProfileManager_AddRequestToTaskList(TaskList_GetBaseTaskList(&connect_request_clients), device, client_task);
        if (!Peer_ConnectIfRequired())
        {
            /* If already connected, send an immediate confirmation */
            ProfileManager_SendConnectCfmToTaskList(TaskList_GetBaseTaskList(&connect_request_clients),
                                                    bd_addr, profile_manager_success, peerProfile_FindClientSendConnectCfm);
        }
    }
}

static void peerProfile_Disconnect(const Task client_task, bdaddr* bd_addr)
{
    DEBUG_LOG("peerProfile_Disconnect");
    PanicNull((bdaddr *)bd_addr);
    device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);
    if (device)
    {
        ProfileManager_AddRequestToTaskList(TaskList_GetBaseTaskList(&disconnect_request_clients), device, client_task);
        if (!Peer_DisconnectIfRequired())
        {
            /* If already disconnected, send an immediate confirmation */
            ProfileManager_SendConnectCfmToTaskList(TaskList_GetBaseTaskList(&disconnect_request_clients),
                                                    bd_addr, profile_manager_success, peerProfile_FindClientSendDisconnectCfm);
        }
    }
}

/*
    It has been assumed that PEER cannot be told to connect by the profile manager, and hence there is no
    peerProfile_Connect callback function defined, and the ProfileManager_RegisterProfile function is called with a
    connect parameter value of NULL.
 */
void PeerProfile_Init(void)
{
    DEBUG_LOG("PeerProfile_Init");
    TaskList_WithDataInitialise(&disconnect_request_clients);
    ProfileManager_RegisterProfile(profile_manager_peer_profile, peerProfile_Connect, peerProfile_Disconnect);
}

/*
    The PeerProfile_SendConnectedInd function needs to be called from within PEER when it has been determined that
    PEER has connected.
 */
void PeerProfile_SendConnectedInd(void)
{
    DEBUG_LOG("PeerProfile_SendConnectedInd");
    bdaddr bd_addr;
    appDeviceGetHandsetBdAddr(&bd_addr);
    ProfileManager_GenericConnectedInd(profile_manager_peer_profile, &bd_addr);
}

void PeerProfile_SendDisconnectedInd(void)
{
    DEBUG_LOG("PeerProfile_SendDisconnectedInd");
    bdaddr bd_addr;
    appDeviceGetHandsetBdAddr(&bd_addr);
    if (TaskList_Size(TaskList_GetBaseTaskList(&disconnect_request_clients)) != 0)
    {
        ProfileManager_SendConnectCfmToTaskList(TaskList_GetBaseTaskList(&disconnect_request_clients),
                                                &bd_addr,
                                                profile_manager_success,
                                                peerProfile_FindClientSendDisconnectCfm);
    }
    ProfileManager_GenericDisconnectedInd(profile_manager_peer_profile, &bd_addr, 0);
}

