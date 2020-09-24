/****************************************************************************
Copyright (c) 2014 - 2018 Qualcomm Technologies International, Ltd.


FILE NAME
    upgrade_host_if_data.c

DESCRIPTION
    Implementation of the module which handles protocol message communications
    between host and device.

NOTES

*/

#define DEBUG_LOG_MODULE_NAME upgrade_peer
#include <logging.h>

#include <stdlib.h>
#include <string.h>
#include <panic.h>
#include <byte_utils.h>
#include <print.h>

#include "upgrade_peer.h"
#include "upgrade_host_if.h"
#include "upgrade_peer_if_data.h"
#include "upgrade_host_if_data.h"

#define SHORT_MSG_SIZE (sizeof(uint8) + sizeof(uint16))

void UpgradePeerIFDataSendShortMsg(UpgradeMsgHost message)
{
    UPGRADE_PEER_COMMON_CMD_T *rsp =
        (UPGRADE_PEER_COMMON_CMD_T *)PanicUnlessMalloc(SHORT_MSG_SIZE);

    rsp->op_code = message - UPGRADE_HOST_MSG_BASE;
    rsp->length = 0;

    UpgradeHostIFClientSendData((uint8 *)rsp, SHORT_MSG_SIZE);
}

void UpgradePeerIFDataSendSyncCfm(uint16 status, uint32 id)
{
    UPGRADE_PEER_SYNC_CFM_T *rsp =
        (UPGRADE_PEER_SYNC_CFM_T *)PanicUnlessMalloc(sizeof(*rsp));

    DEBUG_LOG_INFO("UpgradePeerIFDataSendSyncCfm, status %d, id 0x%lx, version %d", status, id, PROTOCOL_CURRENT_VERSION);

    rsp->common.op_code = UPGRADE_HOST_SYNC_CFM - UPGRADE_HOST_MSG_BASE;
    rsp->common.length = UPGRADE_HOST_SYNC_CFM_BYTE_SIZE;
    rsp->resume_point = (uint8)status;
    rsp->upgrade_file_id = id;
    rsp->protocol_id = PROTOCOL_CURRENT_VERSION;

    UpgradeHostIFClientSendData((uint8 *)rsp, sizeof(*rsp));
}

void UpgradePeerIFDataSendStartCfm(uint16 status, uint16 batteryLevel)
{
    UPGRADE_PEER_START_CFM_T *rsp =
        (UPGRADE_PEER_START_CFM_T *)PanicUnlessMalloc(sizeof(*rsp));

    DEBUG_LOG_INFO("UpgradePeerIFDataSendStartCfm, status %d, batLevel 0x%x", status, batteryLevel);

    rsp->common.op_code = UPGRADE_HOST_START_CFM - UPGRADE_HOST_MSG_BASE;
    rsp->common.length = UPGRADE_HOST_START_CFM_BYTE_SIZE;
    rsp->battery_level = batteryLevel;
    rsp->status = (uint8)status;

    UpgradeHostIFClientSendData((uint8 *)rsp, sizeof(*rsp));
}

void UpgradePeerIFDataSendBytesReq(uint32 numBytes, uint32 startOffset)
{
    UPGRADE_PEER_START_DATA_BYTES_REQ_T *rsp =
        (UPGRADE_PEER_START_DATA_BYTES_REQ_T *)PanicUnlessMalloc(sizeof(*rsp));

    DEBUG_LOG_INFO("UpgradePeerIFDataSendBytesReq, numBytes %u, startOffset %u", numBytes, startOffset);

    rsp->common.op_code = UPGRADE_HOST_DATA_BYTES_REQ - UPGRADE_HOST_MSG_BASE;
    rsp->common.length = UPGRADE_HOST_DATA_BYTES_REQ_BYTE_SIZE;
    rsp->data_bytes = numBytes;
    rsp->start_offset = startOffset;

    UpgradeHostIFClientSendData((uint8 *)rsp, sizeof(*rsp));
}


void UpgradePeerIFDataSendErrorInd(uint16 errorCode)
{
    UPGRADE_PEER_UPGRADE_ERROR_IND_T *rsp =
        (UPGRADE_PEER_UPGRADE_ERROR_IND_T *)PanicUnlessMalloc(sizeof(*rsp));

    DEBUG_LOG_ERROR("UpgradePeerIFDataSendErrorInd, errorCode 0x%x", errorCode);

    rsp->common.op_code = UPGRADE_HOST_ERRORWARN_IND - UPGRADE_HOST_MSG_BASE;
    rsp->common.length = UPGRADE_HOST_ERRORWARN_IND_BYTE_SIZE;
    rsp->error = errorCode;

    UpgradeHostIFClientSendData((uint8 *)rsp, sizeof(*rsp));
}

void UpgradePeerIFDataSendIsCsrValidDoneCfm(uint16 backOffTime)
{
    UPGRADE_PEER_VERIFICATION_DONE_CFM_T *rsp =
        (UPGRADE_PEER_VERIFICATION_DONE_CFM_T *)PanicUnlessMalloc(sizeof(*rsp));

    DEBUG_LOG_INFO("UpgradePeerIFDataSendIsCsrValidDoneCfm, backOffTime %u", backOffTime);

    rsp->common.op_code = UPGRADE_HOST_IS_CSR_VALID_DONE_CFM - UPGRADE_HOST_MSG_BASE;
    rsp->common.length = UPGRADE_HOST_IS_CSR_VALID_DONE_CFM_BYTE_SIZE;
    rsp->delay_time = backOffTime;

    UpgradeHostIFClientSendData((uint8 *)rsp, sizeof(*rsp));
}

