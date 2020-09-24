/****************************************************************************
Copyright (c) 2020 Qualcomm Technologies International, Ltd.


FILE NAME
    gaia_handover.c

DESCRIPTION
    This file is a stub for the TWS Handover and Marshaling interface 
    for GAIA
    
NOTES
    Builds requiring this should include CONFIG_HANDOVER in the
    makefile. e.g.
        CONFIG_FEATURES:=CONFIG_HANDOVER 
*/


/* Not supported in this configuration */

#include "gaia_private.h"

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

extern const handover_interface gaia_handover_if =  {
        &gaiaVeto,
        &gaiaMarshal,
        &gaiaUnmarshal,
        &gaiaHandoverCommit,
        &gaiaHandoverComplete,        
        &gaiaHandoverAbort};

/****************************************************************************
NAME    
    gaiaVeto

DESCRIPTION
    Veto check for GAIA library

    Prior to handover commencing this function is called and
    the libraries internal state is checked to determine if the
    handover should proceed.

RETURNS
    bool TRUE if the GAIA Library wishes to veto the handover attempt.
*/
bool gaiaVeto( void )
{
    /* Not supported in this configuration */
    Panic();
    
    return TRUE;
}

/****************************************************************************
NAME    
    gaiaHandoverCommit

DESCRIPTION
    The GAIA library performs time-critical actions to commit to the specified 
    new role (primary or  secondary)

RETURNS
    void
*/
static void gaiaHandoverCommit(const tp_bdaddr *tp_bd_addr, const bool newRole)
{
    UNUSED(tp_bd_addr);
    UNUSED(newRole);
    
    /* Not supported in this configuration */
    Panic();
    
    return;
}

/****************************************************************************
NAME    
    gaiaHandoverComplete

DESCRIPTION
    The GAIA library performs pending actions and completes the transition to 
    the specified new role.

RETURNS
    void
*/
static void gaiaHandoverComplete( const bool newRole )
{
    UNUSED(newRole);
    
    /* Not supported in this configuration */
    Panic();
    
    return;
}

/****************************************************************************
NAME    
    gaiaHandoverAbort

DESCRIPTION
    Abort the GAIA Handover process, free any memory
    associated with the marshalling process.

RETURNS
    void
*/
static void gaiaHandoverAbort(void)
{
    /* Not supported in this configuration */
    Panic();

    return;
}

/****************************************************************************
NAME    
    gaiaMarshal

DESCRIPTION
    Marshal the data associated with GAIA connections

RETURNS
    bool TRUE if GAIA module marshalling complete, otherwise FALSE
*/
static bool gaiaMarshal(const tp_bdaddr *tp_bd_addr,
                        uint8 *buf,
                        uint16 length,
                        uint16 *written)
{
    UNUSED(tp_bd_addr);
    UNUSED(buf);
    UNUSED(length);
    UNUSED(written);
    
    /* Not supported in this configuration */
    Panic();
    
    return FALSE;
}

/****************************************************************************
NAME    
    gaiaUnmarshal

DESCRIPTION
    Unmarshal the data associated with the GAIA connections

RETURNS
    bool TRUE if GAIA unmarshalling complete, otherwise FALSE
*/
bool gaiaUnmarshal(const tp_bdaddr *tp_bd_addr,
                          const uint8 *buf,
                          uint16 length,
                          uint16 *consumed)
{
    UNUSED(tp_bd_addr);
    UNUSED(buf);
    UNUSED(length);
    UNUSED(consumed);
    
    /* Not supported in this configuration */
    Panic();
    
    return FALSE;
}

