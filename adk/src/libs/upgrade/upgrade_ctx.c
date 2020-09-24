/****************************************************************************
Copyright (c) 2015 Qualcomm Technologies International, Ltd.


FILE NAME
    upgrade_ctx.c
    
DESCRIPTION
    Global state of Upgrade library.
*/

#define DEBUG_LOG_MODULE_NAME upgrade
#include <logging.h>

#include <stdlib.h>
#include <panic.h>
#include <print.h>

#include "upgrade_ctx.h"
#include "upgrade_psstore.h"
#include "logging.h"

UpgradeCtx *upgradeCtx;

/****************************************************************************
NAME
    UpgradeCtxGet

DESCRIPTION
    Sets a new library context
*/

void UpgradeCtxSet(UpgradeCtx *ctx)
{
    PRINT(("size of UpgradeCtx is %d\n", sizeof(*ctx)));
    PRINT(("size of UpgradePartitionDataCtx is %d", sizeof(UpgradePartitionDataCtx)));
    if (ctx)
    {
        PRINT((" of which data buffer is %ld\n", UPGRADE_PARTITION_DATA_BLOCK_SIZE(ctx)));
    }
    else
    {
        PRINT((" with NULL ctx\n"));
    }
    PRINT(("size of UPGRADE_LIB_PSKEY is %d\n", sizeof(UPGRADE_LIB_PSKEY)));
    upgradeCtx = ctx;
}

/****************************************************************************
NAME
    UpgradeCtxGet

RETURNS
    Context of upgrade library
*/
UpgradeCtx *UpgradeCtxGet(void)
{
    if(!upgradeCtx)
    {
        PRINT(("UpgradeGetCtx: You shouldn't have done that\n"));
        Panic();
    }

    return upgradeCtx;
}

/****************************************************************************
NAME
    UpgradeCtxSetPartitionData

DESCRIPTION
    Sets the partition data context in the upgrade context.
*/
void UpgradeCtxSetPartitionData(UpgradePartitionDataCtx *ctx)
{
    PRINT(("size of UpgradePartitionDataCtx is %d of which data buffer is %ld\n",
            sizeof(UpgradePartitionDataCtx), UPGRADE_PARTITION_DATA_BLOCK_SIZE(upgradeCtx)));
    upgradeCtx->partitionData = ctx;
}

/****************************************************************************
NAME
    UpgradeCtxGetPartitionData

RETURNS
    Context of upgrade partition data.
    Note that it may be NULL if it has not been set yet.
*/
UpgradePartitionDataCtx *UpgradeCtxGetPartitionData(void)
{
    return upgradeCtx->partitionData;
}

UpgradeFWIFCtx *UpgradeCtxGetFW(void)
{
    return &upgradeCtx->fwCtx;
}

UPGRADE_LIB_PSKEY *UpgradeCtxGetPSKeys(void)
{
    return &upgradeCtx->UpgradePSKeys;
}

bool UpgradeIsInitialised(void)
{
    return (upgradeCtx != NULL);
}

/****************************************************************************
NAME
    UpgradeCtxGetImageCopyStatus

RETURNS
    imageUpgradeCopyProgress
*/

uint16 *UpgradeCtxGetImageCopyStatus(void)
{
    return &(upgradeCtx->imageUpgradeCopyProgress);
}

/****************************************************************************
NAME
    UpgradeCtxSetImageCopyStatus

DESCRIPTION
    Sets the imageUpgradeCopyProgress in the upgrade context.
*/

void UpgradeCtxSetImageCopyStatus(image_upgrade_copy_status status)
{
    upgradeCtx->imageUpgradeCopyProgress = status;
}

/****************************************************************************
NAME
    UpgradeCtxSetPeerDataTransferComplete

DESCRIPTION
    Sets the upgradePeerDataTransferComplete in the upgrade context.
*/

void UpgradeCtxSetPeerDataTransferComplete(void)
{
    upgradeCtx->upgradePeerDataTransferStatus = UPGRADE_PEER_DATA_TRANSFER_COMPLETED;
}

/****************************************************************************
NAME
    UpgradeCtxGetPeerDataTransferStatus

RETURNS
    upgradePeerDataTransferStatus
*/

uint16 *UpgradeCtxGetPeerDataTransferStatus(void)
{
    return &(upgradeCtx->upgradePeerDataTransferStatus);
}

/*!
    @brief Clear upgrade related local Pskey info maintained in context.
    @param none
    
    Returns none
*/
void UpgradeCtxClearPSKeys(void)
{
    if (upgradeCtx == NULL)
    {
        DEBUG_LOG("UpgradeCtxClearPSKeys: Can't be NULL\n");
        return;
    }

    /*
     * ToDo: Check if the memberwise reset be replaced by a memset except for
     * future_partition_state which is exclusively done wherever requried.
     */
    UpgradeCtxGetPSKeys()->state_of_partitions = UPGRADE_PARTITIONS_ERASED;
    UpgradeCtxGetPSKeys()->version_in_progress.major = 0;
    UpgradeCtxGetPSKeys()->version_in_progress.minor = 0;
    UpgradeCtxGetPSKeys()->config_version_in_progress = 0;
    UpgradeCtxGetPSKeys()->id_in_progress = 0;

    UpgradeCtxGetPSKeys()->upgrade_in_progress_key = UPGRADE_RESUME_POINT_START;
    UpgradeCtxGetPSKeys()->last_closed_partition = 0;
    UpgradeCtxGetPSKeys()->dfu_partition_num = 0;
    UpgradeCtxGetPSKeys()->loader_msg = UPGRADE_LOADER_MSG_NONE;

    UpgradeSavePSKeys();
}
