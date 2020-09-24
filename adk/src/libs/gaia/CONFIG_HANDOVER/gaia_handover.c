/****************************************************************************
Copyright (c) 2020 Qualcomm Technologies International, Ltd.


FILE NAME
    gaia_handover.c

DESCRIPTION
    Implements GAIA handover logic (Veto, Marshals/Unmarshals, Handover, etc).
 
NOTES
    See handover_if.h for further interface description
    
    Builds requiring this should include CONFIG_HANDOVER in the
    makefile. e.g.
        CONFIG_FEATURES:=CONFIG_HANDOVER   
 */

#define DEBUG_LOG_MODULE_NAME gaia
#include <logging.h>

#include "gaia_marshal_desc.h"
#include "gaia_private.h"
#include "gaia_transport.h"
#include "gaia_transport_common.h"
#include "gaia_handover_policy.h"
#include "transport_manager.h"
#include "handover_if.h"
#include "marshal.h"
#include "bdaddr.h"
#include <connection.h>
#include <panic.h>
#include <stdlib.h>
#include <print.h>
#include <sink.h>
#include <stream.h>
#include <source.h>

static bool gaiaVeto(void);

static bool gaiaMarshal(const tp_bdaddr *tp_bd_addr,
                       uint8 *buf,
                       uint16 length,
                       uint16 *written);

static bool gaiaUnmarshal(const tp_bdaddr *tp_bd_addr,
                         const uint8 *buf,
                         uint16 length,
                         uint16 *consumed);

static void gaiaHandoverCommit(const tp_bdaddr *tp_bd_addr, const bool newRole);

static void gaiaHandoverComplete( const bool newRole );

static void gaiaHandoverAbort( void );

#if 0
static void cleanupGaiaTransport(const tp_bdaddr *tp_bd_addr);
static void stitchGaia(gaia_transport *data);
#endif

extern const handover_interface gaia_handover_if =  {
        &gaiaVeto,
        &gaiaMarshal,
        &gaiaUnmarshal,
        &gaiaHandoverCommit,
        &gaiaHandoverComplete,
        &gaiaHandoverAbort};

typedef struct
{
    unmarshaller_t unmarshaller;
    gaia_transport *data;
    tp_bdaddr bd_addr;
} gaia_marshal_instance_t;

#if 0
static gaia_marshal_instance_t *gaia_marshal_inst = NULL;
#endif

/****************************************************************************
NAME    
    gaiaHandoverAbort

DESCRIPTION
    Abort the GAIA library Handover process, free any memory
    associated with the marshalling process.

RETURNS
    void
*/
static void gaiaHandoverAbort(void)
{    
#if 0    
    if (gaia_marshal_inst)
    {
        UnmarshalDestroy(gaia_marshal_inst->unmarshaller, TRUE);
        gaia_marshal_inst->unmarshaller = NULL;
        free(gaia_marshal_inst);
        gaia_marshal_inst = NULL;
    }
#endif    
}

/****************************************************************************
NAME    
    gaiaMarshal

DESCRIPTION
    Marshal the data associated with GAIA connections
    
    Only RFCOMM transport is being marshalled to peer. If there is no transport 
    with type "gaia_transport_rfcomm" then nothing will be marshalled and TRUE
    returned.

RETURNS
    bool TRUE if GAIA module marshalling complete, otherwise FALSE
*/
static bool gaiaMarshal(const tp_bdaddr *tp_bd_addr,
                        uint8 *buf,
                        uint16 length,
                        uint16 *written)
{
#if 0
    bool marshalled = TRUE;
    uint8 idx;
    for (idx = 0; idx < gaia->transport_count; ++idx)
    {
        if (gaia->transport[idx].type == gaia_transport_rfcomm)
        {            
            typed_bdaddr addr;
            GaiaTransportGetBdAddr((GAIA_TRANSPORT*)&gaia->transport[idx], &addr);
            if(BdaddrTypedIsSame(&tp_bd_addr->taddr, &addr))
            {
                marshaller_t marshaller = MarshalInit(mtd_gaia, GAIA_MARSHAL_OBJ_TYPE_COUNT);
                PanicNull(marshaller);
                MarshalSetBuffer(marshaller, (void *) buf, length);
                marshalled = Marshal(marshaller, &(gaia->transport[idx]), MARSHAL_TYPE(gaia_transport));
                *written = marshalled ? MarshalProduced(marshaller) : 0;
                MarshalDestroy(marshaller, FALSE);
                break;
            }
        }
    }

    return marshalled;
#else
    UNUSED(tp_bd_addr);
    UNUSED(buf);
    UNUSED(length);
    UNUSED(written);
    return FALSE;
#endif    
}

/****************************************************************************
NAME    
    gaiaUnmarshal

DESCRIPTION
    Unmarshal the data associated with GAIA connections

RETURNS
    bool TRUE if GAIA unmarshalling complete, otherwise FALSE
*/
static bool gaiaUnmarshal(const tp_bdaddr *tp_bd_addr,
                          const uint8 *buf,
                          uint16 length,
                          uint16 *consumed)
{
#if 0
    marshal_type_t unmarshalled_type;
    bool result = FALSE;

    if (!gaia_marshal_inst)
    {
        /* Initiating unmarshalling, initialize the instance */
        gaia_marshal_inst = PanicUnlessNew(gaia_marshal_instance_t);
        gaia_marshal_inst->unmarshaller = UnmarshalInit(mtd_gaia, GAIA_MARSHAL_OBJ_TYPE_COUNT);
        PanicNull(gaia_marshal_inst->unmarshaller);
        gaia_marshal_inst->bd_addr = *tp_bd_addr;
    }
    else
    {
        /* Resuming the unmarshalling */
    }

    UnmarshalSetBuffer(gaia_marshal_inst->unmarshaller,
                       (void *) buf,
                       length);

    if (Unmarshal(gaia_marshal_inst->unmarshaller,
                  (void**)&gaia_marshal_inst->data,
                  &unmarshalled_type))
    {
        PanicFalse(unmarshalled_type == MARSHAL_TYPE(gaia_transport));

        /* Only expecting one object, so unmarshalling is complete */
        result = TRUE;
    }

    *consumed = UnmarshalConsumed(gaia_marshal_inst->unmarshaller);
    return result;
#else
    UNUSED(tp_bd_addr);
    UNUSED(buf);
    UNUSED(length);
    UNUSED(consumed);
    return FALSE;
#endif    
}

/****************************************************************************
NAME    
    gaiaHandoverCommit

DESCRIPTION
    The GAIA library performs time-critical actions to commit to the specified
    new role (primary or secondary)

RETURNS
    void
*/
static void gaiaHandoverCommit(const tp_bdaddr *tp_bd_addr, const bool newRole)
{
    UNUSED(tp_bd_addr);
#if 0
    if (newRole && gaia_marshal_inst)
    {
        /* Stitch unmarshalled GAIA connection instance */
        stitchGaia(gaia_marshal_inst->data);
    }
    else if(!newRole)
    {
        /* Cleanup on new Secondary */        
        cleanupGaiaTransport(tp_bd_addr);        
        gaia->outstanding_request = NULL;
        gaia->upgrade_transport = NULL;
        gaia->bitfields.data_endpoint_mode = GAIA_DATA_ENDPOINT_MODE_NONE;
        gaia->pfs_sequence = 0;
        gaia->pfs_raw_size = 0;
        gaia->bitfields.pfs_state = PFS_NONE;        
    }
#else
    UNUSED(newRole);
#endif
}

/****************************************************************************
NAME    
    gaiaHandoverComplete

DESCRIPTION
    The GAIA library performs pending actions and completes transition to 
    specified new role (primary or secondary)

RETURNS
    void
*/
static void gaiaHandoverComplete(const bool newRole)
{
#if 0
    if (newRole && gaia_marshal_inst)
    {
        UnmarshalDestroy(gaia_marshal_inst->unmarshaller, FALSE);
        gaia_marshal_inst->unmarshaller = NULL;
        free(gaia_marshal_inst);
        gaia_marshal_inst = NULL;
    }
#else
    UNUSED(newRole);
#endif
}

/****************************************************************************
NAME    
    gaiaVeto

DESCRIPTION
    Veto check for GAIA library

    Prior to handover commencing this function is called and
    the libraries internal state is checked to determine if the
    handover should proceed.

    GAIA library vetos if it has any pending messages to be handled.    

RETURNS
    bool TRUE if the GAIA Library wishes to veto the handover attempt.
*/
static bool gaiaVeto(void)
{
#if 0    
    /* Check message queue status */
    if(MessagesPendingForTask(&gaia->task_data , NULL) != 0)
    {
        return TRUE;
    }

    return FALSE;
#else
    DEBUG_LOG_INFO("gaiaVeto");
    return TRUE;
#endif
}

#if 0
static void stitchGaia(gaia_transport *data)
{    
    Source src;
    transport_mgr_link_cfg_t link_cfg;
    gaia_transport *transport = gaiaTransportFindFree();
    PanicNull (transport);
    
    /* Only RFCOMM handover is currently supported */
    PanicFalse(data->type == gaia_transport_rfcomm);

    transport->type = data->type;    
    memcpy(&transport->fields, &data->fields, sizeof(gaia_transport_bitfields));
    memcpy(&transport->battery_hi_threshold, &data->battery_hi_threshold, sizeof(int16)*2);
    memcpy(&transport->battery_lo_threshold, &data->battery_lo_threshold, sizeof(int16)*2);
    memcpy(&transport->rssi_hi_threshold, &data->rssi_hi_threshold, sizeof(int)*2);
    memcpy(&transport->rssi_lo_threshold, &data->rssi_lo_threshold, sizeof(int)*2);
    transport->state.spp.rfcomm_channel = data->state.spp.rfcomm_channel;

    /* Get Sink using RFCOMM server channel */
    transport->state.spp.sink = StreamRfcommSinkFromServerChannel(&gaia_marshal_inst->bd_addr, 
                                                                  transport->state.spp.rfcomm_channel);

    /* Set the task for connection */
    PanicFalse(VmOverrideRfcommConnContext(SinkGetRfcommConnId(transport->state.spp.sink), (conn_context_t)&gaia->task_data));

    /* Stitch Sink and task */         
    TransportMgrConfigureSink(transport->state.spp.sink);

    /* Configure Sink */
    SinkConfigure(transport->state.spp.sink, VM_SINK_MESSAGES, VM_MESSAGES_ALL);   

    /* Create link data object in Transport Manager */
    link_cfg.type = transport_mgr_type_rfcomm;
    link_cfg.trans_info.non_gatt_trans.trans_link_id = transport->state.spp.rfcomm_channel;
    if (TransportMgrCreateTransLinkData(&gaia->task_data, link_cfg, transport->state.spp.sink))
    {
        /* Existing RFCOMM channel was used for GAIA, deregister SDP record with current rfcomm 
           server handle so that new can be created */
        ConnectionUnregisterServiceRecord(&gaia->task_data, gaia->spp_sdp_handle);
    }

    /* Set the handover policy */
    src = StreamSourceFromSink(transport->state.spp.sink);
    gaiaSourceConfigureHandoverPolicy(src, SOURCE_HANDOVER_ALLOW_WITHOUT_DATA);
}

static void cleanupGaiaTransport(const tp_bdaddr *tp_bd_addr)
{
    uint8 idx;
    gaia_transport * transport = NULL;
    for (idx = 0; idx < gaia->transport_count; ++idx)
    {    
        transport = &gaia->transport[idx];
        if (transport->type == gaia_transport_rfcomm)
        {
            typed_bdaddr addr;

            /* Delete transport link data from transport manager */
            TransportMgrDeleteTransLinkData(gaia_transport_rfcomm, transport->state.spp.rfcomm_channel);

            /* Cleanup Transport state in GAIA */
            GaiaTransportGetBdAddr((GAIA_TRANSPORT*)transport, &addr);            
            if(BdaddrTypedIsSame(&tp_bd_addr->taddr, &addr))
            {      
                transport->state.spp.sink = NULL;
                transport->fields.connected = FALSE;
                transport->fields.enabled = FALSE;
                transport->fields.has_voice_assistant = FALSE;
                transport->type = gaia_transport_none;
            }

            /* Reset threshold states */
            gaiaTransportCommonCleanupThresholdState(transport);
        }
    }
}
#endif
