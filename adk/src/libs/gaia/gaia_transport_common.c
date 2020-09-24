/*****************************************************************
Copyright (c) 2011 - 2015 Qualcomm Technologies International, Ltd.
*/

#define DEBUG_LOG_MODULE_NAME gaia_transport

#include <panic.h>
#include <source.h>
#include <logging.h>

#include "gaia_private.h"

void gaia_TransportSendGaiaConnectCfm(gaia_transport *transport, bool success)
{
    MESSAGE_PMAKE(gcc, GAIA_CONNECT_CFM_T);
    gcc->transport = (GAIA_TRANSPORT*)transport;
    gcc->success = success;
    MessageSend(gaia.app_task, GAIA_CONNECT_CFM, gcc);
}

void gaia_TransportSendGaiaConnectInd(gaia_transport *transport, bool success)
{
    MESSAGE_PMAKE(gci, GAIA_CONNECT_IND_T);
    gci->transport = (GAIA_TRANSPORT*)transport;
    gci->success = success;
    MessageSend(gaia.app_task, GAIA_CONNECT_IND, gci);
}

void gaia_TransportSendGaiaDisconnectInd(gaia_transport *transport)
{
    MESSAGE_PMAKE(gdi, GAIA_DISCONNECT_IND_T);
    gdi->transport = (GAIA_TRANSPORT*)transport;
    MessageSend(gaia.app_task, GAIA_DISCONNECT_IND, gdi);
}

void gaia_TransportSendGaiaDisconnectCfm(gaia_transport *transport)
{
    MESSAGE_PMAKE(gdc, GAIA_DISCONNECT_CFM_T);
    gdc->transport = (GAIA_TRANSPORT*)transport;
    MessageSend(gaia.app_task, GAIA_DISCONNECT_CFM, gdc);
}

void gaia_TransportSendGaiaStartServiceCfm(gaia_transport_type transport_type, gaia_transport *transport, bool success)
{
    MESSAGE_PMAKE(gssc, GAIA_START_SERVICE_CFM_T);
    gssc->transport_type = transport_type;
    gssc->transport = (GAIA_TRANSPORT *)transport;
    gssc->success = success;
    MessageSend(gaia.app_task, GAIA_START_SERVICE_CFM, gssc);
}

void gaia_TransportSendGaiaStopServiceCfm(gaia_transport_type transport_type, gaia_transport *transport, bool success)
{
    MESSAGE_PMAKE(gssc, GAIA_START_SERVICE_CFM_T);
    gssc->transport_type = transport_type;
    gssc->transport = (GAIA_TRANSPORT *)transport;
    gssc->success = success;
    MessageSend(gaia.app_task, GAIA_STOP_SERVICE_CFM, gssc);
}


/*  It's that diagram again ... Gaia V1 protocol packet
 *  0 bytes  1        2        3        4        5        6        7        8          9    len+8      len+9
 *  +--------+--------+--------+--------+--------+--------+--------+--------+ +--------+--/ /---+ +--------+
 *  |  SOF   |VERSION | FLAGS  | LENGTH |    VENDOR ID    |   COMMAND ID    | | PAYLOAD   ...   | | CHECK  |
 *  +--------+--------+--------+--------+--------+--------+--------+--------+ +--------+--/ /---+ +--------+
 *
 *  0 bytes  1        2        3        4        5        6        7        8          9        10   len+9     len+10
 *  +--------+--------+--------+--------+--------+--------+--------+--------+--------+ +--------+--/ /---+ +--------+
 *  |  SOF   |VERSION | FLAGS  | LENGTH          |    VENDOR ID    |   COMMAND ID    | | PAYLOAD   ...   | | CHECK  |
 *  +--------+--------+--------+--------+--------+--------+--------+--------+--------+ +--------+--/ /---+ +--------+
 */

#define GAIA_OFFS_SOF (0)
#define GAIA_OFFS_VERSION (1)
#define GAIA_OFFS_FLAGS (2)
#define GAIA_OFFS_PAYLOAD_LENGTH (3)
#define GAIA_OFFS_VENDOR_ID(is_16) ((is_16) ? 5 : 4)
#define GAIA_OFFS_COMMAND_ID(is_16) ((is_16) ? 7 : 6)
#define GAIA_OFFS_PAYLOAD(is_16) ((is_16) ? 9 : 8)

#define GAIA_SOF (0xFF)

#define HIGH(x) (x >> 8)
#define LOW(x)  (x & 0xFF)
#define W16(x)  (((*(x)) << 8) | (*((x) + 1)))


uint16 Gaia_TransportCommonCalcTxPacketLength(uint16 size_payload, uint8 status)
{
    return (GAIA_OFFS_PAYLOAD(FALSE) + size_payload) +
           (size_payload > 255 ? 1 : 0) +                       /* Extra byte for 16 bit payload length */
           (status != GAIA_STATUS_NONE ? 1 : 0);                /* Extra byte for status-cum-event */
}


uint16 Gaia_TransportCommonCalcRxPacketLength(uint16 size_payload, uint8 flags)
{
    return (GAIA_OFFS_PAYLOAD(FALSE) + size_payload) +
           (flags & GAIA_PROTOCOL_FLAG_CHECK ? 1 : 0) +         /* Extra byte for checksum */
           (flags & GAIA_PROTOCOL_FLAG_16_BIT_LENGTH ? 1 : 0);  /* Extra byte for 16 bit payload length */
}


void Gaia_TransportCommonBuildPacket(const uint8 protocol_version, uint8 *pkt_buf, const uint16 pkt_length,
                                     const uint16 vendor_id, const uint16 command_id, const uint8 status,
                                     const uint16 size_payload, const uint8 *payload)
{
    uint8 *pkt_ptr = pkt_buf;
    const uint16_t total_payload_size = size_payload + (status != GAIA_STATUS_NONE ? 1 : 0);

    /* Calculate flags, if payload length > 255 and protocl version is at least 4 then use 16 bit payload length field */
    const uint8 flags = (protocol_version >= 4 && total_payload_size > 255) ? GAIA_PROTOCOL_FLAG_16_BIT_LENGTH : 0;

    /* Write header */
    *pkt_ptr++ = GAIA_SOF;
    *pkt_ptr++ = protocol_version;
    *pkt_ptr++ = flags;

    /* Write payload size field, either 8 or 16 bits */
    if (flags & GAIA_PROTOCOL_FLAG_16_BIT_LENGTH)
    {
        *pkt_ptr++ = HIGH(total_payload_size);
        *pkt_ptr++ = LOW(total_payload_size);
    }
    else
    {
        PanicFalse(total_payload_size < 256);
        *pkt_ptr++ = total_payload_size;
    }

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

    /* Calculate checksum if enabled */
    if (flags & GAIA_PROTOCOL_FLAG_CHECK)
    {
        /* XOR all bytes apart from last one which is reserved for the checksum  */
        uint8 chksum = 0;
        for (uint16 index = 0; index < (pkt_length - 1); index++)
            chksum ^= pkt_buf[index];

        /* Write checksum into last byte of packet */
        pkt_buf[pkt_length - 1] = chksum;
    }
}


uint16 Gaia_TransportCommonReceivePacket(gaia_transport *transport, const uint8 protocol_version, uint16 data_length, const uint8 *data_buf,
                                         bool (*pkt_callback)(gaia_transport *transport, const uint16 pkt_size, uint16 vendor_id, uint16 command_id, uint16 size_payload, const uint8 *payload))
{
    const uint8 *data_ptr = data_buf;
    for (;;)
    {
        /* Consume everything up to start-of-frame byte */
        if (data_length && *data_ptr != GAIA_SOF)
        {
            DEBUG_LOG_ERROR("gaiaTransportCommonReceivePacket, dropping %02x looking for start-of-frame", *data_ptr);
            Gaia_TransportErrorInd(transport, GAIA_TRANSPORT_FRAMING_ERROR);
            break;
        }

        /* Remaining data must be at least the size of a header */
        if (data_length >= GAIA_OFFS_PAYLOAD(0))
        {
            const uint8 version = data_ptr[GAIA_OFFS_VERSION];
            if (version != protocol_version)
            {
                /* All versions are compatible aren't they? */
                DEBUG_LOG_WARN("gaiaTransportCommonReceivePacket, version %u packet received", version);
            }

            const uint8 flags = data_ptr[GAIA_OFFS_FLAGS];
            const bool is_16_bit = flags & GAIA_PROTOCOL_FLAG_16_BIT_LENGTH;
            uint16 payload_size;
            if (is_16_bit)
            {
                payload_size = W16(data_ptr + GAIA_OFFS_PAYLOAD_LENGTH);
                if (version < 4)
                    DEBUG_LOG_WARN("gaiaTransportCommonReceivePacket, 16 bit payload length but version %u", version);
            }
            else
                payload_size = data_ptr[GAIA_OFFS_PAYLOAD_LENGTH];

            /* Now we can calculate total packet length */
            const uint16 pkt_length = Gaia_TransportCommonCalcRxPacketLength(payload_size, flags);

            /* Now we now packet length, make sure we have enough data */
            if (data_length >= pkt_length)
            {
                /* Calculate checksum if it's enabled */
                uint8 chksum = 0;
                if (flags & GAIA_PROTOCOL_FLAG_CHECK)
                {
                    /*  XOR all bytes including the checksum byte */
                    for (uint16 index = 0; index < pkt_length; index++)
                        chksum ^= data_ptr[index];
                }

                /* Checksum should end up being 0 if all is correct */
                if (chksum == 0)
                {
                    const uint16 vendor_id = W16(data_ptr + GAIA_OFFS_VENDOR_ID(is_16_bit));
                    const uint16 command_id = W16(data_ptr + GAIA_OFFS_COMMAND_ID(is_16_bit));
                    const uint8 *payload = payload_size ? data_ptr + GAIA_OFFS_PAYLOAD(is_16_bit) : NULL;

                    DEBUG_LOG_VERBOSE("gaiaTransportCommonReceivePacket, vendor_id 0x%02x, command_id 0x%04x, pkt_length %u", vendor_id, command_id, pkt_length);
                    DEBUG_LOG_DATA_V_VERBOSE(data_ptr, pkt_length);

                    const bool more_pkts = pkt_callback(transport, pkt_length, vendor_id, command_id, payload_size, payload);
                    if (!more_pkts)
                        break;
                }
                else
                {
                    DEBUG_LOG_ERROR("gaiaTransportCommonReceivePacket, checksum error %02x, pkt_length %u", chksum, pkt_length);
                    DEBUG_LOG_DATA_ERROR(data_ptr, pkt_length);
                    Gaia_TransportErrorInd(transport, GAIA_TRANSPORT_CHECKSUM_ERROR);
                    break;
                }

                /* Skip the whole packet */
                data_ptr += pkt_length;
                data_length -= pkt_length;
            }
            else
            {
                /* Remaining data too small for packet payload */
                DEBUG_LOG_WARN("gaiaTransportCommonReceivePacket, too small for payload, pkt_length %u, size_payload %u", pkt_length, payload_size);
                break;
            }

        }
        else
        {
            /* Remaining data too small for packet header, show warning if data_length > 0 */
            if (data_length)
                DEBUG_LOG_WARN("gaiaTransportCommonReceivePacket, too small to parse header, data_length %u", data_length);
            break;
        }
    }

    /* Return number of bytes consumed */
    return data_ptr - data_buf;
}

