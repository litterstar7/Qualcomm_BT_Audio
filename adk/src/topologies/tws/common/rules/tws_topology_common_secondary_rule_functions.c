/*!
\copyright  Copyright (c) 2005 - 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Common rule functions for all TWS modes.
*/

#include "tws_topology_common_secondary_rule_functions.h"
#include "tws_topology_secondary_ruleset.h"
#include "tws_topology_goals.h"
#include "tws_topology_private.h"

#include <bt_device.h>
#include <phy_state.h>
#include <rules_engine.h>
#include <connection_manager.h>
#include <logging.h>
#include <case.h>
#ifdef DFU_FIXED_ROLE_IN_PREVIOUS_VERSION
#include <dfu.h>
#endif /* DFU_FIXED_ROLE_IN_PREVIOUS_VERSION */

#define TWSTOP_SECONDARY_RULE_LOG(...)         DEBUG_LOG(__VA_ARGS__)

/*! \brief Rule to decide if topology can shut down */
rule_action_t ruleTwsTopSecShutDown(void)
{
    TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecShutDown, run always");

    return rule_action_run;
}

/*! \brief Rule to decide if Secondary should start role selection on peer linkloss. */
rule_action_t ruleTwsTopSecPeerLostFindRole(void)
{
    if (appPhyStateGetState() == PHY_STATE_IN_CASE && TwsTopology_PeerProfilesToRetain() == 0)
    {
        if (!Case_EventsEnabled())
        {
            TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecPeerLostFindRole, ignore as in case and lid events not enabled");
            return rule_action_ignore;
        }

        if (Case_GetLidState() != CASE_LID_STATE_OPEN)
        {
            TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecPeerLostFindRole, ignore as in case and lid is not open");
            return rule_action_ignore;
        }
    }

    /* Use no_role_idle running as indication we went into the case, so don't enable PFR */
    if (twsTopology_JustWentInCase())
    {
        TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecPeerLostFindRole, ignore as just went in the case");
        return rule_action_ignore;
    }

    TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecPeerLostFindRole, run as out of case, or in case but lid is open");
    return rule_action_run;
}

/*! \brief Rule to decide if device should take fixed secondary role. */
/* This need to be removed once DFU_FIXED_ROLE_IN_PREVIOUS_VERSION is removed */
rule_action_t ruleTwsTopSec_20_1_Secondary(void)
{
#ifdef DFU_FIXED_ROLE_IN_PREVIOUS_VERSION
    if (UpgradePeerIsADK_20_1_SecondaryDevice())
    {
        TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSec_20_1_Secondary, run as updated from 20.1");
        return rule_action_run;
    }

    TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSec_20_1_Secondary, ignore as NOT updated from 20.1");
    return rule_action_ignore;
#else
    return rule_action_ignore;
#endif /* DFU_FIXED_ROLE_IN_PREVIOUS_VERSION */
}

/*! \brief Rule to decide if Secondary should connect to Primary. */
rule_action_t ruleTwsTopSecRoleSwitchPeerConnect(void)
{
    bdaddr primary_addr;

    if (!appDeviceGetPrimaryBdAddr(&primary_addr))
    {
        TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecRoleSwitchPeerConnect, ignore as unknown primary address");
        return rule_action_ignore;
    }

    if (appPhyStateGetState() == PHY_STATE_IN_CASE && TwsTopology_PeerProfilesToRetain() == 0)
    {
        if (!Case_EventsEnabled())
        {
            TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecRoleSwitchPeerConnect, ignore as in case and lid events not enabled");
            return rule_action_ignore;
        }

        if (Case_GetLidState() == CASE_LID_STATE_CLOSED)
        {
            TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecRoleSwitchPeerConnect, ignore as in case, lid event enabled and lid is closed");
            return rule_action_ignore;
        }
    }

    if (ConManagerIsConnected(&primary_addr))
    {
        TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecRoleSwitchPeerConnect, ignore as peer already connected");
        return rule_action_ignore;
    }

    TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecRoleSwitchPeerConnect, run as secondary out of case and peer not connected");
    return rule_action_run;
}

rule_action_t ruleTwsTopSecNoRoleIdle(void)
{
    if (appPhyStateGetState() != PHY_STATE_IN_CASE)
    {
        TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecNoRoleIdle, ignore as out of case");
        return rule_action_ignore;
    }

    if ((TwsTopology_PeerProfilesToRetain() != 0 && TwsTopology_HandsetProfilesToRetain() != 0)
         &&  appPhyStateGetState() == PHY_STATE_IN_CASE)
    {
        /* ToDo: Need to decide on the scope to clear it. */
        TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecNoRoleIdle, ignore as profile retention enabled");
        return rule_action_ignore;
    }

    TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecNoRoleIdle, run as secondary in case");
    return rule_action_run;
}

rule_action_t ruleTwsTopSecFailedConnectFindRole(void)
{
    bdaddr primary_addr;

    if (appPhyStateGetState() == PHY_STATE_IN_CASE && TwsTopology_PeerProfilesToRetain() == 0)
    {
        if (!Case_EventsEnabled())
        {
            TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecFailedConnectFindRole, ignore as in the case and lid events not enabled");
            return rule_action_ignore;
        }

        if (Case_GetLidState() != CASE_LID_STATE_CLOSED)
        {
            TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecFailedConnectFindRole, ignore as in the case, lid events enabled but lid isn't closed");
            return rule_action_ignore;
        }
    }

    if (!appDeviceGetPrimaryBdAddr(&primary_addr))
    {
        TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecFailedConnectFindRole, ignore as unknown primary address");
        return rule_action_ignore;
    }

    if (ConManagerIsConnected(&primary_addr))
    {
        TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecFailedConnectFindRole, ignore as peer already connected");
        return rule_action_ignore;
    }

    TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecFailedConnectFindRole, run as secondary out of case with no peer link");
    return rule_action_run;
}

rule_action_t ruleTwsTopSecFailedSwitchSecondaryFindRole(void)
{
    bdaddr primary_addr;

    if (appPhyStateGetState() == PHY_STATE_IN_CASE && TwsTopology_PeerProfilesToRetain() == 0)
    {
        if (!Case_EventsEnabled())
        {
            TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecFailedSwitchSecondaryFindRole, ignore as in the case and lid events not enabled");
            return rule_action_ignore;
        }

        if (Case_GetLidState() != CASE_LID_STATE_CLOSED)
        {
            TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecFailedSwitchSecondaryFindRole, ignore as in the case, lid events enabled but lid isn't closed");
            return rule_action_ignore;
        }
    }

    if (!appDeviceGetPrimaryBdAddr(&primary_addr))
    {
        TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecFailedSwitchSecondaryFindRole, ignore as unknown primary address");
        return rule_action_ignore;
    }

    if((TwsTopology_GetRole() == tws_topology_role_secondary) && ConManagerIsConnected(&primary_addr))
    {
        TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecFailedSwitchSecondaryFindRole, ignore as have secondary role and connected to primary");
        return rule_action_ignore;
    }

    TWSTOP_SECONDARY_RULE_LOG("ruleTwsTopSecFailedSwitchSecondaryFindRole, run as out of case and not a secondary with peer link");
    return rule_action_run;
}

