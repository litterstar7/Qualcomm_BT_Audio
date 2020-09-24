/****************************************************************************
Copyright (c) 2019-2020 Qualcomm Technologies International, Ltd.


FILE NAME
    upgrade_peer.c

DESCRIPTION
    This file handles Upgrade Peer connection state machine and DFU file
    transfers.

NOTES

*/

#define DEBUG_LOG_MODULE_NAME upgrade_peer
#include <logging.h>
DEBUG_LOG_DEFINE_LEVEL_VAR

#include <ps.h>

#include "upgrade_peer_private.h"
#include "upgrade_msg_host.h"
#include "upgrade.h"
#include "upgrade_ctx.h"
#include "upgrade_sm.h"
#include "upgrade_partition_data.h"

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_TYPE(upgrade_peer_internal_msg_t)

/*
 * Optimize back to back pmalloc pool request while handling
 * UPGRADE_PEER_START_DATA_BYTES_REQ from the peer and framing the response DFU
 * data pdu UPGRADE_HOST_DATA in the same context.
 *
 * This optimization is much required especially when the DFU data pdus are
 * large sized and when concurrent DFU is ongoing (i.e both the earbuds are in
 * the data transfer phase).
 *
 * NOTE: Comment the following define, to strip off this optimization.
 */
#define UPGRADE_PEER_DATA_PMALLOC_OPTIMIZE

#define SHORT_MSG_SIZE (sizeof(uint8) + sizeof(uint16))

/* PSKEYS are intentionally limited to 32 words to save stack. */
#define PSKEY_MAX_STORAGE_LENGTH    32

#define MAX_START_REQ_RETRAIL_NUM       (5)

#define HOST_MSG(x)     (x + UPGRADE_HOST_MSG_BASE)

#define INTERNAL_PEER_MSG_DELAY 5000

#define INTERNAL_PEER_MSG_VALIDATION_SEND_DELAY D_SEC(2)

UPGRADE_PEER_INFO_T *upgradePeerInfo = NULL;

/* This is used to store upgrade context when reboot happens */
typedef struct {
    uint16 length;
    uint16 ram_copy[PSKEY_MAX_STORAGE_LENGTH];
} FSTAB_PEER_COPY;

static void SendAbortReq(void);
static void SendConfirmationToPeer(upgrade_confirmation_type_t type,
                                   upgrade_action_status_t status);
static void SendTransferCompleteReq(upgrade_action_status_t status);
static void SendCommitCFM(upgrade_action_status_t status);
static void SendInProgressRes(upgrade_action_status_t status);
#ifndef UPGRADE_PEER_DATA_PMALLOC_OPTIMIZE
static void SendPeerData(uint8 *data, uint16 dataSize);
#else
static void SendPeerData(uint8 *data, uint16 dataSize, bool isPreAllocated);
#endif
static void SendStartReq (void);
static void SendErrorConfirmation(uint16 error);
static void SendValidationDoneReq (void);
static void SendSyncReq (uint32 md5_checksum);
static void SendStartDataReq (void);
static UPGRADE_PEER_CTX_T *UpgradePeerCtxGet(void);
static void StopUpgrade(void);

/**
 * Set resume point as provided by Secondary device.
 */
static void SetResumePoint(upgrade_peer_resume_point_t point)
{
    upgradePeerInfo->SmCtx->mResumePoint = point;
}

/**
 * Return the abort status of Peer DFU.
 */
bool UpgradePeerIsPeerDFUAborted(void)
{
    if(upgradePeerInfo != NULL)
    {
        return upgradePeerInfo ->is_dfu_aborted;
    }
    else
    {
        DEBUG_LOG("UpgradePeerIsPeerDFUAborted: Invalid access of upgradePeerInfo ptr. Panic the app");
        Panic();
        /* To satisfy the compiler */
        return FALSE;
    }
}

/**
 * Abort Peer upgrade is case error occurs.
 */
static void AbortPeerDfu(void)
{
    DEBUG_LOG("AbortPeerDfu: UpgradePeer: Abort DFU");

    /* Once connection is establish, first send abort to peer device */
    if (upgradePeerInfo->SmCtx->isUpgrading)
    {
        SendAbortReq();
        upgradePeerInfo->SmCtx->isUpgrading = FALSE;
    }
    else
    {
        /* We are here because user has aborted upgrade and peer device
         * upgrade is not yet started. Send Link disconnetc request.
         */
        if(upgradePeerInfo->SmCtx->peerState == UPGRADE_PEER_STATE_SYNC)
        {
            StopUpgrade();
        }
    }
    /* Since the Peer DFU is aborted now, set this to TRUE*/
    upgradePeerInfo->is_dfu_aborted = TRUE;
}

/**
 * Primary device got confirmation from Host. Send this to secondary device
 * now.
 */
static void SendConfirmationToPeer(upgrade_confirmation_type_t type,
                                   upgrade_action_status_t status)
{
    DEBUG_LOG("SendConfirmationToPeer: UpgradePeer: Confirm to Peer %x, %x", type, status);

    switch (type)
    {
        case UPGRADE_TRANSFER_COMPLETE:
            if(status == UPGRADE_CONTINUE)
            {
                SendTransferCompleteReq(status);
            }
            else
            {
                AbortPeerDfu();
            }
            break;

        case UPGRADE_COMMIT:
            SendCommitCFM(status);
            break;

        case UPGRADE_IN_PROGRESS:
            if(status == UPGRADE_CONTINUE)
            {
                SendInProgressRes(status);
            }
            else
            {
                AbortPeerDfu();
            }
            break;
        default:
            DEBUG_LOG("SendConfirmationToPeer: UpgradePeer: unhandled msg");
    }

    if (status == UPGRADE_ABORT)
    {
        AbortPeerDfu();
    }
}

/**
 * To continue the process this manager needs the listener to confirm it.
 *
 */
static void AskForConfirmation(upgrade_confirmation_type_t type)
{
    upgradePeerInfo->SmCtx->confirm_type = type;
    DEBUG_LOG("AskForConfirmationL UpgradePeer: Ask Confirmation %x", type);

    switch (type)
    {
        case UPGRADE_TRANSFER_COMPLETE:
            /* Send message to UpgradeSm indicating TRANSFER_COMPLETE_IND */
            UpgradeHandleMsg(NULL, HOST_MSG(UPGRADE_PEER_TRANSFER_COMPLETE_IND), NULL);
            break;
        case UPGRADE_COMMIT:
            /* Send message to UpgradeSm */
            UpgradeCommitMsgFromUpgradePeer();
            break;
        case UPGRADE_IN_PROGRESS:
            /* Device is rebooted let inform Host to continue further.
             * Send message to UpgradeSm.
             */
            if(upgradePeerInfo->SmCtx->peerState ==
                               UPGRADE_PEER_STATE_RESTARTED_FOR_COMMIT)
            {
                UpgradePeerSetState(UPGRADE_PEER_STATE_COMMIT_HOST_CONTINUE);
                /* We can resume DFU only when primary device is rebooted */
                UpgradeHandleMsg(NULL, HOST_MSG(UPGRADE_PEER_SYNC_AFTER_REBOOT_REQ), NULL);
            }
            break;
        default:
            DEBUG_LOG("AskForConfirmationL UpgradePeer: unhandled msg");
            break;
    }
}

/**
 * Destroy UpgradePeer context when upgrade process is aborted or completed.
 */
static void UpgradePeerCtxDestroy(void)
{
    /* Only free UpgradePeer SM context. This can be allocated again once
     * DFU process starts during same power cycle.
     * UpgradePeer Info is allocated only once during boot and never destroyed
     */
    if(upgradePeerInfo != NULL)
    {
        if(upgradePeerInfo->SmCtx != NULL)
        {
            upgradePeerInfo->SmCtx->confirm_type = UPGRADE_TRANSFER_COMPLETE;
            upgradePeerInfo->SmCtx->peerState = UPGRADE_PEER_STATE_SYNC;
            upgradePeerInfo->SmCtx->mResumePoint = UPGRADE_PEER_RESUME_POINT_START;
            if(upgradePeerInfo->SmCtx != NULL)
            {
                free(upgradePeerInfo->SmCtx);
                upgradePeerInfo->SmCtx = NULL;
            }
            /* If peer data transfer is going on then "INTERNAL_PEER_DATA_CFM_MSG" could be queued
             * at this point which can lead to NULL pointer dereference in SelfKickNextDataBlock()
             * function because, we are clearing upgradePeerInfo->SmCtx */
            MessageCancelAll((Task)&upgradePeerInfo->myTask, INTERNAL_PEER_DATA_CFM_MSG);
        }
    }
}

/**
 * To stop the upgrade process.
 */
static void StopUpgrade(void)
{
    upgradePeerInfo->SmCtx->isUpgrading = FALSE;
    MessageSend(upgradePeerInfo->appTask, UPGRADE_PEER_DISCONNECT_REQ, NULL);
    /* Clear Pskey to start next upgrade from fresh */
    memset(&upgradePeerInfo->UpgradePSKeys,0,
               UPGRADE_PEER_PSKEY_USAGE_LENGTH_WORDS*sizeof(uint16));
    UpgradePeerSavePSKeys();
    UpgradePeerCtxDestroy();
}

/**
 * To clean and set the upgradePeerInfo for next DFU
 */
static void CleanUpgradePeerCtx(void)
{
    upgradePeerInfo->SmCtx->isUpgrading = FALSE;
    /* Clear Pskey to start next upgrade from fresh */
    memset(&upgradePeerInfo->UpgradePSKeys,0,
               UPGRADE_PEER_PSKEY_USAGE_LENGTH_WORDS * sizeof(uint16));
    UpgradePeerSavePSKeys();
    UpgradePeerCtxDestroy();
}

/**
 * To reset the file transfer.
 */
static void UpgradePeerCtxSet(void)
{
    upgradePeerInfo->SmCtx->mStartAttempts = 0;
}

/**
 * Immediate answer to secondury device, data is the same as the received one.
 */
static void HandleErrorWarnRes(uint16 error)
{
    DEBUG_LOG("HandleErrorWarnRes: UpgradePeer: Handle Error Ind");
    SendErrorConfirmation(error);
}

/**
 * To send an UPGRADE_START_REQ message.
 */
static void SendStartReq (void)
{
    uint8* payload = NULL;
    uint16 byteIndex = 0;

    DEBUG_LOG("SendStartReq: UpgradePeer: Start REQ");

    payload = PanicUnlessMalloc(UPGRADE_PEER_PACKET_HEADER);
    byteIndex += ByteUtilsSet1Byte(payload, byteIndex, UPGRADE_PEER_START_REQ);
    byteIndex += ByteUtilsSet2Bytes(payload, byteIndex, 0);

#ifndef UPGRADE_PEER_DATA_PMALLOC_OPTIMIZE
    SendPeerData(payload, byteIndex);
#else
    SendPeerData(payload, byteIndex, FALSE);
#endif
}

/****************************************************************************
NAME
    UpgradeHostIFClientSendData

DESCRIPTION
    Send a data packet to a connected upgrade client.

*/
static void UpgradePeerSendErrorMsg(upgrade_peer_status_t error)
{
    UpgradeErrorMsgFromUpgradePeer(error);
}

/**
 * This method is called when we received an UPGRADE_START_CFM message. This
 * method reads the message and starts the next step which is sending an
 * UPGRADE_START_DATA_REQ message, or aborts the upgrade depending on the
 * received message.
 */
static void ReceiveStartCFM(UPGRADE_PEER_START_CFM_T *data)
{
    DEBUG_LOG("ReceiveStartCFM: UpgradePeer: Start CFM");

    // the packet has to have a content.
    if (data->common.length >= UPRAGE_HOST_START_CFM_DATA_LENGTH)
    {
        //noinspection IfCanBeSwitch
        if (data->status == UPGRADE_PEER_SUCCESS)
        {
            upgradePeerInfo->SmCtx->mStartAttempts = 0;
            UpgradePeerSetState(UPGRADE_PEER_STATE_DATA_READY);
            SendStartDataReq();
        }
        else if (data->status == UPGRADE_PEER_ERROR_APP_NOT_READY)
        {
            if (upgradePeerInfo->SmCtx->mStartAttempts < MAX_START_REQ_RETRAIL_NUM)
            {
                // device not ready we will ask it again.
                upgradePeerInfo->SmCtx->mStartAttempts++;
                MessageSendLater((Task)&upgradePeerInfo->myTask,
                                 INTERNAL_START_REQ_MSG, NULL, 2000);
            }
            else
            {
                upgradePeerInfo->SmCtx->mStartAttempts = 0;
                upgradePeerInfo->SmCtx->upgrade_status =
                                   UPGRADE_PEER_ERROR_IN_ERROR_STATE;
                UpgradePeerSendErrorMsg(upgradePeerInfo->SmCtx->upgrade_status);
            }
        }
        else
        {
            upgradePeerInfo->SmCtx->upgrade_status = UPGRADE_PEER_ERROR_IN_ERROR_STATE;
            UpgradePeerSendErrorMsg(upgradePeerInfo->SmCtx->upgrade_status);
        }
    }
    else {
        upgradePeerInfo->SmCtx->upgrade_status = UPGRADE_PEER_ERROR_IN_ERROR_STATE;
        UpgradePeerSendErrorMsg(upgradePeerInfo->SmCtx->upgrade_status);
    }
}

/**
 * To send a UPGRADE_SYNC_REQ message.
 */
static void SendSyncReq (uint32 md5_checksum)
{
    uint8* payload = NULL;
    uint16 byteIndex = 0;

    DEBUG_LOG("UpgradePeer, sending UPGRADE_SYNC_REQ, md5_checksum 0x%x", md5_checksum);

    payload = PanicUnlessMalloc(UPGRADE_SYNC_REQ_DATA_LENGTH +
                                UPGRADE_PEER_PACKET_LENGTH);
    byteIndex += ByteUtilsSet1Byte(payload, byteIndex, UPGRADE_PEER_SYNC_REQ);
    byteIndex += ByteUtilsSet2Bytes(payload, byteIndex,
                                    UPGRADE_SYNC_REQ_DATA_LENGTH);
    byteIndex += ByteUtilsSet4Bytes(payload, byteIndex, md5_checksum);

#ifndef UPGRADE_PEER_DATA_PMALLOC_OPTIMIZE
    SendPeerData(payload, byteIndex);
#else
    SendPeerData(payload, byteIndex, FALSE);
#endif
}

/**
 * This method is called when we received an UPGRADE_SYNC_CFM message.
 * This method starts the next step which is sending an UPGRADE_START_REQ
 * message.
 */
static void ReceiveSyncCFM(UPGRADE_PEER_SYNC_CFM_T *update_cfm)
{
    DEBUG_LOG("ReceiveSyncCFM: UpgradePeer: Sync CFM");

    SetResumePoint(update_cfm->resume_point);
    UpgradePeerSetState(UPGRADE_PEER_STATE_READY);
    SendStartReq();
}

/**
 * To send an UPGRADE_DATA packet.
 */
static void SendDataToPeer (uint32 data_length, uint8 *data, bool is_last_packet)
{
    uint16 byteIndex = 0;

    byteIndex += ByteUtilsSet1Byte(data, byteIndex, UPGRADE_PEER_DATA);
    byteIndex += ByteUtilsSet2Bytes(data, byteIndex, data_length +
                                    UPGRADE_DATA_MIN_DATA_LENGTH);
    byteIndex += ByteUtilsSet1Byte(data, byteIndex,
                                   is_last_packet);

#ifndef UPGRADE_PEER_DATA_PMALLOC_OPTIMIZE
    SendPeerData(data, data_length + UPGRADE_PEER_PACKET_HEADER +
                                     UPGRADE_DATA_MIN_DATA_LENGTH);
#else
    SendPeerData(data, data_length + UPGRADE_PEER_PACKET_HEADER +
                                     UPGRADE_DATA_MIN_DATA_LENGTH, TRUE);
#endif
}

static bool StartPeerData(uint32 dataLength, uint8 *packet, bool is_last_packet)
{
    PRINT(("StartPeerData UpgradePeer: Start Data %x, len %d", packet, dataLength));

    /* Set up parameters */
    if (packet == NULL)
    {
        upgradePeerInfo->SmCtx->upgrade_status = UPGRADE_PEER_ERROR_IN_ERROR_STATE;
        return FALSE;
    }

    SendDataToPeer(dataLength, packet, is_last_packet);

    if (is_last_packet)
    {
        DEBUG_LOG("StartPeerData UpgradePeer: last packet");
        if (upgradePeerInfo->SmCtx->mResumePoint == UPGRADE_PEER_RESUME_POINT_START)
        {
            SetResumePoint(UPGRADE_PEER_RESUME_POINT_PRE_VALIDATE);
            /* For concurrent DFU, Since peer dfu is completed now, set the
             * peer data transfer complete
             */
            UpgradeCtxSetPeerDataTransferComplete();
            SendValidationDoneReq();
        }
    }
    return TRUE;
}



static upgrade_peer_status_t upgradePeerSendData(void)
{
#ifdef UPGRADE_PEER_DATA_PMALLOC_OPTIMIZE
    UPGRADE_PEER_DATA_IND_T *dataInd = NULL;
#endif
    const uint32 remaining_size =  upgradePeerInfo->SmCtx->total_req_size -  upgradePeerInfo->SmCtx->total_sent_size;
    const uint16 req_data_bytes = MIN(remaining_size, 240);

    /* This will be updated as how much data device is sending */
    uint32 data_length = 0;
    bool is_last_packet = FALSE;
    uint8 *payload = NULL;
    upgrade_peer_status_t status;

#ifndef UPGRADE_PEER_DATA_PMALLOC_OPTIMIZE
    const uint16 pkt_len = req_data_bytes + UPGRADE_PEER_PACKET_HEADER + UPGRADE_DATA_MIN_DATA_LENGTH;

    /* Allocate memory for data read from partition */
    payload = PanicUnlessMalloc(pkt_len);
#else
    const uint16 pkt_len = (req_data_bytes + UPGRADE_PEER_PACKET_HEADER +
                            UPGRADE_DATA_MIN_DATA_LENGTH +
                            sizeof(UPGRADE_PEER_DATA_IND_T) - 1);

    /* Allocate memory for data read from partition */
    dataInd =(UPGRADE_PEER_DATA_IND_T *)PanicUnlessMalloc(pkt_len);

    payload = &dataInd->data[0];
    DEBUG_LOG("upgradePeerSendData: dataInd:%u", dataInd);
    DEBUG_LOG("upgradePeerSendData: pkt_len:%d", pkt_len);
    DEBUG_LOG("upgradePeerSendData: payload:%u", payload);

#endif

    status = UpgradePeerPartitionMoreData(&payload[UPGRADE_PEER_PACKET_HEADER + UPGRADE_DATA_MIN_DATA_LENGTH],
                                            &is_last_packet, req_data_bytes,
                                            &data_length, 
                                            upgradePeerInfo->SmCtx->req_start_offset);

    upgradePeerInfo->SmCtx->total_sent_size += data_length;

    /* data has been read from partition, now send to peer device */
    if (status == UPGRADE_PEER_SUCCESS)
    {
        if (!StartPeerData(data_length, payload, is_last_packet))
            status = UPGRADE_PEER_ERROR_PARTITION_OPEN_FAILED;
    }

    if (status != UPGRADE_PEER_SUCCESS)
    {
#ifndef UPGRADE_PEER_DATA_PMALLOC_OPTIMIZE
        if (payload)
            free (payload);
#else
        if (dataInd)
            free (dataInd);
#endif
    }

    return status;
}


/**
 * This method is called when we received an UPGRADE_DATA_BYTES_REQ message.
 * We manage this packet and use it for the next step which is to upload the
 * file on the device using UPGRADE_DATA messages.
 */
static void ReceiveDataBytesREQ(UPGRADE_PEER_START_DATA_BYTES_REQ_T *data)
{
    upgrade_peer_status_t error;

    DEBUG_LOG_VERBOSE("upgradePeerReceiveDataBytesREQ, bytes %u, offset %u", data->data_bytes, data->start_offset);

    /* Checking the data has the good length */
    if (data->common.length == UPGRADE_DATA_BYTES_REQ_DATA_LENGTH)
    {
        UpgradePeerSetState(UPGRADE_PEER_STATE_DATA_TRANSFER);

        upgradePeerInfo->SmCtx->total_req_size = data->data_bytes;
        upgradePeerInfo->SmCtx->total_sent_size = 0;
        /*
         * Honour the peer requested start offset which is significant when
         * peer requests for partition data which was partially received
         * owing to an abrupt reset. This is must to support seamless DFU
         * even after interruption owing to either earbud having being abruptly
         * reset.
         */
        upgradePeerInfo->SmCtx->req_start_offset = data->start_offset;

        error = upgradePeerSendData();
    }
    else
    {
        error = UPGRADE_PEER_ERROR_IN_ERROR_STATE;
        DEBUG_LOG_ERROR("upgradePeerReceiveDataBytesREQ, invalid length %u", data->common.length);
    }

    if(error == UPGRADE_PEER_ERROR_INTERNAL_ERROR_INSUFFICIENT_PSKEY)
    {
        UPGRADE_PEER_DATA_BYTES_REQ_T *msg;
        
        DEBUG_LOG_WARN("ReceiveDataBytesREQ : Concurrent DFU pskey not filled in");
        msg = (UPGRADE_PEER_DATA_BYTES_REQ_T *)PanicUnlessMalloc(sizeof(UPGRADE_PEER_DATA_BYTES_REQ_T));
        memmove(msg, data, sizeof(UPGRADE_PEER_DATA_BYTES_REQ_T));
        MessageSendLater((Task)&upgradePeerInfo->myTask, INTERNAL_PEER_MSG, msg,INTERNAL_PEER_MSG_DELAY);
        return;
    }
 
    if(error != UPGRADE_PEER_SUCCESS)
    {
        DEBUG_LOG_ERROR("upgradePeerReceiveDataBytesREQ, error %u", error);

        upgradePeerInfo->SmCtx->upgrade_status = error;
        UpgradePeerSendErrorMsg(upgradePeerInfo->SmCtx->upgrade_status);
    }
    else
    {
        DEBUG_LOG_VERBOSE("upgradePeerReceiveDataBytesREQ, total_size %u, total_sent %u",
                          upgradePeerInfo->SmCtx->total_req_size,
                          upgradePeerInfo->SmCtx->total_sent_size);
        DEBUG_LOG_VERBOSE("upgradePeerReceiveDataBytesREQ, req_start_offset %u",
                          upgradePeerInfo->SmCtx->req_start_offset);
    }
}


static void SelfKickNextDataBlock(void)
{
    if (upgradePeerInfo->SmCtx->total_sent_size < upgradePeerInfo->SmCtx->total_req_size)
    {
        DEBUG_LOG_VERBOSE("SelfKickNextDataBlock, req_start_offset %u",
                          upgradePeerInfo->SmCtx->req_start_offset);

        /*
         * As per the current optimized scheme, one UPGRADE_PEER_DATA_BYTES_REQ
         * is received per partition field. For the data field of the partition,
         * the whole size of the partition is sent in
         * UPGRADE_PEER_DATA_BYTES_REQ and sender (Primary) self kicks on each
         * MessageMoreSpace event on the Sink stream, to continue sending
         * UPGRADE_PEER_DATA_IND until the whole requested partition size is sent.
         * Hence its insignificant on internal kicks and hence reset to zero.
         */
        upgradePeerInfo->SmCtx->req_start_offset = 0;

        upgrade_peer_status_t error = upgradePeerSendData();

        if (error != UPGRADE_PEER_SUCCESS)
        {
            DEBUG_LOG_ERROR("SelfKickNextDataBlock, error %u", error);

            upgradePeerInfo->SmCtx->upgrade_status = error;
            UpgradePeerSendErrorMsg(upgradePeerInfo->SmCtx->upgrade_status);
        }
        else
        {
            DEBUG_LOG_VERBOSE("SelfKickNextDataBlock, total_size %u, total_sent %u",
                              upgradePeerInfo->SmCtx->total_req_size,
                              upgradePeerInfo->SmCtx->total_sent_size);
        }
    }
}



/**
 * To send an UPGRADE_IS_VALIDATION_DONE_REQ message.
 */
static void SendValidationDoneReq (void)
{
    uint8* payload = NULL;
    uint16 byteIndex = 0;

    DEBUG_LOG("SendValidationDoneReq: UpgradePeer: Validation Done REQ");

    payload = PanicUnlessMalloc(UPGRADE_PEER_PACKET_HEADER);
    byteIndex += ByteUtilsSet1Byte(payload, byteIndex,
                                   UPGRADE_PEER_IS_VALIDATION_DONE_REQ);
    byteIndex += ByteUtilsSet2Bytes(payload, byteIndex, 0);

#ifndef UPGRADE_PEER_DATA_PMALLOC_OPTIMIZE
    SendPeerData(payload, byteIndex);
#else
    SendPeerData(payload, byteIndex, FALSE);
#endif
}



/**
 * This method is called when we received an UPGRADE_IS_VALIDATION_DONE_CFM
 * message. We manage this packet and use it for the next step which is to send
 * an UPGRADE_IS_VALIDATION_DONE_REQ.
 */
static void ReceiveValidationDoneCFM(UPGRADE_PEER_VERIFICATION_DONE_CFM_T *data)
{
    if ((data->common.length == UPGRADE_VAIDATION_DONE_CFM_DATA_LENGTH) &&
         data->delay_time > 0)
    {
        MessageSendLater((Task)&upgradePeerInfo->myTask,
                         INTERNAL_VALIDATION_DONE_MSG,
                         NULL, data->delay_time);
    }
    else
    {
        SendValidationDoneReq();
    }
}

/**
 * This method is called when we received an UPGRADE_TRANSFER_COMPLETE_IND
 * message. We manage this packet and use it for the next step which is to send
 * a validation to continue the process or to abort it temporally.
 * It will be done later.
 */
static void ReceiveTransferCompleteIND(void)
{
    DEBUG_LOG("ReceiveTransferCompleteIND: UpgradePeer: Transfer Complete Ind");
    SetResumePoint(UPGRADE_PEER_RESUME_POINT_PRE_REBOOT);
    /* Send TRANSFER_COMPLETE_IND to host to get confirmation */
    AskForConfirmation(UPGRADE_TRANSFER_COMPLETE);
}

/**
 * This method is called when we received an UPGRADE_COMMIT_RES message.
 * We manage this packet and use it for the next step which is to send a
 * validation to continue the process or to abort it temporally.
 * It will be done later.
 */
static void ReceiveCommitREQ(void)
{
    DEBUG_LOG("ReceiveCommitREQ: UpgradePeer: Commit Req");
    SetResumePoint(UPGRADE_PEER_RESUME_POINT_COMMIT);
    AskForConfirmation(UPGRADE_COMMIT);
}

/**
 * This method is called when we received an UPGRADE_IN PROGRESS_IND message.
 */
static void ReceiveProgressIND(void)
{
    DEBUG_LOG("ReceiveProgressIND: UpgradePeer: Progress Ind");
    AskForConfirmation(UPGRADE_IN_PROGRESS);
}

/**
 * This method is called when we received an UPGRADE_ABORT_CFM message after
 * we asked for an abort to the upgrade process.
 */
static void ReceiveAbortCFM(void)
{
    DEBUG_LOG("ReceiveAbortCFM: UpgradePeer: Abort CFM");
    StopUpgrade();
}

/****************************************************************************
NAME
    SendPeerData

DESCRIPTION
    Send a data packet to a connected upgrade client.

*/
#ifndef UPGRADE_PEER_DATA_PMALLOC_OPTIMIZE
static void SendPeerData(uint8 *data, uint16 dataSize)
#else
static void SendPeerData(uint8 *data, uint16 dataSize, bool isPreAllocated)
#endif
{
    UPGRADE_PEER_DATA_IND_T *dataInd = NULL;
#ifdef UPGRADE_PEER_DATA_PMALLOC_OPTIMIZE
    uint32 offsetof_idx;
#endif

    if(upgradePeerInfo->appTask != NULL)
    {

#ifdef UPGRADE_PEER_DATA_PMALLOC_OPTIMIZE
        if (isPreAllocated)
        {
            offsetof_idx = offsetof(UPGRADE_PEER_DATA_IND_T, data);
            DEBUG_LOG("SendPeerData: data:%u", data);
            DEBUG_LOG("SendPeerData: offsetof_idx:%d", offsetof_idx);
            /*
             * When pre-allocated, data is embedded within dataInd in
             * sequential memory and hence recompute the base pointer for
             * dataInd, as returned by pmalloc heap when it was allocated.
             */
            dataInd = (UPGRADE_PEER_DATA_IND_T *)(data - offsetof_idx);
            DEBUG_LOG("SendPeerData: dataInd:%u", dataInd);

            /*
             * If data is pre-allocated, then dataInd embeds data and shall be freed
             * as part of the schedule handling dataInd. So data too shouldn't
             * be freed.
             */
             DEBUG_LOG("UpgradePeer, sending data to peer, size %u", dataSize);
        }
        else
#endif
        {
            dataInd =(UPGRADE_PEER_DATA_IND_T *)PanicUnlessMalloc(
                                                sizeof(*dataInd) + dataSize - 1);

            DEBUG_LOG("UpgradePeer, sending data to peer, size %u", dataSize);

            ByteUtilsMemCpyToStream(dataInd->data, data, dataSize);

            free(data);
        }


        dataInd->size_data = dataSize;
        dataInd->is_data_state = TRUE;
        MessageSend(upgradePeerInfo->appTask, UPGRADE_PEER_DATA_IND, dataInd);
    }
    else
    {
#ifdef UPGRADE_PEER_DATA_PMALLOC_OPTIMIZE
        if (isPreAllocated)
        {
            if (dataInd)
                free(dataInd);
        }
        else
#endif
        {
            if (data)
                free(data);
        }
    }
}

/**
 * To send an UPGRADE_START_DATA_REQ message.
 */
static void SendStartDataReq (void)
{
    uint8* payload = NULL;
    uint16 byteIndex = 0;

    DEBUG_LOG("SendStartDataReq: UpgradePeer: Start DATA Req");

    SetResumePoint(UPGRADE_PEER_RESUME_POINT_START);

    payload = PanicUnlessMalloc(UPGRADE_PEER_PACKET_HEADER);
    byteIndex += ByteUtilsSet1Byte(payload, byteIndex,
                                   UPGRADE_PEER_START_DATA_REQ);
    byteIndex += ByteUtilsSet2Bytes(payload, byteIndex, 0);

    /* Save the current state to pskeys. This will be used for resuming
     * DFU when upgrade peer is reset during primary to secondary data
     * transfer
     */
    upgradePeerInfo->UpgradePSKeys.currentState =
                                   upgradePeerInfo->SmCtx->peerState;
    UpgradePeerSavePSKeys();

#ifndef UPGRADE_PEER_DATA_PMALLOC_OPTIMIZE
    SendPeerData(payload, byteIndex);
#else
    SendPeerData(payload, byteIndex, FALSE);
#endif
}

/**
 * To send an UPGRADE_TRANSFER_COMPLETE_RES packet.
 */
static void SendTransferCompleteReq(upgrade_action_status_t status)
{
    uint8* payload = NULL;
    uint16 byteIndex = 0;

    DEBUG_LOG("SendTransferCompleteReq: UpgradePeer: Transfer Complete RES");

    payload = PanicUnlessMalloc(UPGRADE_TRANSFER_COMPLETE_RES_DATA_LENGTH +
                                UPGRADE_PEER_PACKET_LENGTH);
    byteIndex += ByteUtilsSet1Byte(payload, byteIndex,
                                   UPGRADE_PEER_TRANSFER_COMPLETE_RES);
    byteIndex += ByteUtilsSet2Bytes(payload, byteIndex,
                                   UPGRADE_TRANSFER_COMPLETE_RES_DATA_LENGTH);
    byteIndex += ByteUtilsSet1Byte(payload, byteIndex, status);

#ifndef UPGRADE_PEER_DATA_PMALLOC_OPTIMIZE
    SendPeerData(payload, byteIndex);
#else
    SendPeerData(payload, byteIndex, FALSE);
#endif
}

/**
 * To send an UPGRADE_IN_PROGRESS_RES packet.
 */
static void SendInProgressRes(upgrade_action_status_t status)
{
    uint8* payload = NULL;
    uint16 byteIndex = 0;

    DEBUG_LOG("SendInProgressRes: UpgradePeer: In Progress Res");

    payload = PanicUnlessMalloc(UPGRADE_IN_PROGRESS_DATA_LENGTH +
                                UPGRADE_PEER_PACKET_LENGTH);
    byteIndex += ByteUtilsSet1Byte(payload, byteIndex,
                                   UPGRADE_PEER_IN_PROGRESS_RES);
    byteIndex += ByteUtilsSet2Bytes(payload, byteIndex,
                                   UPGRADE_IN_PROGRESS_DATA_LENGTH);
    byteIndex += ByteUtilsSet1Byte(payload, byteIndex, status);

#ifndef UPGRADE_PEER_DATA_PMALLOC_OPTIMIZE
    SendPeerData(payload, byteIndex);
#else
    SendPeerData(payload, byteIndex, FALSE);
#endif
}

/**
 * To send an UPGRADE_COMMIT_CFM packet.
 */
static void SendCommitCFM(upgrade_action_status_t status)
{
    uint8* payload = NULL;
    uint16 byteIndex = 0;

    DEBUG_LOG("SendCommitCFM: UpgradePeer: Commit CFM");

    payload = PanicUnlessMalloc(UPGRADE_COMMIT_CFM_DATA_LENGTH +
                                UPGRADE_PEER_PACKET_LENGTH);
    byteIndex += ByteUtilsSet1Byte(payload, byteIndex, UPGRADE_PEER_COMMIT_CFM);
    byteIndex += ByteUtilsSet2Bytes(payload, byteIndex,
                                    UPGRADE_COMMIT_CFM_DATA_LENGTH);
    byteIndex += ByteUtilsSet1Byte(payload, byteIndex, status);

#ifndef UPGRADE_PEER_DATA_PMALLOC_OPTIMIZE
    SendPeerData(payload, byteIndex);
#else
    SendPeerData(payload, byteIndex, FALSE);
#endif
}

/**
 * To send a message to abort the upgrade.
 */
static void SendAbortReq(void)
{
    uint8* payload = NULL;
    uint16 byteIndex = 0;

    DEBUG_LOG("SendAbortReq: UpgradePeer: Abort request");

    payload = PanicUnlessMalloc(UPGRADE_PEER_PACKET_HEADER);
    byteIndex += ByteUtilsSet1Byte(payload, byteIndex, UPGRADE_PEER_ABORT_REQ);
    byteIndex += ByteUtilsSet2Bytes(payload, byteIndex, 0);

#ifndef UPGRADE_PEER_DATA_PMALLOC_OPTIMIZE
    SendPeerData(payload, byteIndex);
#else
    SendPeerData(payload, byteIndex, FALSE);
#endif

    /*Cancel internal peer data_req message - needed in concurrent DFU cases when data request is on same partition
        and internal message is sent.Race condition can hapen with message in the queue and meanwhile abort happens.
        Also can happen during sco active scenarios as well*/
    MessageCancelAll((Task)&upgradePeerInfo->myTask, INTERNAL_PEER_MSG);
}

/**
 * To send an UPGRADE_ERROR_WARN_RES packet.
 */
static void SendErrorConfirmation(uint16 error)
{
    uint8* payload = NULL;
    uint16 byteIndex = 0;

    DEBUG_LOG("SendErrorConfirmation: UpgradePeer: Send Error IND");

    payload = PanicUnlessMalloc(UPGRADE_PEER_PACKET_HEADER);
    byteIndex += ByteUtilsSet1Byte(payload, byteIndex,
                                UPGRADE_PEER_ERROR_WARN_RES);
    byteIndex += ByteUtilsSet2Bytes(payload, byteIndex,
                                   UPGRADE_ERROR_IND_DATA_LENGTH);
    byteIndex += ByteUtilsSet2Bytes(payload, byteIndex, error);

#ifndef UPGRADE_PEER_DATA_PMALLOC_OPTIMIZE
    SendPeerData(payload, byteIndex);
#else
    SendPeerData(payload, byteIndex, FALSE);
#endif
}

static void HandlePeerAppMsg(uint8 *data)
{
    uint16 msgId;
    upgrade_peer_status_t error;
    UPGRADE_PEER_DATA_BYTES_REQ_T *msg;
    uint16 *scoFlag;
    msgId = ByteUtilsGet1ByteFromStream(data);

    PRINT(("UpgradePeer: Handle APP Msg 0x%x\n", msgId));

    switch(msgId)
    {
        case UPGRADE_PEER_SYNC_CFM:
            ReceiveSyncCFM((UPGRADE_PEER_SYNC_CFM_T *)data);
            break;

        case UPGRADE_PEER_START_CFM:
            ReceiveStartCFM((UPGRADE_PEER_START_CFM_T *)data);
            break;

        case UPGRADE_PEER_IS_VALIDATION_DONE_CFM:
            ReceiveValidationDoneCFM(
                            (UPGRADE_PEER_VERIFICATION_DONE_CFM_T *)data);
            break;

        case UPGRADE_PEER_ABORT_CFM:
            ReceiveAbortCFM();
            break;

        case UPGRADE_PEER_START_REQ:
            SendStartReq();
            break;

        case UPGRADE_PEER_DATA_BYTES_REQ:
#ifndef HOSTED_TEST_ENVIRONMENT
            DEBUG_LOG("HandlePeerAppMsg UPGRADE_PEER_DATA_BYTES_REQ : last closed partition %d",UpgradeCtxGetPSKeys()->last_closed_partition - 1);
            DEBUG_LOG("HandlePeerAppMsg UPGRADE_PEER_DATA_BYTES_REQ : peer read partition %d",UpgradePeerPartitionDataCtxGet()->partNum);
            /*ToDo- Add check for concurrent DFU scheme*/
            if(UpgradeIsDataTransferMode())
            {
                DEBUG_LOG("HandlePeerAppMsg UPGRADE_PEER_DATA_BYTES_REQ : Concurrent DFU");
                /* There is the need to synchronise read/write rates from peer and AG respectively. There are potential synchronisation issues especially in case 
                     where the AG is slow in writing and peer reads faster. This is addressed by allowing read only on for closed partitions. 
                     This is been looked to improve further in future*/
                if((UpgradeCtxGetPSKeys()->last_closed_partition == 0) ||(((UpgradeCtxGetPSKeys()->last_closed_partition) - 1) <= (UpgradePeerPartitionDataCtxGet()->partNum)))
                {
                    DEBUG_LOG("HandlePeerAppMsg UPGRADE_PEER_DATA_BYTES_REQ : Concurrent DFU Same partition");                    
                    {
                            DEBUG_LOG("HandlePeerAppMsg UPGRADE_PEER_DATA_BYTES_REQ : Concurrent DFU delay message"); 
                            msg = (UPGRADE_PEER_DATA_BYTES_REQ_T *)PanicUnlessMalloc(sizeof(UPGRADE_PEER_DATA_BYTES_REQ_T));
                            memmove(msg, data, sizeof(UPGRADE_PEER_DATA_BYTES_REQ_T));
                            MessageSendLater((Task)&upgradePeerInfo->myTask,INTERNAL_PEER_MSG, msg,INTERNAL_PEER_MSG_DELAY);
                            return;
                    }
                }                    
            }
            scoFlag = UpgradeIsScoActive();
            if(*scoFlag)
            {
                DEBUG_LOG("HandlePeerAppMsg: UpgradePeer: Defer sending data as SCO IS ACTIVE in UPGRADE_PEER_DATA_BYTES_REQ");
                msg = (UPGRADE_PEER_DATA_BYTES_REQ_T *)PanicUnlessMalloc(sizeof(UPGRADE_PEER_DATA_BYTES_REQ_T));
                memcpy(msg, data, sizeof(UPGRADE_PEER_DATA_BYTES_REQ_T));
                MessageSendConditionally((Task)&upgradePeerInfo->myTask,INTERNAL_PEER_MSG, msg,UpgradeIsScoActive());
            }
            else
#endif
            {
                DEBUG_LOG("HandlePeerAppMsg UPGRADE_PEER_DATA_BYTES_REQ : Process it");
                ReceiveDataBytesREQ((UPGRADE_PEER_START_DATA_BYTES_REQ_T *)data);
            }
            break;

        case UPGRADE_PEER_COMMIT_REQ:
            UpgradePeerSetState(UPGRADE_PEER_STATE_COMMIT_CONFIRM);
            ReceiveCommitREQ();
            break;

        case UPGRADE_PEER_TRANSFER_COMPLETE_IND:
            /* Check if the UpgradeSm state is UPGRADE_STATE_VALIDATED. If not,
             * then wait for 2 sec and send the TRANSFER_COMPLETE_IND to UpgradeSm
             * after 2 sec, until the state is UPGRADE_STATE_VALIDATED.
             * This will ensure that the TRANSFER_COMPLETE_IND message is handled
             * correctly in UpgradeSMHandleValidated().
             */
            if(UpgradeSMGetState() != UPGRADE_STATE_VALIDATED)
            {
                DEBUG_LOG("HandlePeerAppMsg: UpgradeSM state not yet set to UPGRADE_STATE_VALIDATED");
                UPGRADE_PEER_TRANSFER_COMPLETE_IND_T* peer_msg = 
                    (UPGRADE_PEER_TRANSFER_COMPLETE_IND_T *)PanicUnlessMalloc(sizeof(UPGRADE_PEER_TRANSFER_COMPLETE_IND_T));
                memmove(peer_msg, data, sizeof(UPGRADE_PEER_TRANSFER_COMPLETE_IND_T));
                MessageSendLater((Task)&upgradePeerInfo->myTask,INTERNAL_PEER_MSG, peer_msg, INTERNAL_PEER_MSG_VALIDATION_SEND_DELAY);
            }
            else
            {
                UpgradePeerSetState(UPGRADE_PEER_STATE_VALIDATED);
                ReceiveTransferCompleteIND();
            }
            break;

        case UPGRADE_PEER_COMPLETE_IND:
            /* Peer upgrade is finished let disconnect the peer connection */
            StopUpgrade();
            break;

        case UPGRADE_PEER_ERROR_WARN_IND:
            /* send Error message to Host */
            error = ByteUtilsGet2BytesFromStream(data +
                                                UPGRADE_PEER_PACKET_HEADER);
            DEBUG_LOG("HandlePeerAppMsg: UPGRADE_PEER_ERROR_WARN_IND: %d", error);
            UpgradePeerSendErrorMsg(error);
            break;

        case UPGRADE_PEER_IN_PROGRESS_IND:
            ReceiveProgressIND();
            break;

        default:
            DEBUG_LOG("HandlePeerAppMsg: UpgradePeer: unhandled msg");
    }
}

static void HandleLocalMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    PRINT(("UpgradePeer: Local Msg 0x%x", id));
    switch(id)
    {
        case INTERNAL_START_REQ_MSG:
            SendStartReq();
            break;

        case INTERNAL_VALIDATION_DONE_MSG:
            SendValidationDoneReq();
            break;

        case INTERNAL_PEER_MSG:
            HandlePeerAppMsg((uint8 *)message);
            break;

        case INTERNAL_PEER_DATA_CFM_MSG:
            SelfKickNextDataBlock();
            break;

        default:
            DEBUG_LOG("HandleLocalMessage: UpgradePeer: unhandled msg");
    }
}

/****************************************************************************
NAME
    UpgradePeerApplicationReconnect

DESCRIPTION
    Check the upgrade status and decide if the application needs to consider
    restarting communication / UI so that it can connect to a host and start
    the commit process after defined reboot.

    If needed, builds and send an UPGRADE_PEER_RESTARTED_IND message and
    sends to the application task.

NOTE
    UpgradePeerApplicationReconnect() is called after the peer signalling
    is established, which ensures that the peers are connected  and the
    roles are set.
*/
void UpgradePeerApplicationReconnect(void)
{
    if(UpgradePeerIsADK_20_1_SecondaryDevice() && UPGRADE_PEER_IS_PRIMARY)
    {
        /* We are here because reboot happens during upgrade process of version
         * upgrade from ADK20.1 which supported only fixed role post DFU reboot,
         * to new version which supports dynamic role. And ADK20.1 based
         * secondary device has become new primary. Since upgradeResumePoint was
         * not getting set for ADK20.1 based secondary device, using special
         * case here to not check upgradeResumePoint and directly send connect
         * message to peer to complete the upgrade process.
         */
        DEBUG_LOG_INFO("UpgradePeerApplicationReconnect: Version upgrade from ADK20.1");
        UpgradePeerCtxInit();
        upgradePeerInfo->SmCtx->mResumePoint =
                            upgradePeerInfo->UpgradePSKeys.upgradeResumePoint;
        upgradePeerInfo->SmCtx->isUpgrading = TRUE;
    
        /* Primary device is rebooted, lets ask App to establish peer
         * connection as well.
         */
        UpgradePeerSetState(UPGRADE_PEER_STATE_RESTARTED_FOR_COMMIT);
        MessageSend(upgradePeerInfo->appTask, UPGRADE_PEER_CONNECT_REQ, NULL);
        return;
    }

    DEBUG_LOG_INFO("UpgradePeerApplicationReconnect: Resume point after reboot 0x%x",
               upgradePeerInfo->UpgradePSKeys.upgradeResumePoint);

    switch(upgradePeerInfo->UpgradePSKeys.upgradeResumePoint)
    {
        case UPGRADE_PEER_RESUME_POINT_POST_REBOOT:

            /* We are here because reboot happens during upgrade process,
             * first reinit UpgradePeer Sm Context.
             */
            if(UPGRADE_PEER_IS_PRIMARY)
            {
                UpgradePeerCtxInit();
                upgradePeerInfo->SmCtx->mResumePoint =
                    upgradePeerInfo->UpgradePSKeys.upgradeResumePoint;
                upgradePeerInfo->SmCtx->isUpgrading = TRUE;

                /* Primary device is rebooted, lets ask App to establish peer
                 * connection as well.
                 */
                UpgradePeerSetState(UPGRADE_PEER_STATE_RESTARTED_FOR_COMMIT);

                MessageSend(upgradePeerInfo->appTask,
                            UPGRADE_PEER_CONNECT_REQ, NULL);

            }
        break;

        default:
            DEBUG_LOG("UpgradePeerApplicationReconnect: unhandled msg");
    }
}

/**
 * This method starts the upgrade process. It checks that there isn't already a
 * upgrade going on. It resets the manager to start the upgrade and sends the
 * UPGRADE_SYNC_REQ to start the process.
 * This method can dispatch an error object if the manager has not been able to
 * start the upgrade process. The possible errors are the following:
 * UPGRADE_IS_ALREADY_PROCESSING
 */
static bool StartUpgradePeerProcess(uint32 md5_checksum)
{
    UPGRADE_PEER_CTX_T *upgrade_peer = UpgradePeerCtxGet();
    DEBUG_LOG("StartUpgradePeerProcess: UpgradePeer: Start Upgrade 0x%x", md5_checksum);
    if (!upgrade_peer->isUpgrading)
    {
        upgrade_peer->isUpgrading = TRUE;
        UpgradePeerCtxSet();
        SendSyncReq(md5_checksum);
        upgrade_peer->upgrade_status = UPGRADE_PEER_SUCCESS;
    }
    else
    {
        upgrade_peer->upgrade_status = UPGRADE_PEER_ERROR_UPDATE_FAILED;
    }

    return (upgrade_peer->upgrade_status == UPGRADE_PEER_SUCCESS);
}

/****************************************************************************
NAME
    UpgradePeerCtxGet

RETURNS
    Context of upgrade library
*/
static UPGRADE_PEER_CTX_T *UpgradePeerCtxGet(void)
{
    if(upgradePeerInfo->SmCtx == NULL)
    {
        DEBUG_LOG("UpgradeGetCtx: You shouldn't have done that");
        Panic();
    }

    return upgradePeerInfo->SmCtx;
}

/*!
    @brief Clear upgrade related peer Pskey info.
    @param none
    
    Returns none
*/
void UpgradePeerClearPSKeys(void)
{
    if (upgradePeerInfo == NULL)
    {
        DEBUG_LOG("UpgradePeerClearPSKeys: Can't be NULL\n");
        return;
    }

    /* Clear Pskey to prepare for subsequent upgrade from fresh */
    memset(&upgradePeerInfo->UpgradePSKeys,0,
               UPGRADE_PEER_PSKEY_USAGE_LENGTH_WORDS*sizeof(uint16));
    UpgradePeerSavePSKeys();

}


/****************************************************************************
NAME
    UpgradePeerLoadPSStore  -  Load PSKEY on boot

DESCRIPTION
    Save the details of the PSKEY and offset that we were passed on
    initialisation, and retrieve the current values of the key.

    In the unlikely event of the storage not being found, we initialise
    our storage to 0x00 rather than panicking.
*/
static void UpgradePeerLoadPSStore(uint16 dataPskey,uint16 dataPskeyStart)
{
    union {
        uint16      keyCache[PSKEY_MAX_STORAGE_LENGTH];
        FSTAB_PEER_COPY  fstab;
        } stack_storage;
    uint16 lengthRead;

    upgradePeerInfo->upgrade_peer_pskey = dataPskey;
    upgradePeerInfo->upgrade_peer_pskeyoffset = dataPskeyStart;

    /* Worst case buffer is used, so confident we can read complete key
     * if it exists. If we limited to what it should be, then a longer key
     * would not be read due to insufficient buffer
     * Need to zero buffer used as the cache is on the stack.
     */
    memset(stack_storage.keyCache,0,sizeof(stack_storage.keyCache));
    lengthRead = PsRetrieve(dataPskey,stack_storage.keyCache,
                            PSKEY_MAX_STORAGE_LENGTH);
    if (lengthRead)
    {
        memcpy(&upgradePeerInfo->UpgradePSKeys,
               &stack_storage.keyCache[dataPskeyStart],
               UPGRADE_PEER_PSKEY_USAGE_LENGTH_WORDS*sizeof(uint16));
    }
    else
    {
        memset(&upgradePeerInfo->UpgradePSKeys,0,
               UPGRADE_PEER_PSKEY_USAGE_LENGTH_WORDS*sizeof(uint16));
    }
}

void UpgradePeerSavePSKeys(void)
{
    uint16 keyCache[PSKEY_MAX_STORAGE_LENGTH];
    uint16 min_key_length = upgradePeerInfo->upgrade_peer_pskeyoffset +
                            UPGRADE_PEER_PSKEY_USAGE_LENGTH_WORDS;

    /* Find out how long the PSKEY is */
    uint16 actualLength = PsRetrieve(upgradePeerInfo->upgrade_peer_pskey,NULL,0);

    /* Clear the keyCache memory before it is used for reading and writing to
     * UPGRADE LIB PSKEY
     */
    memset(keyCache,0x0000,sizeof(keyCache));

    if (actualLength)
    {
        PsRetrieve(upgradePeerInfo->upgrade_peer_pskey,keyCache,actualLength);
    }
    else
    {
        if (upgradePeerInfo->upgrade_peer_pskeyoffset)
        {
            /* Initialise the portion of key before us */
            memset(keyCache,0x0000,sizeof(keyCache));
        }
        actualLength = min_key_length;
    }

    /* Correct for too short a key */
    if (actualLength < min_key_length)
    {
        actualLength = min_key_length;
    }

    memcpy(&keyCache[upgradePeerInfo->upgrade_peer_pskeyoffset],
           &upgradePeerInfo->UpgradePSKeys,
           UPGRADE_PEER_PSKEY_USAGE_LENGTH_WORDS*sizeof(uint16));
    PsStore(upgradePeerInfo->upgrade_peer_pskey,keyCache,actualLength);
}

/****************************************************************************
NAME
    UpgradePeerProcessHostMsg

DESCRIPTION
    Process message received from Host/UpgradeSm.

*/
void UpgradePeerProcessHostMsg(upgrade_peer_msg_t msgid,
                                 upgrade_action_status_t status)
{
    UPGRADE_PEER_CTX_T *upgrade_peer = upgradePeerInfo->SmCtx;
    DEBUG_LOG("UpgradePeerProcessHostMsg:  0x%x", msgid);

    if(upgrade_peer == NULL)
    {
        /* This can happen only when Host sends DFU commands when process
         * is already aborted or primary device got rebooted.
         * Ideally this should not happen, and if it happens then inform
         * host that device is in error state.
         */
        DEBUG_LOG("UpgradePeerProcessHostMsg: Context is NULL, send error message");

        /* Establish l2cap connection with peer to initiate abort in local
         * device and send error message to Upgrade SM. Also, to initiate
         * state variables clearance in peer device for next DFU
         */
        MessageSend(upgradePeerInfo->appTask, UPGRADE_PEER_CONNECT_REQ, NULL);
        upgradePeerInfo->is_dfu_abort_trigerred = TRUE;
        /* Needed for initiating the abort after the l2cap connection is established */
        UpgradePeerCtxInit();
        return;
    }

    switch(msgid)
    {
        case UPGRADE_PEER_SYNC_REQ:
            SendSyncReq(upgradePeerInfo->md5_checksum);
            break;
        case UPGRADE_PEER_ERROR_WARN_RES:
            /*
             * For Handover, error indicate to the peer else the error code
             * should ideally be same as that received from the peer via
             * UPGRADE_HOST_ERRORWARN_RES. Here since its not known here, so
             * use error_in_error.
             */
            HandleErrorWarnRes((uint16)(status == UPGRADE_HANDOVER_ERROR_IND ? 
                                UPGRADE_PEER_ERROR_HANDOVER_DFU_ABORT :
                                UPGRADE_PEER_ERROR_IN_ERROR_STATE));
            break;

        case UPGRADE_PEER_TRANSFER_COMPLETE_RES:
            {
                /* We are going to reboot save current state */
                /* Save the current state and resume point in both the devices,
                 * which will be need for the commit phase, where any of the
                 * earbuds can become primary post reboot.
                 */
                DEBUG_LOG("UpgradePeerProcessHostMsg: UPGRADE_PEER_TRANSFER_COMPLETE_RES");
                upgradePeerInfo->UpgradePSKeys.upgradeResumePoint =
                                        UPGRADE_PEER_RESUME_POINT_POST_REBOOT;
                upgradePeerInfo->UpgradePSKeys.currentState =
                                               UPGRADE_PEER_STATE_VALIDATED;
                UpgradePeerSavePSKeys();

                /* Send the confirmation only if you are primary device */
                if(UPGRADE_PEER_IS_PRIMARY)
                {
                    SendConfirmationToPeer(upgrade_peer->confirm_type, status);
                }
            }
            break;

        case UPGRADE_PEER_IN_PROGRESS_RES:
            upgrade_peer->confirm_type = UPGRADE_IN_PROGRESS;
            UpgradePeerSetState(UPGRADE_PEER_STATE_COMMIT_VERIFICATION);
            SendConfirmationToPeer(upgrade_peer->confirm_type, status);
            break;

        case UPGRADE_PEER_COMMIT_CFM:
            SendConfirmationToPeer(upgrade_peer->confirm_type, status);
            break;
        case UPGRADE_PEER_ABORT_REQ:
            if(upgradePeerInfo->SmCtx->peerState == UPGRADE_PEER_STATE_ABORTING)
            {
                /* Peer Disconnection already occured. It's time to stop upgrade
                 */
                DEBUG_LOG("UpgradePeerProcessHostMsg: StopUpgrade()");
                StopUpgrade();
            }
            else
            {
                /* Abort the DFU */
                AbortPeerDfu();
            }
            break;
        case UPGRADE_PEER_DATA_SEND_CFM:
            MessageSend((Task)&upgradePeerInfo->myTask,
                         INTERNAL_PEER_DATA_CFM_MSG, NULL);
            break;

        default:
            DEBUG_LOG("UpgradePeerProcessHostMsg: unhandled msg");
    }
}

/****************************************************************************
NAME
    UpgradePeerResumeUpgrade

DESCRIPTION
    resume the upgrade peer procedure.
    If an upgrade is processing, to resume it after a disconnection of the
    process, this method should be called to restart the existing running
    process.

*/
bool UpgradePeerResumeUpgrade(void)
{
    UPGRADE_PEER_CTX_T *upgrade_peer = UpgradePeerCtxGet();
    if (upgrade_peer->isUpgrading)
    {
        UpgradePeerCtxSet();
        SendSyncReq(upgradePeerInfo->md5_checksum);
    }

    return upgrade_peer->isUpgrading;
}

bool UpgradePeerIsSupported(void)
{
    return (upgradePeerInfo != NULL);
}

bool UpgradePeerIsPrimary(void)
{
    return ((upgradePeerInfo != NULL) && upgradePeerInfo->is_primary_device);
}

bool UpgradePeerIsSecondary(void)
{
    return upgradePeerInfo && !upgradePeerInfo->is_primary_device;
}

void UpgradePeerCtxInit(void)
{
    if(upgradePeerInfo != NULL && !UPGRADE_PEER_IS_STARTED)
    {
        UPGRADE_PEER_CTX_T *peerCtx;

        DEBUG_LOG("UpgradePeerCtxInit: UpgradePeer");

        peerCtx = (UPGRADE_PEER_CTX_T *) PanicUnlessMalloc(sizeof(*peerCtx));
        memset(peerCtx, 0, sizeof(*peerCtx));

        upgradePeerInfo->SmCtx = peerCtx;

        UpgradePeerCtxSet();
        UpgradePeerParititonInit();
    }
}

/****************************************************************************
NAME
    UpgradePeerIsDFUMode

DESCRIPTION
    Earbud SM need to know DFU information for the device after reboot.
    Information needed is if the device was in DFU mode before reboot.
    This information might be required even before the upgrade peer context is
    created. In that case creating the context just to read the information and
    then freeing it.
    If context is already created then just read the information from the context.
*/

bool UpgradePeerIsDFUMode(void)
{
    bool is_dfu_mode;
    /* If upgradePeerInfo context is not yet created */
    if(upgradePeerInfo == NULL)
    {
        UPGRADE_PEER_INFO_T *peerInfo;
        peerInfo = (UPGRADE_PEER_INFO_T *) PanicUnlessMalloc(sizeof(*peerInfo));
        memset(peerInfo, 0, sizeof(*peerInfo));

        upgradePeerInfo = peerInfo;

        UpgradePeerLoadPSStore(EARBUD_UPGRADE_CONTEXT_KEY, EARBUD_UPGRADE_PEER_CONTEXT_OFFSET);

        is_dfu_mode = upgradePeerInfo->UpgradePSKeys.is_dfu_mode;

        free(peerInfo);

        upgradePeerInfo = NULL;
    }
    else
    {
        is_dfu_mode = upgradePeerInfo->UpgradePSKeys.is_dfu_mode;
    }
    DEBUG_LOG("UpgradePeerIsDFUMode is_dfu_mode %d", is_dfu_mode);
    return is_dfu_mode;
}

/****************************************************************************
NAME
    UpgradePeerIsADK_20_1_SecondaryDevice

DESCRIPTION
    Check if device was secondary device for ADK20.1 based image
*/

bool UpgradePeerIsADK_20_1_SecondaryDevice(void)
{
    bool is_20_1_secondary_device;
    /* If upgradePeerInfo context is not yet created */
    if(upgradePeerInfo == NULL)
    {
        UPGRADE_PEER_INFO_T *peerInfo;
        peerInfo = (UPGRADE_PEER_INFO_T *) PanicUnlessMalloc(sizeof(*peerInfo));
        memset(peerInfo, 0, sizeof(*peerInfo));

        upgradePeerInfo = peerInfo;

        UpgradePeerLoadPSStore(EARBUD_UPGRADE_CONTEXT_KEY, EARBUD_UPGRADE_PEER_CONTEXT_OFFSET);

        is_20_1_secondary_device = (upgradePeerInfo->UpgradePSKeys.is_dfu_mode &
                    upgradePeerInfo->UpgradePSKeys.unused_is_secondary_device);

        free(peerInfo);

        upgradePeerInfo = NULL;
    }
    else
    {
        is_20_1_secondary_device = (upgradePeerInfo->UpgradePSKeys.is_dfu_mode &
                    upgradePeerInfo->UpgradePSKeys.unused_is_secondary_device);
    }
    DEBUG_LOG_INFO("UpgradePeerIsADK_20_1_SecondaryDevice is_20_1_secondary_device %d", is_20_1_secondary_device);
    return is_20_1_secondary_device;
}

/****************************************************************************
 NAME
    UpgradePeerInit

 DESCRIPTION
    Perform initialisation for the upgrade library. This consists of fixed
    initialisation as well as taking account of the information provided
    by the application.
*/

void UpgradePeerInit(Task appTask, uint16 dataPskey,uint16 dataPskeyStart,
                          upgrade_peer_scheme_t upgrade_peer_scheme)
{
    UPGRADE_PEER_INFO_T *peerInfo;

    DEBUG_LOG("UpgradePeerInit");

    peerInfo = (UPGRADE_PEER_INFO_T *) PanicUnlessMalloc(sizeof(*peerInfo));
    memset(peerInfo, 0, sizeof(*peerInfo));

    upgradePeerInfo = peerInfo;

    upgradePeerInfo->appTask = appTask;
    upgradePeerInfo->myTask.handler = HandleLocalMessage;

    upgradePeerInfo->is_primary_device = TRUE;

    upgradePeerInfo->upgrade_peer_scheme = upgrade_peer_scheme;

    UpgradePeerLoadPSStore(dataPskey,dataPskeyStart);

    MessageSend(upgradePeerInfo->appTask, UPGRADE_PEER_INIT_CFM, NULL);
}

/****************************************************************************
NAME
    UpgradePeerSetState

DESCRIPTION
    Set current state.
*/
void UpgradePeerSetState(upgrade_peer_state_t nextState)
{
    upgradePeerInfo->SmCtx->peerState = nextState;
}

/****************************************************************************
NAME
    UpgradePeerIsRestarted

DESCRIPTION
    After upgrade device is rebooted, check that is done or not.
*/
bool UpgradePeerIsRestarted(void)
{
    return ((upgradePeerInfo->SmCtx != NULL) &&
             (upgradePeerInfo->SmCtx->mResumePoint ==
                UPGRADE_PEER_RESUME_POINT_POST_REBOOT));
}

/****************************************************************************
NAME
    UpgradePeerIsCommited

DESCRIPTION
    Upgrade peer device has sent commit request or not.
*/
bool UpgradePeerIsCommited(void)
{
    return ((upgradePeerInfo->SmCtx != NULL) &&
             (upgradePeerInfo->SmCtx->peerState ==
                UPGRADE_PEER_STATE_COMMIT_CONFIRM));
}

/****************************************************************************
NAME
    UpgradePeerIsCommitContinue

DESCRIPTION
    Upgrade peer should be ready for commit after reboot.
*/
bool UpgradePeerIsCommitContinue(void)
{
    return (upgradePeerInfo->SmCtx->peerState ==
            UPGRADE_PEER_STATE_COMMIT_HOST_CONTINUE);
}

/****************************************************************************
NAME
    UpgradePeerIsStarted

DESCRIPTION
    Peer device upgrade procedure is started or not.
*/
bool UpgradePeerIsStarted(void)
{
    return (upgradePeerInfo->SmCtx != NULL);
}

/****************************************************************************
NAME
    UpgradePeerDeInit

DESCRIPTION
    Perform uninitialisation for the upgrade library once upgrade is done
*/
void UpgradePeerDeInit(void)
{
    DEBUG_LOG("UpgradePeerDeInit");

    free(upgradePeerInfo->SmCtx);
    upgradePeerInfo->SmCtx = NULL;
}

void UpgradePeerStoreMd5(uint32 md5)
{
    if(upgradePeerInfo == NULL)
    {
        return;
    }

    upgradePeerInfo->md5_checksum = md5;
}

/****************************************************************************
NAME
    UpgradePeerStartDfu

DESCRIPTION
    Start peer device DFU procedure.

    If Upgrade Peer library has not been initialised do nothing.
*/
bool UpgradePeerStartDfu(upgrade_image_copy_status_check_t status)
{
    if(upgradePeerInfo == NULL || upgradePeerInfo->appTask == NULL)
    {
        return FALSE;
    }

    DEBUG_LOG("UpgradePeerStartDfu: DFU Started");

    /* Allocate UpgradePeer SM Context */
    UpgradePeerCtxInit();

    /* Peer DFU is starting,lets be in SYNC state */
    UpgradePeerSetState(UPGRADE_PEER_STATE_SYNC);

    /* Peer DFU is going to start, so it's not aborted yet*/
    upgradePeerInfo->is_dfu_aborted = FALSE;

    if(status == UPGRADE_IMAGE_COPY_CHECK_REQUIRED)
    {
        /* Peer DFU will start, once the image upgrade copy is completed */
        MessageSendConditionally(upgradePeerInfo->appTask, UPGRADE_PEER_CONNECT_REQ, NULL, (uint16 *)UpgradeCtxGetImageCopyStatus());
    }
    else if(status == UPGRADE_IMAGE_COPY_CHECK_IGNORE)
    {
        DEBUG_LOG("UpgradePeerStartDfu block_cond:%d", upgradePeerInfo->block_cond);
        MessageSendConditionally(upgradePeerInfo->appTask, UPGRADE_PEER_CONNECT_REQ, NULL, (uint16 *)&upgradePeerInfo->block_cond);
    }

    return TRUE;
}

void UpgradePeerSetDeviceRolePrimary(bool is_primary)
{
    UpgradeSMHostRspSwap(is_primary);
}

void UpgradePeerSetDFUMode(bool is_dfu_mode)
{
    /* If upgradePeerInfo context is not yet created */
    if(upgradePeerInfo == NULL)
    {
        DEBUG_LOG("UpgradePeerSetDFUMode upgradePeerInfo context is not yet created, return");
        return;
    }

    DEBUG_LOG("UpgradePeerSetDFUMode is_dfu_mode %d", is_dfu_mode);
    upgradePeerInfo->UpgradePSKeys.is_dfu_mode = is_dfu_mode;
    UpgradePeerSavePSKeys();
}

void UpgradePeerCancelDFU(void)
{
    MessageCancelAll(upgradePeerInfo->appTask, UPGRADE_PEER_CONNECT_REQ);
}

void UpgradePeerResetStateInfo(void)
{
    /* If upgradePeerInfo context is not yet created */
    if(upgradePeerInfo == NULL)
    {
        DEBUG_LOG("UpgradePeerSetDFUMode upgradePeerInfo context is not yet created, return");
        return;
    }

    upgradePeerInfo->is_dfu_aborted = FALSE;
    upgradePeerInfo->is_primary_device = FALSE;
}

/****************************************************************************
NAME
    UpgradePeerWasPeerDfuInProgress

RETURNS
    Return FALSE if current state is not equal to UPGRADE_PEER_STATE_DATA_READY
    or DFU scheme not equal to serial or, upgradePeerInfo is NULL, else return TRUE
*/

bool UpgradePeerWasPeerDfuInProgress(void)
{
    if((upgradePeerInfo == NULL) || (UpgradePeerGetDFUScheme() != upgrade_peer_scheme_serial)
                                || (upgradePeerInfo->UpgradePSKeys.currentState
                                != UPGRADE_PEER_STATE_DATA_READY))
    {
        DEBUG_LOG("UpgradePeerWasPeerDfuInProgress: No");
        return FALSE;
    }
    else
    {
        return TRUE;
    }
}

/****************************************************************************
NAME
    UpgradePeerResetCurState

BRIEF
    Reset the current state element of PeerPskeys and SmCtx to default value
    as a result of role switch during DFU.
*/

void UpgradePeerResetCurState(void)
{
    if(upgradePeerInfo != NULL)
    {
        DEBUG_LOG("UpgradePeerResetCurState: Reset the current state");
        upgradePeerInfo->UpgradePSKeys.currentState = UPGRADE_PEER_STATE_CHECK_STATUS;
        UpgradePeerSavePSKeys();
        if(upgradePeerInfo->SmCtx != NULL)
        {
            upgradePeerInfo->SmCtx->peerState = UPGRADE_PEER_STATE_CHECK_STATUS;
        }
    }
}

/****************************************************************************
NAME
    UpgradePeerProcessDataRequest

DESCRIPTION
    Process a data packet from an Upgrade Peer client.

    If Upgrade Peer library has not been initialised do nothing.
*/
void UpgradePeerProcessDataRequest(upgrade_peer_app_msg_t id, uint8 *data,
                                   uint16 dataSize)
{
    uint8 *peer_data = NULL;
    upgrade_peer_connect_state_t state;

    if (upgradePeerInfo->SmCtx == NULL)
        return;

    DEBUG_LOG_VERBOSE("UpgradePeerProcessDataRequest, id %u, size %u", id, dataSize);

    switch(id)
    {
        case UPGRADE_PEER_CONNECT_CFM:
            state = ByteUtilsGet1ByteFromStream(data);
            DEBUG_LOG("UpgradePeerProcessDataRequest: Connect CFM state %d", state);
            if(state == UPGRADE_PEER_CONNECT_SUCCESS)
            {
                switch(upgradePeerInfo->SmCtx->peerState)
                {
                    case UPGRADE_PEER_STATE_SYNC:
                        StartUpgradePeerProcess(upgradePeerInfo->md5_checksum);
                    break;
                    case UPGRADE_PEER_STATE_RESTARTED_FOR_COMMIT:
                        UpgradePeerSetState(
                                   UPGRADE_PEER_STATE_COMMIT_HOST_CONTINUE);
                        UpgradeHandleMsg(NULL,
                                         HOST_MSG(UPGRADE_PEER_SYNC_AFTER_REBOOT_REQ),
                                         NULL);
                    break;
                    default:
                    {
                        /* We are here because an l2cap connection is created
                         * due to either Primary or Secondary reset during
                         * Primary to Secondary DFU file transfer. Now abort
                         * the DFU and clear the state variables for both the 
                         * devices.
                         */
                        if(upgradePeerInfo->is_dfu_abort_trigerred)
                        {
                             DEBUG_LOG("UpgradePeerProcessDataRequest: SendErrorConfirmation");
                             /* Send Error confirmation to Peer */
                             SendErrorConfirmation(UPGRADE_PEER_ERROR_UPDATE_FAILED);
                             /* Send Abort request to Upgrade SM*/
                             UpgradePeerSendErrorMsg(UPGRADE_PEER_ERROR_UPDATE_FAILED);
                             /* Since DFU abort is trigeered now, clear the flag */
                             upgradePeerInfo->is_dfu_abort_trigerred = FALSE;
                             /* Set the state to aborting */
                             UpgradePeerSetState(UPGRADE_PEER_STATE_ABORTING);
                             /* L2cap connection is no longer needed, so disconnect */
                             MessageSend(upgradePeerInfo->appTask, UPGRADE_PEER_DISCONNECT_REQ, NULL);
                        }
                        else
                            DEBUG_LOG("UpgradePeerProcessDataRequest: unhandled msg");
                    }
                }
            }
            else
            {
                upgradePeerInfo->SmCtx->upgrade_status =
                                      UPGRADE_PEER_ERROR_APP_NOT_READY;
                UpgradePeerSendErrorMsg(upgradePeerInfo->SmCtx->upgrade_status);
            }
            break;
        case UPGRADE_PEER_DISCONNECT_IND:
            if(upgradePeerInfo->SmCtx->peerState == UPGRADE_PEER_STATE_VALIDATED)
            {
                /* For now, do nothing, as we are supporting concurrent reboot */
                 DEBUG_LOG("UpgradePeerProcessDataRequest: UPGRADE_PEER_DISCONNECT_IND - VALIDATED state. Do nothing");
            }
            else if(upgradePeerInfo->SmCtx->peerState == UPGRADE_PEER_STATE_DATA_TRANSFER)
            {
                l2cap_disconnect_status l2cap_disconnect_reason = 
                                upgradePeerInfo->SmCtx->l2cap_disconnect_reason;

                /* Peer DFU got interrupt due to Secondary reset. Start the DFU
                 * process again
                 */
                /* Cancel all the existing upgradePeerInfo messages */
                MessageCancelAll((Task)&upgradePeerInfo->myTask, INTERNAL_PEER_MSG);

                /*
                 * Free the existing upgradePeerInfo->SmCtx and peerPartitionCtx
                 * Additionally also close, open partition (if any, probably
                 * possible when concurrent DFU is interrupted.
                 */
                UpgradePeerParititonDeInit();
                UpgradePeerDeInit();

                /*
                 * For disconnection due to linkloss, block peer DFU L2CAP
                 * channel setup until peer signaling channel is
                 * established which is a pre-condition to allow
                 * or DFU on a fresh start or resume post abrupt reset.
                 * Trigger the UpgradePeerStartDfu.
                 */
                if(l2cap_disconnect_reason == l2cap_disconnect_link_loss)
                {
                    DEBUG_LOG("UpgradePeerProcessDataRequest: Peer disconnection due to linkloss");
                    UpgradePeerUpdateBlockCond(UPGRADE_PEER_BLOCK_UNTIL_PEER_SIG_CONNECTED);
                    /* Since we are resuming the peer DFU, image upgrade copy of 
                     * primary device is already completed, so ignore the check
                     */
                    UpgradePeerStartDfu(UPGRADE_IMAGE_COPY_CHECK_IGNORE);
                }
            }
            else if(upgradePeerInfo->SmCtx->peerState == UPGRADE_PEER_STATE_ABORTING)
            {
                /* Once the l2cap connection is disconnected, clear the 
                 * upgradePeerInfo flag as part of clearance process
                 */
                CleanUpgradePeerCtx();
            }
            else 
            {
#if 1   /* ToDo: Once config controlled, replace with corresponding CC define */
                /*
                 * On abrupt reset of Secondary during Host->Primary
                 * data transfer results in linkloss to Secondary.
                 * Currently, stop the concurrent DFU and make provision for
                 * Primary to DFU in a serialized fashion.
                 * ToDo:
                 * 1) For DFU continue, this needs to be revisited.
                 * 2) Also reassess if fix for VMCSA-4340 can now be undone or
                 * still required for serialised DFU.
                 */
                MessageCancelAll((Task)&upgradePeerInfo->myTask,
                                    INTERNAL_PEER_MSG);
                UpgradePeerParititonDeInit();
                UpgradePeerDeInit();
                /*
                 * md5_checksum is retained so that same DFU file is
                 * relayed to the Secondary.
                 */
#else
                /* Peer device connection is lost and can't be recovered. Hence
                 * fail the upgrade process. Create the l2cap connection first,
                 * to send the DFU variables clearance to peer device.
                 */
                DEBUG_LOG("UpgradePeerProcessDataRequest: Create the l2cap connection to initiate DFU abort");
                /* Establish l2cap connection with peer to abort */
                MessageSend(upgradePeerInfo->appTask, UPGRADE_PEER_CONNECT_REQ, NULL);
                upgradePeerInfo->is_dfu_abort_trigerred = TRUE;
#endif
            }
            break;
        case UPGRADE_PEER_GENERIC_MSG:
            peer_data = (uint8 *) PanicUnlessMalloc(dataSize);
            ByteUtilsMemCpyToStream(peer_data, data, dataSize);
            MessageSend((Task)&upgradePeerInfo->myTask,
                         INTERNAL_PEER_MSG, peer_data);
            break;

        case UPGRADE_PEER_DATA_SEND_CFM:
            MessageSend((Task)&upgradePeerInfo->myTask,
                         INTERNAL_PEER_DATA_CFM_MSG, NULL);
            break;

        default:
            DEBUG_LOG_ERROR("UpgradePeerProcessDataRequest, unhandled message id %u", id);
    }
}

void UpgradePeerSetDFUScehme(upgrade_peer_scheme_t upgrade_peer_scheme)
{
    /* If upgradePeerInfo context is not yet created */
    if(upgradePeerInfo == NULL)
    {
        DEBUG_LOG("UpgradePeerSetDFUScehme upgradePeerInfo context is not yet created, return");
        return;
    }

    DEBUG_LOG("UpgradePeerSetDFUScehme upgrade_peer_scheme %d", upgrade_peer_scheme);
    upgradePeerInfo->upgrade_peer_scheme = upgrade_peer_scheme;
}

upgrade_peer_scheme_t UpgradePeerGetDFUScheme(void)
{
    /* If upgradePeerInfo context is not yet created */
    if(upgradePeerInfo == NULL)
    {
        DEBUG_LOG("UpgradePeerGetDFUScheme upgradePeerInfo context is not yet created, return");
        return -1;
    }

    DEBUG_LOG("UpgradePeerGetDFUScheme upgrade_peer_scheme %d", upgradePeerInfo->upgrade_peer_scheme);
    return upgradePeerInfo->upgrade_peer_scheme;
}

void UpgradePeerSetRole(bool role)
{
    if (upgradePeerInfo != NULL)
    {
        upgradePeerInfo->is_primary_device = role;
        DEBUG_LOG("UpgradePeerSetRole is_primary_device:%d", upgradePeerInfo->is_primary_device);
    }
}

void UpgradePeerUpdateBlockCond(upgrade_peer_block_cond_for_conn_t cond)
{
    if (upgradePeerInfo != NULL)
    {
        DEBUG_LOG("UpgradePeerUpdateBlockCond block_cond:%d", cond);
        upgradePeerInfo->block_cond = cond;
    }
}

void UpgradePeerStoreDisconReason(l2cap_disconnect_status reason)
{
    if (upgradePeerInfo != NULL)
    {
        DEBUG_LOG("UpgradePeerStoreDisconReason reason:%d", reason);
        upgradePeerInfo->SmCtx->l2cap_disconnect_reason = reason;
    }
}

bool UpgradePeerIsConnected(void)
{
    return (upgradePeerInfo && upgradePeerInfo->is_peer_connected);
}

void UpgradePeerSetConnectedStatus(bool val)
{
    if (upgradePeerInfo != NULL)
    {
        DEBUG_LOG("UpgradePeerSetConnectedStatus is_peer_connected:%d", val);
        upgradePeerInfo->is_peer_connected = val;
    }
}

bool UpgradePeerIsBlocked(void)
{
    return (upgradePeerInfo && upgradePeerInfo->block_cond);
}

uint16 * UpgradePeerGetPeersConnectionStatus(void)
{
    return &(upgradePeerInfo->block_cond);
}
