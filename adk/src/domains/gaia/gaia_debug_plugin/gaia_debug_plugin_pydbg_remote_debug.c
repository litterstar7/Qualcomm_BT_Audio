/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      PyDbg remote debug over Gaia as a part of Gaia Debug Feature.
*/

#ifdef INCLUDE_GAIA_PYDBG_REMOTE_DEBUG

#include "gaia_debug_plugin_pydbg_remote_debug.h"

#include <panic.h>
#include <stdlib.h>

/* Enable debug log outputs with per-module debug log levels.
 * The log output level for this module can be changed with the PyDbg command:
 *      >>> apps1.log_level("gaia_debug_plugin_pydbg", 3)
 * Where the second parameter value means:
 *      0:ERROR, 1:WARN, 2:NORMAL(= INFO), 3:VERBOSE(= DEBUG), 4:V_VERBOSE, 5:V_V_VERBOSE
 * See 'logging.h' and PyDbg 'log_level()' command descriptions for details. */
#define DEBUG_LOG_MODULE_NAME gaia_debug_plugin_pydbg
#include <logging.h>
DEBUG_LOG_DEFINE_LEVEL_VAR

#include "gaia_features.h"
#include "gaia_debug_plugin.h"
#include "remote_debug_prim.h"
#include "power_manager.h"


/* This is needed to cope with different hardware platforms: One is the
 * target device (Kalimba), and the other is PC which sizeof(void*) is 8. */
#if defined(HOSTED_TEST_ENVIRONMENT)
#include <stdint.h>
#if PTRDIFF_MAX > 0xFFFFFFFFUL
typedef uint64_t mem_ptr_t;
extern uint64_t full_64bit_memory_addr;
#else
typedef uint32_t mem_ptr_t;
#endif
extern void* memmove_TestWrapper(void* dest, const void* src, size_t size);
#define MEMMOVE(dest, src, size)    memmove_TestWrapper(dest, src, size)
#elif defined(__KALIMBA__) 
typedef uint32_t mem_ptr_t;
#define MEMMOVE(dest, src, size)    memmove(dest, src, size)
#else
#error "New chip. May need to check sizeof(void*) in case it matters"
#endif


#if defined(HOSTED_TEST_ENVIRONMENT)
#ifndef P1D_SQIF1_CACHED_LOWER
#define P1D_SQIF1_CACHED_LOWER                             (0x78000000u)
#endif

void* gaiaDebugPlugin_GetDataRomWindowAddr(void)
{
    /* Returns the typical values for almost all the platforms.
     * This won't harm as the unit test does not care the difference. */
    return (void*) P1D_SQIF1_CACHED_LOWER;
}

#else /* HOSTED_TEST_ENVIRONMENT */

asm void* gaiaDebugPlugin_GetDataRomWindowAddr(void)
{
    /* using @ { } as return register (instead of r0) allows the
     * compiler to optimize which register is actually used. */
    @{} = $DATA_ROM_WINDOW;
}
#endif /* HOSTED_TEST_ENVIRONMENT */



/*! Macro for setting payload header for Gaia 'Debug_Tunnel_To_Chip' Response.
    Response Payload Format:
        0        1        2        3        4        5        6        7        8       ...        N    (Byte)
    +--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+
    |ClientID|   Tag  |  Type  | Cmd ID |  Payload Length |  Tag (Seq No.)  |     Payload (if any)     |
    +--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+
    |<- Gaia header ->|<-----   PyDbg Remote Debug Protocol Header    ----->|<---- PyDbg Payload  ---->|
 */
#define SET_DEBUG_TUNNEL_TO_CHIP_RESPONSE_PAYLOAD_HEADER(RSP_PAYLOAD, GAIA_CLIENT_ID, GAIA_TAG, PYDBG_CMD_ID, PYDBG_PAYLOAD_LEN, PYDBG_SEQ_NO) \
    do { \
        RSP_PAYLOAD[0] = GAIA_CLIENT_ID; \
        RSP_PAYLOAD[1] = GAIA_TAG; \
        RSP_PAYLOAD[2] = REMOTE_DEBUG_CMD_TYPE_DEBUG_CMD; \
        RSP_PAYLOAD[3] = PYDBG_CMD_ID; \
        RSP_PAYLOAD[4] = (uint8) (PYDBG_PAYLOAD_LEN & 0xFF); \
        RSP_PAYLOAD[5] = (uint8) (PYDBG_PAYLOAD_LEN >> 8); \
        RSP_PAYLOAD[6] = (uint8) (PYDBG_SEQ_NO & 0xFF); \
        RSP_PAYLOAD[7] = (uint8) (PYDBG_SEQ_NO >> 8); \
    } while (0)


/*! \brief Parse PyDbg IP transport protocol and execute the request.

    \param gaia_client_id   An identifier assigned and used by the host (the mobile app).

    \param gaia_tag         Another identifier used by the host (the mobile app).

    \param pydbg_pdu_length Payload size of the PyDbg PDU (Header + Payload) in bytes.

    \param pydbg_pdu        PyDbg PDU data.
*/
static void gaiaDebugPlugin_PyDbgCommandHandler(GAIA_TRANSPORT *t, uint8 gaia_client_id, uint8 gaia_tag, uint16 pydbg_pdu_length, const uint8 *pydbg_pdu);


/*! \brief Handle Protocol_Version_Req PyDbg Remote Debug command.

    \param gaia_client_id   An identifier assigned and used by the host (the mobile app).

    \param gaia_tag         Another identifier used by the host (the mobile app).

    \param pydbg_seq_no     PyDbg Tag (Sequence Number).

    \param payload_length   Payload size of the PyDbg command payload (in bytes).

    \param payload          PyDbg command payload data.
*/
static void gaiaDebugPlugin_RemoteDbgCmd_ProtocolVersionReq(GAIA_TRANSPORT *t, uint8 gaia_client_id, uint8 gaia_tag, uint16 pydbg_seq_no, uint16 payload_length, const uint8 *payload);


/*! \brief Handle Max_PDU_Size_Req PyDbg Remote Debug command.

    \param client_id        An identifier assigned and used by the host (the mobile app).

    \param tag              Another identifier used by the host (the mobile app).

    \param pydbg_seq_no     PyDbg Tag (Sequence Number).

    \param payload_length   Payload size of the PyDbg command payload (in bytes).

    \param payload          PyDbg command payload data.
*/
static void gaiaDebugPlugin_RemoteDbgCmd_MaxPduSizeReq(GAIA_TRANSPORT *t, uint8 gaia_client_id, uint8 gaia_tag, uint16 pydbg_seq_no, uint16 payload_length, const uint8 *payload);


/*! \brief Check if the memory address, the access size, and the read/write size are word/dword aligned or not.

    \param addr         Memory address where the data bytes are read/write from/to.

    \param width        Memory access width in bytes (1 or 2 or 4)

    \param data_size    The size of the data bytes.

    \return REMOTE_DEBUG_TRB_NO_ERROR on success, otherwise returns an transaction error code.
*/
static REMOTE_DEBUG_TRB gaiaDebugPlugin_CheckMemoryReadWriteWordAlignment(mem_ptr_t addr, uint8 width, uint16 data_size);

#if !defined(ENFORCE_STRICT_WIDTH_CHECK_FOR_PYDBG_REMOTE_DEBUG)
/*! \brief Return adjusted access width if either the address or the data size is not word/dword aligned.

    \param addr         Memory address where the data bytes are read/write from/to.

    \param width        Memory access width in bytes (1 or 2 or 4)

    \param data_size    The size of the data bytes.

    \return Adjusted access width that matches with both the memory address and the data size.
*/
static uint8 gaiaDebugPlugin_AdjustAccessWidth(mem_ptr_t addr, uint8 width, uint16 data_size);
#endif /* ENFORCE_STRICT_WIDTH_CHECK_FOR_PYDBG_REMOTE_DEBUG */

/*! \brief Read data bytes from a memory in an aligned manner.

    \param addr         Memory read address where the data bytes are read from.

    \param width        Memory access width in bytes (1 or 2 or 4)

    \param data_size    The size of the data bytes.

    \param buf          Pointer to the buffer which the read data bytes are written to.

    \return REMOTE_DEBUG_TRB_NO_ERROR on success, otherwise returns an transaction error code.
*/
static REMOTE_DEBUG_TRB gaiaDebugPlugin_MemoryRead(mem_ptr_t addr, uint8 width, uint16 data_size, uint8 *buf);


/*! \brief Handle Memory_Read_Req PyDbg Remote Debug command.

    \param client_id        An identifier assigned and used by the host (the mobile app).

    \param tag              Another identifier used by the host (the mobile app).

    \param pydbg_seq_no     PyDbg Tag (Sequence Number).

    \param payload_length   Payload size of the PyDbg command payload (in bytes).

    \param payload          PyDbg command payload data.
*/
static void gaiaDebugPlugin_RemoteDbgCmd_MemoryReadReq(GAIA_TRANSPORT *t, uint8 gaia_client_id, uint8 gaia_tag, uint16 pydbg_seq_no, uint16 payload_length, const uint8 *payload);


/*! \brief Write data bytes to a memory in an aligned manner.

    \param addr         Memory write address where the data bytes are written to.

    \param width        Memory access width in bytes (1 or 2 or 4)

    \param data_size    The size of the data bytes.

    \param data         Data bytes to be written.

    \return REMOTE_DEBUG_TRB_NO_ERROR on success, otherwise returns an transaction error code.
*/
static REMOTE_DEBUG_TRB gaiaDebugPlugin_MemoryWrite(mem_ptr_t addr, uint8 width, uint16 data_size, const uint8 *data);


/*! \brief Handle Memory_Write_Req PyDbg Remote Debug command.

    \param client_id        An identifier assigned and used by the host (the mobile app).

    \param tag              Another identifier used by the host (the mobile app).

    \param pydbg_seq_no     PyDbg Tag (Sequence Number).

    \param payload_length   Payload size of the PyDbg command payload (in bytes).

    \param payload          PyDbg command payload data.
*/
static void gaiaDebugPlugin_RemoteDbgCmd_MemoryWriteReq(GAIA_TRANSPORT *t, uint8 gaia_client_id, uint8 gaia_tag, uint16 pydbg_seq_no, uint16 payload_length, const uint8 *payload);


/*! \brief Handle Chip_Reset_Req PyDbg Remote Debug command.

    \param client_id        An identifier assigned and used by the host (the mobile app).

    \param tag              Another identifier used by the host (the mobile app).

    \param pydbg_seq_no     PyDbg Tag (Sequence Number).

    \param payload_length   Payload size of the PyDbg command payload (in bytes).

    \param payload          PyDbg command payload data.
*/
static void gaiaDebugPlugin_RemoteDbgCmd_ChipResetReq(GAIA_TRANSPORT *t, uint8 gaia_client_id, uint8 gaia_tag, uint16 pydbg_seq_no, uint16 payload_length, const uint8 *payload);



void GaiaDebugPlugin_DebugTunnelToChip(GAIA_TRANSPORT *t, uint16 payload_length, const uint8 *payload)
{
    DEBUG_LOG_DEBUG("GaiaDebugPlugin DbgTunnelToChip");

    if (payload_length >= (number_of_debug_plugin_debug_tunnel_to_chip_cmd_bytes + PYDBG_REMOTE_DEBUG_PDU_HEADER_SIZE))
    {
        uint8 client_id = payload[debug_plugin_debug_tunnel_to_chip_cmd_client_id];
        uint8 tag       = payload[debug_plugin_debug_tunnel_to_chip_cmd_tag];

        gaiaDebugPlugin_PyDbgCommandHandler(t, client_id, tag,
                                            payload_length - number_of_debug_plugin_debug_tunnel_to_chip_cmd_bytes,
                                            &payload[debug_plugin_debug_tunnel_to_chip_cmd_payload_0]);
    }
    else
    {
        DEBUG_LOG_WARN("gaiaDebugPlugin TunnelToChip: ERROR! Invalid PDU Size:%d", payload_length);
        GaiaFramework_SendError(t, GAIA_DEBUG_FEATURE_ID, debug_tunnel_to_chip, debug_plugin_status_invalid_parameters);
    }
}



static void gaiaDebugPlugin_PyDbgCommandHandler(GAIA_TRANSPORT *t, uint8 gaia_client_id, uint8 gaia_tag, uint16 pydbg_pdu_length, const uint8 *pydbg_pdu)
{
    /* PyDbg Remote Debug Command Format
     *      0        1        2        3        4        5        6       ...       N    (Byte)
     *  +--------+--------+--------+--------+--------+--------+--------+--------+--------+
     *  |  Type  | Cmd ID |  Payload Length |  Tag (Seq No.)  |     Payload (if any)     |
     *  +--------+--------+--------+--------+--------+--------+--------+--------+--------+
     *  |<-----   PyDbg Remote Debug Protocol Header    ----->|<---- PyDbg Payload  ---->|
     */
    REMOTE_DEBUG_TRANSPORT_CMD_PAYLOAD *cmd_payload = (REMOTE_DEBUG_TRANSPORT_CMD_PAYLOAD*) &pydbg_pdu[1];
    uint8 pdu_type = pydbg_pdu[0];
    REMOTE_DEBUG_CMD cmd_id     = REMOTE_DEBUG_TRANSPORT_CMD_PAYLOAD_TRANSPORT_COMMAND_GET(cmd_payload);
    uint16 pydbg_seq_no         = REMOTE_DEBUG_TRANSPORT_CMD_PAYLOAD_TAG_GET(cmd_payload);
    uint16 pydbg_payload_length = REMOTE_DEBUG_TRANSPORT_CMD_PAYLOAD_PAYLOAD_LENGTH_GET(cmd_payload);
    const uint8 *pydbg_payload = NULL;

    DEBUG_LOG_DEBUG("gaiaDebugPlugin TunnelToChip: PyDbgCmdHdlr(Type:0x%02X, CmdID:%02X, Len:%04X, SeqNo:%04X)", pdu_type, cmd_id, pydbg_payload_length, pydbg_seq_no);

    if (pydbg_pdu_length != (PYDBG_REMOTE_DEBUG_PDU_HEADER_SIZE + pydbg_payload_length))   /* Sanity check */
    {
        DEBUG_LOG_WARN("gaiaDebugPlugin TunnelToChip: ERROR! Invalid PyDbg PDU length:(%d + %d) != (Received:%d)", pydbg_payload_length, PYDBG_REMOTE_DEBUG_PDU_HEADER_SIZE, pydbg_pdu_length);
        GaiaFramework_SendError(t, GAIA_DEBUG_FEATURE_ID, debug_tunnel_to_chip, debug_plugin_status_invalid_parameters);
        return;
    }

    if (0 < pydbg_payload_length)
    {
        pydbg_payload = &pydbg_pdu[PYDBG_REMOTE_DEBUG_PDU_HEADER_SIZE];
    }


    if (pdu_type == REMOTE_DEBUG_CMD_TYPE_DEBUG_CMD)
    {
        switch (cmd_id)
        {
            case REMOTE_DEBUG_CMD_PROTOCOL_VERSION_REQ:
                gaiaDebugPlugin_RemoteDbgCmd_ProtocolVersionReq(t, gaia_client_id, gaia_tag, pydbg_seq_no, pydbg_payload_length, pydbg_payload);
                break;
            case REMOTE_DEBUG_CMD_MAX_PDU_SIZE_REQ:
                gaiaDebugPlugin_RemoteDbgCmd_MaxPduSizeReq(t, gaia_client_id, gaia_tag, pydbg_seq_no, pydbg_payload_length, pydbg_payload);
                break;
            case REMOTE_DEBUG_CMD_MEMORY_READ_REQ:
                gaiaDebugPlugin_RemoteDbgCmd_MemoryReadReq(t, gaia_client_id, gaia_tag, pydbg_seq_no, pydbg_payload_length, pydbg_payload);
                break;
            case REMOTE_DEBUG_CMD_MEMORY_WRITE_REQ:
                gaiaDebugPlugin_RemoteDbgCmd_MemoryWriteReq(t, gaia_client_id, gaia_tag, pydbg_seq_no, pydbg_payload_length, pydbg_payload);
                break;
            case REMOTE_DEBUG_CMD_CHIP_RESET_REQ:
                gaiaDebugPlugin_RemoteDbgCmd_ChipResetReq(t, gaia_client_id, gaia_tag, pydbg_seq_no, pydbg_payload_length, pydbg_payload);
                break;

            case REMOTE_DEBUG_CMD_APPCMD_REQ:
                /* Fall through */
            default:
                DEBUG_LOG_WARN("gaiaDebugPlugin TunnelToChip: ERROR! Invalid PyDbg CmdID:%d", cmd_id);
                GaiaFramework_SendError(t, GAIA_DEBUG_FEATURE_ID, debug_tunnel_to_chip, debug_plugin_status_invalid_parameters);
                break;
        }
    }
    else
    {
        /* The chip code does not expect other than the 'Debug Type'.
         * ('REMOTE_DEBUG_CMD_TYPE_TRANSPORT_CMD' is expected to be used between the mobile app and PyDbg running on PC.) */
        DEBUG_LOG_WARN("gaiaDebugPlugin TunnelToChip: ERROR! Invalid PyDbg PDU Type:%d", pdu_type);
        GaiaFramework_SendError(t, GAIA_DEBUG_FEATURE_ID, debug_tunnel_to_chip, debug_plugin_status_invalid_parameters);
    }
}


static void gaiaDebugPlugin_RemoteDbgCmd_ProtocolVersionReq(GAIA_TRANSPORT *t, uint8 gaia_client_id, uint8 gaia_tag, uint16 pydbg_seq_no, uint16 payload_length, const uint8 *payload)
{
    UNUSED(payload);

    uint32 protocol_version = REMOTE_DEBUG_PROTOCOL_VERSION_T_PROTOCOL_VERSION;
    uint32 capabilities = GAIA_DEBUG_FEATURE_RESERVED_FOR_FUTURE_USE_UINT32;
    uint16 device_id = GAIA_DEBUG_FEATURE_RESERVED_FOR_FUTURE_USE_UINT16;
    uint8 padding = 0x00;

    DEBUG_LOG_DEBUG("gaiaDebugPlugin TunnelToChip: PyDbg(Type:Dbg) ProtocolVerReq");
    if (payload_length == 0)     /* Sanity check */
    {
        uint16 rsp_payload_size = GAIA_DEBUG_TUNNEL_PYDBG_HEADER_SIZE + sizeof(REMOTE_DEBUG_PROTOCOL_VERSION_RSP_T);
        uint8 *rsp_payload = (uint8*) PanicUnlessMalloc(rsp_payload_size);
        REMOTE_DEBUG_PROTOCOL_VERSION_RSP_T *pydbg_payload_ptr = (REMOTE_DEBUG_PROTOCOL_VERSION_RSP_T*) &rsp_payload[GAIA_DEBUG_TUNNEL_PYDBG_HEADER_SIZE];

        REMOTE_DEBUG_PROTOCOL_VERSION_RSP_T_PACK(pydbg_payload_ptr, protocol_version, capabilities, device_id, padding);

        SET_DEBUG_TUNNEL_TO_CHIP_RESPONSE_PAYLOAD_HEADER(rsp_payload, gaia_client_id, gaia_tag, \
            REMOTE_DEBUG_CMD_PROTOCOL_VERSION_RSP, sizeof(REMOTE_DEBUG_PROTOCOL_VERSION_RSP_T), pydbg_seq_no);

        GaiaFramework_SendResponse(t, GAIA_DEBUG_FEATURE_ID, debug_tunnel_to_chip, rsp_payload_size, rsp_payload);
        free(rsp_payload);
    }
    else
    {
        DEBUG_LOG_WARN("gaiaDebugPlugin TunnelToChip, ProtocolVerReq: ERROR! Invalid PyDbg Cmd Payload length:%d", payload_length);
        GaiaFramework_SendError(t, GAIA_DEBUG_FEATURE_ID, debug_tunnel_to_chip, debug_plugin_status_invalid_parameters);
    }
}


static void gaiaDebugPlugin_RemoteDbgCmd_MaxPduSizeReq(GAIA_TRANSPORT *t, uint8 gaia_client_id, uint8 gaia_tag, uint16 pydbg_seq_no, uint16 payload_length, const uint8 *payload)
{
    UNUSED(payload);

    /* Note that 'pdu_size_bytes' is fixed to PYDBG_REMOTE_DEBUG_PAYLOAD_SIZE
     * for the moment, but this would be replaced by the way Gaia 'Get Transport
     * Information' command will do. */
    uint32 pdu_size_bytes = PYDBG_REMOTE_DEBUG_PAYLOAD_SIZE;
    uint32 number_of_outstanding_packets = PYDBG_REMOTE_DEBUG_NUMBER_OF_OUTSTANDING_PACKETS;

    DEBUG_LOG_DEBUG("gaiaDebugPlugin TunnelToChip: PyDbg(Type:Dbg) MaxPduSizeReq");
    if (payload_length == 0)     /* Sanity check */
    {
        uint16 rsp_payload_size = GAIA_DEBUG_TUNNEL_PYDBG_HEADER_SIZE + sizeof(REMOTE_DEBUG_MAX_PDU_SIZE_RSP_T);
        uint8 *rsp_payload = (uint8*) PanicUnlessMalloc(rsp_payload_size);
        REMOTE_DEBUG_MAX_PDU_SIZE_RSP_T *pydbg_payload_ptr = (REMOTE_DEBUG_MAX_PDU_SIZE_RSP_T*) &rsp_payload[GAIA_DEBUG_TUNNEL_PYDBG_HEADER_SIZE];

        REMOTE_DEBUG_MAX_PDU_SIZE_RSP_T_PACK(pydbg_payload_ptr, pdu_size_bytes, number_of_outstanding_packets);

        SET_DEBUG_TUNNEL_TO_CHIP_RESPONSE_PAYLOAD_HEADER(rsp_payload, gaia_client_id, gaia_tag, \
            REMOTE_DEBUG_CMD_MAX_PDU_SIZE_RSP, sizeof(REMOTE_DEBUG_MAX_PDU_SIZE_RSP_T), pydbg_seq_no);

        GaiaFramework_SendResponse(t, GAIA_DEBUG_FEATURE_ID, debug_tunnel_to_chip, rsp_payload_size, rsp_payload);
        free(rsp_payload);
    }
    else
    {
        DEBUG_LOG_WARN("gaiaDebugPlugin TunnelToChip, MaxPduSizeReq: ERROR! Invalid PyDbg Cmd Payload length:%d", payload_length);
        GaiaFramework_SendError(t, GAIA_DEBUG_FEATURE_ID, debug_tunnel_to_chip, debug_plugin_status_invalid_parameters);
    }
}


static REMOTE_DEBUG_TRB gaiaDebugPlugin_CheckMemoryReadWriteWordAlignment(mem_ptr_t addr, uint8 width, uint16 data_size)
{
    switch (width)
    {
        case 1:     /* Byte access */
            /* No need to check. Single-byte width access has no alignment issues. */
            break;

        case 2:     /* Word (16-bit) access */
            if (addr & 1UL)
                return REMOTE_DEBUG_TRB_BAD_ALIGNMENT;
            if (data_size & 1)
                return REMOTE_DEBUG_TRB_WRONG_LENGTH;
            break;

        case 4:     /* DWord (32-bit) access */
            if (addr & 3UL)
                return REMOTE_DEBUG_TRB_BAD_ALIGNMENT;
            if (data_size & 3)
                return REMOTE_DEBUG_TRB_WRONG_LENGTH;
            break;

        default:
            return REMOTE_DEBUG_TRB_WRONG_LENGTH;
    }

    return REMOTE_DEBUG_TRB_NO_ERROR;
}

#if !defined(ENFORCE_STRICT_WIDTH_CHECK_FOR_PYDBG_REMOTE_DEBUG)
static uint8 gaiaDebugPlugin_AdjustAccessWidth(mem_ptr_t addr, uint8 width, uint16 data_size)
{
    /* Pydbg's initial support for PyDbg Remote Debug fixes the access width,
     * or 'Bytes per transaction' parameter to 4 bytes. Not all the accesses
     * are dword-size and this code overrides those invalid access width. */
    if (width == 2)
    {
        if (data_size & 1 || addr & 1UL)
        {
            DEBUG_LOG_WARN("gaiaDebugPlugin TunnelToChip: Access width overridden: 2 to 1 byte");
            return 1;
        }
    }
    else if (width == 4)
    {
        if (data_size & 1 || addr & 1UL)
        {
            DEBUG_LOG_WARN("gaiaDebugPlugin TunnelToChip: Access width overridden: 4 to 1 byte");
            return 1;
        }
        if (data_size & 3 || addr & 3UL)
        {
            DEBUG_LOG_WARN("gaiaDebugPlugin TunnelToChip: Access width overridden: 4 to 2 byte");
            return 2;
        }
    }
    return width;
}
#endif /* !ENFORCE_STRICT_WIDTH_CHECK_FOR_PYDBG_REMOTE_DEBUG) */

static REMOTE_DEBUG_TRB gaiaDebugPlugin_MemoryRead(mem_ptr_t addr, uint8 width, uint16 data_size, uint8 *buf)
{
    REMOTE_DEBUG_TRB status;

    status = gaiaDebugPlugin_CheckMemoryReadWriteWordAlignment(addr, width, data_size);

    if (status == REMOTE_DEBUG_TRB_NO_ERROR)
    {
        uint16 i;

        if (2 == width)
        {
            uint16 *read_addr = (uint16*) addr;
            uint16 word_value;

            for (i = 0; i < data_size / 2; i++)
            {
                word_value = *read_addr;
                buf[i * 2 + 0] = (uint8)(word_value & 0x00FF);
                buf[i * 2 + 1] = (uint8)(word_value >> 8);
                read_addr++;
            }
        }
        else if (4 == width)
        {
            uint32 *read_addr = (uint32*) addr;
            uint32 dword_value;

            for (i = 0; i < data_size / 4; i++)
            {
                dword_value = *read_addr;
                buf[i * 4 + 0] = (uint8) (dword_value & 0x000000FFUL);
                buf[i * 4 + 1] = (uint8)((dword_value & 0x0000FF00UL) >> 8);
                buf[i * 4 + 2] = (uint8)((dword_value & 0x00FF0000UL) >> 16);
                buf[i * 4 + 3] = (uint8)((dword_value & 0xFF000000UL) >> 24);
                read_addr++;
            }
        }
        else
        {
            MEMMOVE(buf, (void*)addr, data_size);
        }
    }

    return status;
}


static void gaiaDebugPlugin_RemoteDbgCmd_MemoryReadReq(GAIA_TRANSPORT *t, uint8 gaia_client_id, uint8 gaia_tag, uint16 pydbg_seq_no, uint16 payload_length, const uint8 *payload)
{
    uint16 device_id = GAIA_DEBUG_FEATURE_RESERVED_FOR_FUTURE_USE_UINT16;
    REMOTE_DEBUG_MEMORY_READ_REQ_T *req = (REMOTE_DEBUG_MEMORY_READ_REQ_T*) &payload[0];
    subsystem_id_t subsystem_id = REMOTE_DEBUG_MEMORY_READ_REQ_T_SUBSYSTEM_ID_GET(req);
    apps_ss_block_id_t subsystem_block_id = REMOTE_DEBUG_MEMORY_READ_REQ_T_BLOCK_ID_GET(req);
    uint8 access_width = REMOTE_DEBUG_MEMORY_READ_REQ_T_BYTES_PER_TRANSACTION_GET(req);
    /* REMOTE_DEBUG_TR_TYPE transaction_type = REMOTE_DEBUG_MEMORY_READ_REQ_T_TRANSACTION_TYPE_GET(req); */     /* Reserved For Future Use */
    mem_ptr_t read_addr = REMOTE_DEBUG_MEMORY_READ_REQ_T_ADDRESS_GET(req);
    uint32 read_length = REMOTE_DEBUG_MEMORY_READ_REQ_T_READ_LENGTH_BYTES_GET(req);
    REMOTE_DEBUG_TRB status = REMOTE_DEBUG_TRB_NO_ERROR;

    DEBUG_LOG_DEBUG("gaiaDebugPlugin TunnelToChip: PyDbg(Type:Dbg) MemoryReadReq");

    if (payload_length != REMOTE_DEBUG_MEMORY_READ_REQ_T_BYTE_SIZE)
    {
        DEBUG_LOG_WARN("gaiaDebugPlugin TunnelToChip, MemoryReadReq: ERROR! Invalid PyDbg Cmd Payload length:%d", payload_length);
        GaiaFramework_SendError(t, GAIA_DEBUG_FEATURE_ID, debug_tunnel_to_chip, debug_plugin_status_invalid_parameters);
        return;
    }

    if (PYDBG_REMOTE_DEBUG_PAYLOAD_SIZE < read_length)
    {
        /* The requested read length exceeds the maximum payload size Gaia can carry. */
        status = REMOTE_DEBUG_TRB_WRONG_LENGTH;
    }
    else if (subsystem_id != subsystem_id_app || IS_APPS_P1_BLOCK_ID(subsystem_block_id) == FALSE)
    {
        /* (!) This implementation supports only Apps-P1 Data/Program memory reads.
            *     The host tried to a subsystem and/or subsystem block which are not supported. */
        status = REMOTE_DEBUG_TRB_ACCESS_PROTECTION;
    }
    else
    {
        uint16 data_size = (uint16) read_length;
        uint8 dummy_data0 = 0;
        uint16 rsp_payload_size = GAIA_DEBUG_TUNNEL_PYDBG_HEADER_SIZE + sizeof(REMOTE_DEBUG_MEMORY_READ_RSP_T) + data_size - sizeof(uint8);
        uint8 *rsp_payload = (uint8*) PanicUnlessMalloc(rsp_payload_size);
        REMOTE_DEBUG_MEMORY_READ_RSP_T *pydbg_payload_ptr = (REMOTE_DEBUG_MEMORY_READ_RSP_T*) &rsp_payload[GAIA_DEBUG_TUNNEL_PYDBG_HEADER_SIZE];
        uint8 *data_ptr = &rsp_payload[GAIA_DEBUG_TUNNEL_PYDBG_HEADER_SIZE + sizeof(REMOTE_DEBUG_MEMORY_READ_RSP_T) - sizeof(uint8)];

        REMOTE_DEBUG_MEMORY_READ_RSP_T_PACK(pydbg_payload_ptr, status, device_id, dummy_data0);

#if defined(HOSTED_TEST_ENVIRONMENT)
#if PTRDIFF_MAX > 0xFFFFFFFFUL
        read_addr |= (full_64bit_memory_addr & 0xFFFFFFFF00000000ULL);
#endif
#else /* HOSTED_TEST_ENVIRONMENT */
        if (subsystem_block_id == apps_ss_block_id_apps_p1_program_mem)
        {
            mem_ptr_t apps_p1_pm_offset = (mem_ptr_t) gaiaDebugPlugin_GetDataRomWindowAddr();
            read_addr = apps_p1_pm_offset + read_addr;
        }
#endif /* HOSTED_TEST_ENVIRONMENT */

#if !defined(ENFORCE_STRICT_WIDTH_CHECK_FOR_PYDBG_REMOTE_DEBUG)
        access_width = gaiaDebugPlugin_AdjustAccessWidth(read_addr, access_width, data_size);
#endif
        status = gaiaDebugPlugin_MemoryRead(read_addr, access_width, data_size, data_ptr);

        if (status == REMOTE_DEBUG_TRB_NO_ERROR)
        {
            DEBUG_LOG_DEBUG("gaiaDebugPlugin TunnelToChip: PyDbg(Type:Dbg) MemRead Addr: %p (Width:%d, Size:%d)", read_addr, access_width, data_size);

            SET_DEBUG_TUNNEL_TO_CHIP_RESPONSE_PAYLOAD_HEADER(rsp_payload, gaia_client_id, gaia_tag, \
                REMOTE_DEBUG_CMD_MEMORY_READ_RSP, sizeof(REMOTE_DEBUG_MEMORY_READ_RSP_T) - sizeof(uint8) + data_size, pydbg_seq_no);

            GaiaFramework_SendResponse(t, GAIA_DEBUG_FEATURE_ID, debug_tunnel_to_chip, rsp_payload_size, rsp_payload);
        }
        free(rsp_payload);
    }

    if (status != REMOTE_DEBUG_TRB_NO_ERROR)
    {
        /* Cannot fulfill the request due to the requested PyDbg 'MemoryRead_Req' command parameters. */
        uint16 rsp_payload_size = GAIA_DEBUG_TUNNEL_PYDBG_HEADER_SIZE + sizeof(REMOTE_DEBUG_MEMORY_READ_RSP_T);
        uint8 *rsp_payload = (uint8*) PanicUnlessMalloc(rsp_payload_size);
        REMOTE_DEBUG_MEMORY_READ_RSP_T *pydbg_payload_ptr = (REMOTE_DEBUG_MEMORY_READ_RSP_T*) &rsp_payload[GAIA_DEBUG_TUNNEL_PYDBG_HEADER_SIZE];

        DEBUG_LOG_DEBUG("gaiaDebugPlugin TunnelToChip: PyDbg(Type:Dbg) MemRead Addr: %p (Width:%d, Size:%ld) ERR:0x%02X", read_addr, access_width, read_length, status);
        REMOTE_DEBUG_MEMORY_READ_RSP_T_PACK(pydbg_payload_ptr, status, device_id, 0);   /* NB: uint8 status, uint8 device_id, uint8 data = 0 */

        SET_DEBUG_TUNNEL_TO_CHIP_RESPONSE_PAYLOAD_HEADER(rsp_payload, gaia_client_id, gaia_tag, \
            REMOTE_DEBUG_CMD_MEMORY_READ_RSP, sizeof(REMOTE_DEBUG_MEMORY_READ_RSP_T), pydbg_seq_no);

        GaiaFramework_SendResponse(t, GAIA_DEBUG_FEATURE_ID, debug_tunnel_to_chip, sizeof(rsp_payload), rsp_payload);  /* NB: This includes the first data byte (uint16). */
        free(rsp_payload);
    }
}


static REMOTE_DEBUG_TRB gaiaDebugPlugin_MemoryWrite(mem_ptr_t addr, uint8 width, uint16 data_size, const uint8 *data)
{
    REMOTE_DEBUG_TRB status;

    status = gaiaDebugPlugin_CheckMemoryReadWriteWordAlignment(addr, width, data_size);

    if (status == REMOTE_DEBUG_TRB_NO_ERROR)
    {
        uint16 i;

        if (2 == width)
        {
            uint16 *write_addr = (uint16*) addr;
            uint16 a_word;

            for (i = 0; i < data_size; i+= 2)
            {
                a_word = ((uint16) data[i + 1] << 8) | (uint16) data[i + 0];
                *write_addr = a_word;
                write_addr++;
            }
        }
        else if (4 == width)
        {
            uint32 *write_addr = (uint32*) addr;
            uint32 a_dword;

            for (i = 0; i < data_size; i+= 4)
            {
                a_dword = ((uint32) data[i + 3] << 24) | ((uint32) data[i + 2] << 16) | ((uint32) data[i + 1] << 8) | (uint32) data[i + 0];
                *write_addr = a_dword;
                write_addr++;
            }
        }
        else
        {
            MEMMOVE((void*)addr, data, data_size);
        }
    }

    return status;
}


static void gaiaDebugPlugin_RemoteDbgCmd_MemoryWriteReq(GAIA_TRANSPORT *t, uint8 gaia_client_id, uint8 gaia_tag, uint16 pydbg_seq_no, uint16 payload_length, const uint8 *payload)
{
    uint16 device_id = GAIA_DEBUG_FEATURE_RESERVED_FOR_FUTURE_USE_UINT16;

    DEBUG_LOG_DEBUG("gaiaDebugPlugin TunnelToChip: PyDbg(Type:Dbg) MemoryWriteReq");
    if (PYDBG_REMOTE_DEBUG_MEMORY_WRITE_REQ_CMD_HEADER_SIZE < payload_length)   /* Sanity check */
    {
        REMOTE_DEBUG_MEMORY_WRITE_REQ_T *req = (REMOTE_DEBUG_MEMORY_WRITE_REQ_T*) &payload[0];
        subsystem_id_t subsystem_id = REMOTE_DEBUG_MEMORY_WRITE_REQ_T_SUBSYSTEM_ID_GET(req);
        apps_ss_block_id_t subsystem_block_id = REMOTE_DEBUG_MEMORY_WRITE_REQ_T_BLOCK_ID_GET(req);
        REMOTE_DEBUG_TRB status = REMOTE_DEBUG_TRB_NO_ERROR;

        /* (!) This implementation supports only Apps-P1 Data/Program memory reads. */
        if (subsystem_id == subsystem_id_app && subsystem_block_id == apps_ss_block_id_apps_p1_data_mem)
        {
            uint8 access_width = REMOTE_DEBUG_MEMORY_WRITE_REQ_T_BYTES_PER_TRANSACTION_GET(req);
            /* REMOTE_DEBUG_TR_TYPE transaction_type = REMOTE_DEBUG_MEMORY_WRITE_REQ_T_TRANSACTION_TYPE_GET(req); */    /* Reserved For Future Use */
            mem_ptr_t write_addr = REMOTE_DEBUG_MEMORY_WRITE_REQ_T_ADDRESS_GET(req);
#if defined(HOSTED_TEST_ENVIRONMENT) && PTRDIFF_MAX > 0xFFFFFFFFUL
            write_addr |= (full_64bit_memory_addr & 0xFFFFFFFF00000000ULL);
#endif
            const uint8 *data = &payload[PYDBG_REMOTE_DEBUG_MEMORY_WRITE_REQ_CMD_HEADER_SIZE];
            uint16 data_size = payload_length - PYDBG_REMOTE_DEBUG_MEMORY_WRITE_REQ_CMD_HEADER_SIZE;

#if !defined(ENFORCE_STRICT_WIDTH_CHECK_FOR_PYDBG_REMOTE_DEBUG)
            access_width = gaiaDebugPlugin_AdjustAccessWidth(write_addr, access_width, data_size);
#endif
            status = gaiaDebugPlugin_MemoryWrite(write_addr, access_width, data_size, data);
            DEBUG_LOG_DEBUG("gaiaDebugPlugin TunnelToChip: PyDbg(Type:Dbg) MemWrite Addr: %p (Width:%d, Size:%d)", write_addr, access_width, data_size);
        }
        else
        {
            /* The host tried to a subsystem and/or subsystem block which are not supported. */
            status = REMOTE_DEBUG_TRB_ACCESS_PROTECTION;
        }

        /* Send the response to the mobile app. */
        if (status == REMOTE_DEBUG_TRB_NO_ERROR)
        {
            uint16 rsp_payload_size = GAIA_DEBUG_TUNNEL_PYDBG_HEADER_SIZE + sizeof(REMOTE_DEBUG_MEMORY_WRITE_RSP_T);
            uint8 *rsp_payload = (uint8*) PanicUnlessMalloc(rsp_payload_size);
            REMOTE_DEBUG_MEMORY_WRITE_RSP_T *pydbg_payload_ptr = (REMOTE_DEBUG_MEMORY_WRITE_RSP_T*) &rsp_payload[GAIA_DEBUG_TUNNEL_PYDBG_HEADER_SIZE];

            REMOTE_DEBUG_MEMORY_WRITE_RSP_T_PACK(pydbg_payload_ptr, status, device_id);
            SET_DEBUG_TUNNEL_TO_CHIP_RESPONSE_PAYLOAD_HEADER(rsp_payload, gaia_client_id, gaia_tag, \
                REMOTE_DEBUG_CMD_MEMORY_WRITE_RSP, sizeof(REMOTE_DEBUG_MEMORY_WRITE_RSP_T), pydbg_seq_no);
            GaiaFramework_SendResponse(t, GAIA_DEBUG_FEATURE_ID, debug_tunnel_to_chip, rsp_payload_size, rsp_payload);
            free(rsp_payload);
        }
        else
        {
            uint16 rsp_payload_size = GAIA_DEBUG_TUNNEL_PYDBG_HEADER_SIZE + sizeof(REMOTE_DEBUG_MEMORY_WRITE_RSP_T);
            uint8 *rsp_payload = (uint8*) PanicUnlessMalloc(rsp_payload_size);
            REMOTE_DEBUG_MEMORY_WRITE_RSP_T *pydbg_payload_ptr = (REMOTE_DEBUG_MEMORY_WRITE_RSP_T*) &rsp_payload[GAIA_DEBUG_TUNNEL_PYDBG_HEADER_SIZE];

            REMOTE_DEBUG_MEMORY_WRITE_RSP_T_PACK(pydbg_payload_ptr, status, device_id);

            SET_DEBUG_TUNNEL_TO_CHIP_RESPONSE_PAYLOAD_HEADER(rsp_payload, gaia_client_id, gaia_tag, \
                REMOTE_DEBUG_CMD_MEMORY_WRITE_RSP, sizeof(REMOTE_DEBUG_MEMORY_WRITE_RSP_T), pydbg_seq_no);
            GaiaFramework_SendResponse(t, GAIA_DEBUG_FEATURE_ID, debug_tunnel_to_chip, sizeof(rsp_payload), rsp_payload);
            free(rsp_payload);
        }
    }
    else
    {
        DEBUG_LOG_WARN("gaiaDebugPlugin TunnelToChip, MemoryWriteReq: ERROR! Invalid PyDbg Cmd Payload length:%d", payload_length);
        GaiaFramework_SendError(t, GAIA_DEBUG_FEATURE_ID, debug_tunnel_to_chip, debug_plugin_status_invalid_parameters);
    }
}


static void gaiaDebugPlugin_RemoteDbgCmd_ChipResetReq(GAIA_TRANSPORT *t, uint8 gaia_client_id, uint8 gaia_tag, uint16 pydbg_seq_no, uint16 payload_length, const uint8 *payload)
{
    DEBUG_LOG_DEBUG("gaiaDebugPlugin TunnelToChip: PyDbg(Type:Dbg) ChipResetReq");
    if (REMOTE_DEBUG_CHIP_RESET_REQ_T_BYTE_SIZE == payload_length)   /* Sanity check */
    {
        /* Don't bother to use 'REMOTE_DEBUG_CHIP_RESET_REQ_T' which is uint16
         * based, but the command payload is just a single byte. */
        uint8 reset_type = payload[0];    /* Reserved for Future Use. */
        REMOTE_DEBUG_TRB status = REMOTE_DEBUG_TRB_NO_ERROR;
        uint16 rsp_payload_size = GAIA_DEBUG_TUNNEL_PYDBG_HEADER_SIZE + sizeof(REMOTE_DEBUG_CHIP_RESET_RSP_T);
        uint8 *rsp_payload = (uint8*) PanicUnlessMalloc(rsp_payload_size);
        REMOTE_DEBUG_CHIP_RESET_RSP_T *pydbg_payload_ptr = (REMOTE_DEBUG_CHIP_RESET_RSP_T*) &rsp_payload[GAIA_DEBUG_TUNNEL_PYDBG_HEADER_SIZE];

        REMOTE_DEBUG_CHIP_RESET_RSP_T_PACK(pydbg_payload_ptr, status);
        SET_DEBUG_TUNNEL_TO_CHIP_RESPONSE_PAYLOAD_HEADER(rsp_payload, gaia_client_id, gaia_tag, \
            REMOTE_DEBUG_CMD_CHIP_RESET_RSP, sizeof(REMOTE_DEBUG_CHIP_RESET_RSP_T), pydbg_seq_no);
        GaiaFramework_SendResponse(t, GAIA_DEBUG_FEATURE_ID, debug_tunnel_to_chip, rsp_payload_size, rsp_payload);
        free(rsp_payload);

        DEBUG_LOG_DEBUG("gaiaDebugPlugin TunnelToChip: PyDbg(Type:Dbg) ChipResetReq: Type:   0x%02X", reset_type);
        DEBUG_LOG_DEBUG("gaiaDebugPlugin TunnelToChip: PyDbg(Type:Dbg) ChipResetRsp: Status: 0x%02X", status);

        appPowerReboot();   /* Reset the chip here. */
    }
    else
    {
        DEBUG_LOG_WARN("gaiaDebugPlugin TunnelToChip, ChipResetReq: ERROR! Invalid PyDbg Cmd Payload length:%d", payload_length);
        GaiaFramework_SendError(t, GAIA_DEBUG_FEATURE_ID, debug_tunnel_to_chip, debug_plugin_status_invalid_parameters);
    }
}
#endif /* INCLUDE_GAIA_PYDBG_REMOTE_DEBUG */
