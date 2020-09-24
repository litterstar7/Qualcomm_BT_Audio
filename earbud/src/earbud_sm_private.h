/*!
\copyright  Copyright (c) 2005 - 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       earbud_sm_private.h
\brief      Internal interface for SM components.
*/

#ifndef EARBUD_SM_PRIVATE_H_
#define EARBUD_SM_PRIVATE_H_

#include "earbud_sm.h"

/*! \brief Application state machine internal message IDs */
enum sm_internal_message_ids
{
    SM_INTERNAL_PAIR_HANDSET,                      /*!< Start pairing with a handset. */
    SM_INTERNAL_DELETE_HANDSETS,                   /*!< Delete all paired handsets. */
    SM_INTERNAL_FACTORY_RESET,                     /*!< Reset device to factory defaults. */
    SM_INTERNAL_ENTER_DFU_UI,                      /*!< Button for DFU has been pressed */
    SM_INTERNAL_ENTER_DFU_UPGRADED,                /*!< We have restarted after an upgrade requested reboot */
    SM_INTERNAL_ENTER_DFU_STARTUP,                 /*!< We have restarted while a DFU was in progress */
    SM_INTERNAL_NO_DFU,                            /*!< DFU/Upgrade is not supported, move to startup */
    SM_INTERNAL_REBOOT,                            /*!< Reboot the earbud immediately */
    SM_INTERNAL_LINK_DISCONNECTION_COMPLETE,       /*!< All links are now disconnected */
    SM_INTERNAL_BREDR_CONNECTED,                   /*!< A BREDR device has connected */
    SM_INTERNAL_ENTER_PRODUCTION_TEST_MODE,        /*!< Enter production test mode */
    SM_INTERNAL_ENTER_DUT_TEST_MODE, 	           /*!< Enter production test mode */

    SM_INTERNAL_TIMEOUT_DFU_ENTRY = 0x80,          /*!< Timeout occurred because no DFU activity after entering GAIA */
    SM_INTERNAL_TIMEOUT_DFU_MODE_START,            /*!< Timeout occured because did not enter the case, when DFU requested */
    SM_INTERNAL_TIMEOUT_DFU_AWAIT_DISCONNECT,      /*!< Timeout occured because we did not get a DFU disconnect from GAIA */
    SM_INTERNAL_TIMEOUT_LINK_DISCONNECTION,        /*!< Timeout if link disconnection takes too long. */
    SM_INTERNAL_TIMEOUT_OUT_OF_EAR_A2DP,           /*!< Timeout to pause A2DP when earbud removed from ear while streaming. */
    SM_INTERNAL_TIMEOUT_OUT_OF_EAR_SCO,            /*!< Timeout to transfer SCO to AG when earbud removed from ear while call active. */
    SM_INTERNAL_TIMEOUT_IN_EAR_A2DP_START,         /*!< Timeout within which restart audio if earbud put back in ear. */
    SM_INTERNAL_TIMEOUT_IDLE,                      /*!< Timeout when idle. */
    SM_INTERNAL_TIMEOUT_PEER_WAS_PAIRING,          /*!< Timeout to clear notification that peer was pairing when it went in the case */
};

/*! \brief Actions that may be performed after a link disconnect has completed */
typedef enum
{
    POST_DISCONNECT_ACTION_NONE,
    POST_DISCONNECT_ACTION_HANDSET_PAIRING,
    POST_DISCONNECT_ACTION_DELETE_HANDSET_PAIRING,
} smPostDisconnectAction;

/*! \brief SM_INTERNAL_LINK_DISCONNECTION_COMPLETE message content */
typedef struct
{
    smPostDisconnectAction post_disconnect_action;
} SM_INTERNAL_LINK_DISCONNECTION_COMPLETE_T;

#endif /* EARBUD_SM_PRIVATE_H_ */
