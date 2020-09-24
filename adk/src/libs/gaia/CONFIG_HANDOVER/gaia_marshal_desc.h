/****************************************************************************
Copyright (c) 2020 Qualcomm Technologies International, Ltd.


FILE NAME
    gaia_marshal_desc.h

DESCRIPTION
    Creates marshal type descriptors for GAIA data types

NOTES
    Builds requiring this should include CONFIG_HANDOVER in the
    makefile. e.g.
        CONFIG_FEATURES:=CONFIG_HANDOVER
*/

#ifndef _GAIA_MARSHAL_DESC_H_
#define _GAIA_MARSHAL_DESC_H_

#include "marshal_common_desc.h"
#include "types.h"
#include "app/marshal/marshal_if.h"
#include "gaia_private.h"

/*GAIA Marshal types*/
#define GAIA_MARSHAL_TYPES_TABLE(ENTRY) \
    ENTRY(gaia_transport_bitfields) \
    ENTRY(gaia_transport) \
    ENTRY(gaia_transport_type) \
    ENTRY(gaia_transport_spp_data)

/* Use xmacro to expand type table as enumeration of marshal types */
#define EXPAND_AS_ENUMERATION(type) MARSHAL_TYPE(type),
enum
{
    COMMON_MARSHAL_TYPES_TABLE(EXPAND_AS_ENUMERATION)
    COMMON_DYN_MARSHAL_TYPES_TABLE(EXPAND_AS_ENUMERATION)
    GAIA_MARSHAL_TYPES_TABLE(EXPAND_AS_ENUMERATION)
    GAIA_MARSHAL_OBJ_TYPE_COUNT
};
#undef EXPAND_AS_ENUMERATION

/* Array of GAIA type descriptors. Defined in gaia_marshal_desc.c */
extern const marshal_type_descriptor_t * const  mtd_gaia[];

#endif /* _GAIA_MARSHAL_DESC_H_ */