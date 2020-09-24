/*!
\copyright  Copyright (c) 2005 - 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       
\brief      Interface to rule functions common to all TWS modes.
*/

#ifndef _TWS_TOPOLOGY_COMMON_SECONDARY_RULE_FUNCTIONS_H_
#define _TWS_TOPOLOGY_COMMON_SECONDARY_RULE_FUNCTIONS_H_

#include <rules_engine.h>

/*! \name Rule function prototypes

    These allow us to build the rule table twstop_secondary_rules_set. */
/*! @{ */
DEFINE_RULE_FUNCTION(ruleTwsTopSecShutDown);
DEFINE_RULE_FUNCTION(ruleTwsTopSecPeerLostFindRole);
DEFINE_RULE_FUNCTION(ruleTwsTopSecRoleSwitchPeerConnect);
DEFINE_RULE_FUNCTION(ruleTwsTopSecNoRoleIdle);
DEFINE_RULE_FUNCTION(ruleTwsTopSecFailedConnectFindRole);
DEFINE_RULE_FUNCTION(ruleTwsTopSecFailedSwitchSecondaryFindRole);
/* This need to be removed once DFU_FIXED_ROLE_IN_PREVIOUS_VERSION is removed */
DEFINE_RULE_FUNCTION(ruleTwsTopSec_20_1_Secondary);

/*! @} */

#endif /* _TWS_TOPOLOGY_COMMON_SECONDARY_RULE_FUNCTIONS_H_ */
