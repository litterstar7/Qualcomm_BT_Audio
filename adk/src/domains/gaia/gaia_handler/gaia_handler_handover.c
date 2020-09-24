/*!
\copyright  Copyright (c) 2019 - 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Gaia Handover interfaces

*/
#if defined(INCLUDE_MIRRORING) && defined (INCLUDE_DFU)
#include "app_handover_if.h"
#include "gaia_framework_internal.h"

#include <logging.h>

/******************************************************************************
 * Local Function Prototypes
 ******************************************************************************/
static bool gaiaHandlerHandover_Veto(void);

static void gaiaHandlerHandover_Commit(bool is_primary);

/******************************************************************************
 * Global Declarations
 ******************************************************************************/

/* This macro will register the interfaces to be called during handover. No marshalling
 * will take place */
 REGISTER_HANDOVER_INTERFACE_NO_MARSHALLING(GAIA, gaiaHandlerHandover_Veto, gaiaHandlerHandover_Commit);

/******************************************************************************
 * Local Function Definitions
 ******************************************************************************/

/*!
    \brief Handle Veto check during handover

    Veto if there are pending messages for the task

    \return bool
*/
static bool gaiaHandlerHandover_Veto(void)
{
    bool veto = FALSE;

    /* If there is BR/EDR GAIA connection then disconnect it */
    uint8_t index = 0;
    GAIA_TRANSPORT *t = Gaia_TransportIterate(&index);
    while (t)
    {
        if (Gaia_TransportHasFeature(t, GAIA_TRANSPORT_FEATURE_BREDR) && Gaia_TransportIsConnected(t))
        {
            DEBUG_LOG_INFO("gaiaHandlerHandover_Veto, disconnecting transport %p", t);
            gaiaFrameworkInternal_GaiaDisconnect(t);
            veto = TRUE;
        }
        t = Gaia_TransportIterate(&index);
    }

    if (veto)
    {
        DEBUG_LOG_INFO("gaiaHandlerHandover_Veto, Vetoed");
    }

    return veto;
}

/*!
    \brief Component commits to the specified role

    \param[in] is_primary   TRUE if device role is primary, else secondary

*/
static void gaiaHandlerHandover_Commit(bool is_primary)
{
    DEBUG_LOG("gaiaHandlerHandover_Commit");
    UNUSED(is_primary);
}

#endif /* defined(INCLUDE_MIRRORING) && defined (INCLUDE_DFU) */
