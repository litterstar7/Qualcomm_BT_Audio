/*!
\copyright  Copyright (c) 2005 - 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      TWS feature specific rule functions.
*/

#include "tws_topology_twm_primary_rule_functions.h"
#include "tws_topology_primary_ruleset.h"
#include "tws_topology_goals.h"

#include <phy_state.h>
#include <rules_engine.h>
#include <connection_manager.h>
#include <mirror_profile.h>
#include <case.h>
#include <peer_find_role.h>

#include <logging.h>

#define TWSTOP_TWM_PRIMARY_RULE_LOG(...)        DEBUG_LOG(__VA_ARGS__)

rule_action_t ruleTwsTopTwmPriFindRole(void)
{
    if (   (appPhyStateGetState() == PHY_STATE_IN_CASE)
        && (!Case_EventsEnabled()))
    {
        TWSTOP_TWM_PRIMARY_RULE_LOG("ruleTwsTopTwmPriFindRole ignore as in case and lid events not enabled");
        return rule_action_ignore;
    }
    
    if (   (appPhyStateGetState() == PHY_STATE_IN_CASE)
        && (Case_EventsEnabled())
        && (Case_GetLidState() == CASE_LID_STATE_CLOSED))
    {
        TWSTOP_TWM_PRIMARY_RULE_LOG("ruleTwsTopTwmPriFindRole, ignore as in case, lid event enabled and lid is closed");
        return rule_action_ignore;
    }

    if (PeerFindRole_IsActive())
    {
        TWSTOP_TWM_PRIMARY_RULE_LOG("ruleTwsTopTwmPriFindRole, ignore as role selection already running");
        return rule_action_ignore;
    }

    if (   TwsTopology_GetRole() == tws_topology_role_primary
        || TwsTopology_GetRole() == tws_topology_role_secondary)
    {
        TWSTOP_TWM_PRIMARY_RULE_LOG("ruleTwsTopTwmPriFindRole, ignore as already have a role");
        return rule_action_ignore;
    }

    /* if peer pairing is active ignore */
    if (TwsTopology_IsGoalActive(tws_topology_goal_pair_peer))
    {
        TWSTOP_TWM_PRIMARY_RULE_LOG("ruleTwsTopTwmPriFindRole, ignore as peer pairing active");
        return rule_action_ignore;
    }

    if (TwsTopology_IsGoalActive(tws_topology_goal_dynamic_handover) 
        || TwsTopology_IsGoalActive(tws_topology_goal_dynamic_handover_failure))
    {
        /* Ignore as there is a handover goal running  */
        TWSTOP_TWM_PRIMARY_RULE_LOG("ruleTwsTopTwmPriFindRole, Ignore as dynamic handover is still in progress ");
        return rule_action_ignore;
    }

    if (TwsTopology_IsGoalActive(tws_topology_goal_no_role_find_role))
    {
        TWSTOP_TWM_PRIMARY_RULE_LOG("ruleTwsTopTwmPriFindRole, ignore as no role find role in progress ");
        return rule_action_ignore;
    }

    TWSTOP_TWM_PRIMARY_RULE_LOG("ruleTwsTopTwmPriFindRole, run as not in case or in case but lid is open");
    return rule_action_run;
}

rule_action_t ruleTwsTopTwmPriNoRoleIdle(void)
{
    if (appPhyStateGetState() != PHY_STATE_IN_CASE)
    {
        TWSTOP_TWM_PRIMARY_RULE_LOG("ruleTwsTopTwmPriNoRoleIdle, ignore as out of case");
        return rule_action_ignore;
    }

    if (TwsTopology_IsGoalActive(tws_topology_goal_pair_peer))
    {
        TWSTOP_TWM_PRIMARY_RULE_LOG("ruleTwsTopTwmPriNoRoleIdle, ignore as peer pairing active");
        return rule_action_ignore;
    }

    if ((TwsTopology_PeerProfilesToRetain() != 0 && TwsTopology_HandsetProfilesToRetain() != 0)
         && appPhyStateGetState() == PHY_STATE_IN_CASE)
    {
        /* ToDo: Need to decide on the scope to clear it. */
        DEBUG_LOG("ruleTwsTopTwmPriNoRoleIdle, ignore as profile retention enabled");
        return rule_action_ignore;
    }

    /* this permits HDMA to react to the IN_CASE and potentially generate a handover event
     * in the first instance */
    if (TwsTopologyGetTaskData()->hdma_created)
    {
        TWSTOP_TWM_PRIMARY_RULE_LOG("ruleTwsTopTwmPriNoRoleIdle, defer as HDMA is Active and will generate handover recommendation shortly");
        return rule_action_defer;
    }

    /* this prevent IN_CASE stopping an in-progress dynamic handover from continuing to run
     * where we've past the point that HDMA has been destroyed */
    if(TwsTopology_IsGoalActive(tws_topology_goal_dynamic_handover) 
       || TwsTopology_IsGoalActive(tws_topology_goal_dynamic_handover_failure))
    {
        TWSTOP_TWM_PRIMARY_RULE_LOG("ruleTwsTopTwmPriNoRoleIdle, Defer as dynamic handover is Active");
        return rule_action_defer;
    }

    if (   (TwsTopology_GetRole() == tws_topology_role_none)
        || (TwsTopology_IsGoalActive(tws_topology_goal_no_role_idle)))
    {
        TWSTOP_TWM_PRIMARY_RULE_LOG("ruleTwsTopTwmPriNoRoleIdle, ignore as already have no role or already actively going to no role");
        return rule_action_ignore;
    }

    TWSTOP_TWM_PRIMARY_RULE_LOG("ruleTwsTopTwmPriNoRoleIdle, run as primary in case");
    return rule_action_run;
}

/*! Decide whether to run the handover now. 

    The implementation of this rule works on the basis of the following:

     a) Handover is allowed by application now.
     b) No active goals executing.
*/
rule_action_t ruleTwsTopTwmHandoverStart(void)
{
    if (TwsTopology_PeerProfilesToRetain() != 0 && TwsTopology_HandsetProfilesToRetain() != 0)
    {
        /* ToDo: Need to decide on the scope to clear it. */
        DEBUG_LOG("ruleTwsTopTwmHandoverStart, Ignored as profile retention enabled ");
        return rule_action_ignore;
    }

    if (TwsTopology_IsGoalActive(tws_topology_goal_dynamic_handover) 
         || TwsTopology_IsGoalActive(tws_topology_goal_dynamic_handover_failure))
    {
        /* Ignore any further handover requests as there is already one in progress */
        TWSTOP_TWM_PRIMARY_RULE_LOG("ruleTwsTopTwmHandoverStart, Ignored as dynamic handover is still in progress ");
        return rule_action_ignore;
    }

    /* Check if Handover is allowed by application */
    if(TwsTopologyGetTaskData()->app_prohibit_handover)
    {
        TWSTOP_TWM_PRIMARY_RULE_LOG("ruleTwsTopTwmHandoverStart, defer as App has blocked ");
        return rule_action_defer;
    }

    if (   TwsTopology_IsGoalActive(tws_topology_goal_primary_connect_peer_profiles)
        || !MirrorProfile_IsConnected())
    {
        TWSTOP_TWM_PRIMARY_RULE_LOG("ruleTwsTopTwmHandoverStart, defer as handover profiles not ready");
        return rule_action_defer;
    }

    /* Run the rule now */
    TWSTOP_TWM_PRIMARY_RULE_LOG("ruleTwsTopTwmHandoverStart, run");
    return rule_action_run;
}

/*! A previous handover failed. Run failed rule. */ 
rule_action_t ruleTwsTopTwmHandoverFailed(void)
{
    TWSTOP_TWM_PRIMARY_RULE_LOG("ruleTwsTopTwmHandoverFailed, run");

    /* Run the rule now */
    return rule_action_run;
}

/*! A previous handover failure has been handled. Run failure handled rule. */ 
rule_action_t ruleTwsTopTwmHandoverFailureHandled(void)
{
    if (appPhyStateGetState() != PHY_STATE_IN_CASE)
    {
        TWSTOP_TWM_PRIMARY_RULE_LOG("ruleTwsTopTwmHandoverFailureHandled, ignore as not in case anymore");
        return rule_action_ignore;
    }

    if (   (appPhyStateGetState() == PHY_STATE_IN_CASE)
        && (Case_EventsEnabled())
        && (Case_GetLidState() != CASE_LID_STATE_CLOSED))
    {
        TWSTOP_TWM_PRIMARY_RULE_LOG("ruleTwsTopTwmHandoverFailureHandled, ignore as in the case, lid events enabled, but lid isn't closed");
        return rule_action_ignore;
    }

    /* Run the rule now */
    TWSTOP_TWM_PRIMARY_RULE_LOG("ruleTwsTopTwmHandoverFailureHandled, run");
    return rule_action_run;

}

