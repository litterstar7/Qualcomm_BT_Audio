/****************************************************************************
Copyright (c) 2020 Qualcomm Technologies International, Ltd.


FILE NAME
    gaia_marshal_desc.c

DESCRIPTION
    Creates marshal type descriptors for GAIA data types

NOTES
    Builds requiring this should include CONFIG_HANDOVER in the
    makefile. e.g.
        CONFIG_FEATURES:=CONFIG_HANDOVER
*/

#define DEBUG_LOG_MODULE_NAME gaia
#include <logging.h>

#include "gaia_marshal_desc.h"
#include "gaia_private.h"
#include "types.h"
#include <panic.h>
#include <sink.h>
#include <stream.h>
#include <source.h>

/* Marshal member definitions for non basic types */

#if 0
static const marshal_member_descriptor_t mmd_gaia_transport[] =
{
    MAKE_MARSHAL_MEMBER(gaia_transport, gaia_transport_type, type),
    MAKE_MARSHAL_MEMBER(gaia_transport, gaia_transport_bitfields, fields),
    MAKE_MARSHAL_MEMBER_ARRAY(gaia_transport, int16, battery_lo_threshold, 2),
    MAKE_MARSHAL_MEMBER_ARRAY(gaia_transport, int16, battery_hi_threshold, 2),
    MAKE_MARSHAL_MEMBER_ARRAY(gaia_transport, int, rssi_lo_threshold, 2),
    MAKE_MARSHAL_MEMBER_ARRAY(gaia_transport, int, rssi_hi_threshold, 2),
    MAKE_MARSHAL_MEMBER(gaia_transport, gaia_transport_spp_data, state)
};

static const marshal_member_descriptor_t mmd_gaia_transport_spp_data[] =
{
    MAKE_MARSHAL_MEMBER(gaia_transport_spp_data, uint16, rfcomm_channel)
};

static const marshal_type_descriptor_t mtd_gaia_transport_spp_data = 
    MAKE_MARSHAL_TYPE_DEFINITION(gaia_transport_spp_data, mmd_gaia_transport_spp_data);

static const marshal_type_descriptor_t mtd_gaia_transport = 
    MAKE_MARSHAL_TYPE_DEFINITION(gaia_transport, mmd_gaia_transport);

static const marshal_type_descriptor_t mtd_gaia_transport_type = 
    MAKE_MARSHAL_TYPE_DEFINITION_BASIC(gaia_transport_type);

static const marshal_type_descriptor_t mtd_gaia_transport_bitfields = 
    MAKE_MARSHAL_TYPE_DEFINITION_BASIC(gaia_transport_bitfields);

/* Use xmacro to expand type table as array of type descriptors */
#define EXPAND_AS_TYPE_DEFINITION(type) (const marshal_type_descriptor_t *) &mtd_##type,
const marshal_type_descriptor_t * const  mtd_gaia[] =
{
    COMMON_MARSHAL_TYPES_TABLE(EXPAND_AS_TYPE_DEFINITION)
    COMMON_DYN_MARSHAL_TYPES_TABLE(EXPAND_AS_TYPE_DEFINITION)
    GAIA_MARSHAL_TYPES_TABLE(EXPAND_AS_TYPE_DEFINITION)
};
#undef EXPAND_AS_TYPE_DEFINITION
#endif
