/****************************************************************************
Copyright (c) 2020 Qualcomm Technologies International, Ltd.


FILE NAME
    gaia_handover_policy.c

DESCRIPTION
    This file defines stream handover policy configuration for
    platforms that support handover
    
NOTES
    Builds requiring this should include CONFIG_HANDOVER in the
    makefile. e.g.
        CONFIG_FEATURES:=CONFIG_HANDOVER
*/

#define DEBUG_LOG_MODULE_NAME gaia
#include <logging.h>

#include "gaia_handover_policy.h"
#include <source.h>

bool gaiaSourceConfigureHandoverPolicy(Source src, uint32 value)
{    
    return SourceConfigure(src, STREAM_SOURCE_HANDOVER_POLICY, value);
}
