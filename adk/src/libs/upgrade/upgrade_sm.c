/****************************************************************************
Copyright (c) 2014 - 2018, 2020 Qualcomm Technologies International, Ltd.


FILE NAME
    upgrade_sm.c

DESCRIPTION

NOTES

*/

#define DEBUG_LOG_MODULE_NAME upgrade
#include <logging.h>

#include <stdlib.h>
#include <print.h>
#include <boot.h>
#include <loader.h>
#include <ps.h>
#include <panic.h>
#include <upgrade.h>
#ifdef HANDOVER_DFU_ABORT_WITHOUT_ERASE
#include <imageupgrade.h>
#endif

#include "upgrade_ctx.h"
#include "upgrade_host_if_data.h"
#include "upgrade_partition_data.h"
#include "upgrade_psstore.h"
#include "upgrade_partitions.h"
#include "upgrade_partition_validation.h"
#include "upgrade_partitions.h"
#include "upgrade_sm.h"
#include "upgrade_msg_vm.h"
#include "upgrade_msg_host.h"
#include "upgrade_msg_fw.h"
#include "upgrade_msg_internal.h"
#include "upgrade_fw_if.h"
#include "upgrade_peer.h"
#include "upgrade_peer_if_data.h"


#define VALIDATAION_BACKOFF_TIME_MS 100

static bool HandleCheckStatus(MessageId id, Message message);
static bool HandleSync(MessageId id, Message message);
static bool HandleReady(MessageId id, Message message);
static bool HandleProhibited(MessageId id, Message message);
static bool HandleAborting(MessageId id, Message message);
static bool HandleDataReady(MessageId id, Message message);
bool HandleDataTransfer(MessageId id, Message message);
static bool HandleDataTransferSuspended(MessageId id, Message message);
static bool HandleDataHashChecking(MessageId id, Message message);
static bool HandleValidating(MessageId id, Message message);
static bool HandleWaitForValidate(MessageId id, Message message);
static bool HandleRestartedForCommit(MessageId id, Message message);
static bool HandleCommitHostContinue(MessageId id, Message message);
static bool HandleCommitVerification(MessageId id, Message message);
static bool HandleCommitConfirm(MessageId id,Message message);
static bool HandleCommit(MessageId id, Message message);
static bool HandlePsJournal(MessageId id, Message message);
static bool HandleRebootToResume(MessageId id,Message message);
static bool HandleBatteryLow(MessageId id, Message message);

static bool DefaultHandler(MessageId id, Message message, bool handledAlready);

static UpgradeState GetState(void);

static void PsSpaceError(void);
static void CommitConfirmYes(void);

/* permission related functions */
static void UpgradeSMBlockingOpIsDone(void);
static void PsFloodAndReboot(void);
static void InformAppsCompleteGotoSync(void);
static void UpgradeSendUpgradeStatusInd(Task task, upgrade_state_t state, uint32 delay);

static void UpgradeCleanupOnAbort(void);

static bool asynchronous_abort = FALSE;

const upgrade_response_functions_t Upgrade_fptr = {
    .SendSyncCfm = UpgradeHostIFDataSendSyncCfm,
    .SendShortMsg = UpgradeHostIFDataSendShortMsg,
    .SendStartCfm = UpgradeHostIFDataSendStartCfm,
    .SendBytesReq = UpgradeHostIFDataSendBytesReq,
    .SendErrorInd = UpgradeHostIFDataSendErrorInd,
    .SendIsCsrValidDoneCfm = UpgradeHostIFDataSendIsCsrValidDoneCfm
};

const upgrade_response_functions_t UpgradePeer_fptr = {
    .SendSyncCfm = UpgradePeerIFDataSendSyncCfm,
    .SendShortMsg = UpgradePeerIFDataSendShortMsg,
    .SendStartCfm = UpgradePeerIFDataSendStartCfm,
    .SendBytesReq = UpgradePeerIFDataSendBytesReq,
    .SendErrorInd = UpgradePeerIFDataSendErrorInd,
    .SendIsCsrValidDoneCfm = UpgradePeerIFDataSendIsCsrValidDoneCfm
};

/***************************************************************************
NAME
    UpgradeSMInit  -  Initialise the State Machine

DESCRIPTION
    This function performs relevant initialisation of the state machine,
    currently just setting the initial state.

    This is currently determined by checking whether an upgraded
    application is running.

*/
void UpgradeSMInit(void)
{
    /* Set functions for Upgrade SM */
    UpgradeCtxGet()->funcs = &Upgrade_fptr;

    switch(UpgradeCtxGetPSKeys()->upgrade_in_progress_key)
    {
    /* UPGRADE_RESUME_POINT_PRE_VALIDATE:
        @todo: What do we do in this case ? */
    /* UPGRADE_RESUME_POINT_POST_REBOOT:
       UPGRADE_RESUME_POINT_COMMIT:
        @todo: Are these right, we want host to chat */
    default:
        UpgradeSMSetState(UPGRADE_STATE_CHECK_STATUS);
        break;

    /*case UPGRADE_RESUME_POINT_PRE_REBOOT:
        UpgradeSMSetState(UPGRADE_STATE_VALIDATED);
        break;*/

    case UPGRADE_RESUME_POINT_POST_REBOOT:
        PRINT(("UpgradeSMInit() in UPGRADE_RESUME_POINT_POST_REBOOT\n"));
        UpgradeSMSetState(UPGRADE_STATE_COMMIT_HOST_CONTINUE);
        MessageSendLater(UpgradeGetUpgradeTask(), UPGRADE_INTERNAL_RECONNECTION_TIMEOUT, NULL,
                D_SEC(UPGRADE_WAIT_FOR_RECONNECTION_TIME_SEC));
        break;

    /*case UPGRADE_RESUME_POINT_ERASE:
        UpgradeSMMoveToState(UPGRADE_STATE_COMMIT);
        break;*/

    case UPGRADE_RESUME_POINT_ERROR:
        UpgradeSMSetState(UPGRADE_STATE_ABORTING);
        break;
    }
}

UpgradeState UpgradeSMGetState(void)
{
    return GetState();
}

void UpgradeSMHandleMsg(MessageId id, Message message)
{
    bool handled = FALSE;

    DEBUG_LOG("UpgradeSMHandleMsg, state %u, message_id 0x%04x", GetState(), id);

    switch (GetState())
    {
    case UPGRADE_STATE_BATTERY_LOW:
        handled = HandleBatteryLow(id, message);
        break;

    case UPGRADE_STATE_CHECK_STATUS:
        handled = HandleCheckStatus(id, message);
        break;

    case UPGRADE_STATE_SYNC:
        handled = HandleSync(id, message);
        break;

    case UPGRADE_STATE_READY:
        handled = HandleReady(id, message);
        break;

    case UPGRADE_STATE_PROHIBITED:
        handled = HandleProhibited(id, message);
        break;

    case UPGRADE_STATE_ABORTING:
        handled = HandleAborting(id, message);
        break;

    case UPGRADE_STATE_DATA_READY:
        handled = HandleDataReady(id, message);
        break;

    case UPGRADE_STATE_DATA_TRANSFER:
        handled = HandleDataTransfer(id, message);
        break;

    case UPGRADE_STATE_DATA_TRANSFER_SUSPENDED:
        handled = HandleDataTransferSuspended(id, message);
        break;

    case UPGRADE_STATE_DATA_HASH_CHECKING:
        handled = HandleDataHashChecking(id, message);
        break;

    case UPGRADE_STATE_VALIDATING:
        handled = HandleValidating(id, message);
        break;

    case UPGRADE_STATE_WAIT_FOR_VALIDATE:
        handled = HandleWaitForValidate(id, message);
        break;

    case UPGRADE_STATE_VALIDATED:
        handled = UpgradeSMHandleValidated(id, message);
        break;

    case UPGRADE_STATE_RESTARTED_FOR_COMMIT:
        handled = HandleRestartedForCommit(id, message);
        break;

    case UPGRADE_STATE_COMMIT_HOST_CONTINUE:
        handled = HandleCommitHostContinue(id, message);
        break;

    case UPGRADE_STATE_COMMIT_VERIFICATION:
        handled = HandleCommitVerification(id, message);
        break;

    case UPGRADE_STATE_COMMIT_CONFIRM:
        handled = HandleCommitConfirm(id, message);
        break;

    case UPGRADE_STATE_COMMIT:
        handled = HandleCommit(id, message);
        break;

    case UPGRADE_STATE_PS_JOURNAL:
        handled = HandlePsJournal(id, message);
        break;

    case UPGRADE_STATE_REBOOT_TO_RESUME:
        handled = HandleRebootToResume(id,message);
        break;

    default:
        DEBUG_LOG("UpgradeSMHandleMsg, unknown state %u", GetState());
        break;
    }

    if (GetState() != UPGRADE_STATE_CHECK_STATUS)
    {
        handled = DefaultHandler(id, message, handled);
    }

    if (!handled)
    {
        DEBUG_LOG("UpgradeSMHandleMsg, message 0x%04x not handled", id);
    }

    /*
     * TODO: Can be skipped for DFU over BR/EDR transport and also when
     *       upgrade data is relayed from Primary to Secondary over
     *       BR/EDR.
     */
    {
        /* Conditionally decrement only when a packet is processed.*/
        if (UpgradeCtxGet()->pendingDataReq)
        {
            UpgradeCtxGet()->pendingDataReq--;
        }
        /* Unconditionally flow on so that the DFU doesn't stall.*/
        UpgradeFlowOffProcessDataRequest(FALSE);
    }

    DEBUG_LOG("UpgradeSMHandleMsg, new state %u", GetState());
}

bool HandleCheckStatus(MessageId id, Message message)
{
    UNUSED(message);
    switch(id)
    {
    case UPGRADE_VM_PERMIT_UPGRADE:
        UpgradeSMSetState(UPGRADE_STATE_SYNC);
        break;

    case UPGRADE_INTERNAL_IN_PROGRESS:
        UpgradeSMSetState(UPGRADE_STATE_RESTARTED_FOR_COMMIT);
        break;

    default:
        return FALSE;
    }

    return TRUE;
}

bool HandleBatteryLow(MessageId id, Message message)
{
    if(UpgradeCtxGet()->power_state != upgrade_battery_low)
    {
        /* this forces DefaultHandle to be called to handle sync request */
        return FALSE;
    }

    /* in this state always send low battery error message */
    if (id != UPGRADE_HOST_ERRORWARN_RES)
    {
        if (id == UPGRADE_HOST_DATA)
        {
            UPGRADE_HOST_DATA_T *msg = (UPGRADE_HOST_DATA_T *)message;

            /* There may be several of these messages in a row, and
             * we only want to send on error message.
             * Part-process the message. If we get an error response
             * then we can assume that we have already sent an error 
             * and do not send another */
            if (UPGRADE_PARTITION_DATA_XFER_ERROR ==
                          UpgradePartitionDataParse((uint8 *)&msg->data[0], msg->length))
            {
                return TRUE;
            }
            UpgradePartitionDataStopData();
        }

        UpgradeCtxGet()->funcs->SendErrorInd(UPGRADE_HOST_ERROR_BATTERY_LOW);
    }

    return TRUE;
}

bool HandleSync(MessageId id, Message message)
{
    UNUSED(message);
    switch(id)
    {
#ifdef UPGRADE_SYNC_WILL_FORCE_COMMIT_PHASE
    /* @todo
     * This is a temporary solution to force state machine to
     * proceed to commit phase after reboot.
     */
    case UPGRADE_HOST_SYNC_AFTER_REBOOT_REQ:
        UpgradeCtxGet()->funcs->SendShortMsg(UPGRADE_HOST_IN_PROGRESS_IND);
        UpgradeSMSetState(UPGRADE_STATE_COMMIT_HOST_CONTINUE);
        break;
#endif

    case UPGRADE_INTERNAL_RECONNECTION_TIMEOUT:
        UpgradeRevertUpgrades();
        BootSetMode(BootGetMode());
        break;

    default:
        return FALSE;
    }

    return TRUE;
}

bool HandleReady(MessageId id, Message message)
{
    UNUSED(message);
    switch(id)
    {
    case UPGRADE_HOST_START_REQ:
        {
            bool error = FALSE;
            bool newUpgrade = TRUE;

            PRINT(("UPGRADE_HOST_START_REQ handled\n"));
            PRINT(("P&R: going to jump to resume point %d\n", UpgradeCtxGetPSKeys()->upgrade_in_progress_key));

            switch(UpgradeCtxGetPSKeys()->upgrade_in_progress_key)
            {
            case UPGRADE_RESUME_POINT_START:
                UpgradeSMSetState(UPGRADE_STATE_DATA_READY);
                break;
            case UPGRADE_RESUME_POINT_PRE_VALIDATE:
                UpgradePartitionValidationInit();
                UpgradeSMMoveToState(UPGRADE_STATE_VALIDATING);
                break;
            case UPGRADE_RESUME_POINT_PRE_REBOOT:
                UpgradeSMSetState(UPGRADE_STATE_VALIDATED);
                newUpgrade = FALSE;
                break;
            case UPGRADE_RESUME_POINT_POST_REBOOT:
                UpgradeSMSetState(UPGRADE_STATE_COMMIT_HOST_CONTINUE);
                newUpgrade = FALSE;
                break;
            /*case UPGRADE_RESUME_POINT_COMMIT:
                UpgradeSMSetState(UPGRADE_STATE_COMMIT_CONFIRM);
                newUpgrade = FALSE;
                break;*/
            case UPGRADE_RESUME_POINT_ERASE:
                UpgradeSMMoveToState(UPGRADE_STATE_COMMIT);
                newUpgrade = FALSE;
                break;
            case UPGRADE_RESUME_POINT_ERROR:
                UpgradeSMSetState(UPGRADE_STATE_ABORTING);
                /* @todo do we need to set error = TRUE here? We need to send an error to the host
                 * or we'll get stuck in the ABORTING state */
                break;
            default:
                PRINT(("UPGRADE_HOST_START_REQ unexpected in progress key %d\n", UpgradeCtxGetPSKeys()->upgrade_in_progress_key));
                error = TRUE;
            }

            if(!error)
            {
                UpgradeCtxGet()->funcs->SendStartCfm(0, 0x666);
            }
            else
            {
                FatalError(UPGRADE_HOST_ERROR_INTERNAL_ERROR_4);
            }

            /* We are starting/resuming an upgrade so update target partitions */
            if (newUpgrade)
            {
                UpgradePartitionsUpgradeStarted();
            }
        }
        break;

    default:
        return FALSE;
    }

    return TRUE;
}

bool HandleProhibited(MessageId id, Message message)
{
    UNUSED(id);
    UNUSED(message);
    return FALSE;
}

bool HandleAborting(MessageId id, Message message)
{
    UNUSED(message);
    switch(id)
    {
    case UPGRADE_HOST_ERRORWARN_RES:
        DEBUG_LOG("HandleAborting UPGRADE_HOST_ERRORWARN_RES");
#ifdef HANDOVER_DFU_ABORT_WITHOUT_ERASE
        /* Check if Handover triggered DFU abort is completed */
        if (UpgradeGetHandoverDFUAbortState() &&
                !UpgradeIsHandoverDFUAbortComplete())
        {
            DEBUG_LOG("HandleAborting Handover DFU abort: ERRORWARN_RES skipped");
            UpgradeSetHandoverDFUAbortState(handover_upgrade_abort_errorwarn_res_recvd);
            /*
             * Send abort_req to the peer (Secondary) via UpgradeSMAbort(),
             * if peer upgrade is in progress.
             * But override the application granted permission to upgrade library
             * in order to disallow local erase as well as psskey info clear
             * to psstore (i.e. both local & peer) on Primary for the device
             * initiated abort owing to a Handover trigger.
             *
             * psstore info needs to be retained to support seamless DFU
             * especially after a Handover completion.
             */
            UpgradeCtxGet()->perms = upgrade_perm_no;
#ifdef MESSAGE_IMAGE_UPGRADE_COPY_STATUS
            /*
             * There may be non-blocking traps such as ImageUpgradeCopy in progress.
             * Call the ImageUpgradeAbortCommand() trap to abort any of those. It
             * will do no harm if there are no non-blocking traps in progress.
             */
            DEBUG_LOG("HandleAborting ImageUpgradeAbortCommand ret:%d", ImageUpgradeAbortCommand());
#endif  /* MESSAGE_IMAGE_UPGRADE_COPY_STATUS */
        }
#endif
        UpgradeSMAbort();

        break;
    case UPGRADE_HOST_ABORT_REQ:
        DEBUG_LOG("HandleAborting, UPGRADE_HOST_ABORT_REQ recvd");
#ifdef HANDOVER_DFU_ABORT_WITHOUT_ERASE
        /* Check if Handover triggered DFU abort is completed */
        if (UpgradeGetHandoverDFUAbortState() &&
                !UpgradeIsHandoverDFUAbortComplete())
        {
            DEBUG_LOG("HandleAborting Handover DFU abort: ABORT_REQ skipped");
            UpgradeSetHandoverDFUAbortState(handover_upgrade_abort_abort_req_recvd);
            /*
             * Reset the the upgrade library state as erase is bypassed for
             * Handover triggered DFU abort.
             */
            UpgradeSMSetState(UPGRADE_STATE_SYNC);
            /* Send CFM as ABORT_REQ handling is skipped i.e. erase bypassed. */
            UpgradeCtxGet()->funcs->SendShortMsg(UPGRADE_HOST_ABORT_CFM);
            UpgradeSetHandoverDFUAbortState(handover_upgrade_abort_abort_cfm_sent);
            /*
             * Notify application, since Handover initiated abort procedure is
             * now completed and hence application can do the required cleanup,
             * if any.
             */
            UpgradeSendEndUpgradeDataInd(upgrade_end_state_abort);
        }
        else
#endif
        {
            /*
             * Peer (Secondary) device initiated abort owing to internal
             * errors reported via FatalError(), shall be handled here owing to
             * following reason:
             * - Peer has transitioned to UPGRADE_STATE_ABORTING and sending
             *   UPGRADE_HOST_ERRORWARN_IND/UPGRADE_PEER_ERROR_WARN_IND.
             * - Primary on reception of UPGRADE_PEER_ERROR_WARN_IND relays the
             *   same to Host as UPGRADE_HOST_ERRORWARN_IND.
             * - Primary in response receives UPGRADE_HOST_ERRORWARN_RES from
             *   the Host and relays this as UPGRADE_PEER_ABORT_REQ to Secondary
             * - Between Primary to Secondary UPGRADE_PEER_ERROR_WARN_RES/
             *   UPGRADE_HOST_ERRORWARN_RES are not exchanged for internal
             *   errors reported via FatalError() on the peer (Secondary).rather
             *   its UPGRADE_PEER_ABORT_REQ/UPGRADE_HOST_ABORT_REQ.
             * - Hence Peer (Secondary) shall handle UPGRADE_PEER_ABORT_REQ/
             *   UPGRADE_HOST_ABORT_REQ here in this state.
             *
             * Its fine to early notify application without awaiting the
             * internal cleanup that includes reseting of upgrade library pskey
             * info and erase of alternate DFU bank, so that the application
             * can simultaneously do the required cleanup, if any.
             */
            UpgradeSendEndUpgradeDataInd(upgrade_end_state_abort);
            asynchronous_abort = UpgradeSMAbort();
            DEBUG_LOG("HandleAborting UPGRADE_HOST_ABORT_REQ recvd, UpgradeSMAbort() returned %d", asynchronous_abort);
            if (!asynchronous_abort)
            {
                UpgradeCtxGet()->funcs->SendShortMsg(UPGRADE_HOST_ABORT_CFM);
            }
        }
        break;
    case UPGRADE_HOST_SYNC_REQ:
        /* TODO: Maybe we also should send something like last error here */
        UpgradeCtxGet()->funcs->SendErrorInd(UPGRADE_HOST_ERROR_IN_ERROR_STATE);
        break;
    default:
        return FALSE;
    }
    return TRUE;
}

bool HandleDataReady(MessageId id, Message message)
{
    bool WaitForEraseComplete = FALSE;
    UNUSED(message);
    switch(id)
    {
    case UPGRADE_HOST_START_DATA_REQ:
        {
            if (UpgradePartitionDataInit(&WaitForEraseComplete))
            {
                DEBUG_LOG("HandleDataReady WaitForEraseComplete:%d", WaitForEraseComplete);

                if(UPGRADE_PEER_IS_PRIMARY && WaitForEraseComplete)
                {
                    /*
                     * Early notify the DFU domain so that it can request for
                     * a parallel erase on the Peer (viz. Secondary), if applicable
                     */
                    UpgradeSendStartPeerEraseInd();
                 }

                /*
                 * Notify application of UPGRADE_START_DATA_IND on erase
                 * completion to prevent apps P1 being blocked owing to blocking
                 * trap calls of PsStore used to store upgrade pskey info
                 * (i.e. is_secondary_device and is_dfu_mode)
                 */
                UpgradeCtxGet()->isImgUpgradeEraseDone = (uint16)WaitForEraseComplete;
                DEBUG_LOG_INFO("Upgrade, start, waiting for erase %u", WaitForEraseComplete);
                UpgradeSendStartUpgradeDataInd();
                if (!WaitForEraseComplete)
                {
                    uint16 req_size = UpgradePartitionDataGetNextReqSize();
                    DEBUG_LOG_INFO("Upgrade, requesting %u bytes", req_size);
                    UpgradeCtxGet()->funcs->SendBytesReq(req_size, 0);
                    UpgradeSMSetState(UPGRADE_STATE_DATA_TRANSFER);
                }
                /* WaitForEraseComplete == TURE only occurs in CONFIG_HYDRACORE. */                
            }
            else
            {
                FatalError(UPGRADE_HOST_ERROR_NO_MEMORY);
            }
        }
        break;

    default:
        return FALSE;
    }

    return TRUE;
}

bool HandleDataTransfer(MessageId id, Message message)
{
    switch(id)
    {
    case UPGRADE_HOST_DATA:
        if(message)
        {
            UPGRADE_HOST_DATA_T *msg = (UPGRADE_HOST_DATA_T *)message;
            UpgradeHostErrorCode rc;

            /* TODO: Validate message length */

            rc = UpgradePartitionDataParse(msg->data, msg->length);
            DEBUG_LOG_VERBOSE("Upgrade, UPGRADE_HOST_DATA, rc %u", rc);

            /* Check for upgrade file size errors */
            if(rc == UPGRADE_HOST_SUCCESS && msg->lastPacket)
            {
                rc = UPGRADE_HOST_ERROR_FILE_TOO_SMALL;
            }
            else if(rc == UPGRADE_HOST_DATA_TRANSFER_COMPLETE && !msg->lastPacket)
            {
                rc = UPGRADE_HOST_ERROR_FILE_TOO_BIG;
            }

            if(rc == UPGRADE_HOST_SUCCESS)
            {
                uint32 req_size = UpgradePartitionDataGetNextReqSize();
                if(req_size)
                {
                    uint32 offset = UpgradePartitionDataGetNextOffset();
                    DEBUG_LOG_INFO("upgradeHandleDataTransfer, requesting %u bytes at offset %u", req_size, offset);
                    UpgradeCtxGet()->funcs->SendBytesReq(req_size, offset);
                }
                else
                {
                    DEBUG_LOG_VERBOSE("Upgrade, no more bytes to request");
                }
            }
            else if(rc == UPGRADE_HOST_DATA_TRANSFER_COMPLETE)
            {
                /*    Calculate and validate data hash(s).   */
                UpgradeCtxGet()->isCsrValidDoneReqReceived = FALSE;
                /*
                 * Reset as data transfer is completed and validation-copy
                 * to follow.
                 */
                UpgradeCtxGet()->isImgUpgradeCopyDone = FALSE;
                UpgradeCtxGet()->ImgUpgradeCopyStatus = FALSE;

                /* Check for if concurrent DFU is in progress and check if
                 * link loss occurs between the peers.Delay the hash checking of
                 * primary device until the peer dfu is completed or connection between peers is back
                 */
                if(UPGRADE_PEER_IS_BLOCKED || (UPGRADE_PEER_IS_PRIMARY && UPGRADE_PEER_IS_STARTED))
                {
#ifndef HOSTED_TEST_ENVIRONMENT
                    DEBUG_LOG_VERBOSE("HandleDataTransfer: Upgrade, delay HashChecking");
                    UpgradeSMSetState(UPGRADE_STATE_DATA_HASH_CHECKING);
                    MessageSendConditionally(UpgradeGetUpgradeTask(), UPGRADE_INTERNAL_CONTINUE, NULL,
                                                              UpgradeCtxGetPeerDataTransferStatus());
#endif
                }
                else
                {
                    UpgradeSMMoveToState(UPGRADE_STATE_DATA_HASH_CHECKING);
                }
            }
            else
            {
                if(rc == UPGRADE_HOST_ERROR_PARTITION_CLOSE_FAILED_PS_SPACE)
                {
                    PsSpaceError();
                }
                else
                {
                    FatalError(rc);
                }
            }
        }
        break;

    default:
        return FALSE;
    }

    return TRUE;
}

bool HandleDataTransferSuspended(MessageId id, Message message)
{
    UNUSED(id);
    UNUSED(message);
    return FALSE;
}

bool HandleDataHashChecking(MessageId id, Message message)
{
    UNUSED(message);

    UpgradeCtx *ctx = UpgradeCtxGet();

    PRINT(("Message id = %d", id));

    bool hashCheckDone = FALSE;
    bool hashCheckedOk = FALSE;
    
    switch(id)
    {
    case UPGRADE_INTERNAL_CONTINUE:
        {
            if(ctx->vctx != NULL)
            {
                DEBUG_LOG_VERBOSE("HandleDataHashChecking: Already in progress");
            }
            else
            {
                ctx->vctx = ImageUpgradeHashInitialise(SHA256_ALGORITHM);

                if (ctx->vctx == NULL)
                {
                    Panic();
                }
                
                switch(UpgradeFWIFValidateStart(ctx->vctx))
                {
                    case UPGRADE_HOST_OEM_VALIDATION_SUCCESS:
                        hashCheckedOk = UpgradeFWIFValidateFinish(ctx->vctx, ctx->partitionData->signature);
                        if(!hashCheckedOk)
                        {
                            FatalError(UPGRADE_HOST_ERROR_OEM_VALIDATION_FAILED_FOOTER);
                        }
                        hashCheckDone = TRUE;
                        break;
                        
                    case UPGRADE_HOST_HASHING_IN_PROGRESS:
                        break;
                        
                    case UPGRADE_HOST_ERROR_OEM_VALIDATION_FAILED_FOOTER:
                    default:
                        FatalError(UPGRADE_HOST_ERROR_OEM_VALIDATION_FAILED_FOOTER);
                        hashCheckDone = TRUE;
                        break;
                }
            }
        }
        break;

    case UPGRADE_HOST_IS_CSR_VALID_DONE_REQ:
        
        if(UpgradePartitionValidationValidate() == UPGRADE_PARTITION_VALIDATION_IN_PROGRESS)
        {
            UpgradeHostIFDataSendIsCsrValidDoneCfm(VALIDATAION_BACKOFF_TIME_MS);
            ctx->isCsrValidDoneReqReceived = TRUE;
        }
        else
        {
            /* Record arrival the 'UPGRADE_HOST_IS_CSR_VALID_DONE_REQ' message from the host as 
               no 'backoff' mechanism is implemented in the HID (USB) upgrade mechanism. */
            
            ctx->isCsrValidDoneReqReceived = TRUE;
        }
        break;

    case UPGRADE_VM_HASH_ALL_SECTIONS_SUCCESSFUL:
        hashCheckedOk = UpgradeFWIFValidateFinish(ctx->vctx, ctx->partitionData->signature);
        if(!hashCheckedOk)
        {
            FatalError(UPGRADE_HOST_ERROR_OEM_VALIDATION_FAILED_FOOTER);
        }
        hashCheckDone = TRUE;
        break;
        
    case UPGRADE_VM_HASH_ALL_SECTIONS_FAILED:
        FatalError(UPGRADE_HOST_ERROR_OEM_VALIDATION_FAILED_FOOTER);
        hashCheckDone = TRUE;
        break;
        
    default:
        return FALSE;
    }

    if(hashCheckDone)
    {
        /* Once hash check is done, free up the signature and reset the hash ctx
           irrepsctive of the fact if hash check was successful or failure. */
        free(ctx->partitionData->signature);
        ctx->partitionData->signature = 0;
        ctx->vctx = NULL;
    }

    if(hashCheckedOk)
    {
        /* change the resume point, now that all data has been
         * downloaded, and ensure we remember it. */
        UpgradeCtxGetPSKeys()->upgrade_in_progress_key = UPGRADE_RESUME_POINT_PRE_VALIDATE;
        UpgradeSavePSKeys();
        PRINT(("P&R: UPGRADE_RESUME_POINT_PRE_VALIDATE saved\n"));
       
        UpgradePartitionValidationInit();
        UpgradeSMMoveToState(UPGRADE_STATE_VALIDATING);
    }
    return TRUE;
}

bool HandleValidating(MessageId id, Message message)
{
    UNUSED(message);
    switch(id)
    {
    case UPGRADE_INTERNAL_CONTINUE:
        {
            UpgradePartitionValidationResult res;
            res = UpgradePartitionValidationValidate();
            if(res == UPGRADE_PARTITION_VALIDATION_IN_PROGRESS)
            {
                UpgradeSMMoveToState(UPGRADE_STATE_WAIT_FOR_VALIDATE);
            }
            else
            {
                /* For standalone DFU, set the resume point to UPGRADE_RESUME_POINT_PRE_REBOOT
                 * and moved the state to UPGRADE_STATE_VALIDATED along with image copy from
                 * one bank to another.
                 */
                if(!UPGRADE_PEER_IS_PRIMARY)
                {
                    /* Validation completed, and now waiting for UPGRADE_TRANSFER_COMPLETE_RES
                     * protocol message. Update resume point and ensure we remember it. */
                    UpgradeCtxGetPSKeys()->upgrade_in_progress_key = UPGRADE_RESUME_POINT_PRE_REBOOT;
                    UpgradeSavePSKeys();
                    DEBUG_LOG("P&R: UPGRADE_RESUME_POINT_PRE_REBOOT saved");
                    UpgradeSMActionOnValidated();
                    UpgradeCtxSetImageCopyStatus(IMAGE_UPGRADE_COPY_IN_PROGRESS);
                    UpgradeSMMoveToState(UPGRADE_STATE_VALIDATED);
                }
                /* If the primary device is reset during prim to sec data, then by resume point
                 * data, we will land here. Since image copy is already completed, don't 
                 * perform again. Just pass the COPY_SUCCESSFUL id to UpgradeSMHandleValidated
                 * Also set the state as UPGRADE_STATE_VALIDATED, since it was the state before reset.
                 */
                 /* Note: currently the handling of resuming peer dfu on primary reset 
                  * is done for serialized DFU. This section will be updated (if needed) once
                  * the scenario is handled for concurrent DFU as well
                  */
                else if(UpgradePeerWasPeerDfuInProgress())
                {
                    UpgradeCtxGet()->isImgUpgradeCopyDone = TRUE;
                    UpgradeSMHandleValidated(UPGRADE_VM_IMAGE_UPGRADE_COPY_SUCCESSFUL, NULL);
                    UpgradeSMSetState(UPGRADE_STATE_VALIDATED);
                }
                /* For peer DFU, after validation, start the image copy process and move the 
                 * state to UPGRADE_STATE_VALIDATED. Resume point will be set to
                 * UPGRADE_RESUME_POINT_PRE_REBOOT, only after the prim device receives the 
                 * UPGRADE_HOST_TRANSFER_COMPLETE_IND from peer device.
                 */
                else
                {
                    UpgradeSMActionOnValidated();
                    UpgradeCtxSetImageCopyStatus(IMAGE_UPGRADE_COPY_IN_PROGRESS);
                    UpgradeSMMoveToState(UPGRADE_STATE_VALIDATED);
                }
            }
        }
        break;

    case UPGRADE_HOST_IS_CSR_VALID_DONE_REQ:
       {
            UpgradeCtx *ctx = UpgradeCtxGet();
            UpgradeCtxGet()->funcs->SendIsCsrValidDoneCfm(VALIDATAION_BACKOFF_TIME_MS);
            ctx->isCsrValidDoneReqReceived = TRUE;
            break;
       }

    default:
        return FALSE;
    }

    return TRUE;
}

bool HandleWaitForValidate(MessageId id, Message message)
{
    UNUSED(message);
    switch(id)
    {
    case UPGRADE_VM_EXE_FS_VALIDATION_STATUS:
        {
            UPGRADE_VM_EXE_FS_VALIDATION_STATUS_T *msg = (UPGRADE_VM_EXE_FS_VALIDATION_STATUS_T *)message;

            if (msg->result)
            {
                UpgradeSMMoveToState(UPGRADE_STATE_VALIDATING);
            }
            else
            {
                FatalError(UPGRADE_HOST_ERROR_SFS_VALIDATION_FAILED);
            }
        }
        break;

    case UPGRADE_HOST_IS_CSR_VALID_DONE_REQ:
        {
            UpgradeCtx *ctx = UpgradeCtxGet();
            UpgradeCtxGet()->funcs->SendIsCsrValidDoneCfm(VALIDATAION_BACKOFF_TIME_MS);
            ctx->isCsrValidDoneReqReceived = TRUE;
            break;
        }

    default:
        return FALSE;
    }

    return TRUE;
}

bool HandleRestartedForCommit(MessageId id, Message message)
{
    UNUSED(message);
    switch(id)
    {
    case UPGRADE_HOST_SYNC_AFTER_REBOOT_REQ:
        /* Device is rebooted and connection is up. If peer device upgrade
         * is supported then there is possibilty that both GAIA and L2CAP
         * connection are up at same time. In this case there will be race 
         * condition for same meessage UPGRADE_HOST_SYNC_AFTER_REBOOT_REQ
         * beetween UpgradeSm and UpgradePeer. UpgradeSm might receive this
         * message from host as well as UpgradePeer back to back. We need to
         * ensure only one time this message is processed.
         * If Host message comes first then only state is chnage so that
         * UpgradePeer message will take further action and vice versa.
         */
        if(UPGRADE_PEER_IS_PRIMARY && UPGRADE_PEER_IS_STARTED)
        {
            if(UPGRDAE_PEER_IS_COMMIT_CONTINUE &&
              (UpgradeSMGetState() == UPGRADE_STATE_COMMIT_HOST_CONTINUE))
            {
                UpgradeCtxGet()->funcs->SendShortMsg(UPGRADE_HOST_IN_PROGRESS_IND);
            }
            else
            {
                UpgradeSMSetState(UPGRADE_STATE_COMMIT_HOST_CONTINUE);
            }
        }
        else
        {
            UpgradeCtxGet()->funcs->SendShortMsg(UPGRADE_HOST_IN_PROGRESS_IND);
            UpgradeSMSetState(UPGRADE_STATE_COMMIT_HOST_CONTINUE);
        }
        break;

    default:
        return FALSE;
    }

    return TRUE;
}

/* We end up here after reboot */
bool HandleCommitHostContinue(MessageId id, Message message)
{
    switch(id)
    {
    case UPGRADE_HOST_IN_PROGRESS_RES:
        {
            UPGRADE_HOST_IN_PROGRESS_RES_T *msg = (UPGRADE_HOST_IN_PROGRESS_RES_T *)message;

            MessageCancelFirst(UpgradeGetUpgradeTask(), UPGRADE_INTERNAL_RECONNECTION_TIMEOUT);

            /*
             * In the post reboot DFU commit phase, now main role
             * (Primary/Secondary) are no longer fixed rather dynamically
             * selected by Topology using role selection. So if a role swap
             * occurs in the post reboot DFU commit phase.
             * (e.g. Primary on post reboot DFU commit phase becomes Secondary
             * In this scenario, the peer DFU L2CAP channel is established
             * by Old Primary and as a result UPGRADE_PEER_IS_STARTED won't be
             * satisfied on the New Primary.)
             *
             * Since the DFU domain communicates the main role on peer
             * signalling channel establishment which is established earlier
             * then handset connection, the necessary and sufficient pre-
             * conditions to relay UPGRADE_PEER_IN_PROGRESS_RES are as follows:
             * - firstly on the role as Primary and
             * - secondly on the peer DFU channel being setup. If peer DFU
             * channel isn't setup then defer relaying UPGRADE_PEER_IN_PROGRESS_RES
             * to the peer. Peer DFU channel is established post peer signalling
             * channel establishment.
             */
            if(UPGRADE_PEER_IS_PRIMARY)
            {
                if (UPGRADE_PEER_IS_CONNECTED)
                {
                    /* Send msg to peer device first for commit. */
                    UpgradePeerSetConnectedStatus(FALSE);
                    UpgradePeerProcessHostMsg(UPGRADE_PEER_IN_PROGRESS_RES,
                                                msg->action);
                }
                else
                {
                    UPGRADE_HOST_IN_PROGRESS_RES_T *repost_msg;
                    repost_msg = (UPGRADE_HOST_IN_PROGRESS_RES_T *)PanicUnlessMalloc(sizeof(UPGRADE_HOST_IN_PROGRESS_RES_T));
                    memmove(repost_msg, msg, sizeof(UPGRADE_HOST_IN_PROGRESS_RES_T));
                    MessageSendLater(UpgradeGetUpgradeTask(), UPGRADE_HOST_IN_PROGRESS_RES,
                                     repost_msg, UPGRADE_PEER_POLL_INTERVAL);
                    break;
                }
            }

            if(msg->action == 0)
            {
                UpgradeSMMoveToState(UPGRADE_STATE_COMMIT_VERIFICATION);
            }
            else
            {
                UpgradeSMMoveToState(UPGRADE_STATE_SYNC);
            }
        }
        break;

    case UPGRADE_INTERNAL_RECONNECTION_TIMEOUT:
        {
            bool dfu = UpgradePartitionDataIsDfuUpdate();
            uint16 err = UpgradeSMNewImageStatus();

            if(dfu && !err)
            {
                /* Carry on */
                CommitConfirmYes();
            }
            else
            {
                /* Revert */
                UpgradeRevertUpgrades();
                UpgradeCtxGetPSKeys()->upgrade_in_progress_key = UPGRADE_RESUME_POINT_ERROR;
                UpgradeSavePSKeys();
                PRINT(("P&R: UPGRADE_RESUME_POINT_ERROR saved\n"));
                UpgradeSMSetState(UPGRADE_STATE_SYNC);
                BootSetMode(BootGetMode());
            }
        }
        break;

    default:
        return FALSE;
    }

    return TRUE;
}

bool HandleCommitVerification(MessageId id, Message message)
{
    UNUSED(message);
    switch(id)
    {
    case UPGRADE_INTERNAL_CONTINUE:
        {
            if(UPGRADE_PEER_IS_PRIMARY && !UPGRADE_PEER_IS_COMMITED)
            {
                /* If Peer upgrade is running then we will wait for
                 * UPGRADE_PEER_COMMIT_REQ to proceed further.
                 */
                return TRUE;
            }
            UpgradeFWIFApplicationValidationStatus status;

            status = UpgradeFWIFValidateApplication();
            if (UPGRADE_FW_IF_APPLICATION_VALIDATION_SKIP != status)
            {                
                if (UPGRADE_FW_IF_APPLICATION_VALIDATION_RUNNING == status)
                {
                    UpgradeSMMoveToState(UPGRADE_STATE_COMMIT_VERIFICATION);
                }
                else
                {
                    UpgradeCtxGet()->funcs->SendShortMsg(UPGRADE_HOST_COMMIT_REQ);
                    UpgradeSMSetState(UPGRADE_STATE_COMMIT_CONFIRM);
                }
            }
            else
            {
                bool dfu = UpgradePartitionDataIsDfuUpdate();
                uint16 err = UpgradeSMNewImageStatus();

                if(err)
                {
                    /* TODO: Delete transient store = revert upgrade */
                    FatalError(err);
                }
                else
                {
                    /*UpgradeCtxGetPSKeys()->loader_msg = UPGRADE_LOADER_MSG_NONE;
                    UpgradeCtxGetPSKeys()->dfu_partition_num = 0;
                    UpgradeCtxGetPSKeys()->upgrade_in_progress_key = UPGRADE_RESUME_POINT_COMMIT;

                    UpgradeSavePSKeys();
                    PRINT(("P&R: UPGRADE_RESUME_POINT_COMMIT saved\n"));*/

                    if(dfu)
                    {
                        UpgradeSMMoveToState(UPGRADE_STATE_COMMIT_CONFIRM);
                    }
                    else
                    {
                        UpgradeCtxGet()->funcs->SendShortMsg(UPGRADE_HOST_COMMIT_REQ);
                        UpgradeSMSetState(UPGRADE_STATE_COMMIT_CONFIRM);
                    }
                }
            }
        }
        break;

    default:
        return FALSE;
    }

    return TRUE;
}

bool HandleCommitConfirm(MessageId id, Message message)
{
    switch(id)
    {
    case UPGRADE_HOST_COMMIT_CFM:
        {
            UPGRADE_HOST_COMMIT_CFM_T *cfm = (UPGRADE_HOST_COMMIT_CFM_T *)message;
            if(UPGRADE_PEER_IS_PRIMARY && UPGRADE_PEER_IS_STARTED)
            {
                UpgradePeerProcessHostMsg(UPGRADE_PEER_COMMIT_CFM,
                                            cfm->action);
            }
            switch (cfm->action)
            {
            case UPGRADE_HOSTACTION_YES:
                CommitConfirmYes();
                break;

            case UPGRADE_HOSTACTION_NO:
                UpgradeRevertUpgrades();
                /* By design, UPGRADE_HOSTACTION_NO should be followed by abort_req and,
                 * this flag ensures that device will be rebooted after handling the abort */
                UpgradeCtxGet()->isImageRevertNeededOnAbort = TRUE;
                /* Set the state to UPGRADE_STATE_SYNC to satisfy the unit tests */
                UpgradeSMSetState(UPGRADE_STATE_SYNC);
                DEBUG_LOG("HandleCommitConfirm isImageRevertNeededOnAbort set");
                break;
            /** default:
               @todo: Error case. Should we handle ? Who cancels timeout */
            }
        }
        break;

    case UPGRADE_INTERNAL_CONTINUE:
        CommitConfirmYes();
        break;

    default:
        /** @todo We don't handle unexpected messages in most cases, error messages
          are dealt with in the default handler.

           BUT how do we deal with timeout ?  It *should* be in the state
          machine */
        return FALSE;
    }

    return TRUE;
}

/*
NAME
    InformAppsCompleteGotoSync

DESCRIPTION
    Process the required actions from HandleCommit.

    Send messages to both the Host and VM applications to inform them that
    the upgrade is complete.

    Called either immediately or once we receive permission from
    the VM application.
*/
static void InformAppsCompleteGotoSync(void)
{
    /* tell host application we're complete */
    UpgradeCtxGet()->funcs->SendShortMsg(UPGRADE_HOST_COMPLETE_IND);
    /* TODO: Work on proper fix. Adding delay temporarily to allow above message
     * to reach to host before it gets disconnected.
     */
    /* tell VM application we're complete */
    UpgradeSendUpgradeStatusInd(UpgradeGetAppTask(), upgrade_state_done, 2000);
    /* go back to SYNC state to be ready to start again */
    UpgradeSMSetState(UPGRADE_STATE_SYNC);
}

/*
NAME
    HandleCommit

DESCRIPTION
    Handle event messages in the Commit state of the FSM.
*/
bool HandleCommit(MessageId id, Message message)
{
    UNUSED(message);
    switch(id)
    {
    case UPGRADE_INTERNAL_CONTINUE:
        {
            UpgradeCtxGetPSKeys()->upgrade_in_progress_key = UPGRADE_RESUME_POINT_ERASE;
            UpgradeSavePSKeys();
            PRINT(("P&R: UPGRADE_RESUME_POINT_ERASE saved\n"));

            /* We erase all partitions at this point.
             * We have taken the hit of disrupting audio etc. by doing a reboot
             * to get here.
             */
            UpgradeCtxGetPSKeys()->version = UpgradeCtxGetPSKeys()->version_in_progress;
            UpgradeCtxGetPSKeys()->config_version = UpgradeCtxGetPSKeys()->config_version_in_progress;

            /* only erase if we already have permission, get permission if we don't */
            if (UpgradeSMHavePermissionToProceed(UPGRADE_BLOCKING_IND))
            {
                /*
                 * Notify the Host of commit and upgrade completion only
                 * when peer is done with its commit and upgrade completion.
                 *
                 * Poll for peer upgrade completion at fixed intervals
                 * (less frequently) before notify the Host of its commit and
                 * upgrade completion.
                 */
                if (UPGRADE_PEER_IS_PRIMARY && !UPGRADE_PEER_IS_ENDED)
                {
                    MessageSendLater(UpgradeGetUpgradeTask(), UPGRADE_INTERNAL_ERASE,
                                     NULL, UPGRADE_PEER_POLL_INTERVAL);
                }
                else
                {
                    UpgradeSMErase();
                    InformAppsCompleteGotoSync();
                }
            }
        }
        break;

    case UPGRADE_HOST_ABORT_REQ:
        /* ignore abort from host when we are in the commit state */
        DEBUG_LOG("HandleCommit UPGRADE_HOST_ABORT_REQ recvd but ignored");
        break;

        /* VM application permission granted for erase */
    case UPGRADE_INTERNAL_ERASE:
        {
            /*
             * Notify the Host of commit and upgrade completion only
             * when peer is done with its commit and upgrade completion.
             *
             * Poll for peer upgrade completion at fixed intervals
             * (less frequently) before notify the Host of its commit and
             * upgrade completion.
             */
            if (UPGRADE_PEER_IS_PRIMARY && !UPGRADE_PEER_IS_ENDED)
            {
                MessageSendLater(UpgradeGetUpgradeTask(), UPGRADE_INTERNAL_ERASE,
                                 NULL, UPGRADE_PEER_POLL_INTERVAL);
            }
            else
            {
                UpgradeSMErase();
                InformAppsCompleteGotoSync();
            }
        }
        break;

    default:
        return FALSE;
    }

    return TRUE;
}

bool HandlePsJournal(MessageId id, Message message)
{
    UNUSED(id);
    UNUSED(message);
    return FALSE;
}

/*
NAME
    PsFloodAndReboot

DESCRIPTION
    Process the required actions from HandleRebootToResume.

    Flood PS to force a defrag on next boot, then do a warm reboot.

    Called either immediately or once we receive permission from
    the VM application.
*/
static void PsFloodAndReboot(void)
{
    PsFlood();
    BootSetMode(BootGetMode());
}

/*
NAME
    HandleRebootToResume - In UPGRADE_STATE_REBOOT_TO_RESUME

DESCRIPTION
    Unable to continue with an upgrade as we cannot guarantee the required
    PSKEY operations.

    Once the error that got us to this state is acknowledged we will
    reboot if the VM application permits, otherwise remaining in this state
    handling all messages that might otherwise cause an activity.
*/
bool HandleRebootToResume(MessageId id, Message message)
{
    UPGRADE_HOST_ERRORWARN_RES_T *errorwarn_res = (UPGRADE_HOST_ERRORWARN_RES_T *)message;

    switch(id)
    {
    case UPGRADE_HOST_ERRORWARN_RES:
        if (errorwarn_res->errorCode == UPGRADE_HOST_ERROR_PARTITION_CLOSE_FAILED_PS_SPACE)
        {
            if (UpgradeSMHavePermissionToProceed(UPGRADE_APPLY_IND))
            {
                PsFloodAndReboot();
            }
        }
        else
        {
            /* Message not handled */
            return FALSE;
        }
        break;
    
        /* got permission from the application, go ahead with reboot */
    case UPGRADE_INTERNAL_REBOOT:
        {
            PsFloodAndReboot();
        }
        break;

    case UPGRADE_HOST_SYNC_REQ:
    case UPGRADE_HOST_START_REQ:
    case UPGRADE_HOST_ABORT_REQ:
        /*! @todo: Should we be sending the respective CFM messages back in this case ?
         *
         * In most cases there is no error code.
         */
        DEBUG_LOG("HandleRebootToResume, cmd_id:%d recvd and notified", id);
        UpgradeCtxGet()->funcs->SendErrorInd(UPGRADE_HOST_ERROR_PARTITION_CLOSE_FAILED_PS_SPACE);
        break;

    default:
        return FALSE;
    }

    return TRUE;
}


static void upgradeSmDefaultHandlerHandleUpgradeHostSyncReq(const UPGRADE_HOST_SYNC_REQ_T *sync_req)
{
    UpgradeCtx *ctx = UpgradeCtxGet();
    UPGRADE_LIB_PSKEY *upg_pskeys = UpgradeCtxGetPSKeys();

    DEBUG_LOG_INFO("upgradeSmDefaultHandlerHandleUpgradeHostSyncReq, in_progress_id 0x%lx", sync_req->inProgressId);

    /* reset on ever sync */
    ctx->force_erase = FALSE;

    /* refuse to sync if upgrade is not permitted */
    if (ctx->perms == upgrade_perm_no)
    {
        DEBUG_LOG_INFO("upgradeSmDefaultHandlerHandleUpgradeHostSyncReq, not permitted");
        ctx->funcs->SendErrorInd(UPGRADE_HOST_ERROR_APP_NOT_READY);
    }

    /* Check upgrade ID */
    else if(sync_req->inProgressId == 0)
    {
        DEBUG_LOG_INFO("upgradeSmDefaultHandlerHandleUpgradeHostSyncReq, invalid sync id");
        ctx->funcs->SendErrorInd(UPGRADE_HOST_ERROR_INVALID_SYNC_ID);
    }
    else if(   upg_pskeys->id_in_progress == 0
            || upg_pskeys->id_in_progress == sync_req->inProgressId)
    {
        DEBUG_LOG_INFO("upgradeSmDefaultHandlerHandleUpgradeHostSyncReq, allowed, id_in_progress 0x%lx", upg_pskeys->id_in_progress);

        ctx->funcs->SendSyncCfm(upg_pskeys->upgrade_in_progress_key, sync_req->inProgressId);

        upg_pskeys->id_in_progress = sync_req->inProgressId;
        /*!
            @todo Need to minimise the number of times that we write to the PS
                  so this may not be the optimal place. It will do for now.
        */
        UpgradeSavePSKeys();

        UpgradeSMSetState(UPGRADE_STATE_READY);

        if(UPGRADE_PEER_IS_SUPPORTED)
        {
            /* Store MD5 for Peer Upgrade */
            UpgradePeerStoreMd5(upg_pskeys->id_in_progress);
        }
    }
    else
    {
        DEBUG_LOG_INFO("upgradeSmDefaultHandlerHandleUpgradeHostSyncReq, expecting 0x%lx", upg_pskeys->id_in_progress);

        /* Send a warning to a host, which then can force upgrade with this
           file by sending ABORT_REQ and SYNC_REQ again.
         */
        ctx->funcs->SendErrorInd(UPGRADE_HOST_WARN_SYNC_ID_IS_DIFFERENT);
    }
}

/* Send a message to Device upgrade to be sent to application for calling 
 * DFU specific routine for cleanup process
 */
static void UpgradeCleanupOnAbort(void)
{
    DEBUG_LOG("UpgradeCleanupOnAbort()");
    MessageSend(UpgradeCtxGet()->mainTask, UPGRADE_CLEANUP_ON_ABORT, NULL);
}

/*
NAME
    DefaultHandler - Deal with messages which we want to handle in all states

DESCRIPTION
    Default processing of messages which may be handled at any time.

    These are NOT normally processed if they have been dealt with already in
    the state machine, but this can be done.
 */
bool DefaultHandler(MessageId id, Message message, bool handled)
{
    if (!handled)
    {
        switch(id)
        {
        case UPGRADE_HOST_SYNC_REQ:
            upgradeSmDefaultHandlerHandleUpgradeHostSyncReq((const UPGRADE_HOST_SYNC_REQ_T*)message);
            break;

        case UPGRADE_HOST_ABORT_REQ:
            PRINT(("UPGRADE_HOST_ABORT_REQ handled\n"));
            DEBUG_LOG("DefaultHandler UPGRADE_HOST_ABORT_REQ recvd");
                        
#ifdef HANDOVER_DFU_ABORT_WITHOUT_ERASE
            /* Check if Handover triggered DFU abort is completed */
            if (UpgradeGetHandoverDFUAbortState() &&
                    !UpgradeIsHandoverDFUAbortComplete())
            {
                DEBUG_LOG("DefaultHandler Handover DFU abort: ABORT_REQ skipped");
                UpgradeSetHandoverDFUAbortState(handover_upgrade_abort_abort_req_recvd);
                /*
                 * Reset the the upgrade library state as erase is bypassed for
                 * Handover triggered DFU abort.
                 */
                UpgradeSMSetState(UPGRADE_STATE_SYNC);
                /* Send CFM as ABORT_REQ handling is skipped i.e. erase bypassed. */
                UpgradeCtxGet()->funcs->SendShortMsg(UPGRADE_HOST_ABORT_CFM);
                UpgradeSetHandoverDFUAbortState(handover_upgrade_abort_abort_cfm_sent);
                return TRUE; /* Mark as handled */
            }
#endif
            /*
             * Host initiated abort (both for Primary and Secondary), shall be
             * handled here as upgrade can be any of the upgrade states.
             * Whereas for Primary device initiated abort owing to internal
             * errors reported via FatalError(), shall be handled here owing to
             * following reason:
             * - Primary has transitioned to UPGRADE_STATE_ABORTING and sending
             *   UPGRADE_HOST_ERRORWARN_IND to the Host.
             * - Primary in response receives UPGRADE_HOST_ERRORWARN_RES from
             *   the Host. Primary can further relay this as
             *   UPGRADE_PEER_ABORT_REQ/UPGRADE_HOST_ABORT_REQ based on Peer
             *   (Secondary) upgrade has started. And Primary then tranistions
             *   to UPGRADE_STATE_SYNC.
             * - Subsequently UPGRADE_HOST_ABORT_REQ can be received from the
             *   Host.
             * - Hence UPGRADE_HOST_ABORT_REQ be handled here for the
             *   Primary as Primary has transitioned out of
             *   UPGRADE_STATE_ABORTING state.
             *
             * Its fine to early notify application without awaiting the
             * internal cleanup that includes reseting of upgrade library pskey
             * info and erase of alternate DFU bank, so that the application
             * can simultaneously do the required cleanup, if any.
             */
            UpgradeSendEndUpgradeDataInd(upgrade_end_state_abort);
            asynchronous_abort = UpgradeSMAbort();
            PRINT(("UPGRADE_HOST_ABORT_REQ: UpgradeSMAbort() returned %d\n", asynchronous_abort));
            DEBUG_LOG("DefaultHandler UPGRADE_HOST_ABORT_REQ recvd, UpgradeSMAbort() returned %d\n", asynchronous_abort);
            if (!asynchronous_abort)
            {
                PRINT(("UPGRADE_HOST_ABORT_REQ: sending UPGRADE_HOST_ABORT_CFM\n"));
                UpgradeCtxGet()->funcs->SendShortMsg(UPGRADE_HOST_ABORT_CFM);
            }
            if(UpgradeCtxGet()->isImageRevertNeededOnAbort)
            {
                DEBUG_LOG("DefaultHandler UPGRADE_HOST_ABORT_REQ device to reboot in UPGRADE_WAIT_FOR_REBOOT time");
                MessageSendLater(UpgradeGetUpgradeTask(), UPGRADE_INTERNAL_DELAY_REVERT_REBOOT, NULL,
                                                           UPGRADE_WAIT_FOR_REBOOT);
            }
            break;

        case UPGRADE_INTERNAL_DELAY_REVERT_REBOOT:
			/* Pass special BootMode value to BootSetMode so that we can detect and clean the state on boot bank after reboot */ 
            DEBUG_LOG_INFO("DefaultHandler UPGRADE_INTERNAL_DELAY_REVERT_REBOOT device rebooting with BootMode 0x%x", UPGRADE_REVERT_COMMIT);
            BootSetMode(UPGRADE_REVERT_COMMIT);
            break;

        case UPGRADE_HOST_VERSION_REQ:
            PRINT(("UPGRADE_HOST_VERSION_REQ\n"));
            if (UpgradeCtxGet()->funcs->SendVersionCfm != NULL)
            {
                /* reply to UPGRADE_VERSION_REQ with UPGRADE_VERSION_CFM
                 * sending our current upgrade and PS config version numbers */
                UpgradeCtxGet()->funcs->SendVersionCfm(UpgradeCtxGetPSKeys()->version.major,
                                                UpgradeCtxGetPSKeys()->version.minor,
                                                UpgradeCtxGetPSKeys()->config_version);
            }
            break;

        case UPGRADE_HOST_VARIANT_REQ:
            PRINT(("UPGRADE_HOST_VARIANT_REQ\n"));
            if (UpgradeCtxGet()->funcs->SendVariantCfm != NULL)
            {
                UpgradeCtxGet()->funcs->SendVariantCfm(UpgradeFWIFGetDeviceVariant());
            }
            break;

        case UPGRADE_INTERNAL_BATTERY_LOW:
            PRINT(("UPGRADE_INTERNAL_BATTERY_LOW\n"));
            UpgradeSMSetState(UPGRADE_STATE_BATTERY_LOW);
            break;

        /* got required permission from VM app, erase and return to SYNC state */
        case UPGRADE_INTERNAL_ERASE:
            {
                UpgradeSMErase();
                UpgradeSMSetState(UPGRADE_STATE_SYNC);
            }
            break;

       case UPGRADE_HOST_ERRORWARN_RES:
            {
#ifdef HANDOVER_DFU_ABORT_WITHOUT_ERASE
                UPGRADE_HOST_ERRORWARN_RES_T * err_res;
                err_res = (UPGRADE_HOST_ERRORWARN_RES_T *) message;
                if (err_res->errorCode == UPGRADE_HOST_ERROR_HANDOVER_DFU_ABORT)
                {
                    /*
                     * Save the transient state data in upgrade pskey (i.e. both
                     * local & peer), required for seemless DFU especially after
                     * a Handover completion.
                     */
                    UpgradeSavePSKeys();
                    UpgradePeerSavePSKeys();

                    /*
                     * Its not required to override permissions and call
                     * UpgradeSMAbort() in order to disallow local erase as well
                     * as pskey info clear to psstore (i.e. both local & peer)
                     * on Secondary, for the Primary initiated abort owing to a
                     * Handover trigger.
                     *
                     * psstore info needs to be retained to support seamless
                     * DFU especially after a Handover completion.
                     */
#ifdef MESSAGE_IMAGE_UPGRADE_COPY_STATUS
                    /*
                     * There may be non-blocking traps such as ImageUpgradeCopy
                     * in progress. Call the ImageUpgradeAbortCommand() trap to
                     * abort any of those. It will do no harm if there are no
                     * non-blocking traps in progress.
                     */
                    DEBUG_LOG("DefaultHandler ImageUpgradeAbortCommand ret:%d", ImageUpgradeAbortCommand());
#endif  /* MESSAGE_IMAGE_UPGRADE_COPY_STATUS */
                    /*
                     * Reset the the upgrade library state as device cleanup
                     * that includes reseting of upgrade library pskey info and
                     * erase of alternate DFU bank is bypassed for Handover
                     * triggered DFU abort.
                     */
                    UpgradeSMSetState(UPGRADE_STATE_SYNC);

                    /*
                     * ToDo: Check if its fine to error notify the application
                     * to reset its state variables on a Handover.
                     */
                    UpgradeSendEndUpgradeDataInd(upgrade_end_state_abort);
                    break;
                }
#endif
                if(UpgradePeerIsSecondary())
                {
                    DEBUG_LOG("DefaultHandler UPGRADE_HOST_ERRORWARN_RES received from Peer");
                    UpgradeSMAbort();
                    UpgradeCleanupOnAbort();
                }
            }
            break;

       case UPGRADE_INTERNAL_START_PEER_CONCURRENT:
            {
                if(UPGRADE_PEER_IS_PRIMARY)
                {
                    if(!UPGRADE_PEER_IS_STARTED)
                    {
                        DEBUG_LOG("DefaultHandler: UPGRADE_INTERNAL_START_PEER_CONCURRENT : Setting up peer upgrade");
                        UpgradePeerStartDfu(UPGRADE_IMAGE_COPY_CHECK_IGNORE);
                        /* Since concurrent peer dfu about to start, set the status here
                         * This will be used to decide whether to delay the hash checking
                         * after primary dfu completion or go ahead with hash checking.
                         */
                        UpgradeCtxGet()->upgradePeerDataTransferStatus =
                                    UPGRADE_PEER_DATA_TRANSFER_IN_PROGRESS;
                    }
                } 
            }
            break;

        default:
            return FALSE;
        }
        handled = TRUE;
    }

    return handled;
}

void UpgradeSMSetState(UpgradeState nextState)
{
    UpgradeCtxGet()->smState = nextState;
}

UpgradeState GetState(void)
{
    return UpgradeCtxGet()->smState;
}

void UpgradeSMMoveToState(UpgradeState nextState)
{
    UpgradeSMSetState(nextState);
    MessageSend(UpgradeGetUpgradeTask(), UPGRADE_INTERNAL_CONTINUE, NULL);
}

void FatalError(UpgradeHostErrorCode ec)
{
    DEBUG_LOG("FatalError, error_code:%d", ec);
    UpgradeCtxGet()->funcs->SendErrorInd((uint16)ec);
    UpgradeSMSetState(UPGRADE_STATE_ABORTING);

#ifdef HANDOVER_DFU_ABORT_WITHOUT_ERASE
    if(ec == UPGRADE_HOST_ERROR_HANDOVER_DFU_ABORT)
    {
        /*
         * Retain the upgrade info stored in psstore to support seamless DFU
         * once a Handover is completed. So just error report to avoid
         * preparing for a fresh subsequent DFU rather provide for resumption
         * of the current DFU itself on Handover completion.
         */
        DEBUG_LOG("FatalError, Only error report on Handover DFU abort.");
    }
    else
#endif
    {
        UpgradeCtxGetPSKeys()->upgrade_in_progress_key = UPGRADE_RESUME_POINT_ERROR;
        UpgradeSavePSKeys();
    }
    /*
     * Defer notification to the application until the device initiated abort
     * either owing to Handover or owing to internal errors are responded/
     * acknowledged by the Host application with UPGRADE_HOST_ERRORWARN_RES and
     * UPGRADE_HOST_ABORT_REQ.
     */
}

void PsSpaceError(void)
{
    UpgradeCtxGet()->funcs->SendErrorInd((uint16)UPGRADE_HOST_ERROR_PARTITION_CLOSE_FAILED_PS_SPACE);
    UpgradeSMSetState(UPGRADE_STATE_REBOOT_TO_RESUME);
}

/*
NAME
    UpgradeSMErase - Clean up after an upgrade, even if it was aborted.

DESCRIPTION
    UpgradeSMErase any partitions that will be needed for any future upgrade.
    Clear the transient data in the upgrade PS key data.

    Note: Before calling this function make sure it is ok to erase
          partitions on the external flash because it will block other
          services during the erase.
*/
void UpgradeSMErase(void)
{
    UpgradePartitionsState old_ipk;

    DEBUG_LOG("UpgradeSMErase(): begin");
    /* If a partition is currently open, close it now.
       Otherwise the f/w will think it is in use and not erase it. */
    UpgradePartitionDataCtx *ctx = UpgradeCtxGetPartitionData();
    if (ctx)
    {
        if (ctx->partitionHdl)
        {
             UpgradeFWIFPartitionClose(ctx->partitionHdl);
        }
    }

    /* Free the partition related contex memory */
    UpgradePartitionDataDestroy();

    /*
     * Store upgrade_in_progress_key since pskey is stored prior to assessment
     * whether the alternate bank is required to be erased.
     */
    old_ipk = UpgradeCtxGetPSKeys()->upgrade_in_progress_key;

    /* Reset transient state data in upgrade pskey (i.e. both local & peer). */
    UpgradePartitionsUpgradeStarted();
    /*
     * Note: UpgradePartitionsUpgradeStarted() needs to preceed
     *       UpgradeCtxClearPSKeys() to be eventually stored via psstore.
     */
    UpgradeCtxClearPSKeys();
    UpgradePeerClearPSKeys();

    /* ClearHeaderPSKeys */
    UpgradeClearHeaderPSKeys();

    PRINT(("P&R: UPGRADE_RESUME_POINT_START saved\n"));

    /* UpgradeSMErase any partitions that require it. */
    /*
     * Restore upgrade_in_progress_key as alternate bank is now actually
     * assessed for erase.
     */
    UpgradeCtxGetPSKeys()->upgrade_in_progress_key = old_ipk;
    UpgradePartitionsEraseAllManaged();
    /* Synchronize to psstore saved state (above). */
    UpgradeCtxGetPSKeys()->upgrade_in_progress_key = UPGRADE_RESUME_POINT_START;


    /* Let application know that erase is done */
    if (UpgradeSMCheckEraseComplete())
    {
        UpgradeSMBlockingOpIsDone();
    }
    DEBUG_LOG("UpgradeSMErase(): end");
}

void CommitConfirmYes(void)
{
    UpgradeCommitUpgrades();
    
    /* tell VM application we're committing the upgrade */
    UpgradeSendUpgradeStatusInd(UpgradeGetAppTask(), upgrade_state_commiting, 0);

    UpgradeSMMoveToState(UPGRADE_STATE_COMMIT);
}

/***************************************************************************
NAME
    UpgradeSMUpgradeInProgress

DESCRIPTION
    Return boolean indicating if an upgrade is currently in progress.
*/
bool UpgradeSMUpgradeInProgress(void)
{
    return (GetState() >= UPGRADE_STATE_READY);
}

/***************************************************************************
NAME
    UpgradeSMHavePermissionToProceed

DESCRIPTION
    Decide if we should perform an action now, not at all, or ask the
    application for permission.
    
*/
bool UpgradeSMHavePermissionToProceed(MessageId id)
{
    DEBUG_LOG("UpgradeSMHavePermissionToProceed perms:%d id:%d", UpgradeCtxGet()->perms, id);
    switch (UpgradeCtxGet()->perms)
    {
        /* we currently have no permission to upgrade, so no permission
         * to do anything */
        case upgrade_perm_no:
            {
                /* @todo if we're here, for reboot or erase, but have no
                 * permission to do so, should we clean up/abort?
                 */
                return FALSE;
            }

            /* we have permission to do anything without question */
        case upgrade_perm_assume_yes:
            return TRUE;

            /* we have permission, but must ask the application, send
             * a message AND return FALSE so the caller doesn't do anything
             * yet, but waits until the application responds. */
        case upgrade_perm_always_ask:
            MessageSend(UpgradeCtxGet()->mainTask, id, NULL);
            return FALSE;
    }

    return FALSE;
}

/***************************************************************************
NAME
    UpgradeSMBlockingOpIsDone

DESCRIPTION
    Let know application that blocking operation is finished.
*/
static void UpgradeSMBlockingOpIsDone(void)
{
    if(UpgradeCtxGet()->perms == upgrade_perm_always_ask)
    {
        /* Cancelling messages shouldn't be needed but do it anyway */
        MessageCancelAll(UpgradeCtxGet()->mainTask, UPGRADE_BLOCKING_IND);
        MessageSend(UpgradeCtxGet()->mainTask, UPGRADE_BLOCKING_IS_DONE_IND, NULL);
    }
}

/***************************************************************************
NAME
    UpgradeSendUpgradeStatusInd

DESCRIPTION
    Build and send an UPGRADE_STATUS_IND message to the VM application.
*/
static void UpgradeSendUpgradeStatusInd(Task task, upgrade_state_t state, uint32 delay)
{
    UPGRADE_STATUS_IND_T *upgradeStatusInd = (UPGRADE_STATUS_IND_T *)PanicUnlessMalloc(sizeof(UPGRADE_STATUS_IND_T));
    upgradeStatusInd->state = state;

    if(delay == 0)
        MessageSend(task, UPGRADE_STATUS_IND, upgradeStatusInd);
    else
        MessageSendLater(task, UPGRADE_STATUS_IND, upgradeStatusInd, delay);
}

/***************************************************************************
NAME
    UpgradeSMEraseStatus

DESCRIPTION
    Notification that the erase has finished (MESSAGE_IMAGE_UPGRADE_ERASE_STATUS).
    Only occurs in CONFIG_HYDRACORE.
*/

#ifndef MESSAGE_IMAGE_UPGRADE_ERASE_STATUS
/*
 * In the host tests it is picking up the BlueCore version of systme_message.h
 * rather the CONFIG_HYDRACORE version, so define the structure here.
 */
typedef struct
{
    bool erase_status; /*!< TRUE if image erase is successful, else FALSE */
} MessageImageUpgradeEraseStatus;
#endif

void UpgradeSMEraseStatus(Message message)
{
    MessageImageUpgradeEraseStatus *msg = (MessageImageUpgradeEraseStatus *)message;
    UpdateResumePoint ResumePoint = UpgradeCtxGetPSKeys()->upgrade_in_progress_key;
    UpgradeState CurrentState = GetState();

    DEBUG_LOG("UpgradeSMEraseStatus, erase_status %u", msg->erase_status);

    /* Let application know that erase is done */
    UpgradeSMBlockingOpIsDone();

    if (ResumePoint == UPGRADE_RESUME_POINT_START)
    {
        DEBUG_LOG("UpgradeSMEraseStatus, UPGRADE_RESUME_POINT_START, state %u", CurrentState);
        if (UPGRADE_STATE_DATA_READY == CurrentState)
        {
            /*
             * This is the instance where the response to the
             * UPGRADE_HOST_START_DATA_REQ in HandleDataReady has been postponed
             * until the non-blocking SQIF erase has been completed.
             */
            if (msg->erase_status)
            {
                DEBUG_LOG_INFO("Upgrade, SQIF erased");
                /*
                 * Reset to dispatch the conditionally queued notification of
                 * UPGRADE_START_DATA_IND on sucessful erase completion.
                 */
                UpgradeCtxGet()->isImgUpgradeEraseDone = 0;
                /*
                 * The SQIF has been erased successfully.
                 * The host is waiting to be told that it can proceed with the
                 * date transfer, postponed from the receipt of the
                 * UPGRADE_HOST_START_DATA_REQ in HandleDataReady.
                 */
                uint16 req_size = UpgradePartitionDataGetNextReqSize();
                DEBUG_LOG_INFO("Upgrade, requesting %u bytes", req_size);
                UpgradeCtxGet()->funcs->SendBytesReq(req_size, 0);
                UpgradeSMSetState(UPGRADE_STATE_DATA_TRANSFER);
            }
            else
            {
                /*
                 * Cancel the conditionally queued notification of
                 * UPGRADE_START_DATA_IND on erase failure since DFU aborts.
                 */
                MessageCancelAll(UpgradeCtxGet()->mainTask, UPGRADE_START_DATA_IND);

                /* Tell the host that the attempt to erase the SQIF failed. */
                FatalError(UPGRADE_HOST_ERROR_SQIF_ERASE);
            }
        }
        else if (UPGRADE_STATE_SYNC != CurrentState)
        {
            /* 
             * The standard erase after successful update occurs in 
             * UPGRADE_RESUME_POINT_START for UPGRADE_STATE_SYNC. If it was that
             * then it is expected and nothing needs be sent to the host. But if
             * it was not that, what was it?
             */
            DEBUG_LOG("UpgradeSMEraseStatus, unexpected state %u", CurrentState);
        }
        else
        {
            if (asynchronous_abort)
            {
                DEBUG_LOG("UpgradeSMEraseStatus, sending UPGRADE_HOST_ABORT_CFM");
                UpgradeCtxGet()->funcs->SendShortMsg(UPGRADE_HOST_ABORT_CFM);
                asynchronous_abort = FALSE;
            }
        }
    }
    else
    {
        DEBUG_LOG("UpgradeSMEraseStatus, unexpected resume point %u", ResumePoint);
    }
}

#ifndef MESSAGE_IMAGE_UPGRADE_COPY_STATUS
/*
 * In the host tests it is picking up the BlueCore version of systme_message.h
 * rather the CONFIG_HYDRACORE version, so define the structure here.
 */
typedef struct
{
    bool copy_status; /*!< TRUE if image copy is successful, else FALSE */
} MessageImageUpgradeCopyStatus;
#endif

void UpgradeSMCopyStatus(Message message)
{
    MessageImageUpgradeCopyStatus *msg = (MessageImageUpgradeCopyStatus *)message;
    DEBUG_LOG("UpgradeSMCopyStatus, copy_status %u", msg->copy_status);
    /* Let application know that copy is done */
    UpgradeSMBlockingOpIsDone();

    UpgradeCtxGet()->isImgUpgradeCopyDone = TRUE;

    if (msg->copy_status)
    {
        UpgradeCtxGet()->ImgUpgradeCopyStatus = TRUE;

        /* Set the imageUpgradeCopyProgress flag since the image upgarde copy
         * is completed and successful
         */
        UpgradeCtxSetImageCopyStatus(IMAGE_UPGRADE_COPY_COMPLETED);
        /*
         * The SQIF has been copied successfully.
         */
        UpgradeSMHandleValidated(UPGRADE_VM_IMAGE_UPGRADE_COPY_SUCCESSFUL, NULL);
    }
    else
    {
        /* Cancel the Peer DFU request */
        if(UPGRADE_PEER_IS_PRIMARY && UPGRADE_PEER_IS_STARTED)
            UpgradePeerCancelDFU();
        /* Tell the host that the attempt to copy the SQIF failed. */
        FatalError(UPGRADE_HOST_ERROR_SQIF_COPY);
        UpgradeSMHandleValidated(UPGRADE_VM_IMAGE_UPGRADE_COPY_FAILED, NULL);
    }
}

#ifdef MESSAGE_IMAGE_UPGRADE_AUDIO_STATUS
void UpgradeSMCopyAudioStatus(Message message)
{
    MessageImageUpgradeAudioStatus *msg = (MessageImageUpgradeAudioStatus *)message;
    DEBUG_LOG("UpgradeSMCopyAudioStatus, audio_status %u", msg->audio_status);
    /* Let application know that copy is done */
    UpgradeSMBlockingOpIsDone();

    if (msg->audio_status)
    {
        /*
         * The SQIF has been copied successfully.
         */
        UpgradeSMHandleValidated(UPGRADE_VM_DFU_COPY_VALIDATION_SUCCESS, NULL);
    }
    else
    {
        /* Tell the host that the attempt to copy the SQIF failed. */
        FatalError(UPGRADE_HOST_ERROR_AUDIO_SQIF_COPY);
        UpgradeSMHandleValidated(UPGRADE_VM_AUDIO_DFU_FAILURE, NULL);
    }
}
#endif

#ifdef MESSAGE_IMAGE_UPGRADE_HASH_ALL_SECTIONS_UPDATE_STATUS
void UpgradeSMHashAllSectionsUpdateStatus(Message message)
{
    MessageImageUpgradeHashAllSectionsUpdateStatus *msg = (MessageImageUpgradeHashAllSectionsUpdateStatus*)message;
    DEBUG_LOG("UpgradeSMHashAllSectionsUpdateStatus, status %u", msg->status);
    
    if(msg->status)
    {
        (void)UpgradeSMHandleMsg(UPGRADE_VM_HASH_ALL_SECTIONS_SUCCESSFUL, message);
    }
    else
    {
        (void)UpgradeSMHandleMsg(UPGRADE_VM_HASH_ALL_SECTIONS_FAILED, message);
    }
}
#endif

void UpgradeErrorMsgFromUpgradePeer(uint16 error)
{
    DEBUG_LOG("UpgradeErrorMsgFromUpgradePeer: Peer (Secondary) initiated Abort");
    FatalError((UpgradeHostErrorCode)error);
}

void UpgradeCommitMsgFromUpgradePeer(void)
{
    UpgradeFWIFApplicationValidationStatus status;

    status = UpgradeFWIFValidateApplication();
    if (UPGRADE_FW_IF_APPLICATION_VALIDATION_SKIP != status)
    {                
        if (UPGRADE_FW_IF_APPLICATION_VALIDATION_RUNNING == status)
        {
            UpgradeSMMoveToState(UPGRADE_STATE_COMMIT_VERIFICATION);
        }
        else
        {
            UpgradeCtxGet()->funcs->SendShortMsg(UPGRADE_HOST_COMMIT_REQ);
            UpgradeSMSetState(UPGRADE_STATE_COMMIT_CONFIRM);
        }
    }
    else
    {
        bool dfu = UpgradePartitionDataIsDfuUpdate();
        uint16 err = UpgradeSMNewImageStatus();

        if(err)
        {
            /* TODO: Delete transient store = revert upgrade */
            FatalError(err);
        }
        else
        {
            /*UpgradeCtxGetPSKeys()->loader_msg = UPGRADE_LOADER_MSG_NONE;
            UpgradeCtxGetPSKeys()->dfu_partition_num = 0;
            UpgradeCtxGetPSKeys()->upgrade_in_progress_key = UPGRADE_RESUME_POINT_COMMIT;

            UpgradeSavePSKeys();
            PRINT(("P&R: UPGRADE_RESUME_POINT_COMMIT saved\n"));*/

            if(dfu)
            {
                UpgradeSMMoveToState(UPGRADE_STATE_COMMIT_CONFIRM);
            }
            else
            {
                UpgradeCtxGet()->funcs->SendShortMsg(UPGRADE_HOST_COMMIT_REQ);
                UpgradeSMSetState(UPGRADE_STATE_COMMIT_CONFIRM);
            }
        }
    }
}

/****************************************************************************
NAME
    UpgradeSMHostRspSwap

DESCRIPTION
     The default behaviour of Upgrade libarary is to send Host command response
     using UpgradeHostIFData*** APIs. But when device is operating as secondry
     device i.e. connection is over L2CAp for dfu download then upgrade response
     is sent via UpgradePeerIfData** APIs.

RETURNS
    None
*/
void UpgradeSMHostRspSwap(bool is_primary)
{
    if(!is_primary)
    {
        UpgradeCtxGet()->funcs = &UpgradePeer_fptr;
    }
    else
    {
        UpgradeCtxGet()->funcs = &Upgrade_fptr;
    }
}

