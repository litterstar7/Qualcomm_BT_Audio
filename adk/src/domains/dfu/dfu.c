/*!
\copyright  Copyright (c) 2017 - 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       dfu.c
\brief      Device firmware upgrade management.

Over the air upgrade is managed from this file. 
*/

#ifdef INCLUDE_DFU

#include "dfu.h"

#include "system_state.h"
#include "adk_log.h"
#include "phy_state.h"
#include "bt_device.h"

#include <charger_monitor.h>
#include <system_state.h>

#include <vmal.h>
#include <panic.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <upgrade.h>
#include <ps.h>
#include <tws_topology_config.h>
#include <gatt_connect.h>
#include <gatt_handler.h>
#include <gatt_server_gatt.h>

#ifdef INCLUDE_DFU_PEER
#include "dfu_peer.h"
#include "bt_device.h"
#include <app/message/system_message.h>
#include <peer_signalling.h>
#include <connection_manager.h>
#ifdef INCLUDE_MIRRORING
#include <mirror_profile.h>
#include <handover_profile.h>
#else
#include <av.h>
#include <scofwd_profile.h>
#endif
#endif

/*!< Timed wait before polling for Peer SCOFWD connection status. */
#define DFU_INTERNAL_PEER_POLL_INTERVAL_MS 500

/*!< Task information for UPGRADE support */
dfu_task_data_t app_dfu;

/*! Identifiers for messages used internally by the DFU module */
typedef enum dfu_internal_messages_t
{
    DFU_INTERNAL_BASE = INTERNAL_MESSAGE_BASE + 0x80,

#ifdef INCLUDE_DFU_PEER
    DFU_INTERNAL_START_DATA_IND_ON_PEER_ERASE_DONE,

    DFU_INTERNAL_UPGRADE_APPLY_RES_ON_PEER_PROFILES_CONNECTED,

#ifndef INCLUDE_MIRRORING
    DFU_INTERNAL_POLL_PEER_SCOFWD_IS_CONNECTED
#endif
#endif

};
LOGGING_PRESERVE_MESSAGE_ENUM(dfu_internal_messages_t)
LOGGING_PRESERVE_MESSAGE_TYPE(dfu_messages_t)

/* The factory-set upgrade version and PS config version.

   After a successful upgrade the values from the upgrade header
   will be written to the upgrade PS key and used in future.
*/
static const upgrade_version earbud_upgrade_init_version = { 1, 0 };  /* Values should come from config file */
static const uint16 earbud_upgrade_init_config_version = 1;

/* The upgrade libraries use of partitions is not relevant to the
   partitions as used on devices targetted by this application.

   As it is not possible to pass 0 partitions in the Init function
   use a simple entry */
static const UPGRADE_UPGRADABLE_PARTITION_T logicalPartitions[]
                    = {UPGRADE_PARTITION_SINGLE(0x1000,DFU)
                      };

/*! Maximum size of buffer used to hold the variant string
    supplied by the application. 6 chars, plus NULL terminator */
#define VARIANT_BUFFER_SIZE (7)

static void dfu_MessageHandler(Task task, MessageId id, Message message);
static void dfu_GattConnect(uint16 cid);
static void dfu_GattDisconnect(uint16 cid);

static const gatt_connect_observer_callback_t dfu_gatt_connect_callback =
{
    .OnConnection = dfu_GattConnect,
    .OnDisconnection = dfu_GattDisconnect
};

static void dfu_NotifyStartedNeedConfirm(void)
{
    TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(Dfu_GetClientList()), DFU_REQUESTED_TO_CONFIRM);
}


static void dfu_NotifyStartedWithInProgress(void)
{
    TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(Dfu_GetClientList()), DFU_REQUESTED_IN_PROGRESS);
}


static void dfu_NotifyActivity(void)
{
    TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(Dfu_GetClientList()), DFU_ACTIVITY);
}


static void dfu_NotifyStart(void)
{
    TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(Dfu_GetClientList()), DFU_STARTED);
}

#ifdef INCLUDE_DFU_PEER
static void dfu_NotifyPreStart(void)
{
    TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(Dfu_GetClientList()), DFU_PRE_START);
}
#endif

static void dfu_NotifyCompleted(void)
{
    TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(Dfu_GetClientList()), DFU_COMPLETED);
}

static void dfu_NotifyAbort(void)
{
    TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(Dfu_GetClientList()), DFU_CLEANUP_ON_ABORT);
}

static void dfu_NotifyAborted(void)
{
    TaskList_MessageSendId(TaskList_GetFlexibleBaseTaskList(Dfu_GetClientList()), DFU_ABORTED);
}

/*************************************************************************
    Provide the logical partition map.

    For earbuds this is initially hard coded, but may come from other
    storage in time.
*/
static void dfu_GetLogicalPartitions(const UPGRADE_UPGRADABLE_PARTITION_T **partitions, uint16 *count)
{
    uint16 num_partitions = sizeof(logicalPartitions)/sizeof(logicalPartitions[0]);
    *partitions = logicalPartitions;
    *count = num_partitions;
}

/*************************************************************************
    Get the variant Id from the firmware and convert it into a variant
    string that can be passed to UpgradeInit.

    This function allocates a buffer for the string which must be freed
    after the call to UpgradeInit.
*/
static void dfu_GetVariant(char *variant, size_t length)
{
    int i = 0;
    char chr;
    uint32 product_id;

    PanicFalse(length >= VARIANT_BUFFER_SIZE);

    product_id = VmalVmReadProductId();
    if (product_id == 0)
    {
        variant[0] = '\0';
        return;
    }

    /* The product Id is encoded as two ascii chars + 4 integers in BCD format. */

    /* The ascii chars may be undefined or invalid (e.g. '\0').
       If so, don't include them in the variant string. */
    chr = (char)((product_id >> 8) & 0xFF);
    if (isalnum(chr))
        variant[i++] = chr;

    chr = (char)(product_id & 0xFF);
    if (isalnum(chr))
        variant[i++] = chr;

    sprintf(&variant[i], "%04X", ((uint16)((product_id >> 16) & 0xFFFF)));
}


/********************  PUBLIC FUNCTIONS  **************************/


bool Dfu_EarlyInit(Task init_task)
{
    UNUSED(init_task);

    DEBUG_LOG("Dfu_EarlyInit");

    TaskList_InitialiseWithCapacity(Dfu_GetClientList(), THE_DFU_CLIENT_LIST_INIT_CAPACITY);

    return TRUE;
}

/*! Initialisation point for the over the air support in the upgrade library.
 *
 */
bool Dfu_Init(Task init_task)
{
    dfu_task_data_t *the_dfu=Dfu_GetTaskData();
    uint16 num_partitions;
    const UPGRADE_UPGRADABLE_PARTITION_T *logical_partitions;
    char variant[VARIANT_BUFFER_SIZE];

    UNUSED(init_task);

    GattConnect_RegisterObserver(&dfu_gatt_connect_callback);

    the_dfu->dfu_task.handler = dfu_MessageHandler;

#ifdef INCLUDE_DFU_PEER
    /*
     * Register to use marshalled message channel with DFU domain for Peer DFU
     * messages.
     */
    appPeerSigMarshalledMsgChannelTaskRegister(Dfu_GetTask(),
        PEER_SIG_MSG_CHANNEL_DFU,
        dfu_peer_sig_marshal_type_descriptors,
        NUMBER_OF_DFU_PEER_SIG_MARSHAL_TYPES);

    /* Register for peer signaling notifications */
    appPeerSigClientRegister(Dfu_GetTask());

    ConManagerRegisterConnectionsClient(Dfu_GetTask());

#ifdef INCLUDE_MIRRORING
    /* Register for connect / disconnect events from mirror profile */
    MirrorProfile_ClientRegister(Dfu_GetTask());

    HandoverProfile_ClientRegister(Dfu_GetTask());
#else
    appAvStatusClientRegister(Dfu_GetTask());
#endif
#endif

    dfu_GetVariant(variant, sizeof(variant));

    dfu_GetLogicalPartitions(&logical_partitions, &num_partitions);

    /* Allow storage of info at end of (SINK_UPGRADE_CONTEXT_KEY) */
    UpgradeInit(Dfu_GetTask(), EARBUD_UPGRADE_CONTEXT_KEY, EARBUD_UPGRADE_LIBRARY_CONTEXT_OFFSET,
            logical_partitions,
            num_partitions,
            UPGRADE_INIT_POWER_MANAGEMENT,
            variant,
            upgrade_perm_always_ask,
            &earbud_upgrade_init_version,
            earbud_upgrade_init_config_version);

    return TRUE;
}


bool Dfu_HandleSystemMessages(MessageId id, Message message, bool already_handled)
{
    switch (id)
    {
        case MESSAGE_IMAGE_UPGRADE_ERASE_STATUS:
        case MESSAGE_IMAGE_UPGRADE_COPY_STATUS:
        case MESSAGE_IMAGE_UPGRADE_AUDIO_STATUS:
        case MESSAGE_IMAGE_UPGRADE_HASH_ALL_SECTIONS_UPDATE_STATUS:
        {
            Task upg = Dfu_GetTask();

            upg->handler(upg, id, message);
            return TRUE;
        }
    }
    return already_handled;
}

static void dfu_ForwardInitCfm(const UPGRADE_INIT_CFM_T *cfm)
{
    UPGRADE_INIT_CFM_T *copy = PanicUnlessNew(UPGRADE_INIT_CFM_T);
    *copy = *cfm;

    MessageSend(SystemState_GetTransitionTask(), UPGRADE_INIT_CFM, copy);
}

static void dfu_HandleRestartedInd(const UPGRADE_RESTARTED_IND_T *restart)
{
    /* This needs to base its handling on the reason in the message,
       for instance upgrade_reconnect_not_required is a hint that errr,
       reconnect isn't a priority. */

    DEBUG_LOG("dfu_HandleRestartedInd 0x%x", restart->reason);
    switch (restart->reason)
    {
        case upgrade_reconnect_not_required:
            /* No need to reconnect, not even sure why we got this */
            break;

        case upgrade_reconnect_required_for_confirm:
            dfu_NotifyStartedNeedConfirm();
            break;

        case upgrade_reconnect_recommended_as_completed:
        case upgrade_reconnect_recommended_in_progress:
            dfu_NotifyStartedWithInProgress();
            break;
    }
}


static void dfu_HandleUpgradeStatusInd(const UPGRADE_STATUS_IND_T *sts)
{
    dfu_NotifyActivity();

    switch (sts->state)
    {
        case upgrade_state_idle:
            DEBUG_LOG("dfu_HandleUpgradeStatusInd. idle(%d)",sts->state);
            break;

        case upgrade_state_downloading:
            DEBUG_LOG("dfu_HandleUpgradeStatusInd. downloading(%d)",sts->state);
            break;

        case upgrade_state_commiting:
            DEBUG_LOG("dfu_HandleUpgradeStatusInd. commiting(%d)",sts->state);
            break;

        case upgrade_state_done:
            DEBUG_LOG("dfu_HandleUpgradeStatusInd. done(%d)",sts->state);
            dfu_NotifyCompleted();
            break;

        default:
            DEBUG_LOG_ERROR("dfu_HandleUpgradeStatusInd. Unexpected state %d",sts->state);
            Panic();
            break;
    }
}

static void dfu_SwapImage(void)
{
    UpgradeImageSwap();
}

static void dfu_HandleUpgradeShutAudio(void)
{
    DEBUG_LOG("dfu_HandleUpgradeShutAudio");
    dfu_SwapImage();
}


static void dfu_HandleUpgradeCopyAudioImageOrSwap(void)
{
    DEBUG_LOG("dfu_HandleUpgradeCopyAudioImageOrSwap");
    dfu_SwapImage();
}

#ifdef INCLUDE_DFU_PEER
static void dfu_PeerEraseCompletedTx(bool success)
{
    bool is_secondary = !BtDevice_IsMyAddressPrimary();

    DEBUG_LOG("dfu_PeerEraseCompletedTx is_secondary:%d, success:%d", is_secondary, success);

    if (is_secondary)
    {
        dfu_peer_erase_req_res_t *ind = PanicUnlessMalloc(sizeof(dfu_peer_erase_req_res_t));
        memset(ind, 0, sizeof(dfu_peer_erase_req_res_t));

        /* Erase response sent */
        ind->peer_erase_req_res = FALSE;
        ind->peer_erase_status = success;
        appPeerSigMarshalledMsgChannelTx(Dfu_GetTask(),
                                        PEER_SIG_MSG_CHANNEL_DFU,
                                        ind, MARSHAL_TYPE(dfu_peer_erase_req_res_t));
    }
}

static void dfu_PeerEraseCompletedRx(dfu_peer_erase_req_res_t *msg)
{
    bool is_primary = BtDevice_IsMyAddressPrimary();
    bool peer_erase_status = msg->peer_erase_status;

    DEBUG_LOG("dfu_PeerEraseCompletedRx is_primary:%d, peer_erase_status:%d", is_primary, peer_erase_status);

    if (is_primary)
    {
        dfu_task_data_t *the_dfu = Dfu_GetTaskData();

        if (msg->peer_erase_status)
        {
            /* Erase response was successful */

            /*
             * Unblock the conditionally queued
             * DFU_INTERNAL_START_DATA_IND_ON_PEER_ERASE_DONE
             */
            the_dfu->peerEraseDone = FALSE;
        }
        else
        {
            /* Erase response was failure */

            /*
             * One of the erase failure on the peer (Secondary) is out of
             * memory. Though its possible that memory is reclaimed later but
             * still we abort. Because if we progress DFU then peer DFU erase
             * shall be not driven simulataneously to local. And if in such
             * scenarios, DFU is abrupted with local reset when erase was
             * triggered then post reset when the roles are dynamically set and
             * DFU is resumed (as applicable) and also the earbuds are out of
             * case, then profile establishment with the handset and peer can
             * occur concurrently. The profile establishment probably update
             * psstore and since erase is ongoing, it may probably cause the
             * psstore operations to blocked and invariably apps P1 too.
             * To protect against undefined behavior especially panic on
             * assert in concurrent profile establishment, its better to
             * gracefully abort the DFU.
             *
             * Note: Generic error code reported to the host.
             */
            UpgradeHandleAbortDuringUpgrade();

            /*
             * Dont unblock DFU_INTERNAL_START_DATA_IND_ON_PEER_ERASE_DONE
             * if queued rather cancel as DFU is anyhow abored.
             */
            MessageCancelAll(Dfu_GetTask(), 
                                DFU_INTERNAL_START_DATA_IND_ON_PEER_ERASE_DONE);
        }
    }
}

static void dfu_PeerEraseStartTx(void)
{
    bool is_primary = BtDevice_IsMyAddressPrimary();

    DEBUG_LOG("dfu_PeerEraseStartTx is_primary:%d", is_primary);

    if (is_primary)
    {
        dfu_task_data_t *the_dfu = Dfu_GetTaskData();
        dfu_peer_erase_req_res_t *ind = PanicUnlessMalloc(sizeof(dfu_peer_erase_req_res_t));
        memset(ind, 0, sizeof(dfu_peer_erase_req_res_t));

        /*
         * Block/Hold DFU_INTERNAL_START_DATA_IND_ON_PEER_ERASE_DONE until
         * peer (Secondary) erase is done.
         */
        the_dfu->peerEraseDone = TRUE;

        /* Erase request sent */
        ind->peer_erase_req_res = TRUE;
        appPeerSigMarshalledMsgChannelTx(Dfu_GetTask(),
                                        PEER_SIG_MSG_CHANNEL_DFU,
                                        ind, MARSHAL_TYPE(dfu_peer_erase_req_res_t));
    }
}

static void dfu_PeerEraseStartRx(void)
{
    bool is_secondary = !BtDevice_IsMyAddressPrimary();

    DEBUG_LOG("dfu_PeerEraseStartRx is_secondary:%d", is_secondary);

    if (is_secondary)
    {
        bool wait_for_erase_complete;
        if (UpgradePartitionDataInitWrapper(&wait_for_erase_complete))
        {
            DEBUG_LOG("dfu_PeerEraseStartRx wait_for_erase_complete:%d", wait_for_erase_complete);
            if (!wait_for_erase_complete)
            {
                /* Already erased, response sent as success */
                dfu_PeerEraseCompletedTx(TRUE);
            }
        }
        else
        {
            DEBUG_LOG("dfu_PeerEraseStartRx no_memory error");
            /* Erase response sent as failed */
            dfu_PeerEraseCompletedTx(FALSE);
        }
    }
}

static void dfu_HandlePeerSigMarshalledMsgChannelRxInd(const PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND_T *ind)
{
    DEBUG_LOG("dfu_HandlePeerSigMarshalledMsgChannelRxInd. Channel 0x%x, type %d", ind->channel, ind->type);

    switch (ind->type)
    {
        case MARSHAL_TYPE_dfu_peer_erase_req_res_t:
            {
                dfu_peer_erase_req_res_t *msg = (dfu_peer_erase_req_res_t *)ind->msg;
                if (msg->peer_erase_req_res)
                {
                    /* Erase request received */
                    dfu_PeerEraseStartRx();
                }
                else
                {
                    /* Erase response received */
                    dfu_PeerEraseCompletedRx(msg);
                }
            }
            break;

        default:
            break;
    }

    /* free unmarshalled msg */
    free(ind->msg);
}

static void dfu_HandlePeerSigMarshalledMsgChannelTxCfm(const PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM_T *cfm)
{
    peerSigStatus status = cfm->status;

    if (peerSigStatusSuccess != status)
    {
        DEBUG_LOG("dfu_HandlePeerSigMarshalledMsgChannelTxCfm reports failure code 0x%x(%d)", status, status);
    }

}

static void dfu_HandlePeerSigConnectInd(const PEER_SIG_CONNECTION_IND_T *ind)
{
    DEBUG_LOG("dfu_HandlePeerSigConnectInd, status %u", ind->status);

    /*
     * Make DFU domain aware of the current device role (Primary/Secondary)
     */
    if (ind->status == peerSigStatusConnected)
    {
        dfu_task_data_t *the_dfu = Dfu_GetTaskData();
        bool is_primary = BtDevice_IsMyAddressPrimary();

        if (is_primary)
        {
            the_dfu->peerProfilesToConnect &= ~DEVICE_PROFILE_PEERSIG;
            DEBUG_LOG("dfu_HandlePeerSigConnectInd (profiles:x%x) pending to connect", the_dfu->peerProfilesToConnect);
        }
        else
        {
            /*Cancel pending UPGRADE_PEER_CONNECT_REQ, if any*/
            UpgradePeerCancelDFU();
        }

        Dfu_SetRole(is_primary);

        /* Unblock the peer DFU L2CAP connection (if any) */
        UpgradePeerUpdateBlockCond(UPGRADE_PEER_BLOCK_NONE);

        /* If the reboot reason is a defined reset as part of DFU process, then
         * start the peer connection once again, and continue with commit phase
         */
        if(Dfu_GetRebootReason() == REBOOT_REASON_DFU_RESET)
        {
            DEBUG_LOG("dfu_HandlePeerSigConnectInd: UpgradePeerApplicationReconnect()");
            /* Device is  restarted in upgrade process, send connect request again */
            UpgradePeerApplicationReconnect();
        }

    }
    /* In Panic situation, the peer device gets disconneted and peerSigStatusDisconnected is sent by peer_signalling which needs to be handled */
    else if (ind->status == peerSigStatusLinkLoss || ind->status == peerSigStatusDisconnected)
    {
        /*
         * In the post reboot DFU commit phase, now main role (Primary/Secondary)
         * are no longer fixed rather dynamically selected by Topology using role
         * selection. This process may take time so its recommendable to reset
         * this reconnection timer in linkloss scenarios (if any) in the post
         * reboot DFU commit phase.
         */
        UpgradeRestartReconnectionTimer();

        /* Block the peer DFU L2CAP connection in cases of link-loss to peer */
        UpgradePeerUpdateBlockCond(UPGRADE_PEER_BLOCK_UNTIL_PEER_SIG_CONNECTED);
    }

}

static void dfu_HandleConManagerConnectionInd(const CON_MANAGER_CONNECTION_IND_T* ind)
{
    bool is_dfu_mode = UpgradePeerIsDFUMode();
    bool is_primary = BtDevice_IsMyAddressPrimary();

    DEBUG_LOG("dfu_HandleConManagerConnectionInd Conn:%u BLE:%u %04x,%02x,%06lx", ind->connected,
                                                                                          ind->ble,
                                                                                          ind->bd_addr.nap,
                                                                                          ind->bd_addr.uap,
                                                                                          ind->bd_addr.lap);
    if(!ind->ble && appDeviceIsPeer(&ind->bd_addr) && ind->connected && is_dfu_mode && is_primary)
    {
        dfu_task_data_t *the_dfu = Dfu_GetTaskData();
        the_dfu->peerProfilesToConnect = appPhyStateGetState() == PHY_STATE_IN_CASE ?
                        DEVICE_PROFILE_PEERSIG :
#ifndef INCLUDE_MIRRORING
                        /*
                         * After A2DP, AVRCP is subsequently setup, so shall
                         * await AVRCP connection establishment too.
                         */
                        DEVICE_PROFILE_AVRCP |
#endif
                            TwsTopologyConfig_PeerProfiles();
        DEBUG_LOG("dfu_HandleConManagerConnectionInd PEER BREDR Connected (profiles:x%x) to connect", the_dfu->peerProfilesToConnect);
    }
}
#endif

static void dfu_MessageHandler(Task task, MessageId id, Message message)
{
    UNUSED(task);
    UNUSED(message);

    DEBUG_LOG("dfu_MessageHandler. MESSAGE:dfu_internal_messages_t:0x%X", id);

    switch (id)
    {
#ifdef INCLUDE_DFU_PEER
        case PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND:
            dfu_HandlePeerSigMarshalledMsgChannelRxInd((PEER_SIG_MARSHALLED_MSG_CHANNEL_RX_IND_T *)message);
            break;

        case PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM:
            dfu_HandlePeerSigMarshalledMsgChannelTxCfm((PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM_T *)message);
            break;

        case CON_MANAGER_CONNECTION_IND:
            dfu_HandleConManagerConnectionInd((CON_MANAGER_CONNECTION_IND_T*)message);
            break;

        case PEER_SIG_CONNECTION_IND:
            dfu_HandlePeerSigConnectInd((const PEER_SIG_CONNECTION_IND_T *)message);
            break;

#ifdef INCLUDE_MIRRORING
        /* MIRROR PROFILE MESSAGES */
        case MIRROR_PROFILE_CONNECT_IND:
            {
                dfu_task_data_t *the_dfu = Dfu_GetTaskData();
                bool is_primary = BtDevice_IsMyAddressPrimary();

                if (is_primary)
                {
                    the_dfu->peerProfilesToConnect &= ~DEVICE_PROFILE_MIRROR;
                    DEBUG_LOG("dfu_MessageHandler (profiles:x%x) pending to connect", the_dfu->peerProfilesToConnect);
                }
            }
            break;

        case HANDOVER_PROFILE_CONNECTION_IND:
            {
                dfu_task_data_t *the_dfu = Dfu_GetTaskData();
                bool is_primary = BtDevice_IsMyAddressPrimary();

                if (is_primary)
                {
                    the_dfu->peerProfilesToConnect &= ~DEVICE_PROFILE_HANDOVER;
                    DEBUG_LOG("dfu_MessageHandler (profiles:x%x) pending to connect", the_dfu->peerProfilesToConnect);
                }
            }
            break;
#else
        /* AV status change indications */
        case AV_A2DP_CONNECTED_IND:
            {
                const AV_A2DP_CONNECTED_IND_T * ind = (AV_A2DP_CONNECTED_IND_T *)message;
                dfu_task_data_t *the_dfu = Dfu_GetTaskData();
                bool is_primary = BtDevice_IsMyAddressPrimary();

                DEBUG_LOG("dfu_MessageHandler AV_A2DP_CONNECTED_IND %04x,%02x,%06lx", ind->bd_addr.nap, ind->bd_addr.uap, ind->bd_addr.lap);

                if (is_primary && appDeviceIsPeer(&ind->bd_addr))
                {
                    the_dfu->peerProfilesToConnect &= ~DEVICE_PROFILE_A2DP;
                    DEBUG_LOG("dfu_MessageHandler (profiles:x%x) pending to connect", the_dfu->peerProfilesToConnect);

                    /*
                     * SCOFWD profile only provides SFWD_CONNECT_CFM which is
                     * sent to the task that initiated the connection.
                     * SCOFWD connection is initiated elsewhere by Topology
                     * goal/procedure but DFU domain needs to track the
                     * SCOFWD connection and the only way is to poll/check
                     * on other profile connection indications as all these
                     * profile connections are triggered by the same Topology
                     * goal/procedure.
                     * After arrival of other profile connection indications,
                     * give an additional delay to ensure SCOFWD is established.
                     * Ideally SCOFWD should be setup earlier than A2DP/AVRCP
                     * based on the connection order defined by the Topology
                     * goal/procedure.
                     */
                    if (ScoFwdIsConnected())
                    {
                        the_dfu->peerProfilesToConnect &= ~DEVICE_PROFILE_SCOFWD;
                        DEBUG_LOG("dfu_MessageHandler (profiles:x%x) pending to connect", the_dfu->peerProfilesToConnect);
                    }
                    else
                    {
                        MessageCancelAll(Dfu_GetTask(),
                                        DFU_INTERNAL_POLL_PEER_SCOFWD_IS_CONNECTED);
                        MessageSendLater(Dfu_GetTask(),
                                    DFU_INTERNAL_POLL_PEER_SCOFWD_IS_CONNECTED,
                                    NULL, DFU_INTERNAL_PEER_POLL_INTERVAL_MS);
                    }
                }
            }
            break;

        case AV_AVRCP_CONNECTED_IND:
            {
                const AV_AVRCP_CONNECTED_IND_T * ind = (AV_AVRCP_CONNECTED_IND_T *)message;
                dfu_task_data_t *the_dfu = Dfu_GetTaskData();
                bool is_primary = BtDevice_IsMyAddressPrimary();

                DEBUG_LOG("dfu_MessageHandler AV_AVRCP_CONNECTED_IND %04x,%02x,%06lx", ind->bd_addr.nap, ind->bd_addr.uap, ind->bd_addr.lap);

                if (is_primary && appDeviceIsPeer(&ind->bd_addr))
                {
                    the_dfu->peerProfilesToConnect &= ~DEVICE_PROFILE_AVRCP;
                    DEBUG_LOG("dfu_MessageHandler (profiles:x%x) pending to connect", the_dfu->peerProfilesToConnect);

                    /* Same explanation as under AV_A2DP_CONNECTED_IND */
                    if (ScoFwdIsConnected())
                    {
                        the_dfu->peerProfilesToConnect &= ~DEVICE_PROFILE_SCOFWD;
                        DEBUG_LOG("dfu_MessageHandler (profiles:x%x) pending to connect", the_dfu->peerProfilesToConnect);
                    }
                    else
                    {
                        MessageCancelAll(Dfu_GetTask(),
                                        DFU_INTERNAL_POLL_PEER_SCOFWD_IS_CONNECTED);
                        MessageSendLater(Dfu_GetTask(),
                                    DFU_INTERNAL_POLL_PEER_SCOFWD_IS_CONNECTED,
                                    NULL, DFU_INTERNAL_PEER_POLL_INTERVAL_MS);
                    }
                }
            }
            break;

        case DFU_INTERNAL_POLL_PEER_SCOFWD_IS_CONNECTED:
            {
                dfu_task_data_t *the_dfu = Dfu_GetTaskData();

                /*
                 * Cancel if any, one possiblility is when A2DP and AVRCP
                 * connection indication arrive simultanously.
                 */
                MessageCancelAll(Dfu_GetTask(),
                                DFU_INTERNAL_POLL_PEER_SCOFWD_IS_CONNECTED);

                /*
                 * By now SCOFWD should have been setup (as explained under
                 * AV_A2DP_CONNECTED_IND). But if its not then its safe to
                 * unconditionally mark SCOFWD connection as complete.
                 */
                DEBUG_LOG("dfu_MessageHandler ScoFwdIsConnected:%d", ScoFwdIsConnected());
                the_dfu->peerProfilesToConnect &= ~DEVICE_PROFILE_SCOFWD;
                DEBUG_LOG("dfu_MessageHandler (profiles:x%x) pending to connect", the_dfu->peerProfilesToConnect);
            }
#endif
#endif

            /* Message sent in response to UpgradeInit().
             * In this case we need to forward to the app to unblock initialisation.
             */
        case UPGRADE_INIT_CFM:
            {
                const UPGRADE_INIT_CFM_T *init_cfm = (const UPGRADE_INIT_CFM_T *)message;

                DEBUG_LOG("dfu_MessageHandler. UPGRADE_INIT_CFM %d (sts)",init_cfm->status);

                dfu_ForwardInitCfm(init_cfm);
            }
            break;

            /* Message sent during initialisation of the upgrade library
                to let the VM application know that a restart has occurred
                and reconnection to a host may be required. */
        case UPGRADE_RESTARTED_IND:
            dfu_HandleRestartedInd((UPGRADE_RESTARTED_IND_T*)message);
            break;

            /* Message sent to application to request applying a downloaded upgrade.
                Note this may include a warm reboot of the device.
                Application must respond with UpgradeApplyResponse() */
        case UPGRADE_APPLY_IND:
            {
#ifdef INCLUDE_DFU_PEER
                bool isPrimary = BtDevice_IsMyAddressPrimary();
                dfu_task_data_t *the_dfu = Dfu_GetTaskData();

                DEBUG_LOG("dfu_MessageHandler UPGRADE_APPLY_IND, isPrimary:%d", isPrimary);
#ifndef HOSTED_TEST_ENVIRONMENT
                if (isPrimary)
                {
                    /*
                     * As per the legacy scheme, Primary reboots post Secondary
                     * having rebooted. And as part of Secondary reboot, the
                     * peer links (including DFU L2CAP channel) are
                     * re-established. Wait for the connections of these other
                     * peer profiles to complete before Primary reboot, in order
                     * to avoid undefined behavior on the Secondary
                     * (such as panic on asserts) owing to invalid connection
                     * handles while handling disconnection sequence because
                     * of linkloss to Primary if the Primary didn't await for
                     * the peer profile connections to complete.
                     * Since no direct means to cancel the peer connection from
                     * DFU domain except Topology which can cancel through
                     * cancellable goals, for now its better to wait for the
                     * peer profile connections to be done before Primary
                     * reboot for a deterministic behavior and avoid the problem
                     * as described above.
                     *
                     * (Note: The invalid connection handle problem was seen
                     *        with Mirror Profile.)
                     *
                     */
                    MessageSendConditionally(Dfu_GetTask(),
                                            DFU_INTERNAL_UPGRADE_APPLY_RES_ON_PEER_PROFILES_CONNECTED, NULL,
                                            (uint16 *)&the_dfu->peerProfilesToConnect);
                }
                else
#endif
#endif
                {
                    DEBUG_LOG("dfu_MessageHandler. UPGRADE_APPLY_IND saying now !");
                    dfu_NotifyActivity();
                    UpgradeApplyResponse(0);
                }
            }
            break;

            /* Message sent to application to request blocking the system for an extended
                period of time to erase serial flash partitions.
                Application must respond with UpgradeBlockingResponse() */
        case UPGRADE_BLOCKING_IND:
            DEBUG_LOG("dfu_MessageHandler. UPGRADE_BLOCKING_IND");
            dfu_NotifyActivity();
            UpgradeBlockingResponse(0);
            break;

            /* Message sent to application to indicate that blocking operation is finished */
        case UPGRADE_BLOCKING_IS_DONE_IND:
            DEBUG_LOG("dfu_MessageHandler. UPGRADE_BLOCKING_IS_DONE_IND");
            dfu_NotifyActivity();
            break;

            /* Message sent to application to inform of the current status of an upgrade. */
        case UPGRADE_STATUS_IND:
            dfu_HandleUpgradeStatusInd((const UPGRADE_STATUS_IND_T *)message);
            break;

            /* Message sent to application to request any audio to get shut */
        case UPGRADE_SHUT_AUDIO:
            dfu_HandleUpgradeShutAudio();
            break;

            /* Message sent to application set the audio busy flag and copy audio image */
        case UPRGADE_COPY_AUDIO_IMAGE_OR_SWAP:
            dfu_HandleUpgradeCopyAudioImageOrSwap();
            break;

            /* Message sent to application to reset the audio busy flag */
        case UPGRADE_AUDIO_COPY_FAILURE:
            DEBUG_LOG("dfu_MessageHandler. UPGRADE_AUDIO_COPY_FAILURE (not handled)");
            break;

#ifdef INCLUDE_DFU_PEER
        case UPGRADE_START_PEER_ERASE_IND:
            DEBUG_LOG("dfu_MessageHandler. UPGRADE_START_PEER_ERASE_IND");
            /*
             * Notify application that DFU is about to start so that DFU timers
             * can be cancelled to avoid false DFU timeouts owing to actual
             * DFU start indication being deferred as DFU erase was ongoing.
             */
            dfu_NotifyPreStart();
            dfu_PeerEraseStartTx();
            break;
#endif

            /* Message sent to application to inform that the actual upgrade has started */
        case UPGRADE_START_DATA_IND:
            {    
#ifdef INCLUDE_DFU_PEER
                bool is_primary = BtDevice_IsMyAddressPrimary();
                dfu_task_data_t *the_dfu = Dfu_GetTaskData();

                DEBUG_LOG("dfu_MessageHandler UPGRADE_START_DATA_IND, is_primary:%d", is_primary);
#ifndef HOSTED_TEST_ENVIRONMENT
                if (is_primary)
                {
                    MessageSendConditionally(Dfu_GetTask(),
                                            DFU_INTERNAL_START_DATA_IND_ON_PEER_ERASE_DONE, NULL,
                                            (uint16 *)&the_dfu->peerEraseDone);
                }
                else
#endif
#endif
                {
                    dfu_NotifyStart();
                }
            }
            break;

            /* Message sent to application to inform that the actual upgrade has ended */
        case UPGRADE_END_DATA_IND:
            {
                UPGRADE_END_DATA_IND_T *end_data_ind = (UPGRADE_END_DATA_IND_T *)message;
                DEBUG_LOG("dfu_MessageHandler. UPGRADE_END_DATA_IND %d (handled for abort indication)", end_data_ind->state);

#ifdef INCLUDE_DFU_PEER
                /*
                 * If DFU is ended either as complete or aborted (device
                 * initiated: Handover or internal FatalError OR Host 
                 * initiated), cancel the queued DFU start indication (if any)
                 * as its pointless to notify start indication after DFU has
                 * ended.
                 */
                MessageCancelAll(Dfu_GetTask(),
                                DFU_INTERNAL_START_DATA_IND_ON_PEER_ERASE_DONE);
#endif

                /* Notify application that upgrade has ended owing to abort. */
                if (end_data_ind->state == upgrade_end_state_abort)
                    dfu_NotifyAborted();
            }
            break;

            /* Message sent to application to inform for cleaning up DFU state variables on Abort */
        case UPGRADE_CLEANUP_ON_ABORT:
            DEBUG_LOG("dfu_MessageHandler. UPGRADE_CLEANUP_ON_ABORT");
            dfu_NotifyAbort();
            break;

#ifdef INCLUDE_DFU_PEER
        case DFU_INTERNAL_START_DATA_IND_ON_PEER_ERASE_DONE:
            {
                bool isPrimary = BtDevice_IsMyAddressPrimary();
                DEBUG_LOG("dfu_MessageHandler. DFU_INTERNAL_START_DATA_IND_ON_PEER_ERASE_DONE");
                dfu_NotifyStart();

                /*
                 * Ideally the handled msg is triggered on the Primary.
                 * Even then its still safe to rely on bt_device to pass the
                 * appropriate main role (i.e. Primary/Secondary).
                 *
                 * This is required because concurrent DFU is always started by
                 * the Primary and if the role is not updated then peer DFU(
                 * (either conncurrent or serial) shall fail to start.
                 */
                Dfu_SetRole(isPrimary);

                UpgradeStartConcurrentDFU();
            }
            break;

            case DFU_INTERNAL_UPGRADE_APPLY_RES_ON_PEER_PROFILES_CONNECTED:
            {
                DEBUG_LOG("dfu_MessageHandler. DFU_INTERNAL_UPGRADE_APPLY_RES_ON_PEER_PROFILES_CONNECTED, Respond to UPGRADE_APPLY_IND now!");
                dfu_NotifyActivity();
                UpgradeApplyResponse(0);
            }
            break;
#endif

        case MESSAGE_IMAGE_UPGRADE_ERASE_STATUS:
            DEBUG_LOG("dfu_MessageHandler. MESSAGE_IMAGE_UPGRADE_ERASE_STATUS");

            dfu_NotifyActivity();
#ifdef INCLUDE_DFU_PEER
            MessageImageUpgradeEraseStatus *msg =
                                    (MessageImageUpgradeEraseStatus *)message;
            dfu_PeerEraseCompletedTx(msg->erase_status);
#endif
            UpgradeEraseStatus(message);
            break;

        case MESSAGE_IMAGE_UPGRADE_COPY_STATUS:
            DEBUG_LOG("dfu_MessageHandler. MESSAGE_IMAGE_UPGRADE_COPY_STATUS");

            dfu_NotifyActivity();
            UpgradeCopyStatus(message);
            break;

        case MESSAGE_IMAGE_UPGRADE_HASH_ALL_SECTIONS_UPDATE_STATUS:
            DEBUG_LOG("dfu_MessageHandler. MESSAGE_IMAGE_UPGRADE_HASH_ALL_SECTIONS_UPDATE_STATUS");
            UpgradeHashAllSectionsUpdateStatus(message);
            break;
            /* Catch-all panic for unexpected messages */
        default:
            if (UPGRADE_UPSTREAM_MESSAGE_BASE <= id && id <  UPGRADE_UPSTREAM_MESSAGE_TOP)
            {
                DEBUG_LOG_ERROR("dfu_MessageHandler. Unexpected upgrade library message MESSAGE:0x%x", id);
            }
#ifdef INCLUDE_DFU_PEER
            else if (PEER_SIG_INIT_CFM <= id && id <= PEER_SIG_LINK_LOSS_IND)
            {
                DEBUG_LOG("dfu_MessageHandler. Unhandled peer sig message MESSAGE:0x%x", id);
            }
#endif
            else
            {
                DEBUG_LOG_ERROR("dfu_MessageHandler. Unexpected message MESSAGE:dfu_internal_messages_t:0x%X", id);
            }
            break;
    }

}

static void dfu_GattConnect(uint16 cid)
{
    DEBUG_LOG("dfu_GattConnect. cid:0x%X", cid);
    /* Send GATT service change request */
    GattServerGatt_SetServerServicesChanged(cid);
}

static void dfu_GattDisconnect(uint16 cid)
{
    DEBUG_LOG("dfu_GattDisconnect. cid:0x%X", cid);

    /* We choose not to do anything when GATT is disconnect */
}

bool Dfu_AllowUpgrades(bool allow)
{
    upgrade_status_t sts = (upgrade_status_t)-1;
    bool successful = FALSE;

    /* The Upgrade library API can panic very easily if UpgradeInit had
       not been called previously */
    if (SystemState_GetState() > system_state_initialised)
    {
         sts = UpgradePermit(allow ? upgrade_perm_assume_yes : upgrade_perm_no);
         successful = (sts == upgrade_status_success);
    }

    DEBUG_LOG("Dfu_AllowUpgrades(%d) - success:%d (sts:%d)", allow, successful, sts);

    return successful;
}

void Dfu_ClientRegister(Task tsk)
{
    TaskList_AddTask(TaskList_GetFlexibleBaseTaskList(Dfu_GetClientList()), tsk);
}

void Dfu_SetContext(dfu_context_t context)
{
    uint16 actual_length = PsRetrieve(EARBUD_UPGRADE_CONTEXT_KEY, NULL, 0);
    if ((actual_length > 0) && (actual_length <= PSKEY_MAX_STORAGE_LENGTH))
    {
        uint16 key_cache[PSKEY_MAX_STORAGE_LENGTH];
        PsRetrieve(EARBUD_UPGRADE_CONTEXT_KEY, key_cache, actual_length);
        key_cache[APP_UPGRADE_CONTEXT_OFFSET] = (uint16) context;
        PsStore(EARBUD_UPGRADE_CONTEXT_KEY, key_cache, actual_length);
    }
}

dfu_context_t Dfu_GetContext(void)
{
    dfu_context_t context = DFU_CONTEXT_UNUSED;
    uint16 actual_length = PsRetrieve(EARBUD_UPGRADE_CONTEXT_KEY, NULL, 0);
    if ((actual_length > 0) && (actual_length <= PSKEY_MAX_STORAGE_LENGTH))
    {
        uint16 key_cache[PSKEY_MAX_STORAGE_LENGTH];
        PsRetrieve(EARBUD_UPGRADE_CONTEXT_KEY, key_cache, actual_length);
        context = (dfu_context_t) key_cache[APP_UPGRADE_CONTEXT_OFFSET];
    }

    return context;
}

/*! Abort the ongoing Upgrade if the device is disconnected from GAIA app */
void Dfu_AbortDuringDeviceDisconnect(void)
{
    DEBUG_LOG("Dfu_AbortDuringDeviceDisconnect");
    UpgradeAbortDuringDeviceDisconnect();
}

/*! \brief Return REBOOT_REASON_DFU_RESET for defined reboot phase of upgrade
           else REBOOT_REASON_ABRUPT_RESET for abrupt reset.
 */
dfu_reboot_reason_t Dfu_GetRebootReason(void)
{
    return Dfu_GetTaskData()->dfu_reboot_reason;
}

/*! \brief Set to REBOOT_REASON_DFU_RESET for defined reboot phase of upgrade
           else REBOOT_REASON_ABRUPT_RESET for abrupt reset.
 */
void Dfu_SetRebootReason(dfu_reboot_reason_t val)
{
    Dfu_GetTaskData()->dfu_reboot_reason = val;
}

extern bool UpgradePSClearStore(void);
/*! \brief Clear upgrade related PSKeys.
 */
bool Dfu_ClearPsStore(void)
{
    /* Clear out any in progress DFU status */
    return UpgradePSClearStore();
}

void Dfu_SetRole(bool role)
{
#ifdef INCLUDE_DFU_PEER
    DfuPeer_SetRole(role);
    UpgradePeerSetRole(role);
#else
    UNUSED(role);
#endif
}

uint32 Dfu_GetDFUHandsetProfiles(void)
{
    return (DEVICE_PROFILE_GAIA | DEVICE_PROFILE_GAA | DEVICE_PROFILE_ACCESSORY);
}

#endif /* INCLUDE_DFU */
