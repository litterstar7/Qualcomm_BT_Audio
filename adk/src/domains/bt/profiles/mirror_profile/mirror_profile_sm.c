/*!
\copyright  Copyright (c) 2019-2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      State machine transitions and logic for mirror_profile.
*/

#ifdef INCLUDE_MIRRORING

#include "bt_device.h"
#include "link_policy.h"

#include "a2dp_profile.h"
#include "hfp_profile.h"
#include "kymera.h"
#include "kymera_adaptation.h"
#include "source_param_types.h"
#include "voice_sources.h"
#include "sdp.h"
#include "timestamp_event.h"
#include "power_manager.h"
#include "telephony_messages.h"

#include "mirror_profile_protected.h"
#include "mirror_profile_signalling.h"
#include "mirror_profile_private.h"
#include "mirror_profile_mdm_prim.h"
#include "mirror_profile_sm.h"
#include "mirror_profile_audio_source.h"
#include "mirror_profile_voice_source.h"

/*
    State transition functions
*/

static void mirrorProfile_CompleteA2dpAudioSourceConnection(void)
{
    if (MirrorProfile_IsPrimary())
    {
        mirror_profile_task_data_t *mp = MirrorProfile_Get();
        if (mp->a2dp_source_to_connect != audio_source_none)
        {
            mp->SendAudioConnect(mp->a2dp_source_to_connect);
            mp->a2dp_source_to_connect = audio_source_none;
        }
    }
}

static void mirrorProfile_CompleteVoiceSourceConnection(void)
{
    if (MirrorProfile_IsPrimary())
    {
        mirror_profile_task_data_t *mp = MirrorProfile_Get();
        if (mp->voice_source_to_connect != voice_source_none)
        {
            Telephony_NotifyCallAudioConnected(mp->voice_source_to_connect);
            mp->voice_source_to_connect = voice_source_none;
        }
    }
}

/*! \brief Enter ACL_CONNECTING state */
static void mirrorProfile_EnterAclConnecting(void)
{
    MIRROR_LOG("mirrorProfile_EnterAclConnecting");

    /* Should never reach this state as Secondary */
    assert(MirrorProfile_IsPrimary());

    /* Send MDM prim to create mirror ACL connection */
    MirrorProfile_MirrorConnectReq(LINK_TYPE_ACL);
}

/*! \brief Exit ACL_CONNECTING state */
static void mirrorProfile_ExitAclConnecting(void)
{
    MIRROR_LOG("mirrorProfile_ExitAclConnecting");
}

/*! \brief Enter ACL_CONNECTED parent state */
static void mirrorProfile_EnterAclConnected(void)
{
    MIRROR_LOG("mirrorProfile_EnterAclConnected");

    if (MirrorProfile_IsPrimary())
    {
        /* Required if the handset has connected A2DP before the secondary connects */
        MirrorProfile_SendA2dpStreamContextToSecondary();
    }

    MirrorProfile_SendAclConnectInd();
}

/*! \brief Exit ACL_CONNECTED parent state */
static void mirrorProfile_ExitAclConnected(void)
{
    MIRROR_LOG("mirrorProfile_ExitAclConnected");
    MirrorProfile_SendAclDisconnectInd();
    MirrorProfile_ClearA2dpMirrorStartLock();
    MirrorProfile_ClearStreamChangeLock();
    mirrorProfile_CompleteVoiceSourceConnection();
}

/*! \brief Enter ACL_DISCONNECTING state */
static void mirrorProfile_EnterAclDisconnecting(void)
{
    mirror_profile_task_data_t *sp = MirrorProfile_Get();

    MIRROR_LOG("mirrorProfile_EnterAclDisconnecting");

    /* Should never reach this state as Secondary
       Well, actually we can because when going into the case the upper layers
       put this into Secondary role before the mirror ACL is disconnected. */
    /*assert(MirrorProfile_IsPrimary());*/

    /* Send MDM prim to disconnect the mirror ACL */
    MirrorProfile_MirrorDisconnectReq(sp->acl.conn_handle, HCI_SUCCESS);
}

/*! \brief Exit ACL_DISCONNECTING state */
static void mirrorProfile_ExitAclDisconnecting(void)
{
    MIRROR_LOG("mirrorProfile_ExitAclDisconnecting");
}

/*! \brief Enter ESCO_CONNECTING state */
static void mirrorProfile_EnterEscoConnecting(void)
{
    source_defined_params_t source_params;
    MIRROR_LOG("mirrorProfile_EnterEscoConnecting");

    TimestampEvent(TIMESTAMP_EVENT_ESCO_MIRRORING_CONNECTING);

    /* Should never reach this state as Secondary */
    assert(MirrorProfile_IsPrimary());

    /* The local HFP SCO should already have been setup and started at this
       point, so we can read the codec params from the hfp voice source. */
    if (VoiceSources_GetConnectParameters(voice_source_hfp_1, &source_params))
    {
        mirror_profile_esco_t *esco = MirrorProfile_GetScoState();
        voice_connect_parameters_t *voice_params = (voice_connect_parameters_t *)source_params.data;
        assert(source_params.data_length == sizeof(voice_connect_parameters_t));

        MirrorProfile_SendHfpCodecAndVolumeToSecondary(voice_params->codec_mode, appHfpGetVolume());

        /* Store parameters locally so state is known on primary->secondary transition */
        esco->codec_mode = voice_params->codec_mode;
        esco->wesco = voice_params->wesco;

        VoiceSources_ReleaseConnectParameters(voice_source_hfp_1, &source_params);
    }

    /* Request creation of mirror eSCO link */
    MirrorProfile_MirrorConnectReq(LINK_TYPE_ESCO);
}

/*! \brief Exit ESCO_CONNECTING state */
static void mirrorProfile_ExitEscoConnecting(void)
{
    MIRROR_LOG("mirrorProfile_ExitEscoConnecting");
}

/*! \brief Enter ESCO_CONNECTED parent state */
static void mirrorProfile_EnterScoConnected(void)
{
    MIRROR_LOG("mirrorProfile_EnterScoConnected");

    TimestampEvent(TIMESTAMP_EVENT_ESCO_MIRRORING_CONNECTED);

    if (MirrorProfile_IsPrimary())
    {
        mirrorProfile_CompleteVoiceSourceConnection();
    }
    else
    {
        if (MirrorProfile_GetScoState()->codec_mode != hfp_codec_mode_none)
        {
            MirrorProfile_StartScoAudio();
            MirrorProfile_Get()->voice_source_to_connect = voice_source_none;
        }
    }

    /* Notify clients that the mirror SCO connection has connected */
    MirrorProfile_SendScoConnectInd();
    MirrorProfile_PeerLinkPolicySetEscoActive();
}

/*! \brief Exit ESCO_CONNECTED parent state */
static void mirrorProfile_ExitScoConnected(void)
{
    MIRROR_LOG("mirrorProfile_ExitScoConnected");

    if (MirrorProfile_IsSecondary())
    {
        MirrorProfile_StopScoAudio();
    }

    /* Notify clients that the mirror SCO connection has disconnected */
    MirrorProfile_SendScoDisconnectInd();
    MirrorProfile_PeerLinkPolicySetIdle();
}

/*! \brief Enter MIRROR_PROFILE_STATE_A2DP_CONNECTED sub-state */
static void mirrorProfile_EnterA2dpConnected(void)
{
    MIRROR_LOG("mirrorProfile_EnterA2dpConnected");
    MirrorProfile_SendA2dpStreamActiveInd();
    if (MirrorProfile_IsSecondary())
    {
        /* Secondary sets new subrate for A2DP, primary waits for subrate
           indication and then its sets the A2DP subrate */
        MirrorProfile_PeerLinkPolicySetA2dpActive();
    }
    TimestampEvent(TIMESTAMP_EVENT_A2DP_MIRRORING_CONNECTED);

    mirrorProfile_CompleteA2dpAudioSourceConnection();

    if (MirrorProfile_IsPrimary())
    {
        /* Primary needs to start audio synchronisation (secondary starts when
        entering A2dpConnecting state).
        Audio will clear lock once audio synchronisation is setup.
        Delay starting audio sync by one message to allow the main
        a2dp start to be delivered to kymera first. */
        MirrorProfile_SetAudioStartLock();
        MessageSend(MirrorProfile_GetTask(), MIRROR_INTERNAL_START_AUDIO_SYNC, NULL);
    }
}

/*! \brief Exit MIRROR_PROFILE_STATE_A2DP_CONNECTED sub-state */
static void mirrorProfile_ExitA2dpConnected(void)
{
    MIRROR_LOG("mirrorProfile_ExitA2dpConnected");
    MirrorProfile_ClearAudioStartLock();
    MirrorProfile_SendA2dpStreamInactiveInd();
    MirrorProfile_StopAudioSynchronisation();
    if (MirrorProfile_IsSecondary())
    {
        MirrorProfile_StopAudio();
    }
    mirrorProfile_CompleteA2dpAudioSourceConnection();
    MirrorProfile_PeerLinkPolicySetIdle();
}

/*! \brief Enter ESCO_DISCONNECTING state */
static void mirrorProfile_EnterEscoDisconnecting(void)
{
    mirror_profile_task_data_t *sp = MirrorProfile_Get();

    MIRROR_LOG("mirrorProfile_EnterEscoDisconnecting");

    /* Should never reach this state as Secondary */
    assert(MirrorProfile_IsPrimary());

    /* Send MDM prim to disconnect the mirror eSCO */
    MirrorProfile_MirrorDisconnectReq(sp->esco.conn_handle, HCI_SUCCESS);
}

/*! \brief Exit ESCO_DISCONNECTING state */
static void mirrorProfile_ExitEscoDisconnecting(void)
{
    MIRROR_LOG("mirrorProfile_ExitEscoDisconnecting");
}

/* \brief Enter MIRROR_PROFILE_STATE_A2DP_CONNECTING state */
static void mirrorProfile_EnterA2dpConnecting(void)
{
    mirror_profile_task_data_t *sp = MirrorProfile_Get();

    MIRROR_LOG("mirrorProfile_EnterA2dpConnecting");
    TimestampEvent(TIMESTAMP_EVENT_A2DP_MIRRORING_CONNECTING);
    appPowerPerformanceProfileRequest();

    if (MirrorProfilePeerMode_GetState() == MIRROR_PROFILE_PEER_MODE_STATE_ACTIVE)
    {
        /* If the link is in active mode, set the subrate policy now then upon
           re-entering sniff mode the link will subrate. */
        MirrorProfile_PeerLinkPolicySetA2dpActive();
    }

    if (MirrorProfile_IsPrimary())
    {
        MirrorProfile_MirrorL2capConnectReq(sp->acl.conn_handle, sp->a2dp.cid);
        MirrorProfile_SetA2dpMirrorStartLock();
    }
    else
    {
        MirrorProfile_StartAudio();
        MirrorProfile_StartAudioSynchronisation();
    }
}

/* \brief Exit MIRROR_PROFILE_STATE_A2DP_CONNECTING state */
static void mirrorProfile_ExitA2dpConnecting(mirror_profile_state_t new_state)
{
    MIRROR_LOG("mirrorProfile_ExitA2dpConnecting");

    MirrorProfile_ClearA2dpMirrorStartLock();
    appPowerPerformanceProfileRelinquish();

    /* Failed to correctly start mirroring, stop audio */
    if (new_state != MIRROR_PROFILE_STATE_A2DP_CONNECTED)
    {
        if (MirrorProfile_IsSecondary())
        {
            MirrorProfile_StopAudioSynchronisation();
            MirrorProfile_StopAudio();
        }
        mirrorProfile_CompleteA2dpAudioSourceConnection();
        MirrorProfile_PeerLinkPolicySetIdle();
    }
}

/* \brief Enter MIRROR_PROFILE_STATE_A2DP_DISCONNECTING state */
static void mirrorProfile_EnterA2dpDisconnecting(void)
{
    mirror_profile_task_data_t *sp = MirrorProfile_Get();

    MIRROR_LOG("mirrorProfile_EnterA2dpDisconnecting");

    if (MirrorProfile_IsPrimary())
    {
        MirrorProfile_MirrorL2capDisconnectReq(sp->a2dp.cid);
    }
}

/* \brief Exit MIRROR_PROFILE_STATE_A2DP_DISCONNECTING state */
static void mirrorProfile_ExitA2dpDisconnecting(void)
{
    MIRROR_LOG("mirrorProfile_ExitA2dpDisconnecting");
}

static bool mirrorProfile_StateTransitionRequiresPeerSniffMode(mirror_profile_state_t state)
{
    switch (state)
    {
    /* Link must be in sniff mode to start ACL mirroring */
    case MIRROR_PROFILE_STATE_ACL_CONNECTING:
        return TRUE;
    default:
        return FALSE;
    }
}

inline static bool mirrorProfile_StartA2dpInActiveMode(void)
{
    /* Entering active mode when already streaming may provoke a glitch */
    return (MirrorProfile_Get()->a2dp_source_to_connect != audio_source_none);
}

static bool mirrorProfile_StateTransitionRequiresPeerActiveMode(mirror_profile_state_t state)
{
    switch (state)
    {
    /* Active mode speeds up eSCO and A2DP mirror connection */
    case MIRROR_PROFILE_STATE_ESCO_CONNECTING:
        return TRUE;
    case MIRROR_PROFILE_STATE_A2DP_CONNECTING:
        return mirrorProfile_StartA2dpInActiveMode();
    default:
        return FALSE;
    }
}

static bool mirrorProfile_StateTransitionRequiresHandsetActiveMode(mirror_profile_state_t state)
{
    switch (state)
    {
    /* Handset must be active prior to starting A2DP mirror */
    case MIRROR_PROFILE_STATE_A2DP_CONNECTING:
        return TRUE;
    default:
        return FALSE;
    }
}

void MirrorProfile_SetState(mirror_profile_state_t state)
{
    mirror_profile_task_data_t *sp = MirrorProfile_Get();
    mirror_profile_state_t old_state = sp->state;
    mirror_profile_peer_mode_state_t pm_state = MIRROR_PROFILE_PEER_MODE_STATE_SNIFF;

    /* It is not valid to re-enter the same state */
    assert(old_state != state);

    DEBUG_LOG_STATE("MirrorProfile_SetState state(%02x) old_state(%02x)", state, old_state);

    /* Handle state exit functions */
    switch (old_state)
    {
    case MIRROR_PROFILE_STATE_ACL_CONNECTING:
        mirrorProfile_ExitAclConnecting();
        break;
    case MIRROR_PROFILE_STATE_ACL_DISCONNECTING:
        mirrorProfile_ExitAclDisconnecting();
        break;
    case MIRROR_PROFILE_STATE_ESCO_CONNECTING:
        mirrorProfile_ExitEscoConnecting();
        break;
    case MIRROR_PROFILE_STATE_ESCO_DISCONNECTING:
        mirrorProfile_ExitEscoDisconnecting();
        break;
    case MIRROR_PROFILE_STATE_A2DP_CONNECTING:
        mirrorProfile_ExitA2dpConnecting(state);
        break;
    case MIRROR_PROFILE_STATE_A2DP_DISCONNECTING:
        mirrorProfile_ExitA2dpDisconnecting();
        break;
    default:
        break;
    }

    /* Check if exiting ACL connected sub-state */
    if (MirrorProfile_IsSubStateAclConnected(old_state) && !MirrorProfile_IsSubStateAclConnected(state))
        mirrorProfile_ExitAclConnected();

    /* Check if exiting SCO connected sub-state */
    if (MirrorProfile_IsSubStateEscoConnected(old_state) && !MirrorProfile_IsSubStateEscoConnected(state))
        mirrorProfile_ExitScoConnected();

    /* Check if exiting A2DP connected sub-state */
    if (MirrorProfile_IsSubStateA2dpConnected(old_state) && !MirrorProfile_IsSubStateA2dpConnected(state))
        mirrorProfile_ExitA2dpConnected();

    /* Check if exiting a steady state */
    if (MirrorProfile_IsSteadyState(old_state) && !MirrorProfile_IsSteadyState(state))
        MirrorProfile_SetTransitionLockBitSm();

    /* Set new state */
    sp->state = state;

    /* Check if entering ACL connected sub-state */
    if (!MirrorProfile_IsSubStateAclConnected(old_state) && MirrorProfile_IsSubStateAclConnected(state))
        mirrorProfile_EnterAclConnected();

    /* Check if entering SCO connected sub-state */
    if (!MirrorProfile_IsSubStateEscoConnected(old_state) && MirrorProfile_IsSubStateEscoConnected(state))
        mirrorProfile_EnterScoConnected();

    /* Check if entering A2DP connected sub-state */
    if (!MirrorProfile_IsSubStateA2dpConnected(old_state) && MirrorProfile_IsSubStateA2dpConnected(state))
        mirrorProfile_EnterA2dpConnected();

    /* Check if entering a steady state */
    if (!MirrorProfile_IsSteadyState(old_state) && MirrorProfile_IsSteadyState(state))
        MirrorProfile_ClearTransitionLockBitSm();

    pm_state = MIRROR_PROFILE_PEER_MODE_STATE_SNIFF;

    /* Handle state entry functions */
    switch (sp->state)
    {
    case MIRROR_PROFILE_STATE_ACL_CONNECTING:
        mirrorProfile_EnterAclConnecting();
        break;
    case MIRROR_PROFILE_STATE_ACL_CONNECTED:
        break;
    case MIRROR_PROFILE_STATE_ACL_DISCONNECTING:
        mirrorProfile_EnterAclDisconnecting();
        break;
    case MIRROR_PROFILE_STATE_ESCO_CONNECTING:
        mirrorProfile_EnterEscoConnecting();
        pm_state = MIRROR_PROFILE_PEER_MODE_STATE_ACTIVE;
        break;
    case MIRROR_PROFILE_STATE_ESCO_CONNECTED:
        break;
    case MIRROR_PROFILE_STATE_ESCO_DISCONNECTING:
        mirrorProfile_EnterEscoDisconnecting();
        break;
    case MIRROR_PROFILE_STATE_A2DP_CONNECTING:
        mirrorProfile_EnterA2dpConnecting();
        if (mirrorProfile_StartA2dpInActiveMode())
        {
            pm_state = MIRROR_PROFILE_PEER_MODE_STATE_ACTIVE;
        }
        break;
    case MIRROR_PROFILE_STATE_A2DP_CONNECTED:
        break;
    case MIRROR_PROFILE_STATE_A2DP_DISCONNECTING:
        mirrorProfile_EnterA2dpDisconnecting();
        break;
    default:
        break;
    }

    /*  Now the state change is complete, kick the SM to transition towards
        the target state. The target state is only used in primary role. */
    if (MirrorProfile_IsPrimary())
    {
        /* Having entered the new state, ensure the peer mode is in the correct state */
        mirrorProfilePeerMode_SetTargetState(pm_state);
        MirrorProfile_SmKick();
    }
}

/*! \brief Handle mirror_profile error

    Some error occurred in the mirror_profile state machine.

    To avoid the state machine getting stuck, if instance is connected then
    drop connection and move to 'disconnecting' state.
*/
void MirrorProfile_StateError(MessageId id, Message message)
{
    UNUSED(message);
    UNUSED(id);

    MIRROR_LOG("MirrorProfile_StateError state 0x%x id 0x%x", MirrorProfile_GetState(), id);

    Panic();
}

/*! \brief Logic to transition from current state to target state.

    \return The next state to enter in the transition to the target state.

    Generally, the logic determines the transitionary state to enter from the
    current steady state.
 */
static mirror_profile_state_t mirrorProfile_SmTransition(void)
{
    switch (MirrorProfile_GetTargetState())
    {
    case MIRROR_PROFILE_STATE_DISCONNECTED:
        switch (MirrorProfile_GetState())
        {
        case MIRROR_PROFILE_STATE_ACL_CONNECTED:
            return MIRROR_PROFILE_STATE_ACL_DISCONNECTING;
        case MIRROR_PROFILE_STATE_ESCO_CONNECTED:
            return MIRROR_PROFILE_STATE_ESCO_DISCONNECTING;
        case MIRROR_PROFILE_STATE_A2DP_CONNECTED:
            return MIRROR_PROFILE_STATE_A2DP_DISCONNECTING;
        default:
            break;
        }
        break;

    case MIRROR_PROFILE_STATE_ACL_CONNECTED:
        switch (MirrorProfile_GetState())
        {
        case MIRROR_PROFILE_STATE_DISCONNECTED:
            return MIRROR_PROFILE_STATE_ACL_CONNECTING;
        case MIRROR_PROFILE_STATE_ESCO_CONNECTED:
            return MIRROR_PROFILE_STATE_ESCO_DISCONNECTING;
        case MIRROR_PROFILE_STATE_A2DP_CONNECTED:
            return MIRROR_PROFILE_STATE_A2DP_DISCONNECTING;
        default:
            break;
        }
        break;

    case MIRROR_PROFILE_STATE_ESCO_CONNECTED:
        switch (MirrorProfile_GetState())
        {
        case MIRROR_PROFILE_STATE_DISCONNECTED:
            return MIRROR_PROFILE_STATE_ACL_CONNECTING;
        case MIRROR_PROFILE_STATE_ACL_CONNECTED:
            return MIRROR_PROFILE_STATE_ESCO_CONNECTING;
        case MIRROR_PROFILE_STATE_A2DP_CONNECTED:
            return MIRROR_PROFILE_STATE_A2DP_DISCONNECTING;
        default:
            break;
        }
        break;

    case MIRROR_PROFILE_STATE_A2DP_CONNECTED:
        switch (MirrorProfile_GetState())
        {
        case MIRROR_PROFILE_STATE_DISCONNECTED:
            return MIRROR_PROFILE_STATE_ACL_CONNECTING;
        case MIRROR_PROFILE_STATE_ACL_CONNECTED:
            return MIRROR_PROFILE_STATE_A2DP_CONNECTING;
        case MIRROR_PROFILE_STATE_ESCO_CONNECTED:
            return MIRROR_PROFILE_STATE_ESCO_DISCONNECTING;
        default:
            break;
        }
        break;
    default:
        Panic();
        break;
    }
    return MirrorProfile_GetState();
}

static void mirrorProfile_DoSmTransition(void)
{
    mirror_profile_state_t next = mirrorProfile_SmTransition();
    bool peer_mode_ready = TRUE;
    bool handset_mode_ready = TRUE;

    if (next != MirrorProfile_GetState())
    {
        /* Handle sniff/active mode requirements before changing state. */
        if (mirrorProfile_StateTransitionRequiresPeerSniffMode(next))
        {
            peer_mode_ready = mirrorProfilePeerMode_SetTargetState(MIRROR_PROFILE_PEER_MODE_STATE_SNIFF);
        }
        else if (mirrorProfile_StateTransitionRequiresPeerActiveMode(next))
        {
            peer_mode_ready = mirrorProfilePeerMode_SetTargetState(MIRROR_PROFILE_PEER_MODE_STATE_ACTIVE);
        }

        if (mirrorProfile_StateTransitionRequiresHandsetActiveMode(next))
        {
            /* Mirror profile does not activity attempt to change the handset
               mode - it passively waits for the correct mode to be entered */
            handset_mode_ready = (MirrorProfile_Get()->handset_mode == lp_active);
        }
        MIRROR_LOG("mirrorProfile_DoSmTransition %d %d", peer_mode_ready, handset_mode_ready);

        if (peer_mode_ready && handset_mode_ready)
        {
            MirrorProfile_SetState(next);
        }
        /* If not in the right mode, peer mode SM will kick back when in the required mode */
    }
}

void MirrorProfile_SmKick(void)
{
    mirror_profile_state_t current = MirrorProfile_GetState();

    if (MirrorProfile_GetDelayKick())
    {
        mirror_profile_state_t target = MirrorProfile_GetTargetState();

        MirrorProfile_ClearDelayKick();
        if (target != current)
        {
            /* If not in the target state, then schedule a message to kick the
               SM later */
            MessageCancelFirst(MirrorProfile_GetTask(), MIRROR_INTERNAL_DELAYED_KICK);
            MessageSendLater(MirrorProfile_GetTask(), MIRROR_INTERNAL_DELAYED_KICK,
                             NULL, MIRROR_PROFILE_KICK_LATER_DELAY);
        }
    }
    else
    {
        /* Only allow when in steady state. */
        if (MirrorProfile_IsSteadyState(current) && mirrorProfilePeerMode_IsInSteadyState())
        {
            if (MirrorProfile_IsAudioSyncL2capConnected())
            {
                mirrorProfile_DoSmTransition();
                MessageCancelAll(MirrorProfile_GetTask(), MIRROR_INTERNAL_DELAYED_KICK);
            }
            else
            {
                /* If the audio sync L2CAP is disconnected, it means the secondary
                is disconnecting the link to the primary (e.g. it has gone in the case).
                In this scenario, the target state is MIRROR_PROFILE_STATE_DISCONNECTED.
                Instead of initiating disconnects on the mirror links, just wait
                for the links to naturally drop as a result of the ACL between the
                two buds disconnecting. As the links drop, the state will thus
                naturally return to state MIRROR_PROFILE_STATE_DISCONNECTED. */
                MIRROR_LOG("MirrorProfile_SmKick ignoring l2cap disconnected");
            }
        }
        else
        {
            MIRROR_LOG("MirrorProfile_SmKick not steady state %d %d ", MirrorProfile_IsSteadyState(current), mirrorProfilePeerMode_IsInSteadyState());
        }
    }
}

void MirrorProfile_SetTargetState(mirror_profile_state_t target_state)
{
    MessageCancelFirst(MirrorProfile_GetTask(), MIRROR_INTERNAL_SET_TARGET_STATE);

    if (MirrorProfile_GetLock())
    {
        /* Change in target state must be deferred until the SMs reach steady state */
        MESSAGE_MAKE(msg, MIRROR_INTERNAL_SET_TARGET_STATE_T);
        msg->target_state = target_state;
        MessageSendConditionally(MirrorProfile_GetTask(), MIRROR_INTERNAL_SET_TARGET_STATE,
                                 msg, &MirrorProfile_GetLock());

        DEBUG_LOG_INFO("MirrorProfile_SetTargetState 0x%x waiting for stable state", target_state);
    }
    else
    {
        DEBUG_LOG_INFO("MirrorProfile_SetTargetState 0x%x", target_state);

        /* Target state can be changed immeidately */
        MirrorProfile_Get()->target_state = target_state;
        MirrorProfile_SmKick();
    }
}

#endif /* INCLUDE_MIRRORING */
