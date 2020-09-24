/*****************************************************************
Copyright (c) 2011 - 2020 Qualcomm Technologies International, Ltd.
*/
#define DEBUG_LOG_MODULE_NAME gaia_transport
#include <logging.h>

#include "gaia.h"
#include "gaia_transport.h"

#include <source.h>
#include <sink.h>
#include <stream.h>
#include <panic.h>
#include <connection.h>
#include <pmalloc.h>


#define GAIA_TRANSPORT_RFCOMM_DEFAULT_PROTOCOL_VERSION  (3)
#define GAIA_TRANSPORT_RFCOMM_MAX_PROTOCOL_VERSION      (4)

#define GAIA_TRANSPORT_RFCOMM_DEFAULT_TX_PKT_SIZE     (48)
#define GAIA_TRANSPORT_RFCOMM_V4_MAX_TX_PKT_SIZE      (48)    /* Keep this low as packets to send are malloc'ed at the moment */
#define GAIA_TRANSPORT_RFCOMM_V3_MAX_TX_PKT_SIZE      (48)    /* Keep this low as packets to send are malloc'ed at the moment */

#define GAIA_TRANSPORT_RFCOMM_DEFAULT_RX_PKT_SIZE     (48)
#define GAIA_TRANSPORT_RFCOMM_V4_MAX_RX_PKT_SIZE      (1600)
#define GAIA_TRANSPORT_RFCOMM_V3_MAX_RX_PKT_SIZE      (254)
#define GAIA_TRANSPORT_RFCOMM_V4_OPT_RX_PKT_SIZE      (850)
#define GAIA_TRANSPORT_RFCOMM_V3_OPT_RX_PKT_SIZE      (254)

#define GAIA_TRANSPORT_RFCOMM_MAX_RX_PENDING_PKTS     (2)

typedef struct
{
    gaia_transport common;
    uint8 channel;         /*!< RFCOMM channel used by this transport. */
    Sink sink;              /*!< Stream sink of this transport. */
    uint32 service_handle;  /*!< Service record handle. */
    unsigned max_tx_size:12;
    unsigned protocol_version:4;
    uint8  rx_packets_pending;
    uint16 rx_data_pending;
} gaia_transport_rfcomm_t;


static void gaiaTransport_RfcommHandleMessage(Task task, MessageId id, Message message);

static const uint8 gaia_transport_rfcomm_service_record[] =
{
    0x09, 0x00, 0x01,           /*  0  1  2  ServiceClassIDList(0x0001) */
    0x35,   17,                 /*  3  4     DataElSeq 17 bytes */
    0x1C, 0x00, 0x00, 0x11, 0x07, 0xD1, 0x02, 0x11, 0xE1, 0x9B, 0x23, 0x00, 0x02, 0x5B, 0x00, 0xA5, 0xA5,
                                /*  5 .. 21  UUID GAIA (0x00001107-D102-11E1-9B23-00025B00A5A5) */
    0x09, 0x00, 0x04,           /* 22 23 24  ProtocolDescriptorList(0x0004) */
    0x35,   12,                 /* 25 26     DataElSeq 12 bytes */
    0x35,    3,                 /* 27 28     DataElSeq 3 bytes */
    0x19, 0x01, 0x00,           /* 29 30 31  UUID L2CAP(0x0100) */
    0x35,    5,                 /* 32 33     DataElSeq 5 bytes */
    0x19, 0x00, 0x03,           /* 34 35 36  UUID RFCOMM(0x0003) */
    0x08, SPP_DEFAULT_CHANNEL,  /* 37 38     uint8 RFCOMM channel */
#define GAIA_RFCOMM_SR_CH_IDX (38)
    0x09, 0x00, 0x06,           /* 39 40 41  LanguageBaseAttributeIDList(0x0006) */
    0x35,    9,                 /* 42 43     DataElSeq 9 bytes */
    0x09,  'e',  'n',           /* 44 45 46  Language: English */
    0x09, 0x00, 0x6A,           /* 47 48 49  Encoding: UTF-8 */
    0x09, 0x01, 0x00,           /* 50 51 52  ID base: 0x0100 */
    0x09, 0x01, 0x00,           /* 53 54 55  ServiceName 0x0100, base + 0 */
    0x25,   4,                  /* 56 57     String length 4 */
    'G', 'A', 'I', 'A',         /* 58 59 60 61  "GAIA" */
};

static const uint8 gaia_transport_spp_service_record[] =
{
    0x09, 0x00, 0x01,           /*  0  1  2  ServiceClassIDList(0x0001) */
    0x35,    3,                 /*  3  4     DataElSeq 3 bytes */
    0x19, 0x11, 0x01,           /*  5  6  7  UUID SerialPort(0x1101) */
    0x09, 0x00, 0x04,           /*  8  9 10  ProtocolDescriptorList(0x0004) */
    0x35,   12,                 /* 11 12     DataElSeq 12 bytes */
    0x35,    3,                 /* 13 14     DataElSeq 3 bytes */
    0x19, 0x01, 0x00,           /* 15 16 17  UUID L2CAP(0x0100) */
    0x35,    5,                 /* 18 19     DataElSeq 5 bytes */
    0x19, 0x00, 0x03,           /* 20 21 22  UUID RFCOMM(0x0003) */
    0x08, SPP_DEFAULT_CHANNEL,  /* 23 24     uint8 RFCOMM channel */
#define GAIA_SR_CH_IDX (24)
    0x09, 0x00, 0x06,           /* 25 26 27  LanguageBaseAttributeIDList(0x0006) */
    0x35,    9,                 /* 28 29     DataElSeq 9 bytes */
    0x09,  'e',  'n',           /* 30 31 32  Language: English */
    0x09, 0x00, 0x6A,           /* 33 34 35  Encoding: UTF-8 */
    0x09, 0x01, 0x00,           /* 36 37 38  ID base: 0x0100 */
    0x09, 0x01, 0x00,           /* 39 40 41  ServiceName 0x0100, base + 0 */
    0x25,   4,                  /* 42 43     String length 4 */
     'G',  'A',  'I',  'A',     /* 44 45 46 47 "GAIA" */
    0x09, 0x00, 0x09,           /* 48 49 50  BluetoothProfileDescriptorList(0x0009) */
    0x35, 0x08,                 /* 51 52     DataElSeq 8 bytes [List size] */
    0x35, 0x06,                 /* 53 54     DataElSeq 6 bytes [List item] */
    0x19, 0x11, 0x01,           /* 55 56 57  UUID SerialPort(0x1101) */
    0x09, 0x01, 0x02,           /* 58 59 60  SerialPort Version (0x0102) */
};


/*! @brief Send a GAIA packet over RFCOMM
 */
static bool gaiaTransport_RfcommSendPacket(gaia_transport *t, uint16 vendor_id, uint16 command_id,
                                           uint8 status, uint16 size_payload, const void *payload)
{
    gaia_transport_rfcomm_t *tr = (gaia_transport_rfcomm_t *)t;
    uint16 trans_info = tr->channel;
    const uint16 pkt_length = Gaia_TransportCommonCalcTxPacketLength(size_payload, status);
    const uint16 trans_space = TransportMgrGetAvailableSpace(transport_mgr_type_rfcomm, trans_info);
    if (trans_space >= pkt_length)
    {
        uint8 *pkt_buf = TransportMgrClaimData(transport_mgr_type_rfcomm, trans_info, pkt_length);
        if (pkt_buf)
        {
            /* Build packet into buffer */
            Gaia_TransportCommonBuildPacket(tr->protocol_version, pkt_buf, pkt_length, vendor_id, command_id, status, size_payload, payload);

            /* Send packet */
            if (TransportMgrDataSend(transport_mgr_type_rfcomm, trans_info, pkt_length))
            {
                DEBUG_LOG_VERBOSE("gaiaTransportRfcommSendPacket, sending, vendor_id %u, command_id %u, pkt_length %u", vendor_id, command_id, pkt_length);
                DEBUG_LOG_DATA_V_VERBOSE(pkt_buf, pkt_length);
                return TRUE;
            }
        }
    }
    else
    {
        DEBUG_LOG_ERROR("gaiaTransportRfcommSendPacket, not enough space %u", pkt_length);
        Gaia_TransportErrorInd(t, GAIA_TRANSPORT_INSUFFICENT_BUFFER_SPACE);
    }

    return FALSE;
}


#if 0
static uint8 *gaiaTransport_RfcommCreatePacket(gaia_transport *t, uint16 vendor_id, uint16 command_id,
                                               uint8 status, uint16 size_payload)
{
    gaia_transport_rfcomm_t *tr = (gaia_transport_rfcomm_t *)t;
    uint16 trans_info = tr->channel;
    const uint16 pkt_length = Gaia_TransportCommonCalcPacketLength(size_payload, tr->common.flags, status);
    const uint16 trans_space = TransportMgrGetAvailableSpace(transport_mgr_type_rfcomm, trans_info);
    if (trans_space >= pkt_length)
    {
        uint8 *pkt_buf = TransportMgrClaimData(transport_mgr_type_rfcomm, trans_info, pkt_length);
        if (pkt_buf)
        {
            /* Build packet into buffer */
            Gaia_TransportCommonBuildPacketHeader(pkt_buf, pkt_length, vendor_id, command_id, status, tr->common.flags, size_payload);

            DEBUG_LOG_VERBOSE("gaiaTransport_RfcommCreatePacket, claimed space, vendor_id %u, command_id %u, pkt_length %u", vendor_id, command_id, pkt_length);

            return pkt_buf;
        }
    }

    DEBUG_LOG_ERROR("gaiaTransport_RfcommCreatePacket, failed to claimed space, vendor_id %u, command_id %u, pkt_length %u", vendor_id, command_id, pkt_length);
    return NULL;
}
#endif


static bool gaiaTransport_RfcommProcessCommand(gaia_transport *t, uint16 pkt_size, uint16 vendor_id, uint16 command_id, uint16 size_payload, const uint8 *payload)
{
    gaia_transport_rfcomm_t *tr = (gaia_transport_rfcomm_t *)t;
    tr->rx_data_pending += pkt_size;
    tr->rx_packets_pending += 1;
    Gaia_ProcessCommand(t, vendor_id, command_id, size_payload, payload);
    return tr->rx_packets_pending < GAIA_TRANSPORT_RFCOMM_MAX_RX_PENDING_PKTS;
}


/*! @brief Received GAIA packet over RFCOMM
 */
static void gaiaTransport_RfcommReceivePacket(gaia_transport_rfcomm_t *tr)
{
    if (tr->rx_data_pending == 0)
    {
        const uint16 trans_info = tr->channel;
        const uint16 data_length = TransportMgrGetAvailableDataSize(transport_mgr_type_rfcomm, trans_info);
        const uint8 *data_buf = (const uint8 *)TransportMgrReadData(transport_mgr_type_rfcomm, trans_info);

        if (data_length)
        {
            DEBUG_LOG_VERBOSE("gaiaTransportRfcommReceivePacket, channel %u, data_length %u", trans_info, data_length);
            Gaia_TransportCommonReceivePacket(&tr->common, tr->protocol_version, data_length, data_buf, gaiaTransport_RfcommProcessCommand);
        }
        else
            DEBUG_LOG_V_VERBOSE("gaiaTransportRfcommReceivePacket, channel %u, data_length %u", trans_info, data_length);
    }
    else
        DEBUG_LOG_WARN("gaiaTransportRfcommReceivePacket, receive data being processed");

}


/*! @brief Received GAIA packet has now been handled by upper layers
 */
static void gaiaTransport_RfcommPacketHandled(gaia_transport *t, uint16 size_payload, const void *payload)
{
    gaia_transport_rfcomm_t *tr = (gaia_transport_rfcomm_t *)t;
    PanicNull(tr);
    UNUSED(payload);

    /* Decrement number of packets pending */
    PanicFalse(tr->rx_packets_pending > 0);
    tr->rx_packets_pending -= 1;

    DEBUG_LOG_VERBOSE("gaiaTransport_RfcommPacketHandled, size %u, remaining %d", size_payload, tr->rx_packets_pending);

    /* Wait until all packets have been processed before removing from buffer as we can't be
     * certain that packets will be handled in order they are received */
    if (tr->rx_packets_pending == 0)
    {
        DEBUG_LOG_VERBOSE("gaiaTransport_RfcommPacketHandled, all data processed");

        /* Inform transport manager we've consumed the data up to end of packet */
        TransportMgrDataConsumed(transport_mgr_type_rfcomm, tr->channel, tr->rx_data_pending);
        tr->rx_data_pending = 0;

        /* Check if more data has arrived since we started processing */
        gaiaTransport_RfcommReceivePacket(tr);
    }
}


/*! @brief Utility function to construct a GAIA SDP record
 *
 *  @param record The constant record to use as a base
 *  @param size_record The size of the base record
 *  @param channel_offset The channel offset in the base record
 *  @param channel The channel to advertise in the SDP record
 */
static const uint8 *gaiaTransport_RfcommAllocateServiceRecord(const uint8 *record, uint16 size_record, uint8 channel_offset, uint8 channel)
{
    uint8 *sr;

    /* TODO: Parse service record to find location of RFCOMM server channel number, rather
     * than using pre-defined offset
     * see SdpParseGetMultipleRfcommServerChannels() & SdpParseInsertRfcommServerChannel()
     */

    /* If channel in record matches, nothing needs to be done, use const version */
    if (channel == record[channel_offset])
        return record;

    /* Allocate a dynamic record */
    sr = PanicUnlessMalloc(size_record);

    /* Copy in the record and set the channel */
    memcpy(sr, record, size_record);
    sr[channel_offset] = channel;
    return (const uint8 *)sr;
}


/*! @brief Register SDP record for transport
 *
 *  @param t Pointer to transport
 *  @param channel The channel to advertise in the SDP record
 */
static void gaiaTransport_RfcommSdpRegister(gaia_transport_rfcomm_t *tr)
{
    const uint8 *sr;
    uint16 size_of_rec;

    if (tr->common.type == gaia_transport_rfcomm)
    {
        /* Default to use const record */
        size_of_rec = sizeof(gaia_transport_rfcomm_service_record);
        sr = gaiaTransport_RfcommAllocateServiceRecord(gaia_transport_rfcomm_service_record, size_of_rec, GAIA_RFCOMM_SR_CH_IDX, tr->channel);
    }
    else
    {
        /* Default to use const record */
        size_of_rec = sizeof(gaia_transport_spp_service_record);
        sr = gaiaTransport_RfcommAllocateServiceRecord(gaia_transport_spp_service_record, size_of_rec, GAIA_SR_CH_IDX, tr->channel);
    }

    DEBUG_LOG_INFO("gaiaTransportRfcommSdpRegister, channel %u", tr->channel);

    /* Register the SDP record */
    ConnectionRegisterServiceRecord(&tr->common.task, size_of_rec, sr);
}


/*! @brief Called from GAIA transport to start RFCOMM service
 *
 *  @result Pointer to transport instance
 */
static void gaiaTransport_RfcommStartService(gaia_transport *t)
{
    PanicNull(t);
    gaia_transport_rfcomm_t *tr = (gaia_transport_rfcomm_t *)t;
    DEBUG_LOG_INFO("gaiaTransportRfcommStartService");

    /* Initialise task */
    t->task.handler = gaiaTransport_RfcommHandleMessage;

    /* Initialise default parameters */
    tr->max_tx_size = GAIA_TRANSPORT_RFCOMM_DEFAULT_TX_PKT_SIZE;
    tr->protocol_version = GAIA_TRANSPORT_RFCOMM_DEFAULT_PROTOCOL_VERSION;

    /* Register with transport manager */
    transport_mgr_link_cfg_t link_cfg;
    link_cfg.type = transport_mgr_type_rfcomm;
    link_cfg.trans_info.non_gatt_trans.trans_link_id = SPP_DEFAULT_CHANNEL;
    TransportMgrRegisterTransport(&t->task, &link_cfg);

    /* Wait for TRANSPORT_MGR_REGISTER_CFM before informing GAIA */
}


/*! @brief Called from GAIA transport to stop RFCOMM service
 *
 *  @param Pointer to transport instance
 */
static void gaiaTransport_RfcommStopService(gaia_transport *t)
{
    PanicNull(t);
    gaia_transport_rfcomm_t *tr = (gaia_transport_rfcomm_t *)t;
    DEBUG_LOG_INFO("gaiaTransportRfcommStopService");

    /* Unregister with transport manager */
    TransportMgrDeRegisterTransport(&tr->common.task, transport_mgr_type_rfcomm, tr->channel);
}


static void gaiaTransport_RfcommDisconnectReq(gaia_transport *t)
{
    gaia_transport_rfcomm_t *tr = (gaia_transport_rfcomm_t *)t;
    PanicNull(tr);
    DEBUG_LOG_INFO("gaiaTransportRfcommDisconnectReq, sink %04x", tr->sink);

    /* Initiate disconnect */
    TransportMgrDisconnect(transport_mgr_type_rfcomm, tr->sink);
}


static void gaiaTransport_RfcommError(gaia_transport *t)
{
    gaia_transport_rfcomm_t *tr = (gaia_transport_rfcomm_t *)t;
    PanicNull(tr);
    DEBUG_LOG_INFO("gaiaTransportRfcommError, sink %04x", tr->sink);

    /* Initiate disconnect */
    TransportMgrDisconnect(transport_mgr_type_rfcomm, tr->sink);
}


static uint8 gaiaTransport_RfcommFeatures(gaia_transport *t)
{
    gaia_transport_rfcomm_t *tr = (gaia_transport_rfcomm_t *)t;
    PanicNull(tr);
    DEBUG_LOG_INFO("gaiaTransportRfcommFeatures");

    /* RFCOMM is BR/EDR */
    return GAIA_TRANSPORT_FEATURE_BREDR;
}


static bool gaiaTransport_RfcommGetInfo(gaia_transport *t, gaia_transport_info_key_t key, uint32 *value)
{
    gaia_transport_rfcomm_t *tr = (gaia_transport_rfcomm_t *)t;
    PanicNull(tr);
    DEBUG_LOG_INFO("gaiaTransport_RfcommGetInfo");

    switch (key)
    {
        case GAIA_TRANSPORT_MAX_TX_PAYLOAD:
        case GAIA_TRANSPORT_OPTIMUM_TX_PAYLOAD:
            *value = tr->max_tx_size;
            break;
        case GAIA_TRANSPORT_MAX_RX_PAYLOAD:
            if (tr->protocol_version >= 4)
                *value = GAIA_TRANSPORT_RFCOMM_V4_MAX_RX_PKT_SIZE;
            else
                *value = GAIA_TRANSPORT_RFCOMM_V3_MAX_RX_PKT_SIZE;
            break;
        case GAIA_TRANSPORT_OPTIMUM_RX_PAYLOAD:
            if (tr->protocol_version >= 4)
                *value = GAIA_TRANSPORT_RFCOMM_V4_OPT_RX_PKT_SIZE;
            else
                *value = GAIA_TRANSPORT_RFCOMM_V3_OPT_RX_PKT_SIZE;
            break;
        case GAIA_TRANSPORT_TX_FLOW_CONTROL:
        case GAIA_TRANSPORT_RX_FLOW_CONTROL:
            *value =1;
            break;

        case GAIA_TRANSPORT_PROTOCOL_VERSION:
            *value = tr->protocol_version;
            break;

        default:
            return FALSE;
    }
    return TRUE;
}


static bool gaiaTransport_RfcommSetParameter(gaia_transport *t, gaia_transport_info_key_t key, uint32 *value)
{
    gaia_transport_rfcomm_t *tr = (gaia_transport_rfcomm_t *)t;
    PanicNull(tr);
    DEBUG_LOG_INFO("gaiaTransport_RfcommSetParameter");

    switch (key)
    {
        case GAIA_TRANSPORT_MAX_TX_PAYLOAD:
            if (tr->protocol_version >= 4)
                tr->max_tx_size = MIN(*value, GAIA_TRANSPORT_RFCOMM_V4_MAX_TX_PKT_SIZE);
            else
                tr->max_tx_size = MIN(*value, GAIA_TRANSPORT_RFCOMM_V3_MAX_TX_PKT_SIZE);
            *value = tr->max_tx_size;
            break;

        case GAIA_TRANSPORT_PROTOCOL_VERSION:
            if ((*value >= GAIA_TRANSPORT_RFCOMM_DEFAULT_PROTOCOL_VERSION) && (*value <= GAIA_TRANSPORT_RFCOMM_MAX_PROTOCOL_VERSION))
                tr->protocol_version = *value;
            *value = tr->protocol_version;
            break;

        default:
            /* Ignore any request to set parameters, just return current value */
            return gaiaTransport_RfcommGetInfo(t, key, value);
    }

    return TRUE;
}


static void gaiaTransport_RfcommHandleTransportMgrRegisterCfm(gaia_transport_rfcomm_t *tr, const TRANSPORT_MGR_REGISTER_CFM_T *cfm)
{
    DEBUG_LOG_INFO("gaiaRfcommHandleTransportMgrRegisterCfm, channel %u, status %u",
                   cfm->link_cfg.trans_info.non_gatt_trans.trans_link_id, cfm->status);

    if (cfm->status)
    {
        tr->channel = cfm->link_cfg.trans_info.non_gatt_trans.trans_link_id;
        gaiaTransport_RfcommSdpRegister(tr);
    }
    else
        Gaia_TransportStartServiceCfm(&tr->common, FALSE);
}


static void gaiaTransport_RfcommHandleTransportMgrDeregisterCfm(gaia_transport_rfcomm_t *tr, const TRANSPORT_MGR_DEREGISTER_CFM_T *cfm)
{
    DEBUG_LOG_INFO("gaiaRfcommHandleTransportMgrDeregisterCfm, channel %u, status %u",
                   cfm->trans_link_id, cfm->status);

    /* Unregister SDP record */
    if (tr->service_handle)
    {
        ConnectionUnregisterServiceRecord(&tr->common.task, tr->service_handle);
        tr->service_handle = 0;
    }
    else
        Panic();
}


static void gaiaTransport_RfcommHandleTransportMgrLinkCreatedCfm(gaia_transport_rfcomm_t *tr, const TRANSPORT_MGR_LINK_CREATED_CFM_T *cfm)
{
    DEBUG_LOG_INFO("gaiaTransportRfcommHandleTransportMgrLinkCreatedCfm, status %u", cfm->status);

    if (cfm->status)
    {
        tr->sink = cfm->trans_sink;

        /* Unregister SDP record now that we're connected */
        if (tr->service_handle)
        {
            ConnectionUnregisterServiceRecord(&tr->common.task, tr->service_handle);
            tr->service_handle = 0;
        }
    }

    Gaia_TransportConnectInd(&tr->common, cfm->status);

    /* Check if any data has already arrived */
    gaiaTransport_RfcommReceivePacket(tr);
}


static void gaiaTransport_RfcommHandleTransportMgrLinkDisconnectedCfm(gaia_transport_rfcomm_t *tr, const TRANSPORT_MGR_LINK_DISCONNECTED_CFM_T *cfm)
{
    DEBUG_LOG_INFO("gaiaTransportRfcommHandleTransportMgrLinkDisconnectedCfm, status %u", cfm->status);

    if (cfm->status)
    {
        StreamConnectDispose(StreamSourceFromSink(tr->sink));

        /* Re-register SDP record */
        gaiaTransport_RfcommSdpRegister(tr);

        Gaia_TransportDisconnectInd(&tr->common);
    }
}


static void gaiaTransport_RfcommHandleClSdpRegisterCfm(gaia_transport_rfcomm_t *tr, CL_SDP_REGISTER_CFM_T *cfm)
{
    DEBUG_LOG_INFO("gaiaTransportRfcommHandleClSdpRegisterCfm, status %u, state %u", cfm->status, tr->common.state);

    if (cfm->status == sds_status_success)
    {
        /* Send CFM if service is starting */
        if (tr->common.state == GAIA_TRANSPORT_STARTING)
            Gaia_TransportStartServiceCfm(&tr->common, TRUE);

        tr->service_handle = cfm->service_handle;
    }
    else if (tr->common.state == GAIA_TRANSPORT_STARTING)
        Gaia_TransportStartServiceCfm(&tr->common, FALSE);
}


static void gaiaTransport_RfcommHandleClSdpUnregisterCfm(gaia_transport_rfcomm_t *tr, CL_SDP_UNREGISTER_CFM_T *cfm)
{
    DEBUG_LOG_INFO("gaiaTransportRfcommHandleClSdpUnregisterCfm, status %u, state %u", cfm->status, tr->common.state);

    if (tr->common.state == GAIA_TRANSPORT_STOPPING && cfm->status == sds_status_success)
    {
        /* Complete unregistered (both SDP and transport manager, so tell GAIA we're done */
        Gaia_TransportStopServiceCfm(&tr->common, TRUE);
    }
}


static void gaiaTransport_RfcommHandleMessage(Task task, MessageId id, Message message)
{
    gaia_transport_rfcomm_t *tr = (gaia_transport_rfcomm_t *)task;

    switch (id)
    {
        case TRANSPORT_MGR_MORE_DATA:
            gaiaTransport_RfcommReceivePacket(tr);
            break;

        case TRANSPORT_MGR_MORE_SPACE:
            break;

        case TRANSPORT_MGR_REGISTER_CFM:
            gaiaTransport_RfcommHandleTransportMgrRegisterCfm(tr, (TRANSPORT_MGR_REGISTER_CFM_T *)message);
            break;

        case TRANSPORT_MGR_DEREGISTER_CFM:
            gaiaTransport_RfcommHandleTransportMgrDeregisterCfm(tr, (TRANSPORT_MGR_DEREGISTER_CFM_T *)message);
            break;

        case TRANSPORT_MGR_LINK_CREATED_CFM:
            gaiaTransport_RfcommHandleTransportMgrLinkCreatedCfm(tr, (TRANSPORT_MGR_LINK_CREATED_CFM_T *)message);
            break;

        case TRANSPORT_MGR_LINK_DISCONNECTED_CFM:
            gaiaTransport_RfcommHandleTransportMgrLinkDisconnectedCfm(tr, (TRANSPORT_MGR_LINK_DISCONNECTED_CFM_T *)message);
            break;

        case CL_SDP_REGISTER_CFM:
            gaiaTransport_RfcommHandleClSdpRegisterCfm(tr, (CL_SDP_REGISTER_CFM_T *)message);
            break;

        case CL_SDP_UNREGISTER_CFM:
            gaiaTransport_RfcommHandleClSdpUnregisterCfm(tr, (CL_SDP_UNREGISTER_CFM_T *)message);
            break;

        default:
            DEBUG_LOG_ERROR("gaiaTransportRfcommHandleMessage, unhandled message MESSAGE:0x%04x", id);
            DEBUG_LOG_DATA_ERROR(message, psizeof(message));
            break;
    }
}


void GaiaTransport_RfcommInit(void)
{
    static const gaia_transport_functions_t functions =
    {
        .service_data_size      = sizeof(gaia_transport_rfcomm_t),
        .start_service          = gaiaTransport_RfcommStartService,
        .stop_service           = gaiaTransport_RfcommStopService,
        .packet_handled         = gaiaTransport_RfcommPacketHandled,
        .send_command_packet    = gaiaTransport_RfcommSendPacket,
        .send_data_packet       = NULL,
        .connect_req            = NULL,
        .disconnect_req         = gaiaTransport_RfcommDisconnectReq,
        .set_data_endpoint      = NULL,
        .get_data_endpoint      = NULL,
        .error                  = gaiaTransport_RfcommError,
        .features               = gaiaTransport_RfcommFeatures,
        .get_info               = gaiaTransport_RfcommGetInfo,
        .set_parameter          = gaiaTransport_RfcommSetParameter,
    };

    /* Register this transport with GAIA */
    /*Gaia_TransportRegister(gaia_transport_rfcomm, &functions);*/
    Gaia_TransportRegister(gaia_transport_spp, &functions);
}


