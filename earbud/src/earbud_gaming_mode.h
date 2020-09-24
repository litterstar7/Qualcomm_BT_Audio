/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       earbud_gaming_mode.h
\brief      Header file for the gaming mode module
*/

#ifndef EARBUD_GAMING_MODE_H_
#define EARBUD_GAMING_MODE_H_

#include "domain_message.h"
#include "earbud_sm_marshal_defs.h"
#include "peer_signalling.h"

/*! \brief Message IDs for Gaming mode messages to UI clients */
enum gaming_mode_ui_events
{
    GAMING_MODE_ON = GAMING_MODE_UI_MESSAGE_BASE,
    GAMING_MODE_OFF
};

#ifdef INCLUDE_GAMING_MODE
/*! \brief Enable gaming mode
    \return TRUE if gaming mode was enabled
*/
bool EarbudGamingMode_Enable(void);

/*! \brief Disable gaming mode
    \return TRUE if gaming mode was disabled
*/
bool EarbudGamingMode_Disable(void);

/*! \brief Initialise gaming mode module
    \param[in] init_task    Init Task handle
    \param[out] bool        Result of intialization
*/    
bool EarbudGamingMode_init(Task init_task);

/*! \brief Handle Gaming Mode message from Primary
    \param[in] msg    Gaming mode message
*/    
void EarbudGamingMode_HandlePeerMessage(earbud_sm_msg_gaming_mode_t *msg);

/*! \brief Handle peer signalling channel connecting/disconnecting
    \param[in] ind The connect/disconnect indication
*/
void EarbudGamingMode_HandlePeerSigConnected(const PEER_SIG_CONNECTION_IND_T *ind);

/*! \brief Check if Gaming Mode is enabled
    \param[out] TRUE if Enabled, FALSE otherwise
*/
bool EarbudGamingMode_IsGamingModeEnabled(void);

/*! \brief Handle peer signalling message tx confirmation
    \param[in] cfm message confirmation status
*/
void EarbudGamingMode_HandlePeerSigTxCfm(const PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM_T* cfm);

#else
#define EarbudGamingMode_Enable()
#define EarbudGamingMode_Disable()
#define EarbudGamingMode_init(tsk) (FALSE)
#define EarbudGamingMode_HandlePeerMessage(msg)
#define EarbudGamingMode_HandlePeerSigConnected(ind)
#define EarbudGamingMode_IsGamingModeEnabled() (FALSE)
#define EarbudGamingMode_HandlePeerSigTxCfm(cfm)
#endif /* INCLUDE_GAMING_MODE */
#endif /* EARBUD_GAMING_MODE_H_ */
