/*****************************************************************
Copyright (c) 2011 - 2020 Qualcomm Technologies International, Ltd.
*/
#define DEBUG_LOG_MODULE_NAME gaia_transport
#include <logging.h>

#include "gaia.h"
#include "gaia_transport.h"

#include <panic.h>
#include <pmalloc.h>
#include <stdlib.h>

#ifdef INCLUDE_ACCESSORY

#include "accessory.h"

#define GAIA_IAP2_PROTOCOL_NAME "com.qtil.gaia"
#define GAIA_TRANSPORT_IAP2_DEFAULT_PROTOCOL_VERSION  (3)
#define GAIA_TRANSPORT_IAP2_MAX_PROTOCOL_VERSION      (3)

#define GAIA_TRANSPORT_IAP2_DEFAULT_TX_PKT_SIZE     (48)
#define GAIA_TRANSPORT_IAP2_V4_MAX_TX_PKT_SIZE      (48)    /* Keep this low as packets to send are malloc'ed at the moment */
#define GAIA_TRANSPORT_IAP2_V3_MAX_TX_PKT_SIZE      (48)    /* Keep this low as packets to send are malloc'ed at the moment */

#define GAIA_TRANSPORT_IAP2_DEFAULT_RX_PKT_SIZE     (140)
#define GAIA_TRANSPORT_IAP2_V4_MAX_RX_PKT_SIZE      (140)    /* Keep this low as packets to send are malloc'ed at the moment */
#define GAIA_TRANSPORT_IAP2_V3_MAX_RX_PKT_SIZE      (140)    /* Keep this low as packets to send are malloc'ed at the moment */

#define GAIA_TRANSPORT_IAP2_MAX_RX_PENDING_PKTS     (2)

typedef struct
{
    gaia_transport common;
    iap2_link *link;
    uint8 protocol_id;
    uint16 session_id;
    unsigned max_tx_size:12;
    unsigned protocol_version:4;
    uint8  rx_packets_pending;
    uint16 rx_data_pending;
} gaia_transport_iap2_t;

static void gaiaTransport_Iap2HandleMessage(Task task, MessageId id, Message message);


static bool gaiaTransport_Iap2SendPacket(gaia_transport *t, uint16 vendor_id, uint16 command_id,
                                         uint8 status, uint16 size_payload, const void *payload)
{
    gaia_transport_iap2_t *ti = (gaia_transport_iap2_t *)t;
    const uint16 pkt_length = Gaia_TransportCommonCalcTxPacketLength(size_payload, status);
    uint8 *pkt_buf = malloc(pkt_length);
    if (pkt_buf)
    {
        /* Build packet into buffer */
        Gaia_TransportCommonBuildPacket(ti->protocol_version, pkt_buf, pkt_length, vendor_id, command_id, status, size_payload, payload);

        DEBUG_LOG_VERBOSE("gaiaTransport_Iap2SendPacket, sending, vendor_id %u, command_id %u, pkt_length %u", vendor_id, command_id, pkt_length);
        DEBUG_LOG_DATA_V_VERBOSE(pkt_buf, pkt_length);

        Accessory_SendEapDynamicData(ti->link, ti->session_id, pkt_length, pkt_buf);
    }
    else
    {
        DEBUG_LOG_ERROR("gaiaTransport_Iap2SendPacket, not enough space %u", pkt_length);
        Gaia_TransportErrorInd(t, GAIA_TRANSPORT_INSUFFICENT_BUFFER_SPACE);
    }

    return FALSE;
}


static bool gaiaTransport_Iap2ProcessCommand(gaia_transport *t, const uint16 pkt_size, uint16 vendor_id, uint16 command_id, uint16 size_payload, const uint8 *payload)
{
    PanicNull(t);
    gaia_transport_iap2_t *ti = (gaia_transport_iap2_t *)t;
    ti->rx_data_pending += pkt_size;
    ti->rx_packets_pending += 1;

    /* Create a copy of the payload as the original packet in the IAP2_EA_SESSION_DATA_IND message will be freed
     * once we return from the message handler */
    uint8 *copy_payload = PanicUnlessMalloc(size_payload);
    memcpy(copy_payload, payload, size_payload);
    Gaia_ProcessCommand(t, vendor_id, command_id, size_payload, copy_payload);
    return ti->rx_packets_pending < GAIA_TRANSPORT_IAP2_MAX_RX_PENDING_PKTS;
}

static void gaiaTransport_Iap2HandleSessionData(gaia_transport_iap2_t *ti, const IAP2_EA_SESSION_DATA_IND_T *ind)
{
    const uint8 *payload = ind->payload;
    uint16 size_payload = ind->size_payload;

    DEBUG_LOG_VERBOSE("gaiaTransport_Iap2HandleSessionData, link %p, session_id %u, size_payload %u", ind->link, ind->session_id, size_payload);
    if (size_payload)
        Gaia_TransportCommonReceivePacket(&ti->common, ti->protocol_version, size_payload, payload, gaiaTransport_Iap2ProcessCommand);
}


static void gaiaTransport_Iap2PacketHandled(gaia_transport *t, uint16 size_payload, const void *payload)
{
    PanicNull(t);
    gaia_transport_iap2_t *ti = (gaia_transport_iap2_t *)t;

    /* Decrement number of packets pending */
    PanicFalse(ti->rx_packets_pending > 0);
    ti->rx_packets_pending -= 1;

    DEBUG_LOG_VERBOSE("gaiaTransport_Iap2PacketHandled, size %u, remaining %d", size_payload, ti->rx_packets_pending);

    free((void *)payload);
}



/*! @brief Called from GAIA transport to start IAP2 service
 *
 *  @result Pointer to transport instance
 */
static void gaiaTransport_Iap2StartService(gaia_transport *t)
{
    PanicNull(t);
    gaia_transport_iap2_t *ti = (gaia_transport_iap2_t *)t;

    /* Initialise task */
    t->task.handler = gaiaTransport_Iap2HandleMessage;

    /* Initialise default parameters */
    ti->max_tx_size = GAIA_TRANSPORT_IAP2_DEFAULT_TX_PKT_SIZE;
    ti->protocol_version = GAIA_TRANSPORT_IAP2_DEFAULT_PROTOCOL_VERSION;

    ti->protocol_id = Accessory_RegisterExternalAccessoryProtocol(
                      GAIA_IAP2_PROTOCOL_NAME,
                      iap2_app_match_no_action,
                      (Task)&t->task);

    DEBUG_LOG_INFO("gaiaTransport_Iap2StartService, protocol_id %u", ti->protocol_id);

    /* Send confirm, success dependent on register protocol status */
    Gaia_TransportStartServiceCfm(&ti->common, ti->protocol_id != 0);
}


/*! @brief Called from GAIA transport to stop IAP2 service
 *
 *  @param Pointer to transport instance
 */
static void gaiaTransport_Iap2StopService(gaia_transport *t)
{
    gaia_transport_iap2_t *ti = (gaia_transport_iap2_t *)t;
    PanicNull(ti);
    DEBUG_LOG_INFO("gaiaTransport_Iap2StopService");

    /* TODO: What do we do here?  There's no API to unregister EAP */
    Gaia_TransportStopServiceCfm(&ti->common, FALSE);
}


static void gaiaTransport_Iap2Error(gaia_transport *t)
{
    gaia_transport_iap2_t *ti = (gaia_transport_iap2_t *)t;
    PanicNull(ti);
    DEBUG_LOG_INFO("gaiaTransport_Iap2Error");

    /* TODO: Disconnect */
}


static uint8 gaiaTransport_Iap2Features(gaia_transport *t)
{
    gaia_transport_iap2_t *ti = (gaia_transport_iap2_t *)t;
    PanicNull(ti);
    DEBUG_LOG_INFO("gaiaTransport_Iap2Features");

    return GAIA_TRANSPORT_FEATURE_BREDR;
}


static bool gaiaTransport_Iap2GetInfo(gaia_transport *t, gaia_transport_info_key_t key, uint32 *value)
{
    gaia_transport_iap2_t *ti = (gaia_transport_iap2_t *)t;
    PanicNull(ti);
    DEBUG_LOG_INFO("gaiaTransport_Iap2GetInfo");

    switch (key)
    {
        case GAIA_TRANSPORT_MAX_TX_PAYLOAD:
        case GAIA_TRANSPORT_OPTIMUM_TX_PAYLOAD:
            *value = ti->max_tx_size;
            break;
        case GAIA_TRANSPORT_MAX_RX_PAYLOAD:
            if (ti->protocol_version >= 4)
                *value = GAIA_TRANSPORT_IAP2_V4_MAX_RX_PKT_SIZE;
            else
                *value = GAIA_TRANSPORT_IAP2_V3_MAX_RX_PKT_SIZE;
            break;
        case GAIA_TRANSPORT_OPTIMUM_RX_PAYLOAD:
            if (ti->protocol_version >= 4)
                *value = GAIA_TRANSPORT_IAP2_V4_MAX_RX_PKT_SIZE;
            else
                *value = GAIA_TRANSPORT_IAP2_V3_MAX_RX_PKT_SIZE;
            break;
        case GAIA_TRANSPORT_TX_FLOW_CONTROL:
        case GAIA_TRANSPORT_RX_FLOW_CONTROL:
            *value =1;
            break;

        case GAIA_TRANSPORT_PROTOCOL_VERSION:
            *value = ti->protocol_version;
            break;

        default:
            return FALSE;
    }
    return TRUE;
}


static bool gaiaTransport_Iap2SetParameter(gaia_transport *t, gaia_transport_info_key_t key, uint32 *value)
{
    gaia_transport_iap2_t *ti = (gaia_transport_iap2_t *)t;
    PanicNull(ti);
    DEBUG_LOG_INFO("gaiaTransport_Iap2SetParameter");

    switch (key)
    {
        case GAIA_TRANSPORT_MAX_TX_PAYLOAD:
            if (ti->protocol_version >= 4)
                ti->max_tx_size = MIN(*value, GAIA_TRANSPORT_IAP2_V4_MAX_TX_PKT_SIZE);
            else
                ti->max_tx_size = MIN(*value, GAIA_TRANSPORT_IAP2_V3_MAX_TX_PKT_SIZE);
            *value = ti->max_tx_size;
            break;

        case GAIA_TRANSPORT_PROTOCOL_VERSION:
            if ((*value >= GAIA_TRANSPORT_IAP2_DEFAULT_PROTOCOL_VERSION) && (*value <= GAIA_TRANSPORT_IAP2_MAX_PROTOCOL_VERSION))
                ti->protocol_version = *value;
            *value = ti->protocol_version;
            break;

        default:
            /* Ignore any request to set parameters, just return current value */
            return gaiaTransport_Iap2GetInfo(t, key, value);
    }

    return TRUE;
}



static void gaiaTransport_Iap2HandleSessionStart(gaia_transport_iap2_t *ti, const IAP2_EA_SESSION_START_IND_T *ind)
{
    DEBUG_LOG_INFO("gaiaTransport_Iap2HandleSessionStart, link %u, session_id %u", ind->link, ind->session_id);

    ti->link = ind->link;
    ti->session_id = ind->session_id;

    Gaia_TransportConnectInd(&ti->common, TRUE);
}


static void gaiaTransport_Iap2HandleSessionStop(gaia_transport_iap2_t *ti, const IAP2_EA_SESSION_STOP_IND_T *ind)
{
    DEBUG_LOG_INFO("gaiaTransport_Iap2HandleSessionStop, link %u, session_id %u", ind->link, ind->session_id);

    /* TODO: Only call Gaia_TransportDisconnectInd if ti->rx_pkt_processing == 0, otherwise transport
     * could be destroyed when there are packets still being processed */

    Gaia_TransportDisconnectInd(&ti->common);
}


static void gaiaTransport_Iap2HandleMessage(Task task, MessageId id, Message message)
{
    gaia_transport_iap2_t *ti = (gaia_transport_iap2_t *)task;

    switch (id)
    {
        case IAP2_EA_REGISTER_HANDLER_CFM:
            break;

        case IAP2_EA_SESSION_START_IND:
            gaiaTransport_Iap2HandleSessionStart(ti, (const IAP2_EA_SESSION_START_IND_T *)message);
            break;

        case IAP2_EA_SESSION_STOP_IND:
            gaiaTransport_Iap2HandleSessionStop(ti, (const IAP2_EA_SESSION_STOP_IND_T *)message);
            break;

        case IAP2_EA_SESSION_DATA_IND:
            gaiaTransport_Iap2HandleSessionData(ti, (const IAP2_EA_SESSION_DATA_IND_T *)message);
            break;

        default:
            DEBUG_LOG_ERROR("gaiaTransport_Iap2HandleMessage, unhandled message MESSAGE:0x%04x", id);
            DEBUG_LOG_DATA_ERROR(message, psizeof(message));
            break;
    }
}


void GaiaTransport_Iap2Init(void)
{
    static const gaia_transport_functions_t functions =
    {
        .service_data_size      = sizeof(gaia_transport_iap2_t),
        .start_service          = gaiaTransport_Iap2StartService,
        .stop_service           = gaiaTransport_Iap2StopService,
        .packet_handled         = gaiaTransport_Iap2PacketHandled,
        .send_command_packet    = gaiaTransport_Iap2SendPacket,
        .send_data_packet       = NULL,
        .connect_req            = NULL,
        .disconnect_req         = NULL,
        .set_data_endpoint      = NULL,
        .get_data_endpoint      = NULL,
        .error                  = gaiaTransport_Iap2Error,
        .features               = gaiaTransport_Iap2Features,
        .get_info               = gaiaTransport_Iap2GetInfo,
        .set_parameter          = gaiaTransport_Iap2SetParameter,
    };

    /* Register this transport with GAIA */
    Gaia_TransportRegister(gaia_transport_iap2, &functions);
}

#endif /* INCLUDE_ACCESSORY */
