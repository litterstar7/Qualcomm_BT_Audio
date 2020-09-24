/*****************************************************************
Copyright (c) 2011 - 2018 Qualcomm Technologies International, Ltd.
 */

#define DEBUG_LOG_MODULE_NAME gaia_transport
#include <logging.h>

#include "gaia_private.h"
#include <stdlib.h>

static const gaia_transport_functions_t *gaia_transport_functions[gaia_transport_max];
static gaia_transport *gaia_transports[gaia_transport_max];


bool Gaia_TransportSendPacket(gaia_transport *t,
                              uint16 vendor_id, uint16 command_id, uint16 status,
                              uint16 payload_length, const uint8 *payload)
{
    PanicNull(t);
    return t->functions->send_command_packet(t, vendor_id, command_id, status,
                                             payload_length, payload);
}

bool Gaia_TransportSendDataPacket(gaia_transport *t,
                                  uint16 vendor_id, uint16 command_id, uint16 status,
                                  uint16 payload_length, const uint8 *payload)
{
    PanicNull(t);
    if (t->functions->send_data_packet)
        return t->functions->send_data_packet(t, vendor_id, command_id, status,
                                              payload_length, payload);
    else
        return t->functions->send_command_packet(t, vendor_id, command_id, status,
                                                 payload_length, payload);
}


/*! @brief Register GAIA server for a given transport.
 */
void Gaia_TransportRegister(gaia_transport_type type, const gaia_transport_functions_t *functions)
{
    DEBUG_LOG_INFO("gaiaTransportRegister, type %u", type);
    PanicFalse(gaia_transport_functions[type] == NULL);
    gaia_transport_functions[type] = functions;
}


/*! @brief Start GAIA server on a given transport.
 */
void Gaia_TransportStartService(gaia_transport_type type)
{
    DEBUG_LOG_INFO("gaiaTransportStartService, type %u", type);
    PanicFalse(gaia_transport_functions[type]);
    PanicZero(gaia_transport_functions[type]->start_service);

    /* Only malloc enough memory for transport type */
    gaia_transport *t = calloc(1, gaia_transport_functions[type]->service_data_size);
    if (t)
    {
        t->functions = gaia_transport_functions[type];
        t->type = type;
        t->client_data = 0;
        t->state = GAIA_TRANSPORT_STARTING;
        gaia_transports[t->type] = t;

        /* Attempt to start service */
        t->functions->start_service(t);
    }
    else
    {
        DEBUG_LOG_ERROR("gaiaTransportStartService, failed to allocate instance");
        gaia_TransportSendGaiaStartServiceCfm(type, NULL, FALSE);
    }
}


/*! @brief Called from transport to confirm if server is started or not.
 */
void Gaia_TransportStartServiceCfm(gaia_transport *t, bool success)
{
    DEBUG_LOG_ERROR("gaiaTransportStartServiceCfm, type %u, success %u", t->type, success);
    PanicFalse(t);

    gaia_TransportSendGaiaStartServiceCfm(t->type, success ? t : NULL, success);

    if (success)
        t->state = GAIA_TRANSPORT_STARTED;
    else
    {
        /* Ensure any message still to be delivered are flushed */
        MessageFlushTask(&t->task);

        /* Remove transport and free instance */
        gaia_transports[t->type] = NULL;
        free(t);
    }
}


/*! @brief Stop GAIA server on a given transport.
 */
void Gaia_TransportStopService(gaia_transport_type type)
{
    DEBUG_LOG_INFO("gaiaTransportStopService, type %u", type);
    gaia_transport *t = gaia_transports[type];
    if (t)
    {
        PanicZero(t->functions->stop_service);
        t->functions->stop_service(t);
    }
    else
    {
        DEBUG_LOG_ERROR("gaiaTransportStopService, no instance");
        gaia_TransportSendGaiaStopServiceCfm(type, NULL, FALSE);
    }
}


/*! @brief Called from transport to confirm if server is stopped or not.
 */
void Gaia_TransportStopServiceCfm(gaia_transport *t, bool success)
{
    DEBUG_LOG_ERROR("gaiaTransportStopServiceCfm, type %u, success %u", t->type, success);
    PanicFalse(t && gaia_transports[t->type]);
    gaia_TransportSendGaiaStopServiceCfm(t->type, t, success);
    if (success)
    {
        gaia_transports[t->type] = NULL;
        free(t);
    }
    else
        Panic();
}



/*! @brief Connect GAIA server on a given transport.
 */
void Gaia_TransportConnectReq(gaia_transport_type type, const gaia_transport_connect_params_t *params)
{
    gaia_transport *t = gaia_transports[type];
    if (t && t->functions->connect_req)
        t->functions->connect_req(t, params);
    else
        gaia_TransportSendGaiaConnectCfm(t, FALSE);
}


/*! @brief Called from transport to indicate new connection.
 */
void Gaia_TransportConnectInd(gaia_transport *t, bool success)
{
    PanicNull(t);
    if (t->state == GAIA_TRANSPORT_CONNECTING)
        gaia_TransportSendGaiaConnectCfm(t, success);
    else
        gaia_TransportSendGaiaConnectInd(t, success);

    t->state = success ? GAIA_TRANSPORT_CONNECTED : GAIA_TRANSPORT_STARTED;
}


bool Gaia_TransportIsConnected(gaia_transport *t)
{
    PanicNull(t);
    return (t->state == GAIA_TRANSPORT_CONNECTED);
}


/*! @brief Disconnect GAIA server on a given transport.
 */
void Gaia_TransportDisconnectReq(gaia_transport *t)
{
    PanicNull(t);
    if (t->functions->disconnect_req && t->state == GAIA_TRANSPORT_CONNECTED)
    {
        t->state = GAIA_TRANSPORT_DISCONNECTING;
        t->functions->disconnect_req(t);
    }
    else
        gaia_TransportSendGaiaDisconnectCfm(t);
}


/*! @brief Called from transport to indicate disconnection.
 */
void Gaia_TransportDisconnectInd(gaia_transport *t)
{
    PanicNull(t);
    if (t->state == GAIA_TRANSPORT_DISCONNECTING)
        gaia_TransportSendGaiaDisconnectCfm(t);
    else
        gaia_TransportSendGaiaDisconnectInd(t);

    t->state = GAIA_TRANSPORT_STARTED;
}


void Gaia_TransportErrorInd(gaia_transport *t, gaia_transport_error error)
{
    UNUSED(t);
    UNUSED(error);

    PanicNull(t);
    PanicZero(t->functions->error);
    t->state = GAIA_TRANSPORT_ERROR;
    t->functions->error(t);
}


gaia_transport *Gaia_TransportFindFeature(gaia_transport_type start, uint8 feature)
{
    for (int type = start; type++; type < gaia_transport_max)
    {
        gaia_transport *t = gaia_transports[type];
        if (t && Gaia_TransportHasFeature(t, feature))
            return t;
    }
    return NULL;
}


gaia_transport *Gaia_TransportIterate(uint8_t *index)
{
    while ((*index) < gaia_transport_max)
    {
        gaia_transport *t = gaia_transports[(*index)++];
        if (t)
            return t;
    }
    return NULL;
}


bool Gaia_TransportHasFeature(gaia_transport *t, uint8 feature)
{
    PanicNull(t);
    PanicZero(t->functions->features);

    return (t->functions->features(t) & feature) ? TRUE : FALSE;
}


gaia_transport *Gaia_TransportFindService(gaia_transport_type type, gaia_transport *start)
{
    // TODO: Can only have instance of a type of service */
    if (start)
        return NULL;
    else
        return gaia_transports[type];
}


bool Gaia_TransportSetDataEndpointMode(gaia_transport *t, gaia_data_endpoint_mode_t mode)
{
    PanicNull(t);
    if (t->functions->set_data_endpoint)
        return t->functions->set_data_endpoint(t, mode);
    else
        return FALSE;
}


gaia_data_endpoint_mode_t Gaia_TransportGetDataEndpointMode(gaia_transport *t)
{
    PanicNull(t);
    if (t->functions->get_data_endpoint)
        return t->functions->get_data_endpoint(t);
    else
        return GAIA_DATA_ENDPOINT_MODE_NONE;
}

gaia_data_endpoint_mode_t Gaia_TransportGetPayloadDataEndpointMode(gaia_transport *t, uint16 size_payload, const uint8 *payload)
{
    PanicNull(t);
    if (t->functions->get_payload_data_endpoint)
        return t->functions->get_payload_data_endpoint(t, size_payload, payload);
    else
        return GAIA_DATA_ENDPOINT_MODE_NONE;
}


bool Gaia_TransportGetInfo(gaia_transport *t, gaia_transport_info_key_t key, uint32 *value)
{
    PanicNull(t);
    PanicNull(value);
    return t->functions->get_info ? t->functions->get_info(t, key, value) : FALSE;
}


bool Gaia_TransportSetParameter(gaia_transport *t, gaia_transport_info_key_t key, uint32 *value)
{
    PanicNull(t);
    PanicNull(value);
    return t->functions->get_info ? t->functions->set_parameter(t, key, value) : FALSE;
}


uint32 Gaia_TransportGetClientData(gaia_transport *t)
{
    PanicNull(t);
    return t->client_data;
}


void Gaia_TransportSetClientData(gaia_transport *t, uint32 client_data)
{
    PanicNull(t);
    t->client_data = client_data;
}


void Gaia_TransportPacketHandled(gaia_transport *t, uint16 payload_size, const void *payload)
{
    PanicNull(t);
    t->functions->packet_handled(t, payload_size, payload);
}
