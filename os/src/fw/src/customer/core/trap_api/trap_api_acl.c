/* Copyright (c) 2018 Qualcomm Technologies International, Ltd. */
/*   %%version */
/**
 * \file
 * Implementations of non-autogenerated TW Mirroring-related trap calls
 */


#include "trap_api/trap_api_private.h"

#if TRAPSET_ACL_CONTROL

bool AclReceiveEnable(const tp_bdaddr *addr, bool enable, uint32 timeout)
{
    IPC_ACL_RECEIVE_ENABLE ipc_send_prim;
    IPC_BOOL_RSP ipc_recv_prim;
    ipc_send_prim.addr = (const TP_BD_ADDR_T *)addr;
    ipc_send_prim.enable = enable;
    ipc_send_prim.timeout = timeout;
    ipc_transaction(IPC_SIGNAL_ID_ACL_RECEIVE_ENABLE, &ipc_send_prim, sizeof(ipc_send_prim),
                    IPC_SIGNAL_ID_ACL_RECEIVE_ENABLE_RSP, &ipc_recv_prim);
    return ipc_recv_prim.ret;
}

acl_rx_processed_status AclReceivedDataProcessed(const tp_bdaddr *addr, uint32 timeout)
{
    IPC_ACL_RECEIVED_DATA_PROCESSED ipc_send_prim;
    IPC_ACL_RX_PROCESSED_STATUS_RSP ipc_recv_prim;
    ipc_send_prim.addr = (const TP_BD_ADDR_T *)addr;
    ipc_send_prim.timeout = timeout;
    ipc_transaction(IPC_SIGNAL_ID_ACL_RECEIVED_DATA_PROCESSED, &ipc_send_prim, sizeof(ipc_send_prim),
                    IPC_SIGNAL_ID_ACL_RECEIVED_DATA_PROCESSED_RSP, &ipc_recv_prim);
    return ipc_recv_prim.ret;
}

bool AclTransmitDataPending(const tp_bdaddr *addr)
{
    IPC_ACL_TRANSMIT_DATA_PENDING ipc_send_prim;
    IPC_BOOL_RSP ipc_recv_prim;
    ipc_send_prim.addr = (const TP_BD_ADDR_T *)addr;
    ipc_transaction(IPC_SIGNAL_ID_ACL_TRANSMIT_DATA_PENDING, &ipc_send_prim, sizeof(ipc_send_prim),
                    IPC_SIGNAL_ID_ACL_TRANSMIT_DATA_PENDING_RSP, &ipc_recv_prim);
    return ipc_recv_prim.ret;
}

#endif /* TRAPSET_ACL_CONTROL */

#if TRAPSET_MIRRORING

uint16 AclHandoverPrepare(const tp_bdaddr *acl_addr, const tp_bdaddr *recipient)
{
    IPC_ACL_HANDOVER_PREPARE ipc_send_prim;
    IPC_UINT16_RSP ipc_recv_prim;
    ipc_send_prim.acl_addr = (const TP_BD_ADDR_T *)acl_addr;
    ipc_send_prim.recipient = (const TP_BD_ADDR_T *)recipient;
    ipc_transaction(IPC_SIGNAL_ID_ACL_HANDOVER_PREPARE, &ipc_send_prim, sizeof(ipc_send_prim),
                    IPC_SIGNAL_ID_ACL_HANDOVER_PREPARE_RSP, &ipc_recv_prim);
    return ipc_recv_prim.ret;
}

bool AclReliableMirrorDebugStatistics(const tp_bdaddr *tpaddr, 
                acl_reliable_mirror_debug_statistics_t *statistics)
{
    IPC_ACL_RELIABLE_MIRROR_DEBUG_STATISTICS ipc_send_prim;
    IPC_BOOL_RSP ipc_recv_prim;
    
    if ((tpaddr == NULL) || (statistics == NULL))
    {
        return FALSE;
    }

    ipc_send_prim.tpaddr = (const TP_BD_ADDR_T *)tpaddr;
    ipc_send_prim.statistics = statistics;

    ipc_transaction(IPC_SIGNAL_ID_ACL_RELIABLE_MIRROR_DEBUG_STATISTICS,
                    &ipc_send_prim, sizeof(ipc_send_prim),
                    IPC_SIGNAL_ID_ACL_RELIABLE_MIRROR_DEBUG_STATISTICS_RSP,
                    &ipc_recv_prim);
    return ipc_recv_prim.ret;
}

#endif /* TRAPSET_MIRRORING */

