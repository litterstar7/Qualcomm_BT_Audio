/****************************************************************************
Copyright (c) 2019 Qualcomm Technologies International, Ltd.


FILE NAME
    upgrade_peer_partition.c

DESCRIPTION
    This file handles read request state machine and sends data of requested
    length.

*/

#define DEBUG_LOG_MODULE_NAME upgrade_peer
#include <logging.h>

#include <stream.h>
#include <source.h>
#include <imageupgrade.h>
#include <ps.h>

#include "upgrade_private.h"
#include "upgrade_partition_data.h"
#include "upgrade_peer_private.h"
#include "upgrade_ctx.h"


#define PEER_UPGRADE_HEADER_MIN_SECOND_PART_SIZE 22

/** Header image section. */
/* NOTE: this image section is for internal use cannot be upgraded
 * using image upgrade traps.
 * This image section is used to store all headers of DFU file for peer
 * upgrade support.
 */
#define UPGRADE_IMAGE_SECTION_DFU_HEADERS                  (0xd)

#define UPGRADE_RSA_2048

#define FIRST_WORD_SIZE 4

#if defined (UPGRADE_RSA_2048)
/*
 * EXPECTED_SIGNATURE_SIZE is the size of an RSA-2048 signature.
 * 2048 bits / 8 bits per byte is 256 bytes.
 */
#define EXPECTED_SIGNATURE_SIZE 256
#elif defined (UPGRADE_RSA_1024)
/*
 * EXPECTED_SIGNATURE_SIZE is the size of an RSA-1024 signature.
 * 1024 bits / 8 bits per byte is 128 bytes.
 */
#define EXPECTED_SIGNATURE_SIZE 128
#else
#error "Neither UPGRADE_RSA_2048 nor UPGRADE_RSA_1024 defined."
#endif

UpgradePeerPartitionDataCtx *peerPartitionCtx;

/****************************************************************************
NAME
    UpgradePeerPartitionDataCtxGet

RETURNS
    Context of upgrade peer partition
*/
UpgradePeerPartitionDataCtx *UpgradePeerPartitionDataCtxGet(void)
{
    if(!peerPartitionCtx)
    {
        DEBUG_LOG("UpgradePeerPartitionDataCtxGet: Not there");
        Panic();
    }

    return peerPartitionCtx;
}

/******************************************************************************
NAME
        PartitionOpenSource
DESCRIPTION
        Open a read handle to a physical partition on the external flash.

PARAMS
        physPartition Physical partition number in external flash.
RETURNS
        A valid handle or NULL if the open failed.
*/

static Source PartitionOpenSource(uint16 physPartition)
{
    uint16 QSPINum = 0;
    Source src = NULL;

    DEBUG_LOG("UpgradePeerPartition: Opening partition %u \n", physPartition);

    src = ImageUpgradeStreamGetSource(QSPINum, physPartition);

    if (src == NULL)
    {
        DEBUG_LOG("UpgradePeerPartition: Parition Open failed %u \n",
                                       physPartition);
        return (Source)NULL;
    }

    return src;
}

static const char *UpgradeGetHeaderID(void)
{
    return "APPUHDR5";
}

static const char *UpgradeGetPartitionID(void)
{
    return "PARTDATA";
}

static const char *UpgradeGetFooterID(void)
{
    return "APPUPFTR";
}

/******************************************************************************
NAME
    IsValidPartitionNum

DESCRIPTION
    Checks if the provided partition is accessible or not.

*/
static bool IsValidPartitionNum(uint16 partNum)
{
    switch (partNum)
    {
    case IMAGE_SECTION_NONCE:
    case IMAGE_SECTION_APPS_P0_HEADER:
    case IMAGE_SECTION_APPS_P1_HEADER:
    case IMAGE_SECTION_AUDIO_HEADER:
    case IMAGE_SECTION_CURATOR_FILESYSTEM:
    case IMAGE_SECTION_APPS_P0_IMAGE:
    case IMAGE_SECTION_APPS_RO_CONFIG_FILESYSTEM:
    case IMAGE_SECTION_APPS_RO_FILESYSTEM:
    case IMAGE_SECTION_APPS_P1_IMAGE:
    case IMAGE_SECTION_APPS_DEVICE_RO_FILESYSTEM:
    case IMAGE_SECTION_AUDIO_IMAGE:
    case IMAGE_SECTION_APPS_RW_CONFIG:
        return TRUE;

    case IMAGE_SECTION_APPS_RW_FILESYSTEM:
    case UPGRADE_IMAGE_SECTION_DFU_HEADERS:
    default:
        return FALSE;
    }
}

static bool IsValidSqifNum(uint16 sqifNum, uint16 partNum)
{
    UNUSED(partNum);
    /* Until audio is supported, only SQIF zero is valid.*/
    return (sqifNum == 0);
}

/******************************************************************************
NAME
    getNextUpgradePartitionData

DESCRIPTION
    Fetch the next chunk of upgrade data from required partition.

RETURNS
    Boolean value.
*/
static bool getNextUpgradePartitionData(Source src,
                                        uint8 *peer_data,
                                        uint32 req_len)
{
    uint32 available_bytes;
    uint32 upgrade_len;
    uint8 *data;

    available_bytes = SourceSize(src);

    upgrade_len = MIN(available_bytes, req_len);
    data = (uint8 *) SourceMap(src);

    if(data == NULL)
    {
        return FALSE;
    }

    memmove(peer_data, data, upgrade_len);
    SourceDrop(src, upgrade_len);

    return TRUE;
}

/******************************************************************************
NAME
    getNextUpgradePskeyData

DESCRIPTION
    Fetch the next chunk of upgrade data from required pskey.

RETURNS
    Boolean value.
*/
static bool getNextUpgradePskeyData(uint8 *peer_data, uint32 req_len)
{
    uint32 read_len, available_bytes;
    uint32 data_offset = 0;
    uint16 key_length;
    uint16 *key_data;

    if(peerPartitionCtx == NULL || peerPartitionCtx->pskey > DFU_HEADER_PSKEY_END ||
       req_len == 0)
    {
        return FALSE;
    }

    DEBUG_LOG("UpgradePeerPartition: Read Header of length %d", req_len);

    /* Allocate temperory buffer to store PSKEY data */
    key_data = malloc(PSKEY_MAX_STORAGE_LENGTH * sizeof(uint16));

    /* Fetch PSKEY length */
    key_length = PsRetrieve(peerPartitionCtx->pskey, NULL, 0);

    DEBUG_LOG("UpgradePeerPartition, PSKEY %u, length %u", peerPartitionCtx->pskey, key_length);

    while(req_len)
    {
        /* First get available data size in current PSKEY and then take
         * least value of requested data length or available data.
         */
        available_bytes = key_length - peerPartitionCtx->read_offset;

        DEBUG_LOG("UpgradePeerPartition: available_bytes %d", available_bytes);
        DEBUG_LOG("UpgradePeerPartition: peerPartitionCtx->read_offset %d", peerPartitionCtx->read_offset);

        read_len = MIN(available_bytes, MIN(req_len, key_length));

        DEBUG_LOG("UpgradePeerPartition: Read pskey and len %d %d",
                          peerPartitionCtx->pskey, read_len);

        if(PsRetrieve(peerPartitionCtx->pskey, key_data, key_length) != 0)
        {
            memcpy(peer_data + data_offset,
                   key_data + peerPartitionCtx->read_offset,
                   read_len* sizeof(uint16));

            /* Increment offset by read length */
            peerPartitionCtx->read_offset += read_len;
            data_offset += read_len* sizeof(uint16);
            req_len -= read_len;
        }
        else
        {
            DEBUG_LOG("UpgradePeerPartition: PSRetrieve Failed");
            return FALSE;
        }

        if(peerPartitionCtx->read_offset == PSKEY_MAX_STORAGE_LENGTH)
        {
            /* If length reaches to PSKEY size then move to next PSKEY */
            peerPartitionCtx->read_offset = 0;
            peerPartitionCtx->pskey += 1;
            key_length = PsRetrieve(peerPartitionCtx->pskey, NULL, 0);
        }
    }

    free(key_data);

    return TRUE;
}
/******************************************************************************
NAME
    checkPskeyDataAvailable

DESCRIPTION
    check if the header data size is available in the pskeys.

PARAMS
        size Size requested in words    

RETURNS
    Boolean value.
*/
static bool checkPskeyDataAvailable(uint32 size)
{
    uint16 key_length;
    uint8 diff_len;
    UpgradePartitionDataCtx *ctx = UpgradeCtxGetPartitionData();
    UpgradePeerPartitionDataCtx *peer_ctx = UpgradePeerPartitionDataCtxGet();
    uint16 readable_size;


    DEBUG_LOG("checkPskeyDataAvailable : ctx->dfuHeaderPskey %d",ctx->dfuHeaderPskey); 
    DEBUG_LOG("checkPskeyDataAvailable : peer_ctx->pskey %d",peer_ctx->pskey); 
    DEBUG_LOG("checkPskeyDataAvailable : ctx->dfuHeaderPskeyOffset %d",ctx->dfuHeaderPskeyOffset); 
    DEBUG_LOG("checkPskeyDataAvailable : peer_ctx->read_offset %d",peer_ctx->read_offset); 
    
    if(ctx->dfuHeaderPskey < peer_ctx->pskey)
    {
        DEBUG_LOG("checkPskeyDataAvailable : write is slower"); 
        return FALSE;
    }
    else
    {
        diff_len = ctx->dfuHeaderPskey - peer_ctx->pskey;
    }

    /*check if same pskey is used, ensure write offset not crossed*/
    if (diff_len == 0)
    {
        /* dfuHeaderPskeyOffset is in bytes, so convert to words with rounding down */
        if ((ctx->dfuHeaderPskeyOffset / 2) < peer_ctx->read_offset)
        {
            DEBUG_LOG("checkPskeyDataAvailable : write offset is lower"); 
            return FALSE;
        }
    }

    key_length = PsRetrieve(peer_ctx->pskey, NULL, 0);

    /*check if requested size goes beyond the write pskey offset*/
    readable_size = ((diff_len * PSKEY_MAX_STORAGE_LENGTH) + (ctx->dfuHeaderPskeyOffset / 2)) - peer_ctx->read_offset;

    DEBUG_LOG("checkPskeyDataAvailable : readable_size  %d size requested %d",readable_size, size);
    
    if((readable_size < size ) ||(peerPartitionCtx->read_offset == key_length ))
    {
        DEBUG_LOG("checkPskeyDataAvailable : PSKEY len insufficient"); 
        return FALSE;
    }

    DEBUG_LOG("checkPskeyDataAvailable : data is fine"); 

    return TRUE;
}

/******************************************************************************
NAME
    UpgradePeerDfuHandleGeneric1stPartState  -  Parser for ID and
    length part of a header.

DESCRIPTION
    Parses common beginning of any header and determines which header it is.
    All headers have the same first two fields which are 'header id' and
    length.
*/
static upgrade_peer_status_t UpgradePeerPartitionDataHandleGeneric1stPartState(
                                                                uint8* data_ptr,
                                                                uint32 req_len)
{
    uint32 length;
    uint8 index;

    if(req_len < HEADER_FIRST_PART_SIZE)
    {
        return UPGRADE_PEER_ERROR_BAD_LENGTH_TOO_SHORT;
    }

    /*ToDo -If concurrent scheme, check for availability of requested bytes*/
    if(!checkPskeyDataAvailable(BYTES_TO_WORDS(req_len)))
    {
        DEBUG_LOG("UpgradePeerPartitionDataHandleGeneric1stPartState : PSKEY len insufficient"); 
        return UPGRADE_PEER_ERROR_INTERNAL_ERROR_INSUFFICIENT_PSKEY;
    }

    if(getNextUpgradePskeyData(data_ptr, BYTES_TO_WORDS(req_len)) == FALSE)
    {
        DEBUG_LOG("UpgradePeerPartition: Generic1stPartHeaderState Error\n");
        return UPGRADE_PEER_ERROR_PARTITION_OPEN_FAILED;
    }

    length = ByteUtilsGet4BytesFromStream(&data_ptr[ID_FIELD_SIZE]);

    DEBUG_LOG("UpgradePeerPartition: len, data 0x%x", length);
    for(index =0; index < ID_FIELD_SIZE; index++)
        DEBUG_LOG("%c\n", data_ptr[index]);

    /*If Header is "APPUHDR5"*/
    if(0 == strncmp((char *)data_ptr, UpgradeGetHeaderID(), ID_FIELD_SIZE))
    {
        peerPartitionCtx->app_hdr_length = length;
        if(peerPartitionCtx->app_hdr_length <
                          PEER_UPGRADE_HEADER_MIN_SECOND_PART_SIZE)
        {
            return UPGRADE_PEER_ERROR_BAD_LENGTH_UPGRADE_HEADER;
        }
        peerPartitionCtx->peerPartitionState =
                          UPGRADE_PEER_PARTITION_DATA_STATE_HEADER;
        return UPGRADE_PEER_SUCCESS;
    }
    /*If Header is "PARTDATA"*/
    else if(0 == strncmp((char *)data_ptr, UpgradeGetPartitionID(),
                                                                ID_FIELD_SIZE))
    {
        peerPartitionCtx->part_data_length = length;
        if(peerPartitionCtx->part_data_length < PARTITION_SECOND_HEADER_SIZE)
        {
            return UPGRADE_PEER_ERROR_BAD_LENGTH_UPGRADE_HEADER;
        }
        peerPartitionCtx->peerPartitionState =
                          UPGRADE_PEER_PARTITION_DATA_STATE_DATA_HEADER;
        return UPGRADE_PEER_SUCCESS;
    }
    /*If Header is "APPUPFTR"*/
    else if(0 == strncmp((char *)data_ptr, UpgradeGetFooterID(), ID_FIELD_SIZE))
    {
        peerPartitionCtx->footer_length = length;
        if(peerPartitionCtx->footer_length != EXPECTED_SIGNATURE_SIZE)
        {
            /* The length of signature must match expected length.
             * Otherwise OEM signature checking could be omitted by just
             * setting length to 0 and not sending signature.
             */
            return UPGRADE_PEER_ERROR_BAD_LENGTH_SIGNATURE;
        }
        peerPartitionCtx->peerPartitionState =
                          UPGRADE_PEER_PARTITION_DATA_STATE_FOOTER;
        return UPGRADE_PEER_SUCCESS;
    }
    else
    {
        return UPGRADE_PEER_ERROR_UNKNOWN_ID;
    }
}

/******************************************************************************
NAME
    UpgradePeerPartitionDataHandleHeaderState  -  Parser for the main header.

DESCRIPTION
    Handles Peer partiton header state.

RETURNS
    Upgrade peer library error code.
*/
static upgrade_peer_status_t UpgradePeerPartitionDataHandleHeaderState(
                                                                uint8* data_ptr,
                                                                uint32 req_len)

{

    /* ToDo -If concurrent scheme, check for availability of requested bytes*/
    if(!checkPskeyDataAvailable(BYTES_TO_WORDS(req_len)))
    {
        DEBUG_LOG("UpgradePeerPartitionDataHandleHeaderState : PSKEY len insufficient"); 
        return UPGRADE_PEER_ERROR_INTERNAL_ERROR_INSUFFICIENT_PSKEY;
    }
    
    if(getNextUpgradePskeyData(data_ptr, BYTES_TO_WORDS(req_len)) == FALSE)
    {
        DEBUG_LOG("UpgradePeerPartition: DataHandleHeaderState Error\n");
        return UPGRADE_PEER_ERROR_PARTITION_WRITE_FAILED_DATA;
    }

    peerPartitionCtx->peerPartitionState =
                            UPGRADE_PEER_PARTITION_DATA_STATE_GENERIC_1ST_PART;
    return UPGRADE_PEER_SUCCESS;
}

/******************************************************************************
NAME
    UpgradePeerPartitionDataHandleDataHeaderState  -  Parser for the partition
    data header.


DESCRIPTION
    Validates content of the partition data header and saves the required
    partion open handle in the context.

RETURNS
    Upgrade peer library error code.
*/
static upgrade_peer_status_t UpgradePeerPartitionDataHandleDataHeaderState(
                                                            uint8* data_ptr,
                                                            uint32 req_len)
{
    if(req_len < PARTITION_SECOND_HEADER_SIZE + FIRST_WORD_SIZE)
    {
        return UPGRADE_PEER_ERROR_BAD_LENGTH_DATAHDR_RESUME;
    }

    /*ToDo -If concurrent scheme, check for availability of requested bytes*/
    if(!checkPskeyDataAvailable(BYTES_TO_WORDS(req_len)))
    {
        DEBUG_LOG("UpgradePeerPartitionDataHandleDataHeaderState : PSKEY len insufficient"); 
        return UPGRADE_PEER_ERROR_INTERNAL_ERROR_INSUFFICIENT_PSKEY;
    }

    if(getNextUpgradePskeyData(data_ptr,
                                 BYTES_TO_WORDS(PARTITION_SECOND_HEADER_SIZE + FIRST_WORD_SIZE)) == FALSE)
    {
        DEBUG_LOG("UpgradePeerPartition: header read error");
        return UPGRADE_PEER_ERROR_PARTITION_WRITE_FAILED_DATA;
    }

    peerPartitionCtx->part_data_length -= PARTITION_SECOND_HEADER_SIZE;

    peerPartitionCtx->sqifNum = ByteUtilsGet2BytesFromStream(data_ptr);
    DEBUG_LOG("UpgradePeerPartition: SQIF number %u\n",
                                   peerPartitionCtx->sqifNum);

    peerPartitionCtx->partNum = ByteUtilsGet2BytesFromStream(&data_ptr[2]);
    DEBUG_LOG("UpgradePeerPartition: partition number %u\n",
                                   peerPartitionCtx->partNum);

    if(!IsValidPartitionNum(peerPartitionCtx->partNum))
    {
        return UPGRADE_PEER_ERROR_WRONG_PARTITION_NUMBER;
    }

    if(!IsValidSqifNum(peerPartitionCtx->sqifNum, peerPartitionCtx->partNum))
    {
        return UPGRADE_PEER_ERROR_PARTITION_TYPE_NOT_MATCHING;
    }

    peerPartitionCtx->peerPartitionState =
                                        UPGRADE_PEER_PARTITION_DATA_STATE_DATA;
    return UPGRADE_PEER_SUCCESS;
}

/******************************************************************************
NAME
    UpgradePeerPartitionDataHandleDataState  -  Partition data handling.

DESCRIPTION
    Reads data from the required partition for requested length.

RETURNS
    Upgrade peer library error code.
*/
static upgrade_peer_status_t UpgradePeerPartitionDataHandleDataState(
                                                            uint8* data_ptr,
                                                            uint32 req_len,
                                                            uint32 *sent_len,
                                                            uint32 part_offset)
{
    uint32 partition_data_len;

    if(!peerPartitionCtx->partitionHdl)
    {
        uint8 first_word[FIRST_WORD_SIZE];
        peerPartitionCtx->partitionHdl = PartitionOpenSource(peerPartitionCtx->partNum);
        DEBUG_LOG("UpgradePeerPartitionDataHandleDataState Partition open : %u",
                                                     peerPartitionCtx->partNum);
        if(!peerPartitionCtx->partitionHdl)
        {
            return UPGRADE_PEER_ERROR_PARTITION_OPEN_FAILED;
        }

        /*Read and skip the first word from partition source, it is 
         *already copied from pskey*/
        if(getNextUpgradePartitionData(peerPartitionCtx->partitionHdl,
                                       &first_word[0], FIRST_WORD_SIZE) == FALSE)
        {
            /*Close stream source during failure condition.*/
            SourceClose(peerPartitionCtx->partitionHdl);
            /*Reset the partition handle for next iteration*/
            peerPartitionCtx->partitionHdl = NULL;
            DEBUG_LOG_ERROR("UpgradePeerPartition: DataHandleDataState Error");
            return UPGRADE_PEER_ERROR_PARTITION_WRITE_FAILED_DATA;
        }

        peerPartitionCtx->part_data_length -= FIRST_WORD_SIZE;
    }

    DEBUG_LOG_VERBOSE("UpgradePeerPartitionDataHandleDataState: partition length %u, partition offset %u",
                      peerPartitionCtx->part_data_length, part_offset);

    /* Handling of resume of data transfer */
    if(part_offset != 0)
    {
        uint32 read_len = UPGRADE_PEER_MAX_DATA_BLOCK_SIZE;
        /* If the offset size is equal to the part_data_length, it indicates 
         * that the partition read need to be skipped and moved to generic
         * 1st part
         */
        if(part_offset == peerPartitionCtx->part_data_length)
        {
            upgrade_peer_status_t ret = UPGRADE_PEER_SUCCESS;
            SourceClose(peerPartitionCtx->partitionHdl);
            /*Reset the partition handle for next iteration*/
            peerPartitionCtx->partitionHdl = NULL;
            ret = UpgradePeerPartitionDataHandleGeneric1stPartState(data_ptr, req_len);
            *sent_len = req_len;
            return ret;
        }
        /* Resumption of data handling from the partition where it was last read
           Read data till the point of the partition offset and drop it */
        else
        {
            while(part_offset != 0)
            {
                DEBUG_LOG_VERBOSE("UpgradePeerPartitionDataHandleDataState: part_offset %u", part_offset);
                read_len = MIN(read_len, part_offset);
                if(getNextUpgradePartitionData(peerPartitionCtx->partitionHdl,
                                                    data_ptr, read_len) == FALSE)
                {
                    SourceClose(peerPartitionCtx->partitionHdl);
                    /*Reset the partition handle for next iteration*/
                    peerPartitionCtx->partitionHdl = NULL;
                    return UPGRADE_PEER_ERROR_PARTITION_WRITE_FAILED_DATA;
                }
                part_offset -= read_len;
                peerPartitionCtx->part_data_length -= read_len;
            }
        }
    }
    /* Resumption of data handling is completed now. Now we read the data from
     * the offset requested and send it to peer device
     */
    partition_data_len = MIN(peerPartitionCtx->part_data_length, req_len);

    if(getNextUpgradePartitionData(peerPartitionCtx->partitionHdl,
                                data_ptr, partition_data_len) == FALSE)
    {
        SourceClose(peerPartitionCtx->partitionHdl);
        /*Reset the partition handle for next iteration*/
        peerPartitionCtx->partitionHdl = NULL;
        DEBUG_LOG_ERROR("UpgradePeerPartition: DataHandleDataState Error");
        return UPGRADE_PEER_ERROR_PARTITION_WRITE_FAILED_DATA;
    }

    peerPartitionCtx->part_data_length -= partition_data_len;

    /* Updated the amount of data is read */
    *sent_len = partition_data_len;

    if(peerPartitionCtx->part_data_length == 0)
    {
        peerPartitionCtx->peerPartitionState =
                            UPGRADE_PEER_PARTITION_DATA_STATE_GENERIC_1ST_PART;
        SourceClose(peerPartitionCtx->partitionHdl);
        /* Reset the partition handle for next iteration*/
        peerPartitionCtx->partitionHdl = NULL;
        DEBUG_LOG("Partition Handle reset");
    }

    return UPGRADE_PEER_SUCCESS;
}

/******************************************************************************
NAME
    HandlePeerFooterState  -  get footer data.

DESCRIPTION
    Handles footer state and indicates about the last packet.

RETURNS
    Upgrade peer library error code.
*/
static upgrade_peer_status_t HandlePeerFooterState(uint8* data_ptr,
                                                    uint32 req_len,
                                                    bool *last_packet)
{

    DEBUG_LOG("HandlePeerFooterState : req_len bytes  %d",req_len); 
        
    /*ToDo -If concurrent scheme, check for availability of requested bytes*/
    if(!checkPskeyDataAvailable(BYTES_TO_WORDS(req_len)))
    {
        DEBUG_LOG("HandlePeerFooterState : PSKEY len insufficient offset %d req_len %d",peerPartitionCtx->read_offset, req_len); 
        return UPGRADE_PEER_ERROR_INTERNAL_ERROR_INSUFFICIENT_PSKEY;
    }
    else
    {
        DEBUG_LOG("HandlePeerFooterState : Success offset %d req_len %d",peerPartitionCtx->read_offset, req_len); 
    }

    if(getNextUpgradePskeyData(data_ptr, BYTES_TO_WORDS(req_len)) == FALSE)
    {
        DEBUG_LOG("UpgradePeerPartition: HandlePeerFooterState Error");
        return UPGRADE_PEER_ERROR_PARTITION_WRITE_FAILED_DATA;
    }

    peerPartitionCtx->footer_length -= req_len;

    if(peerPartitionCtx->footer_length == 0)
    {
        *last_packet = TRUE;
    }

    return UPGRADE_PEER_SUCCESS;
}

/******************************************************************************
NAME
    read_request - Peer state machine

DESCRIPTION
    Calls state handlers depending on current state.
    All state handlers are setting size of next data request.

RETURNS
    Upgrade peer library error code.
*/
static upgrade_peer_status_t read_request(uint8* data_ptr,
                                            uint32 req_len,
                                            bool *last_packet,
                                            uint32 *sentLength,
                                            uint32 offset)
{
    upgrade_peer_status_t ret = UPGRADE_PEER_SUCCESS;
    *last_packet = FALSE;

    switch(peerPartitionCtx->peerPartitionState)
    {
    case UPGRADE_PEER_PARTITION_DATA_STATE_GENERIC_1ST_PART:
        DEBUG_LOG_INFO("UpgradePeerPartition, Generic1stPartState");
        ret= UpgradePeerPartitionDataHandleGeneric1stPartState(data_ptr,
                                                                req_len);
        *sentLength = req_len;
        break;

    case UPGRADE_PEER_PARTITION_DATA_STATE_HEADER:
        DEBUG_LOG_INFO("UpgradePeerPartition, DataHandleHeaderState");
        ret= UpgradePeerPartitionDataHandleHeaderState(data_ptr,
                                                        req_len);
        *sentLength = req_len;
        break;

    case UPGRADE_PEER_PARTITION_DATA_STATE_DATA_HEADER:
        DEBUG_LOG_INFO("UpgradePeerPartition, DataHandleDataHeaderState");
        ret = UpgradePeerPartitionDataHandleDataHeaderState(data_ptr,
                                                            req_len);
        *sentLength = req_len;
        break;

    case UPGRADE_PEER_PARTITION_DATA_STATE_DATA:
        DEBUG_LOG_VERBOSE("UpgradePeerPartition, DataHandleDataState len %d", req_len);
        ret = UpgradePeerPartitionDataHandleDataState(data_ptr, req_len, sentLength, offset);
        break;

    case UPGRADE_PEER_PARTITION_DATA_STATE_FOOTER:
        DEBUG_LOG_INFO("UpgradePeerPartition, HandlePeerFooterState");
        ret = HandlePeerFooterState(data_ptr, req_len, last_packet);
        *sentLength = req_len;
        break;

    default:
        DEBUG_LOG("UpgradePeerPartition, DefaultState");
        ret = UPGRADE_PEER_ERROR_UNKNOWN_ID;
        break;
    }
    return ret;
}

/******************************************************************************
NAME
    UpgradePeerPartitionMoreData - Peer state machine

DESCRIPTION
    Sends data from corresponding DFU partitions of required length.
*/
upgrade_peer_status_t UpgradePeerPartitionMoreData(uint8 *dataPtr,
                                                 bool *last_packet,
                                                 uint32 mBytesReq,
                                                 uint32 *mBytesSent,
                                                 uint32 offset)
{
    upgrade_peer_status_t ret = UPGRADE_PEER_SUCCESS;

    DEBUG_LOG_VERBOSE("UpgradePeerPartitionMoreData, data_size %u", mBytesReq);

    if (dataPtr == NULL)
        ret = UPGRADE_PEER_ERROR_NO_MEMORY;
    else
        ret = read_request(dataPtr, mBytesReq, last_packet, mBytesSent, offset);

    DEBUG_LOG_VERBOSE("UpgradePeerPartitionMoreData, status %u", ret);

    return ret;
}

/******************************************************************************
NAME
    UpgradePeerParititonInit

DESCRIPTION
    Initializes the upgrade peer partition state machine.
*/
void UpgradePeerParititonInit(void)
{
    DEBUG_LOG("UpgradePeerPartition: Init\n");
    UpgradePeerPartitionDataCtx *parititonCtx;

    parititonCtx =
       (UpgradePeerPartitionDataCtx *) PanicUnlessMalloc(sizeof(*parititonCtx));
    memset(parititonCtx, 0, sizeof(*parititonCtx));

    peerPartitionCtx = parititonCtx;
    peerPartitionCtx->pskey = DFU_HEADER_PSKEY_START;
}

/******************************************************************************
NAME
    UpgradePeerPartitionCtxFree

DESCRIPTION
    Free the peerPartitionCtx context
*/
void UpgradePeerPartitionCtxFree(void)
{
    free(peerPartitionCtx);
}

bool UpgradePeerPartitionIsDataState(void)
{
    return (peerPartitionCtx->peerPartitionState == UPGRADE_PEER_PARTITION_DATA_STATE_DATA);
}

uint32 UpgradePeerPartitionDataLength(void)
{
    return peerPartitionCtx->part_data_length;
}

void UpgradePeerParititonDeInit(void)
{
    DEBUG_LOG("UpgradePeerParititonDeInit");

    if(peerPartitionCtx && peerPartitionCtx->partitionHdl != NULL)
    {
        DEBUG_LOG("UpgradePeerParititonDeInit SourceClose(%d)", peerPartitionCtx->partNum);
        SourceClose(peerPartitionCtx->partitionHdl);
    }

    if(peerPartitionCtx)
    {
        free(peerPartitionCtx);
        peerPartitionCtx = NULL;
    }
}
