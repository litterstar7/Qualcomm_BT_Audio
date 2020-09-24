/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       earbud_gaming_mode.c
\brief      Interface for toggling gaming mode
*/
#ifdef INCLUDE_GAMING_MODE
#include "earbud_gaming_mode.h"
#include "earbud_tones.h"
#include "kymera_latency_manager.h"
#include "earbud_sm.h"
#include "peer_signalling.h"
#include "av.h"
#include "mirror_profile.h"
#include "kymera.h"
#include "ui.h"
#include "ui_inputs.h"
#include <logging.h>
#include <message.h>
#include <panic.h>
#include <stdio.h>
#include <system_clock.h>
#include <rtime.h>

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_ENUM(gaming_mode_ui_events)


#define UNLOCK 0
#define LOCK 1

#define US_TO_MS(us) ((us) / US_PER_MS)

/* Delay(ms) to allow time to transmit new gaming mode state to peer earbud before
   transitioning to new state. */
#define GAMING_MODE_MUTE_DELAY (200)
/* Max delay(ms) to allow time to transmit new gaming mode state to peer earbud before
   transitioning to new state. */
#define GAMING_MODE_MUTE_DELAY_MAX (450)
/* delay(ms) to account Kymera tone configuration and to be used when PeerSig is not
 * connected or other reasons, currently using same as GAMING_MODE_MUTE_DELAY */
#define GAMING_MODE_MUTE_DELAY_DEFAULT GAMING_MODE_MUTE_DELAY
/* Max time(ms) for which to wait for PeerSig message Tx */
#define GAMING_MODE_PEER_SIGMSG_TX_WAIT_MAX (450)

const message_group_t gaming_mode_ui_inputs[] = {
    UI_INPUTS_GAMING_MODE_MESSAGE_GROUP
};

/*! Gaming Mode States */
typedef enum
{
    GAMING_MODE_STATE_NONE,
    /*! Gaming Mode module has been enabled */
    GAMING_MODE_STATE_ENABLED,
    /*! Transition from Enabled to Disabled State */
    GAMING_MODE_STATE_ENABLING,
    /*! Transition from Disabled to Enabled State */
    GAMING_MODE_STATE_DISABLING,
    /*! Gaming Mode module has been disabled */
    GAMING_MODE_STATE_DISABLED,
} gaming_mode_state_t;

/*! Gaming Mode Internal Messages */
typedef enum
{
    /*! A2DP disconnected */
    GAMING_MODE_INTERNAL_A2DP_DISCONNECTED,
    /*! Kymera latency reconfiguration */
    GAMING_MODE_INTERNAL_LATENCY_RECONFIGURE,
    /*! Internal message to cancel wait for PeerSig Msg Tx */
    GAMING_MODE_INTERNAL_CANCEL_PEER_SIGMSG_TX_WAIT,
    /*! Request to enable/disable gaming mode by PEER */
    GAMING_MODE_INTERNAL_TOGGLE_MESSAGE,
}gaming_mode_internal_msg_t;

/*!GAMING_MODE_INTERNAL_LATENCY_RECONFIGURE message data */
typedef struct{
    rtime_t mute_timestamp;
}GAMING_MODE_INTERNAL_LATENCY_RECONFIGURE_T;

/*! Gaming Mode Data */
typedef struct {
    /*! Task data */
    TaskData taskdata;
    /*! state */
    gaming_mode_state_t state;
    /*! Task ID registered for UI Message group */
    Task ui_client_task;
    /*! Lock to handle messages received during state transitions (Enabling/Disabling) */
    uint16 state_lock;
    /*! Lock to handle PeerSig tranmission of gaming_mode_state_t to Peer */
    uint16 tx_lock;
} gaming_mode_task_data_t;

gaming_mode_task_data_t gaming_mode;
#define GamingMode_GetTaskData() (&gaming_mode)
#define GamingMode_GetState() (GamingMode_GetTaskData()->state)
#define GamingMode_SetStateTransitionLock() (GamingMode_GetTaskData()->state_lock = LOCK)
#define GamingMode_ClearStateTransitionLock() (GamingMode_GetTaskData()->state_lock = UNLOCK)
#define GamingMode_GetStateTransitionLockAddr() &(GamingMode_GetTaskData()->state_lock)
#define GamingMode_SetPeerTxLock()  (GamingMode_GetTaskData()->tx_lock = LOCK)
#define GamingMode_ClearPeerTxLock() (GamingMode_GetTaskData()->tx_lock = UNLOCK)
#define GamingMode_GetPeerTxLockAddr() &(GamingMode_GetTaskData()->tx_lock)


static void GamingMode_SetState(gaming_mode_state_t state, rtime_t timestamp);

static rtime_t delayToTimestamp(uint32 delay)
{
    rtime_t now = SystemClockGetTimerTime();
    return rtime_add(now, (delay * US_PER_MS));
}

static void gamingMode_EnterDisabled(void)
{
    DEBUG_LOG("gamingMode_EnterDisabled");
    GamingMode_ClearStateTransitionLock();
    Av_ReportChangedLatency();
}

static void gamingMode_EnterEnabled(void)
{
    DEBUG_LOG("gamingMode_EnterEnabled");
    GamingMode_ClearStateTransitionLock();
    Av_ReportChangedLatency();
}

static rtime_t gamingMode_GetMuteInstant(void)
{
    uint32 mute_delay;
    if(appPeerSigIsConnected())
    {
        uint8 pending_msgs_num = appPeerSigGetPendingMarshalledMsgNum();
        mute_delay = GAMING_MODE_MUTE_DELAY + US_TO_MS(pending_msgs_num * appPeerGetPeerRelayDelayBasedOnSystemContext());
        /* Limit the max peer transmission delay to GAMING_MODE_MUTE_DELAY_MAX */
        mute_delay = MIN(mute_delay, GAMING_MODE_MUTE_DELAY_MAX);
    }
    else
    {
        mute_delay = GAMING_MODE_MUTE_DELAY_DEFAULT;
    }
    return delayToTimestamp(mute_delay);
}

static void gamingMode_HandleInternalLatencyReconfiguration(rtime_t mute_instant)
{
    DEBUG_LOG("gamingMode_HandleInternalLatencyReconfiguration, mute_timestamp:%d", mute_instant);

    if(GAMING_MODE_STATE_ENABLING == GamingMode_GetState())
    {
        if (appAvIsStreaming())
        {
            Kymera_LatencyManagerReconfigureLatency(&GamingMode_GetTaskData()->taskdata, mute_instant, app_tone_gaming_mode_on);
        }
        else
        {
            /* Play gaming mode ON Tune */
            MessageSend(GamingMode_GetTaskData()->ui_client_task, GAMING_MODE_ON, NULL);
            GamingMode_SetState(GAMING_MODE_STATE_ENABLED, 0);
        }
    }
    else if(GAMING_MODE_STATE_DISABLING == GamingMode_GetState())
    {
        if (appAvIsStreaming())
        {
            Kymera_LatencyManagerReconfigureLatency(&GamingMode_GetTaskData()->taskdata, mute_instant, app_tone_gaming_mode_off);
        }
        else
        {
            /* Play gaming mode OFF Tune */
            MessageSend(GamingMode_GetTaskData()->ui_client_task, GAMING_MODE_OFF, NULL);
            GamingMode_SetState(GAMING_MODE_STATE_DISABLED, 0);
        }
    }
}

static bool gamingMode_SendGamingModeToPeer(bool enable, marshal_rtime_t mute_timestamp)
{
    bool msg_send = FALSE;
    if (appPeerSigIsConnected())
    {
        earbud_sm_msg_gaming_mode_t *ind = PanicUnlessMalloc(sizeof(*ind));
        ind->mute_timestamp = mute_timestamp;
        ind->enable = enable;
        appPeerSigMarshalledMsgChannelTx(SmGetTask(),
                                        PEER_SIG_MSG_CHANNEL_APPLICATION,
                                        ind, MARSHAL_TYPE(earbud_sm_msg_gaming_mode_t));
        GamingMode_SetPeerTxLock();
        msg_send = TRUE;
    }
    return msg_send;
}

static void gamingMode_SendGamingModeToPeerAndReconfigureLatency(bool enable, rtime_t mute_instant)
{
    if(gamingMode_SendGamingModeToPeer(enable, mute_instant))
    {
        MESSAGE_MAKE(msg, GAMING_MODE_INTERNAL_LATENCY_RECONFIGURE_T);
        msg->mute_timestamp = mute_instant;
        /* wait for PeerSig TX CFM message to configure Kymera latency */
        MessageSendConditionally(&GamingMode_GetTaskData()->taskdata,
                                 GAMING_MODE_INTERNAL_LATENCY_RECONFIGURE,
                                 msg,
                                 GamingMode_GetPeerTxLockAddr());
        MessageSendLater(&GamingMode_GetTaskData()->taskdata,
                         GAMING_MODE_INTERNAL_CANCEL_PEER_SIGMSG_TX_WAIT,
                         NULL,
                         GAMING_MODE_PEER_SIGMSG_TX_WAIT_MAX);
    }
    else
    {
        /* PeerSig is not connected, go ahead with Kymera tone play and latency configuration */
        gamingMode_HandleInternalLatencyReconfiguration(mute_instant);
    }
}

static void gamingMode_EnterEnabling(rtime_t mute_instant)
{
    GamingMode_SetStateTransitionLock();

    Kymera_LatencyManagerEnableGamingMode();

    if(BtDevice_IsMyAddressPrimary())
    {
        gamingMode_SendGamingModeToPeerAndReconfigureLatency(TRUE, mute_instant);
    }
    else
    {
        if (MirrorProfile_IsA2dpActive())
        {
            Kymera_LatencyManagerReconfigureLatency(&GamingMode_GetTaskData()->taskdata, mute_instant, app_tone_gaming_mode_on);
        }
        else
        {
            GamingMode_SetState(GAMING_MODE_STATE_ENABLED, 0);
        }
    }
}

static void gamingMode_EnterDisabling(rtime_t mute_instant)
{
    GamingMode_SetStateTransitionLock();

    Kymera_LatencyManagerDisableGamingMode();

    if (BtDevice_IsMyAddressPrimary())
    {
        gamingMode_SendGamingModeToPeerAndReconfigureLatency(FALSE, mute_instant);
    }
    else
    {
        if (appPeerSigIsConnected() && MirrorProfile_IsA2dpActive())
        {
            Kymera_LatencyManagerReconfigureLatency(&GamingMode_GetTaskData()->taskdata, mute_instant, app_tone_gaming_mode_off);
        }
        else
        {
            GamingMode_SetState(GAMING_MODE_STATE_DISABLED, 0);
        }
    }
}

static void GamingMode_SetState(gaming_mode_state_t state, rtime_t mute_instant)
{
    gaming_mode_task_data_t *data = GamingMode_GetTaskData();
    DEBUG_LOG("GamingMode_SetState, state %x", state);

    /* Ignore if state is already set, Otherwise it may lead to playing repetitive
     * tone of GAMING_MODE_ENABLE/GAMING_MODE_DISABLE while target state is not
     * yet acquired */
    if(data->state == state)
    {
        DEBUG_LOG("GamingMode_SetState, state %x already set", state);
        return;
    }
    /* Set new state */
    data->state = state;

    /* Handle state entry functions */
    switch (state)
    {
        case GAMING_MODE_STATE_ENABLING:
            gamingMode_EnterEnabling(mute_instant);
            break;
        case GAMING_MODE_STATE_DISABLING:
            gamingMode_EnterDisabling(mute_instant);
            break;
        case GAMING_MODE_STATE_ENABLED:
            gamingMode_EnterEnabled();
            break;
        case GAMING_MODE_STATE_DISABLED:
            gamingMode_EnterDisabled();
            break;
        default:
            break;
    }
}

static void gamingMode_HandleLatencyReconfigComplete(void)
{
    gaming_mode_task_data_t *data = GamingMode_GetTaskData();

    DEBUG_LOG("gamingMode_HandleLatencyReconfigComplete");

    if (data->state == GAMING_MODE_STATE_ENABLING)
    {
        GamingMode_SetState(GAMING_MODE_STATE_ENABLED, 0);
    }
    else if (data->state == GAMING_MODE_STATE_DISABLING)
    {
        GamingMode_SetState(GAMING_MODE_STATE_DISABLED, 0);
    }
}

static void gamingMode_HandleA2DPDisconnectedInd(void)
{
    DEBUG_LOG("gamingMode_HandleA2DPDisconnectedInd");
    MessageSendConditionally(&GamingMode_GetTaskData()->taskdata,
                                     GAMING_MODE_INTERNAL_A2DP_DISCONNECTED,
                                     NULL,
                                     GamingMode_GetStateTransitionLockAddr());
}

static void gamingMode_HandleInternalA2dpDisconnectedInd(void)
{
    switch(GamingMode_GetState())
    {
        case GAMING_MODE_STATE_ENABLED:
            DEBUG_LOG("gamingMode_HandleInternalA2DPDisconnectedInd: Disable Gaming Mode");
            EarbudGamingMode_Disable();
            break;       
        case GAMING_MODE_STATE_DISABLED:        
            /*ignore*/
            break;
        default:
            break;
    }
}

static void gamingMode_HandleGamingModeToggle(void)
{
    DEBUG_LOG("gamingMode_HandleGamingModeToggle: State:%d",GamingMode_GetState());
    
    switch(GamingMode_GetState())
    {
        case GAMING_MODE_STATE_DISABLED:
            EarbudGamingMode_Enable();
            break;
        
        case GAMING_MODE_STATE_ENABLED:
            EarbudGamingMode_Disable();
            break;

        default:
            break;
    }
}

static void gamingMode_HandleInternalCancelPeerSigMsgTxWait(void)
{
    DEBUG_LOG("gamingMode_HandleInternalCancelPeerSigMsgTxWait");
    GamingMode_ClearPeerTxLock();
    MessageCancelAll(&GamingMode_GetTaskData()->taskdata, GAMING_MODE_INTERNAL_LATENCY_RECONFIGURE);
    /* re-calculate new mute instant to confgiure Kymera tone */
    rtime_t mute_instant = delayToTimestamp(GAMING_MODE_MUTE_DELAY_DEFAULT);
    gamingMode_HandleInternalLatencyReconfiguration(mute_instant);
}

static void gamingMode_HandleGamingModeMsg(earbud_sm_msg_gaming_mode_t *msg)
{
    DEBUG_LOG("gamingMode_HandleGamingModeMsg: State:%d",GamingMode_GetState());

    /* Secondary's Enabled/Disabled states could be out of sync with primary's if the 
       gaming mode was changed while secondary is in case. So process the peer message
       regardless of local state.
     */
    if (msg->enable)
    {
        GamingMode_SetState(GAMING_MODE_STATE_ENABLING, msg->mute_timestamp);
    }
    else
    {
        GamingMode_SetState(GAMING_MODE_STATE_DISABLING, msg->mute_timestamp);
    }

}

static void earbudGamingModeHandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch(id)
    {
        case KYMERA_LATENCY_MANAGER_RECONFIG_COMPLETE_IND:
            gamingMode_HandleLatencyReconfigComplete();
            break;

        case AV_A2DP_DISCONNECTED_IND:
            gamingMode_HandleA2DPDisconnectedInd();
            break;
        
        case GAMING_MODE_INTERNAL_A2DP_DISCONNECTED:
            gamingMode_HandleInternalA2dpDisconnectedInd();
            break;

        case ui_input_gaming_mode_toggle:
            gamingMode_HandleGamingModeToggle();
            break;

        case GAMING_MODE_INTERNAL_LATENCY_RECONFIGURE:
            gamingMode_HandleInternalLatencyReconfiguration(((const GAMING_MODE_INTERNAL_LATENCY_RECONFIGURE_T*)message)->mute_timestamp);
            break;

        case GAMING_MODE_INTERNAL_CANCEL_PEER_SIGMSG_TX_WAIT:
            gamingMode_HandleInternalCancelPeerSigMsgTxWait();
            break;

        case GAMING_MODE_INTERNAL_TOGGLE_MESSAGE:
            gamingMode_HandleGamingModeMsg((earbud_sm_msg_gaming_mode_t*)message);
            break;


        default:
            break;
    }
}

static void RegisterMessageGroup(Task task, message_group_t group)
{
    UNUSED(group);
    GamingMode_GetTaskData()->ui_client_task = task;
}

void EarbudGamingMode_HandlePeerMessage(earbud_sm_msg_gaming_mode_t *msg)
{
    DEBUG_LOG("EarbudGamingMode_HandlePeerMessage: %d",msg->enable);

    /* If the gaming mode is being enabled or, disabled, process after steady state */
    MESSAGE_MAKE(message, earbud_sm_msg_gaming_mode_t);
    message->enable = msg->enable;
    message->mute_timestamp = msg->mute_timestamp;

    MessageSendConditionally(&GamingMode_GetTaskData()->taskdata,
                             GAMING_MODE_INTERNAL_TOGGLE_MESSAGE,
                             message, 
                             GamingMode_GetStateTransitionLockAddr());

}


bool EarbudGamingMode_IsGamingModeEnabled(void)
{
    return ((GamingMode_GetState() == GAMING_MODE_STATE_ENABLED) ||
            (GamingMode_GetState() == GAMING_MODE_STATE_DISABLING));
}

bool EarbudGamingMode_Enable(void)
{
    /* Gaming mode is enabled only if there is an A2DP profile connection */
    if (Av_IsA2dpConnected() && BtDevice_IsMyAddressPrimary() &&
       (GamingMode_GetState() == GAMING_MODE_STATE_DISABLED))
    {
        rtime_t timestamp = gamingMode_GetMuteInstant();
        GamingMode_SetState(GAMING_MODE_STATE_ENABLING, timestamp);
        return TRUE;
    }
    else
    {
        DEBUG_LOG("EarbudGamingMode_Enable: FAILED. State:%d, AV Connected:%d",
                   GamingMode_GetState(), Av_IsA2dpConnected());
        return FALSE;
    }    
}

bool EarbudGamingMode_Disable(void)
{
    if((GamingMode_GetState() == GAMING_MODE_STATE_ENABLED) && 
       BtDevice_IsMyAddressPrimary())
    {
        rtime_t timestamp = gamingMode_GetMuteInstant();
        GamingMode_SetState(GAMING_MODE_STATE_DISABLING, timestamp);
        return TRUE;
    }
    else
    {
        DEBUG_LOG("EarbudGamingMode_Disable: FAILED. State:%d", GamingMode_GetState());
        return FALSE;
    }
}

bool EarbudGamingMode_init(Task init_task)
{
    UNUSED(init_task);
    gaming_mode_task_data_t *data = GamingMode_GetTaskData();
    data->taskdata.handler = earbudGamingModeHandleMessage;
    data->state = GAMING_MODE_STATE_DISABLED;
    appAvStatusClientRegister(&data->taskdata);
    Ui_RegisterUiInputConsumer(&data->taskdata,  gaming_mode_ui_inputs, ARRAY_DIM(gaming_mode_ui_inputs));
    return TRUE;
}

void EarbudGamingMode_HandlePeerSigConnected(const PEER_SIG_CONNECTION_IND_T *ind)
{
    if (ind->status == peerSigStatusConnected)
    {
        if (BtDevice_IsMyAddressPrimary())
        {
            if (GamingMode_GetState() == GAMING_MODE_STATE_ENABLED ||
                GamingMode_GetState() == GAMING_MODE_STATE_ENABLING)
            {
                rtime_t timestamp = delayToTimestamp(GAMING_MODE_MUTE_DELAY);
                gamingMode_SendGamingModeToPeer(TRUE, timestamp);
            }
        }
    }
    else if(ind->status == peerSigStatusDisconnected ||
            ind->status == peerSigStatusLinkLoss)
    {
        /* clear PeerSig Tx lock upon PeerSig disconnection */
        GamingMode_ClearPeerTxLock();
        MessageCancelAll(&GamingMode_GetTaskData()->taskdata, GAMING_MODE_INTERNAL_CANCEL_PEER_SIGMSG_TX_WAIT);
        if(!BtDevice_IsMyAddressPrimary())
        {
           if (GamingMode_GetState() != GAMING_MODE_STATE_DISABLED)
            {
               DEBUG_LOG("EarbudGamingMode_HandlePeerSigConnected connection lost disable gaming mode");
               GamingMode_SetState(GAMING_MODE_STATE_DISABLING, 0);
            }
        }
    }
}

void EarbudGamingMode_HandlePeerSigTxCfm(const PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM_T* cfm)
{
    UNUSED(cfm);
    GamingMode_ClearPeerTxLock();
    MessageCancelAll(&GamingMode_GetTaskData()->taskdata, GAMING_MODE_INTERNAL_CANCEL_PEER_SIGMSG_TX_WAIT);
}

MESSAGE_BROKER_GROUP_REGISTRATION_MAKE(GAMING_MODE_UI, RegisterMessageGroup, NULL);
#endif
