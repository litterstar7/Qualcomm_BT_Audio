/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Interface to HEADSET Topology goal handling.
*/

#ifndef HEADSET_TOPOLOGY_GOALS_H
#define HEADSET_TOPOLOGY_GOALS_H

#include <message.h>


/*! Definition of goals which the topology may be instructed (by rules) to achieve. */
typedef enum
{
    /*! empty goal*/
    hs_topology_goal_none,
    /*! Goal is to make the headset connectable to a handset. */
    hs_topology_goal_connectable_handset,
    /*! Goal is to notify clients of connection manager that headset is now connectable */
    hs_topology_goal_allow_handset_connect,
    /*! Goal is to connect to a handset. */
    hs_topology_goal_connect_handset,
    /*! Goal is to disconnect from a handset. */
    hs_topology_goal_disconnect_handset,
    /*! Goal is to make the headset connectable over LE. */
    hs_topology_goal_allow_le_connection,
    /*! Goal is to shut down the system and cancel activities */
    hs_topology_goal_system_stop,
    
    /* ADD ENTRIES ABOVE HERE */
    /*! Final entry to get the number of IDs */
    HEADSET_TOPOLOGY_GOAL_NUMBER_IDS,
} headset_topology_goal_id_t;


/*! A set of goals, stored as bits in a mask. */
typedef unsigned long long hs_topology_goal_msk;

/*! Macro to create a goal mask from a #hs_topology_goal_id. */
#define GOAL_MASK(id) (((hs_topology_goal_msk)1)<<(id))


/*! Macro to iterate over all goals.
    Helper allows for the fact that the first entry is unused

    \param loop_index name of variable to define and use as the loop index
 */
#define FOR_ALL_HS_GOALS(loop_index)\
    for (int loop_index = 1; (loop_index) < HEADSET_TOPOLOGY_GOAL_NUMBER_IDS; (loop_index)++)

/*! \brief Query if a goal is currently active.

    \param goal The goal being queried for status.

    \return TRUE if goal is currently active.
*/
bool HeadsetTopology_IsGoalActive(headset_topology_goal_id_t goal);

/*! \brief Check if there are any pending goals.

    \return TRUE if there are one or more goals queued; FALSE otherwise.
*/
bool HeadsetTopology_IsAnyGoalPending(void);

/*! \brief Handler for new and queued goals.
    
    \note This is a common handler for new goals generated by a topology
          rule decision and goals queued waiting on completion or cancelling
          already in-progress goals. 
*/
void HeadsetTopology_HandleGoalDecision(Task task, MessageId id, Message message);

/*! \brief Initialise the Headset topology goals */
void HeadsetTopology_GoalsInit(void);

#endif /* HEADSET_TOPOLOGY_GOALS_H */
