/*****************************************************************
Copyright (c) 2011 - 2020 Qualcomm Technologies International, Ltd.
*/
#define DEBUG_LOG_MODULE_NAME gaia_transport
#include <logging.h>

#include "gaia.h"
#include "gaia_transport.h"
#include "gaia_framework_internal.h"
#include "gatt_handler_db_if.h"
#include "gatt_connect.h"

#include <pmalloc.h>
#include <source.h>
#include <sink.h>
#include <stream.h>
#include <panic.h>
#include <gatt.h>
#include <gatt_manager.h>
#include <rwcp_server.h>

#define GAIA_GATT_RESPONSE_BUFFER_SIZE  (20)

#define GAIA_TRANSPORT_GATT_DEFAULT_PROTOCOL_VERSION  (3)

/* Transport specific data */
typedef struct
{
    gaia_transport common;
    uint16 cid;

    unsigned response_notifications_enabled:1;    /*!< response notifications enabled on response endpoint. */
    unsigned response_indications_enabled:1;      /*!< response indications enabled on response endpoint. */
    unsigned data_notifications_enabled:1;        /*!< response notifications enabled on data endpoint. */
    unsigned data_indications_enabled:1;          /*!< response indications enabled on data endpoint. */

    unsigned size_response:5;
    uint8 response[GAIA_GATT_RESPONSE_BUFFER_SIZE];

    Source att_stream_source;
    Sink   att_stream_sink;

    uint16 handle_data_endpoint;                    /*!< Data endpoint handle for ATT stream */
    uint16 handle_response_endpoint;                /*!< Response endpoint handle for ATT stream */

    gaia_data_endpoint_mode_t data_endpoint_mode;   /*!< Current mode of data endpoint */

} gaia_transport_gatt_t;


/*
    Over the air packet format:
    0 bytes  1        2        3        4               len+5
    +--------+--------+--------+--------+ +--------+--/ /---+
    |   VENDOR ID     |   COMMAND ID    | | PAYLOAD   ...   |
    +--------+--------+--------+--------+ +--------+--/ /---+
*/
#define GAIA_GATT_OFFS_VENDOR_ID        (0)
#define GAIA_GATT_OFFS_COMMAND_ID       (2)
#define GAIA_GATT_OFFS_PAYLOAD          (4)

#define GAIA_GATT_HEADER_SIZE           (GAIA_GATT_OFFS_PAYLOAD - GAIA_GATT_OFFS_VENDOR_ID)
#define GAIA_GATT_RESPONSE_STATUS_SIZE  (1)

#define GAIA_HANDLE_SIZE   (2)


static void gaiaTransport_GattHandleMessage(Task task, MessageId id, Message message);


/*************************************************************************
NAME
    gaiaTransportGattRes

DESCRIPTION
    Copy a response to the transport buffer and notify the central
*/
static void gaiaTransport_GattRes(gaia_transport_gatt_t *tg, uint16 size_response, uint8 *response, uint8 handle)
{
    PanicNull(tg);

    /* Check if data endpoint response notifcations enabled and we're sending on the data endpoint */
    if (tg->data_notifications_enabled && (handle == Gaia_GetGattDataEndpoint()))
    {
        DEBUG_LOG_VERBOSE("gaiaTransportGattRes, data_endpoint handle");
        GattManagerRemoteClientNotify(&tg->common.task, tg->cid, handle, size_response, response);
    }
    else
    {
        PanicFalse(size_response >= GAIA_GATT_HEADER_SIZE);

        if (tg->response_notifications_enabled)
        {
            DEBUG_LOG_VERBOSE("gaiaTransportGattRes, reponse notification %02X %02X %02X %02X", response[0], response[1], response[2], response[3]);
            GattManagerRemoteClientNotify(&tg->common.task, tg->cid, handle, size_response, response);
        }

        if (tg->response_indications_enabled)
        {
            DEBUG_LOG_VERBOSE("gaiaTransportGattRes, reponse indication %02X %02X %02X %02X", response[0], response[1], response[2], response[3]);
            GattManagerRemoteClientIndicate(&tg->common.task, tg->cid, handle, size_response, response);
        }

        /* TODO: What does GAIA_ACK_MASK_H mean, is command an ACK?, or command requires an ACK?*/
        if ((response[GAIA_GATT_OFFS_COMMAND_ID] & GAIA_ACK_MASK_H) || (GaiaGetAppVersion() != gaia_app_version_2))
        {
            /*  If not enough space to store the complete response just store vendor + command + status only */
            if (size_response > sizeof(tg->response))
                size_response = GAIA_GATT_HEADER_SIZE + GAIA_GATT_RESPONSE_STATUS_SIZE;

            tg->size_response = size_response;
            memcpy(tg->response, response, size_response);
        }
    }
}


/*************************************************************************
NAME
    calculate_response_size

DESCRIPTION
    Returns payload response size based on the unpack parameter.
 */
static uint16 gaiaTransport_GattCalcPacketLength(uint8 size_payload, uint8 status)
{
    return (GAIA_GATT_HEADER_SIZE + size_payload) + (status == GAIA_STATUS_NONE ? 0 : 1);
}


static bool gaiaTransport_GattCreateAttStream(gaia_transport_gatt_t *tg, uint16 cid)
{
    /* Obtain source from Enhanced ATT streams */
    tg->att_stream_source = StreamAttServerSource(cid);
    tg->att_stream_sink = StreamAttServerSink(cid);

    if (tg->att_stream_source && tg->att_stream_sink)
    {
       DEBUG_LOG_INFO("gaiaTransport_GattCreateAttStream, ATT stream %04x,%04x, CID %u", tg->att_stream_source, tg->att_stream_sink, cid);

        tg->handle_data_endpoint = PanicZero(GattManagerGetServerDatabaseHandle(&tg->common.task, Gaia_GetGattDataEndpoint()));
        tg->handle_response_endpoint = PanicZero(GattManagerGetServerDatabaseHandle(&tg->common.task, Gaia_GetGattResponseEndpoint()));

        PanicFalse(StreamAttAddHandle(tg->att_stream_source, tg->handle_data_endpoint));

        SourceConfigure(tg->att_stream_source, VM_SOURCE_MESSAGES, VM_MESSAGES_SOME);
        MessageStreamTaskFromSink(StreamSinkFromSource(tg->att_stream_source), &tg->common.task);

        SinkConfigure(tg->att_stream_sink, VM_SINK_MESSAGES, VM_MESSAGES_NONE);
        return TRUE;
    }
    else
    {
        DEBUG_LOG_ERROR("gaiaTransport_GattCreateAttStream, failed to create ATT stream %04x,%04x, CID %u", tg->att_stream_source, tg->att_stream_sink, cid);
        return FALSE;
    }
}


static void gaiaTransport_GattDestroyAttStream(gaia_transport_gatt_t *tg)
{
    StreamAttSourceRemoveAllHandles(tg->cid);
    tg->att_stream_sink = 0;
}



/*************************************************************************
NAME
    gaiaTransportGattSend

DESCRIPTION
    Build and send a short format GAIA packet
    If unpack is true then the payload is treated as uint16[]

    0 bytes  1        2        3        4               len+5
    +--------+--------+--------+--------+ +--------+--/ /---+
    |   VENDOR ID     |   COMMAND ID    | | PAYLOAD   ...   |
    +--------+--------+--------+--------+ +--------+--/ /---+
 */

static bool gaiaTransport_GattSendPacketWithHandle(gaia_transport_gatt_t *tg, uint16 vendor_id, uint16 command_id,
                                                   uint8 status, uint8 size_payload, const void *payload, uint16 handle)
{
    const uint16 pkt_length = gaiaTransport_GattCalcPacketLength(size_payload, status);
    uint8 *pkt_buf = malloc(pkt_length);
    if (pkt_buf)
    {
        uint8 *pkt_ptr = pkt_buf;

        /* Write header */
        *pkt_ptr++ = HIGH(vendor_id);
        *pkt_ptr++ = LOW(vendor_id);
        *pkt_ptr++ = HIGH(command_id);
        *pkt_ptr++ = LOW(command_id);

        /* Write status byte */
        if (status != GAIA_STATUS_NONE)
            *pkt_ptr++ = status;

        /* Copy payload */
        memcpy(pkt_ptr, payload, size_payload);
        pkt_ptr += size_payload;

        /* Send response */
        gaiaTransport_GattRes(tg, pkt_length, pkt_buf, handle);
        DEBUG_LOG_VERBOSE("gaiaTransportGattSendPacketWithHandle, sending, handle %u, vendor_id %u, command_id %u, pkt_length %u", handle, vendor_id, command_id, pkt_length);
        DEBUG_LOG_DATA_V_VERBOSE(pkt_buf, pkt_length);

        /* Free buffer */
        free(pkt_buf);
        return TRUE;
    }
    else
    {
        DEBUG_LOG_ERROR("gaiaTransportGattSendPacket, not enough space %u", pkt_length);
        Gaia_TransportErrorInd(&tg->common, GAIA_TRANSPORT_INSUFFICENT_BUFFER_SPACE);
        return FALSE;
    }
}

static bool gaiaTransport_GattSendPacketWithStream(gaia_transport_gatt_t *tg, uint16 vendor_id, uint16 command_id,
                                                   uint8 status, uint8 size_payload, const void *payload, uint16 handle)
{
    const uint16 pkt_length = gaiaTransport_GattCalcPacketLength(size_payload, status) + GAIA_HANDLE_SIZE;
    if (SinkSlack(tg->att_stream_sink) >= pkt_length)
    {
        uint8 *pkt_buf = SinkMap(tg->att_stream_sink);
        uint16 claimed = SinkClaim(tg->att_stream_sink, pkt_length);
        if (pkt_buf && claimed != 0xFFFF)
        {
            uint8 *pkt_ptr = pkt_buf;

            /* Prepending the response endpoint handle to which the data is sent over the air */
            *pkt_ptr++ = LOW(handle);
            *pkt_ptr++ = HIGH(handle);

            /* Write header */
            *pkt_ptr++ = HIGH(vendor_id);
            *pkt_ptr++ = LOW(vendor_id);
            *pkt_ptr++ = HIGH(command_id);
            *pkt_ptr++ = LOW(command_id);

            /* Write status byte */
            if (status != GAIA_STATUS_NONE)
                *pkt_ptr++ = status;

            /* Copy payload */
            memcpy(pkt_ptr, payload, size_payload);
            pkt_ptr += size_payload;

            DEBUG_LOG_VERBOSE("gaiaTransport_GattSendPacketWithStream, sending, handle %u, vendor_id %u, command_id %u, pkt_length %u", handle, vendor_id, command_id, pkt_length);
            DEBUG_LOG_DATA_V_VERBOSE(pkt_buf, pkt_length);
            return SinkFlush(tg->att_stream_sink, pkt_length);
        }
        else
        {
            DEBUG_LOG_ERROR("gaiaTransport_GattSendPacketWithStream, failed to claim space %u", pkt_length);
            Gaia_TransportErrorInd(&tg->common, GAIA_TRANSPORT_INSUFFICENT_BUFFER_SPACE);
            return FALSE;
        }
    }
    else
    {
        DEBUG_LOG_ERROR("gaiaTransport_GattSendPacketWithStream, not enough space %u", pkt_length);
        Gaia_TransportErrorInd(&tg->common, GAIA_TRANSPORT_INSUFFICENT_BUFFER_SPACE);
        return FALSE;
    }
}



static bool gaiaTransport_GattSendPacket(gaia_transport *t, uint16 vendor_id, uint16 command_id,
                                         uint8 status, uint16 size_payload, const void *payload)
{
    gaia_transport_gatt_t *tg = (gaia_transport_gatt_t *)t;
    if (tg->response_notifications_enabled)
        return gaiaTransport_GattSendPacketWithStream(tg, vendor_id, command_id, status, size_payload, payload, tg->handle_response_endpoint);
    else
        return gaiaTransport_GattSendPacketWithHandle(tg, vendor_id, command_id, status, size_payload, payload, Gaia_GetGattResponseEndpoint());
}


static bool gaiaTransport_GattSendDataPacket(gaia_transport *t, uint16 vendor_id, uint16 command_id,
                                             uint8 status, uint16 size_payload, const void *payload)
{
    gaia_transport_gatt_t *tg = (gaia_transport_gatt_t *)t;
    switch (tg->data_endpoint_mode)
    {
        case GAIA_DATA_ENDPOINT_MODE_RWCP:
        case GAIA_DATA_ENDPOINT_MODE_NONE:
            if (tg->response_notifications_enabled)
                return gaiaTransport_GattSendPacketWithStream(tg, vendor_id, command_id, status, size_payload, payload, tg->handle_response_endpoint);
            else
                return gaiaTransport_GattSendPacketWithHandle(tg, vendor_id, command_id, status, size_payload, payload, Gaia_GetGattResponseEndpoint());

        default:
            DEBUG_LOG_ERROR("gaiaTransportGattSendDataPacket, unsupported data_endpoint_mode %u", tg->data_endpoint_mode);

    }

    return FALSE;
}


static void gaiaTransport_GattReceivePacket(gaia_transport_gatt_t *tg, uint16 data_length, const uint8 *data_buf, gaia_data_endpoint_mode_t mode)
{
    if (data_length >= GAIA_GATT_HEADER_SIZE)
    {        
        const uint16 vendor_id = W16(data_buf + GAIA_GATT_OFFS_VENDOR_ID);
        const uint16 command_id = W16(data_buf + GAIA_GATT_OFFS_COMMAND_ID);
        const uint16 payload_size = data_length - GAIA_GATT_HEADER_SIZE;

        /* Copy payload and llocate extra byte at end to store data endpoint mode */
        uint8 *payload = PanicUnlessMalloc(payload_size + 1);
        memcpy(payload, data_buf + GAIA_GATT_OFFS_PAYLOAD, payload_size);
        payload[payload_size] = mode;

        DEBUG_LOG_VERBOSE("gaiaTransportGattReceivePacket, vendor_id 0x%02x, command_id 0x%04x, payload_size %u, payload %p",
                          vendor_id, command_id, payload_size, payload);
        DEBUG_LOG_DATA_V_VERBOSE(payload, payload_size);

        /* Prepare response, which is copy of header with additional status byte */
        tg->size_response = GAIA_GATT_HEADER_SIZE + GAIA_GATT_RESPONSE_STATUS_SIZE;;
        memcpy(tg->response, data_buf, GAIA_GATT_HEADER_SIZE);
        tg->response[GAIA_GATT_OFFS_COMMAND_ID] |= GAIA_ACK_MASK_H;
        tg->response[GAIA_GATT_OFFS_PAYLOAD] = GAIA_STATUS_IN_PROGRESS;

        /* Call common command processing code */
        Gaia_ProcessCommand(&tg->common, vendor_id, command_id, payload_size, payload);
    }
    else
        DEBUG_LOG_ERROR("gaiaTransportGattReceivePacket, command size %u is too short", data_length);
}


static void gaiaTransport_GattPacketHandled(gaia_transport *t, uint16 size_payload, const void *payload)
{
    PanicNull(t);
    DEBUG_LOG_VERBOSE("gaiaTransport_GattPacketHandled, payload %p, size %u", payload, size_payload);
    free((void *)payload);
}


static void gaiaTransport_GattReceiveDataPacket(gaia_transport_gatt_t *tg, uint16 data_length, const uint8 *data_buf)
{
    DEBUG_LOG_VERBOSE("gaiaTransport_GattReceiveDataPacket, packet_size %u, packet %p",
                      data_length, data_buf);
    DEBUG_LOG_DATA_V_VERBOSE(data_buf, data_length);

    switch (tg->data_endpoint_mode)
    {
        case GAIA_DATA_ENDPOINT_MODE_RWCP:
            RwcpServerHandleMessage(data_buf, data_length);
            break;

        default:
            DEBUG_LOG_ERROR("gaiaTransport_GattReceiveDataPacket, unsupported data_endpoint_mode %u", tg->data_endpoint_mode);
    }
}


static bool gaiaTransport_GattReceiveDataPacketFromSource(gaia_transport_gatt_t *tg, Source source)
{
    UNUSED(tg);
    uint16 data_length = SourceBoundary(source);
    while (data_length)
    {
        DEBUG_LOG_VERBOSE("gaiaTransport_GattProcessSource, source %u, data_length %u", source, data_length);
        const uint8 *data_buf = SourceMap(source);

        if (data_length > GAIA_HANDLE_SIZE)
        {
            const uint16 handle = data_buf[0] | (data_buf[1] << 8);
            if (handle == tg->handle_data_endpoint)
                gaiaTransport_GattReceiveDataPacket(tg, data_length - GAIA_HANDLE_SIZE, data_buf + GAIA_HANDLE_SIZE);
            else
                DEBUG_LOG_ERROR("gaiaTransport_GattProcessSource, unknown handle %u", handle);
        }
        else
            DEBUG_LOG_WARN("gaiaTransport_GattProcessSource, packet too short");

        SourceDrop(source, data_length);

        /* Get length of next packet */
        data_length = SourceBoundary(source);
    }

    return FALSE;
}



/*! @brief
 */
static void gaiaTransport_GattHandleAccessInd(gaia_transport_gatt_t *tg, const GATT_MANAGER_SERVER_ACCESS_IND_T *ind)
{
    uint16 flags = ind->flags;
    gatt_status_t status = gatt_status_success;
    uint8 *response = NULL;
    uint16 size_response = 0;

    DEBUG_LOG_VERBOSE("gaiaTransport_GattHandleAccessInd, CID 0x%04X, handle 0x%04X, flags %c%c%c%c, offset %u size %u",
                      ind->cid, ind->handle,
                      flags & ATT_ACCESS_PERMISSION ? 'p' : '-', flags & ATT_ACCESS_WRITE_COMPLETE   ? 'c' : '-',
                      flags & ATT_ACCESS_WRITE      ? 'w' : '-', flags & ATT_ACCESS_READ             ? 'r' : '-',
                      ind->offset, ind->size_value);

    /* Latch onto this CID if no other CID has accessed server */
    if (tg->common.state == GAIA_TRANSPORT_STARTED)
    {
        /* Store CID must be 0 if we're not in CONNECTED state */
        PanicFalse(tg->cid == 0);

        /* Create ATT stream */
        status = gaiaTransport_GattCreateAttStream(tg, ind->cid) ? gatt_status_success : gatt_status_insufficient_resources;
        if (status == gatt_status_success)
        {
            /* Remember CID for subsequent accesses */
            tg->cid = ind->cid;

            /* TODO: According to original code we're really supposed to persist these */
            tg->response_indications_enabled = FALSE;
            tg->response_notifications_enabled = TRUE;

            /* Inform transport common code that we're connected */
            Gaia_TransportConnectInd(&tg->common, TRUE);
        }
    }
    else
    {
        /* Check this CID matches the one allowed to access server */
        if (ind->cid != tg->cid)
        {
            DEBUG_LOG_ERROR("gaiaTransport_GattHandleAccessInd, unknown CID 0x%04x, expecting 0x%04x", ind->cid, tg->cid);
            status = gatt_status_insufficient_resources;
        }
    }

    if (status == gatt_status_success)
    {
        DEBUG_LOG_VERBOSE("gaiaTransport_GattHandleAccessInd, is_command %u, is_data %u, is_response_client_config %u, is_data_client_config %u, is_response %u",
                          Gaia_IsGattCommandEndpoint(ind->handle), Gaia_IsGattDataEndpoint(ind->handle),
                          Gaia_IsGattResponseClientConfig(ind->handle), Gaia_IsGattDataClientConfig(ind->handle),
                          Gaia_IsGattResponseEndpoint(ind->handle));

        if (flags == (ATT_ACCESS_PERMISSION | ATT_ACCESS_WRITE_COMPLETE | ATT_ACCESS_WRITE))
        {
            if (Gaia_IsGattCommandEndpoint(ind->handle))
                gaiaTransport_GattReceivePacket(tg, ind->size_value, ind->value, GAIA_DATA_ENDPOINT_MODE_NONE);
            else if (Gaia_IsGattDataEndpoint(ind->handle))
                gaiaTransport_GattReceiveDataPacket(tg, ind->size_value, ind->value);
            else if (Gaia_IsGattResponseClientConfig(ind->handle))
            {
                tg->response_notifications_enabled = (ind->value[0] & 1) != 0;
                tg->response_indications_enabled = (ind->value[0] & 2) != 0;
                DEBUG_LOG_INFO("gaiaTransport_GattHandleAccessInd, client config for response endpoint, notifications %u, indications %u",
                               tg->response_notifications_enabled, tg->response_indications_enabled);
            }
            else if (Gaia_IsGattDataClientConfig(ind->handle))
            {
                tg->data_notifications_enabled = (ind->value[0] & 1) != 0;
                tg->data_indications_enabled = (ind->value[0] & 2) != 0;
                DEBUG_LOG_INFO("gaiaTransport_GattHandleAccessInd, client config for data endpoint, notifications %u, indications %u",
                               tg->data_notifications_enabled, tg->data_indications_enabled);
            }
            else
                status = gatt_status_write_not_permitted;
        }
        else if (flags == (ATT_ACCESS_PERMISSION | ATT_ACCESS_READ))
        {
            if (Gaia_IsGattResponseEndpoint(ind->handle))
            {
                /* Send stored response */
                response = tg->response;
                size_response = tg->size_response;
            }
            else if (Gaia_IsGattDataEndpoint(ind->handle))
            {
                /* TODO: What should go here, it's empty... */
            }
            else
                status = gatt_status_read_not_permitted;
        }
        else
            status = gatt_status_request_not_supported;
    }

    /* Handle 0 is handled by the demultiplexer */
    if (ind->handle)
        GattManagerServerAccessResponse(&tg->common.task, ind->cid, ind->handle, status, size_response, response);
}



/*! @brief Called from GAIA transport to start GATT service
 *
 *  @result Pointer to transport instance
 */
static void gaiaTransport_GattStartService(gaia_transport *t)
{
    gaia_transport_gatt_t *tg = (gaia_transport_gatt_t *)t;
    PanicNull(tg);
    DEBUG_LOG_INFO("gaiaTransport_GattStartService, transport %p", tg);

    /* Initialise task */
    t->task.handler = gaiaTransport_GattHandleMessage;

    /* No data endpoint initially */
    tg->data_endpoint_mode = GAIA_DATA_ENDPOINT_MODE_NONE;

    /* No response initially */
    tg->size_response = 0;

    /* Register with GATT manager */
    gatt_manager_server_registration_params_t registration_params;
    registration_params.task = &t->task;
    registration_params.start_handle = HANDLE_GAIA_SERVICE;
    registration_params.end_handle = HANDLE_GAIA_SERVICE_END;
    gatt_manager_status_t status = GattManagerRegisterServer(&registration_params);
    DEBUG_LOG_INFO("gaiaTransport_GattStartService, GattManagerRegisterServer status %u", status);

    /* Enable RWCP */
    RwcpServerInit(0);      /*!< @todo the side passed does not matter. The parameter can be removed */
    RwcpSetClientTask(&t->task);

    /* Send confirm, success dependent on GATT manager status */
    Gaia_TransportStartServiceCfm(&tg->common, status == gatt_manager_status_success);
}


/*! @brief Called from GAIA transport to stop RFCOMM service
 *
 *  @param Pointer to transport instance
 */
static void gaiaTransport_GattStopService(gaia_transport *t)
{
    gaia_transport_gatt_t *tg = (gaia_transport_gatt_t *)t;
    PanicNull(tg);
    DEBUG_LOG_INFO("gaiaTransport_GattStopService");

    /* Stop is not implemented, so send failure */
    Gaia_TransportStopServiceCfm(&tg->common, FALSE);
}


static bool gaiaTransport_GattGetInfo(gaia_transport *t, gaia_transport_info_key_t key, uint32 *value)
{
    gaia_transport_gatt_t *tg = (gaia_transport_gatt_t *)t;
    PanicNull(tg);
    DEBUG_LOG_INFO("gaiaTransport_GattGetInfo");

    switch (key)
    {
        case GAIA_TRANSPORT_MAX_TX_PAYLOAD:
        case GAIA_TRANSPORT_OPTIMUM_TX_PAYLOAD:
            *value = 64;
            break;
        case GAIA_TRANSPORT_MAX_RX_PAYLOAD:
        case GAIA_TRANSPORT_OPTIMUM_RX_PAYLOAD:
            *value = 64;
            break;
        case GAIA_TRANSPORT_TX_FLOW_CONTROL:
        case GAIA_TRANSPORT_RX_FLOW_CONTROL:
            *value = 0;
            break;
        case GAIA_TRANSPORT_PROTOCOL_VERSION:
            *value = GAIA_TRANSPORT_GATT_DEFAULT_PROTOCOL_VERSION;
            break;

        default:
            return FALSE;
    }
    return TRUE;
}


static bool gaiaTransport_GattSetParameter(gaia_transport *t, gaia_transport_info_key_t key, uint32 *value)
{
    /* Ignore any request to set parameters, just return current value */
    return gaiaTransport_GattGetInfo(t, key, value);
}


static bool gaiaTransport_GattSetDataEndpointMode(gaia_transport *t, gaia_data_endpoint_mode_t mode)
{
    gaia_transport_gatt_t *tg = (gaia_transport_gatt_t *)t;
    PanicNull(tg);
    DEBUG_LOG_INFO("gaiaTransport_GattSetDataEndpointMode, mode %u", mode);

    switch (mode)
    {
        case GAIA_DATA_ENDPOINT_MODE_NONE:
            tg->data_endpoint_mode = mode;
            return TRUE;

        case GAIA_DATA_ENDPOINT_MODE_RWCP:
            tg->data_endpoint_mode = mode;
            return TRUE;

        default:
            break;
    }

    return FALSE;
}


static gaia_data_endpoint_mode_t gaiaTransport_GattGetDataEndpointMode(gaia_transport *t)
{
    gaia_transport_gatt_t *tg = (gaia_transport_gatt_t *)t;
    PanicNull(tg);
    DEBUG_LOG_INFO("gaiaTransport_GattGetDataEndpointMode, mode %u", tg->data_endpoint_mode);
    return tg->data_endpoint_mode;
}


static gaia_data_endpoint_mode_t gaiaTransport_GattGetPayloadDataEndpointMode(gaia_transport *t, uint16 size_payload, const uint8 *payload)
{
    gaia_transport_gatt_t *tg = (gaia_transport_gatt_t *)t;
    PanicNull(tg);
    const gaia_data_endpoint_mode_t mode = payload ? payload[size_payload] : GAIA_DATA_ENDPOINT_MODE_NONE;
    DEBUG_LOG_VERBOSE("gaiaTransport_GattGetPayloadDataEndpointMode, mode %u", mode);
    return mode;
}


static void gaiaTransport_GattError(gaia_transport *t)
{
    gaia_transport_gatt_t *tg = (gaia_transport_gatt_t *)t;
    PanicNull(tg);
    DEBUG_LOG_INFO("gaiaTransport_GattError, CID %u", tg->cid);
}


static uint8 gaiaTransport_GattFeatures(gaia_transport *t)
{
    gaia_transport_gatt_t *tg = (gaia_transport_gatt_t *)t;
    PanicNull(tg);
    DEBUG_LOG_INFO("gaiaTransport_GattFeatures");

    return 0;
}


static void gaiaTransport_GattHandleMessage(Task task, MessageId id, Message message)
{
    gaia_transport_gatt_t *tg = (gaia_transport_gatt_t *)task;

    switch (id)
    {
        case GATT_MANAGER_SERVER_ACCESS_IND:
            gaiaTransport_GattHandleAccessInd(tg, (GATT_MANAGER_SERVER_ACCESS_IND_T *)message);
            break;

        case MESSAGE_MORE_DATA:
            gaiaTransport_GattReceiveDataPacketFromSource(tg, ((MessageMoreData *)message)->source);
        break;

        case GATT_MANAGER_REMOTE_CLIENT_NOTIFICATION_CFM:
        {
            const GATT_MANAGER_REMOTE_CLIENT_NOTIFICATION_CFM_T *cfm = (const GATT_MANAGER_REMOTE_CLIENT_NOTIFICATION_CFM_T *)message;
            if (cfm->status != gatt_status_success)
                DEBUG_LOG_ERROR("gaiaTransport_GattHandleMessage, GATT_MANAGER_REMOTE_CLIENT_NOTIFICATION_CFM, status %u", cfm->status);
            else
                DEBUG_LOG_VERBOSE("gaiaTransport_GattHandleMessage, GATT_MANAGER_REMOTE_CLIENT_NOTIFICATION_CFM, status %u", cfm->status);
        }
        break;

        case GATT_MANAGER_REMOTE_CLIENT_INDICATION_CFM:
            /* TODO: Do we check for success? */
            break;

        default:
            DEBUG_LOG_ERROR("gaiaTransport_GattHandleMessage, unhandled message MESSAGE:0x%04x", id);
            DEBUG_LOG_DATA_ERROR(message, psizeof(message));
            break;
    }
}


static void gaiaTransport_GattConnect(uint16 cid)
{
    DEBUG_LOG_ERROR("gaiaTransport_GattConnect, wait for access for CID %u", cid);
}


static void gaiaTransport_GattDisconnect(uint16 cid)
{
    gaia_transport *t = Gaia_TransportFindService(gaia_transport_gatt, 0);
    while (t)
    {
        gaia_transport_gatt_t *tg = (gaia_transport_gatt_t *)t;
        if (tg->cid == cid)
        {
            DEBUG_LOG("gaiaTransport_GattDisconnect, found transport for CID %u", cid);

            gaiaTransport_GattDestroyAttStream(tg);

            tg->cid = 0;
            Gaia_TransportDisconnectInd(&tg->common);
            return;
        }
        else
            t = Gaia_TransportFindService(gaia_transport_gatt, t);
    }

    DEBUG_LOG_ERROR("gaiaTransport_GattDisconnect, no transport found for CID %u", cid);
}


static void gaiaTransport_GattDisconnectRequested(uint16 cid, gatt_connect_disconnect_req_response response)
{
    DEBUG_LOG("gaiaTransport_GattDisconnectRequested, CID %u", cid);

    gaiaTransport_GattDisconnect(cid);

    /* Call response callback to allow disconnect to proceed */
    response(cid);
}

static const gatt_connect_observer_callback_t gatt_gaia_observer_callback =
{
    .OnConnection = gaiaTransport_GattConnect,
    .OnDisconnection = gaiaTransport_GattDisconnect,
    .OnDisconnectRequested = gaiaTransport_GattDisconnectRequested
};

void GaiaTransport_GattInit(void)
{
    static const gaia_transport_functions_t functions =
    {
        .service_data_size          = sizeof(gaia_transport_gatt_t),
        .start_service              = gaiaTransport_GattStartService,
        .stop_service               = gaiaTransport_GattStopService,
        .packet_handled             = gaiaTransport_GattPacketHandled,
        .send_command_packet        = gaiaTransport_GattSendPacket,
        .send_data_packet           = gaiaTransport_GattSendDataPacket,
        .connect_req                = NULL,
        .disconnect_req             = NULL,
        .set_data_endpoint          = gaiaTransport_GattSetDataEndpointMode,
        .get_data_endpoint          = gaiaTransport_GattGetDataEndpointMode,
        .get_payload_data_endpoint  = gaiaTransport_GattGetPayloadDataEndpointMode,
        .error                      = gaiaTransport_GattError,
        .features                   = gaiaTransport_GattFeatures,
        .get_info                   = gaiaTransport_GattGetInfo,
        .set_parameter              = gaiaTransport_GattSetParameter,
    };

    /* Register this transport with GAIA */
    Gaia_TransportRegister(gaia_transport_gatt, &functions);

    GattConnect_RegisterObserver(&gatt_gaia_observer_callback);
}


/*************************************************************************
 *  NAME
 *      GaiaRwcpSendNotification
 *
 *  DESCRIPTION
 *      This function handles payloads sent from RWCP server
 */
void GaiaRwcpSendNotification(const uint8 *payload, uint16 payload_length)
{
    /* TODO: Handle multiple GATT transports */
    gaia_transport_gatt_t *tg = (gaia_transport_gatt_t *)Gaia_TransportFindService(gaia_transport_gatt, 0);

    DEBUG_LOG_VERBOSE("GaiaRwcpSendNotification, payload_length %u", payload_length);
    DEBUG_LOG_DATA_V_VERBOSE(payload, payload_length);

    if (tg && tg->att_stream_sink)
    {        
        /* Check notifications are enabled on data endpoint */
        if (tg->data_notifications_enabled)
        {
            const uint16 pkt_length = payload_length + GAIA_HANDLE_SIZE;

            /* Send notifications via ATT streams if streams were configured and space was available */
            if (SinkSlack(tg->att_stream_sink) >= pkt_length)
            {
                uint8 *pkt_buf = SinkMap(tg->att_stream_sink);
                uint16 claimed = SinkClaim(tg->att_stream_sink, pkt_length);

                if (pkt_buf && claimed != 0xFFFF)
                {
                    uint8 *pkt_ptr = pkt_buf;

                    /* Prepending the response endpoint handle to which the data is sent over the air */
                    *pkt_ptr++ = LOW(tg->handle_data_endpoint);
                    *pkt_ptr++ = HIGH(tg->handle_data_endpoint);

                    /* Copy payload */
                    memcpy(pkt_ptr, payload, payload_length);

                    /* Send packet */
                    SinkFlush(tg->att_stream_sink, pkt_length);
                }
                else
                    DEBUG_LOG_ERROR("GaiaRwcpSendNotification, failed to claim space %u", pkt_length);
            }
            else
                DEBUG_LOG_ERROR("GaiaRwcpSendNotification, not enough space %u", pkt_length);
        }
        else
            DEBUG_LOG_ERROR("GaiaRwcpSendNotification, notifications not enabled");
    }
    else
        DEBUG_LOG_ERROR("GaiaRwcpSendNotification, no transport or no ATT sink");
}


/*************************************************************************
 *  NAME
 *      GaiaRwcpProcessCommand
 *
 *  DESCRIPTION
 *      This function processes the GAIA packet and extracts vendor_id and command_id
 */
void GaiaRwcpProcessCommand(const uint8 *command, uint16 size_command)
{
    /* TODO: Handle multiple GATT transports */
    gaia_transport_gatt_t *tg = (gaia_transport_gatt_t *)Gaia_TransportFindService(gaia_transport_gatt, 0);

    DEBUG_LOG_VERBOSE("GaiaRwcpProcessCommand, size_command %u", size_command);
    DEBUG_LOG_DATA_V_VERBOSE(command, size_command);

    if (tg)
        gaiaTransport_GattReceivePacket(tg, size_command, command, GAIA_DATA_ENDPOINT_MODE_RWCP);
    else
        DEBUG_LOG_ERROR("GaiaRwcpProcessCommand, no transport");
}


