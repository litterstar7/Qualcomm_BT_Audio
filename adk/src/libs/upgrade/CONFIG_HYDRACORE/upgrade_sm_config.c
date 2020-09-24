/****************************************************************************
Copyright (c) 2014 - 2015 Qualcomm Technologies International, Ltd.


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
#include <imageupgrade.h>

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

static uint8 is_validated = 0;

static void upgradeSmConfig_ResetValidation(void)
{
    PRINT(("upgradeSmConfig_ResetValidation()\n"));
    is_validated = 0;
}

/*
NAME
    IsValidatedToTrySwap - Ensures all data are validated before trying to swap image.

DESCRIPTION
    Uses an incremental flag to ensure that all parts of a DFU image have been
    copied and validated before trying to call the ImageUpgradeSwapTry() trap.
*/
static void IsValidatedToTrySwap(bool reset)
{
    if(reset)
    {
        upgradeSmConfig_ResetValidation();
        return;
    }
	
    DEBUG_LOG_INFO("IsValidatedToTrySwap, is_validated %d", is_validated);

    switch(is_validated)
    {
    /* Last part of the DFU image has been copied and validated */
    case 0:
        DEBUG_LOG_INFO("IsValidatedToTrySwap, all DFU images have been validated");
        is_validated++;
        break;

    /* All part have been copied and validated */
    case 1:
        {
            DEBUG_LOG_INFO("IsValidatedToTrySwap(): Shutdown audio before calling ImageUpgradeSwapTry()");

            /*
             * The audio needs to be shut down before calling the ImageUpgradeSwapTry trap.
             * This is applicable to audio SQIF or ROM, to avoid deadlocks in Apps P0, causing P0 to not stream audio data or process image swap request.
             */
            UpgradeApplyAudioShutDown();
        }
        break;

    default:
        return;
    }
}


/* This is the last state before reboot */
bool UpgradeSMHandleValidated(MessageId id, Message message)
{
    UpgradeCtx *ctx = UpgradeCtxGet();

    DEBUG_LOG_INFO("UpgradeSMHandleValidated, id %u, message %p", id, message);

    switch(id)
    {
        
    case UPGRADE_INTERNAL_CONTINUE:

         /* Check if UPGRADE_HOST_IS_CSR_VALID_DONE_REQ message is received */
        if(ctx->isCsrValidDoneReqReceived)
        {
            /* If Peer DFU is supported, then start the DFU of Peer Device */
            if(UPGRADE_PEER_IS_PRIMARY)
            {
                /*Concurrent DFU case this would be already started. This part is needed for serialized DFU cases*/
                if(!UPGRADE_PEER_IS_STARTED)
                {
                    /*
                     * Setup Peer connection for DFU once image upgarde copy is
                     * completed successfully.
                     */
                    if(UpgradeCtxGet()->isImgUpgradeCopyDone)
                    {
                        if(UpgradeCtxGet()->ImgUpgradeCopyStatus)
                            UpgradeCtxSetImageCopyStatus(IMAGE_UPGRADE_COPY_COMPLETED);
                        else
                            break; /* No need to setup peer connection */
                    }
                    else
                    {
                        UpgradeCtxSetImageCopyStatus(IMAGE_UPGRADE_COPY_IN_PROGRESS);
                    }

                    if(UpgradePeerStartDfu(UPGRADE_IMAGE_COPY_CHECK_REQUIRED)== FALSE)
                    {
                        /* TODO: An error has occured, fail the DFU */
                    }
                }
            }
#ifndef HOSTED_TEST_ENVIRONMENT
            /* Send UPGRADE_HOST_TRANSFER_COMPLETE_IND once standalone upgrade
             * is done.
             */
            else
            {
                /* this check is for: when link loss between peers 
                 * then do not send UPGRADE_HOST_TRANSFER_COMPLETE_IND, wait until
                 * connection is back
                 */
                if(!UpgradePeerIsBlocked())
                {
                    /* Check for image upgrade copy status. If the status is in
                     * progress,then send conditional message to send
                     * UPGRADE_HOST_TRANSFER_COMPLETE_IND to Host once the image
                     * upgrade copy is completed.
                     */
                    if(UpgradeCtxGet()->isImgUpgradeCopyDone)
                    {
                        UpgradeCtxGet()->funcs->SendShortMsg(UPGRADE_HOST_TRANSFER_COMPLETE_IND);
                    }
                    else
                    {
                        MessageSendConditionally(UpgradeGetUpgradeTask(), UPGRADE_INTERNAL_CONTINUE, NULL,
                                                          UpgradeCtxGetImageCopyStatus());
                    }
                }
                else
                {
                    /* it's a link loss between peers, wait for peer connection to come back */
                    DEBUG_LOG_INFO("UpgradeSMHandleValidated, waiting for peers connection to come back");
                    MessageSendConditionally(UpgradeGetUpgradeTask(), UPGRADE_INTERNAL_CONTINUE, NULL,
                                                    UpgradePeerGetPeersConnectionStatus());
                }
            }
#endif
        }
        else
        {
#ifndef HOSTED_TEST_ENVIRONMENT
            /* We come to this scenario when primary device is reset during primary
             * to secondary file transfer and role switch happens. The new primary
             * will connect to new secondary after the dfu file is transferred to 
             * new primary from Host. After the connection, the new primary does not
             * know whether the new secondary got all the dfu data or not. So, the
             * new secondary on reaching to UpgradeSMHandleValidated through resume
             * point on boot-up will wait for the image copy to get completed first,
             * as this get initiated in HandleValidating(), and then send the
             * UPGRADE_HOST_TRANSFER_COMPLETE_IND to new primary device, so that the
             * new primary knows that file transfer is completed in new secondary,
             * and proceed ahead with reboot and commit phase of the DFU.
             */
            DEBUG_LOG("UpgradeSMHandleValidated: Send UPGRADE_HOST_TRANSFER_COMPLETE_IND");
            if(UpgradeCtxGet()->isImgUpgradeCopyDone)
            {
                /* Set the currentState of UpgradePSKeys and SmCtx to default value
                 * after role switch since the new secondary don't need this
                 * information at this stage. Moreover, if it is not set, then
                 * during subsequent DFU, the currentState incorrect value can
                 * lead to not starting the peer DFU.
                 */
                UpgradePeerResetCurState();
                /* Send the UPGRADE_HOST_TRANSFER_COMPLETE_IND to complete the
                 * data transfer process of DFU.
                 */
                UpgradeCtxGet()->funcs->SendShortMsg(UPGRADE_HOST_TRANSFER_COMPLETE_IND);
            }
            else
                MessageSendConditionally(UpgradeGetUpgradeTask(), UPGRADE_INTERNAL_CONTINUE, NULL,
                                                     UpgradeCtxGetImageCopyStatus());
#endif
        }
        break;

    case UPGRADE_HOST_TRANSFER_COMPLETE_RES:
        {
            UPGRADE_HOST_TRANSFER_COMPLETE_RES_T *msg = (UPGRADE_HOST_TRANSFER_COMPLETE_RES_T *)message;
            PRINT(("UPGRADE_HOST_TRANSFER_COMPLETE_RES\n"));
            if(msg !=NULL && msg->action == 0)
            {
                /* Initialize the peer context for peer device, because
                 * UPGRADE_PEER_TRANSFER_COMPLETE_RES will be sent to both the
                 * devices upgrade peer librabry.
                 */
                if(!UPGRADE_PEER_IS_PRIMARY)
                {
                    UpgradePeerCtxInit();
                }
                /* Send UPGRADE_PEER_TRANSFER_COMPLETE_RES message to 
                 * upgrade_peer library of both the devices
                 * since the action set with this message will be required
                 * during the dynamic role commit phase.
                 */
                UpgradePeerProcessHostMsg(UPGRADE_PEER_TRANSFER_COMPLETE_RES,
                                                UPGRADE_CONTINUE);
                UpgradeCtxGetPSKeys()->upgrade_in_progress_key = UPGRADE_RESUME_POINT_POST_REBOOT;
                UpgradeSavePSKeys();
                PRINT(("P&R: UPGRADE_RESUME_POINT_POST_REBOOT saved\n"));

                /* Read the Upgrade PSKEY again, and check for the upgrade_in_progress_key
                 * value. If the set value is UPGRADE_RESUME_POINT_POST_REBOOT,
                 * then proceed with the defined reboot, else Abort the DFU.
                 */
                UpgradeLoadPSStore(EARBUD_UPGRADE_CONTEXT_KEY,EARBUD_UPGRADE_LIBRARY_CONTEXT_OFFSET);

                if(UpgradeCtxGetPSKeys()->upgrade_in_progress_key == UPGRADE_RESUME_POINT_POST_REBOOT)
                {
                    /* After the UPGRADE_PEER_TRANSFER_COMPLETE_RES msg is sent to the peer
                     * device, the primary device wait for 1 sec before
                     * reboot. The handling of device reboot will be done in
                     * UPGRADE_INTERNAL_DELAY_REBOOT case.
                     */
#ifndef HOSTED_TEST_ENVIRONMENT
                    MessageSendLater(UpgradeGetUpgradeTask(), UPGRADE_INTERNAL_DELAY_REBOOT, NULL,
                                                           UPGRADE_WAIT_FOR_REBOOT);
#endif
                }
                else
                {
                    DEBUG_LOG_INFO("UpgradeSMHandleValidated: Upgrade PSKYEY write failed. Abort the DFU");
                    FatalError(UPGRADE_HOST_ERROR_UPDATE_FAILED);
                }
            }
            else
            {
                if(UPGRADE_PEER_IS_PRIMARY && UPGRADE_PEER_IS_STARTED)
                {
                    /* Host has aborted th DFU, inform peer device as well */
                    UpgradePeerProcessHostMsg(UPGRADE_PEER_TRANSFER_COMPLETE_RES,
                                                UPGRADE_ABORT);
                }
                else
                {
                    UpgradeSendEndUpgradeDataInd(upgrade_end_state_abort);
                    IsValidatedToTrySwap(TRUE);
                    UpgradeSMMoveToState(UPGRADE_STATE_SYNC);
                }
            }
        }
        break;

    case UPGRADE_INTERNAL_DELAY_REBOOT:
        {
            UpgradeSendEndUpgradeDataInd(upgrade_end_state_complete);

            if(UPGRADE_PEER_IS_PRIMARY)
            {
                /*
                 * Trigger reboot after DFU domain confirms that its ok
                 * to reboot.
                 */
                UpgradeCtxGet()->perms = upgrade_perm_always_ask;
            }

            /*Can consider disconnecting streams here*/

            /* if we have permission, go ahead and call loader/reboot */
            if (UpgradeSMHavePermissionToProceed(UPGRADE_APPLY_IND))
            {
                PRINT(("IsValidatedToTrySwap() in UPGRADE_HOST_TRANSFER_COMPLETE_RES\n"));
                IsValidatedToTrySwap(FALSE);
            }
        }
        break;
    case UPGRADE_HOST_IS_CSR_VALID_DONE_REQ:
        {
            PRINT(("UPGRADE_HOST_IS_CSR_VALID_DONE_REQ\n"));

            /* If Peer DFU is supported, then start the DFU of Peer Device */
            if(UPGRADE_PEER_IS_PRIMARY)
            {
                if(!UPGRADE_PEER_IS_STARTED)
                {
                    /* If during prim to sec data transfer, prim device is reset, 
                     * then start the peer DFU once again without checking for the
                     * status of image copy from one bank to another.
                     */
                    if(UpgradePeerWasPeerDfuInProgress())
                    {
                        DEBUG_LOG("UpgradeSMHandleValidated: Setup Peer connection for DFU after Primary reset");
                        UpgradeCtxSetImageCopyStatus(IMAGE_UPGRADE_COPY_COMPLETED);
                    }
                    else
                    {
                        DEBUG_LOG("UpgradeSMHandleValidated: Setup Peer connection for DFU");
                        /*
                         * Setup Peer connection for DFU once image upgarde copy is
                         * completed successfully.
                         */
                        if(UpgradeCtxGet()->isImgUpgradeCopyDone)
                        {
                            if(UpgradeCtxGet()->ImgUpgradeCopyStatus)
                                UpgradeCtxSetImageCopyStatus(IMAGE_UPGRADE_COPY_COMPLETED);
                            else
                                break; /* No need to setup peer connection */
                        }
                        else
                        {
                            UpgradeCtxSetImageCopyStatus(IMAGE_UPGRADE_COPY_IN_PROGRESS);
                        }
                    }
                    if(UpgradePeerStartDfu(UPGRADE_IMAGE_COPY_CHECK_REQUIRED)== FALSE)
                    {
                    /* TODO: An error has occured, failed the DFU */
                    }
                }
            }
#ifndef HOSTED_TEST_ENVIRONMENT
            /* Send UPGRADE_HOST_TRANSFER_COMPLETE_IND later once upgrade is
             * done during standalone DFU.
             */
            else
            {
                /* Check for image upgrade copy status. If the status is in
                 * progress, then send conditional message to send 
                 * UPGRADE_HOST_TRANSFER_COMPLETE_IND to Host once the image
                 * upgrade copy is completed.
                 */
                if(UpgradeCtxGet()->isImgUpgradeCopyDone)
                    UpgradeCtxGet()->funcs->SendShortMsg(UPGRADE_HOST_TRANSFER_COMPLETE_IND);
                else
                    MessageSendConditionally(UpgradeGetUpgradeTask(), UPGRADE_HOST_IS_CSR_VALID_DONE_REQ, NULL,
                                                            UpgradeCtxGetImageCopyStatus());
            }
#endif
        }
        break;

    case UPGRADE_HOST_TRANSFER_COMPLETE_IND:
        /* Receive the UPGRADE_HOST_TRANSFER_COMPLETE_IND message from Peer
         * device once the DFU data is successfully transferred and validated
         * in Peer device. Then, send UPGRADE_HOST_TRANSFER_COMPLETE_IND
         * to Host now.
         */
        {
            /* Check if the Image copy is completed or not in Primary device. 
             * If not, then wait for the completion, else send the UPGRADE_HOST_TRANSFER_COMPLETE_IND
             * to Host on Image copy completion
             */
            if(UpgradeCtxGet()->isImgUpgradeCopyDone)
            {
                /* During Peer DFU process, set the resume point as 
                 * UPGRADE_RESUME_POINT_PRE_REBOOT here. Refer to HandleValidating()
                 * routine for more details.
                 */
                UpgradeCtxGetPSKeys()->upgrade_in_progress_key = UPGRADE_RESUME_POINT_PRE_REBOOT;
                UpgradeSavePSKeys();
                DEBUG_LOG("P&R: UPGRADE_RESUME_POINT_PRE_REBOOT saved");

                /* Validation completed, and now waiting for UPGRADE_TRANSFER_COMPLETE_RES
                 * protocol message. Update resume point and ensure we remember it.
                 */
                DEBUG_LOG("UPGRADE_HOST_TRANSFER_COMPLETE_IND sent to Host");
                UpgradeCtxGet()->funcs->SendShortMsg(UPGRADE_HOST_TRANSFER_COMPLETE_IND);
            }
#ifndef HOSTED_TEST_ENVIRONMENT
            else
            {
                DEBUG_LOG("UpgradeSMHandleValidated: Copy not completed in primary, wait");
                MessageSendConditionally(UpgradeGetUpgradeTask(), UPGRADE_HOST_TRANSFER_COMPLETE_IND,
                                         NULL, UpgradeCtxGetImageCopyStatus());
            }
#endif
            break;
        }

    /* application finally gave permission, warm reboot */
    case UPGRADE_INTERNAL_REBOOT:
        {
            PRINT(("IsValidatedToTrySwap() in UPGRADE_INTERNAL_REBOOT\n"));
            IsValidatedToTrySwap(FALSE);
        }
        break;

    case UPGRADE_VM_IMAGE_UPGRADE_COPY_SUCCESSFUL:
        DEBUG_LOG_INFO("UpgradeSMHandleValidated, UPGRADE_VM_IMAGE_UPGRADE_COPY_SUCCESSFUL");
        /*
         * Try the images from the "other image bank" in all QSPI devices.
         * The apps p0 will initiate a warm reset.
         */
        PRINT(("IsValidatedToTrySwap() in UPGRADE_VM_IMAGE_UPGRADE_COPY_SUCCESSFUL\n"));
        IsValidatedToTrySwap(FALSE);
        break;

    case UPGRADE_VM_DFU_COPY_VALIDATION_SUCCESS:
        {
            PRINT(("ImageUpgradeSwapTry() in UPGRADE_VM_DFU_COPY_VALIDATION_SUCCESS\n"));
            ImageUpgradeSwapTry();
        }
        break;

    case UPGRADE_VM_AUDIO_DFU_FAILURE:
        UpgradeApplyAudioCopyFailed();
    case UPGRADE_VM_IMAGE_UPGRADE_COPY_FAILED:
        PRINT(("FAILED COPY\n"));
        UpgradeSMMoveToState(UPGRADE_STATE_SYNC);
        break;

    default:
        return FALSE;
    }

    return TRUE;
}

/*
NAME
    UpgradeSMAbort - Clean everything and go to the sync state.

DESCRIPTION
    Common handler for clearing up an upgrade after an abort
    and going back to a state that is ready for a new upgrade.
*/
bool UpgradeSMAbort(void)
{
    upgradeSmConfig_ResetValidation();

    /* If peer upgrade is supported then inform UpgradePeer lib as well */
    if(UPGRADE_PEER_IS_PRIMARY && UPGRADE_PEER_IS_STARTED)
    {
#ifdef HANDOVER_DFU_ABORT_WITHOUT_ERASE
        if (UpgradeGetHandoverDFUAbortState())
        {
            /*
             * Error notify the peer of Handover so that it can pause the
             * DFU. Instead of a new PDU, use UPGRADE_PEER_ERROR_WARN_RES to
             * indicate Handover to the peer and even when peer hasn't 
             * previously sent UPGRADE_HOST_ERRORWARN_IND.
             */
            UpgradePeerProcessHostMsg(UPGRADE_PEER_ERROR_WARN_RES,
                                        UPGRADE_HANDOVER_ERROR_IND);
        }
        else
#endif
        {
            UpgradePeerProcessHostMsg(UPGRADE_PEER_ABORT_REQ, UPGRADE_ABORT);
        }
    }

    /* if we are going to reboot to revert commit, then we are alredy running from alternate bank
     * so, we shouldn't erase. return False to inform synchronous abort*/
    if(UpgradeCtxGet()->isImageRevertNeededOnAbort)
    {
        DEBUG_LOG("UpgradeSMAbort return false to inform synchronous abort and without erase");
        return FALSE;
    }

    /* if we have permission erase immediately and return to the SYNC state
     * to start again if required */
    if (UpgradeSMHavePermissionToProceed(UPGRADE_BLOCKING_IND))
    {
#ifdef MESSAGE_IMAGE_UPGRADE_COPY_STATUS
        /*
         * There may be non-blocking traps such as ImageUpgradeCopy in progress.
         * Call the ImageUpgradeAbortCommand() trap to abort any of those. It
         * will do no harm if there are no non-blocking traps in progress.
         */
        PRINT(("ImageUpgradeAbortCommand()\n"));
        ImageUpgradeAbortCommand();
#endif  /* MESSAGE_IMAGE_UPGRADE_COPY_STATUS */
        UpgradeSMErase();
        UpgradeSMSetState(UPGRADE_STATE_SYNC);
    }

    return TRUE;
}

uint16 UpgradeSMNewImageStatus(void)
{
    uint16 err = 0;
    bool result = ImageUpgradeSwapTryStatus();
    PRINT(("ImageUpgradeSwapTryStatus() returns %d\n", result));
    if (!result)
    {
        err = UPGRADE_HOST_ERROR_LOADER_ERROR;
    }
    return err;
}

/*
NAME
    UpgradeSMCheckEraseComplete

DESCRIPTION
    Indicate whether the erase has completed.
    Returns FALSE for CONFIG_HYDRACORE as UpgradePartitionsEraseAllManaged
    is non-blocking and completion is indicated by the
    MESSAGE_IMAGE_UPGRADE_ERASE_STATUS message.
*/
bool UpgradeSMCheckEraseComplete(void)
{
    if (UPGRADE_RESUME_POINT_ERASE != UpgradeCtxGetPSKeys()->upgrade_in_progress_key)
    {
        if(UpgradeCtxGet()->smState == UPGRADE_STATE_COMMIT)
        {
            return TRUE;
        }
        else
        {
            return FALSE;
        }
    }
    else
    {
        return TRUE;
    }
}

/*
NAME
    UpgradeSMActionOnValidated

DESCRIPTION
    Calls ImageUpgradeCopy().
*/
void UpgradeSMActionOnValidated(void)
{
#ifdef MESSAGE_IMAGE_UPGRADE_COPY_STATUS
    PRINT(("ImageUpgradeCopy()\n"));
    ImageUpgradeCopy();
#endif  /* MESSAGE_IMAGE_UPGRADE_COPY_STATUS */
}

/*
NAME
    UpgradeSMHandleAudioDFU

DESCRIPTION
    Calls UpgradeSMHandleAudioImageCopy().
*/
void UpgradeSMHandleAudioDFU(void)
{
#ifdef MESSAGE_IMAGE_UPGRADE_AUDIO_STATUS
    PRINT(("ImageUpgradeAudio()\n"));
    ImageUpgradeAudio();
#endif
}

