/*!
\copyright  Copyright (c) 2008 - 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       a2dp_profile.c
\brief      A2DP State Machine
*/

/* Only compile if AV defined */
#ifdef INCLUDE_AV

#include "a2dp_profile.h"

#include "av.h"
#include <av_instance.h>
#include "a2dp_profile_audio.h"
#include "a2dp_profile_caps.h"
#include "a2dp_profile_config.h"
#include "a2dp_profile_sync.h"
#include "a2dp_profile_protected.h"
#include "audio_sources.h"
#include "audio_sources_list.h"
#include "audio_sync.h"
#include <device.h>
#include <device_list.h>
#include <device_properties.h>
#include "kymera.h"
#include "kymera_adaptation.h"
#include "kymera_config.h"
#include "kymera_latency_manager.h"
#include "latency_config.h"
#include "power_manager.h"
#include "ui.h"
#include "timestamp_event.h"

#include <connection_manager.h>

#include <a2dp.h>
#include <avrcp.h>
#include <panic.h>
#include <connection.h>
#include <kalimba.h>
#include <kalimba_standard_messages.h>
#include <ps.h>
#include <string.h>
#include <stdlib.h>
#include <stream.h>

/*! Code assertion that can be checked at run time. This will cause a panic. */
#define assert(x) PanicFalse(x)

/*! Macro for simplifying creating messages */
#define MAKE_AV_MESSAGE(TYPE) \
    TYPE##_T *message = PanicUnlessNew(TYPE##_T);

#include "adk_log.h"

static bool pts_mode_enabled = FALSE;

/* Local Function Prototypes */
static void appA2dpSetState(avInstanceTaskData *theInst, avA2dpState a2dp_state);

/*! \brief Convert from Sink SEID to Source SEID
    \param seid The sink seid to convert
    \return The converted source seid

    This function takes a Sink SEID and attempts to convert it
    into the appropriate Source SEID.
*/
uint8 appA2dpConvertSeidFromSinkToSource(uint8 seid)
{
    /* In PTS mode, source CODEC should be SBC */
    if (A2dpProfile_IsPtsMode())
    {
        switch (seid)
        {
            case AV_SEID_AAC_SNK:
            case AV_SEID_SBC_SNK:
                return AV_SEID_SBC_SRC;
            default:
                return AV_SEID_INVALID;
        }
    }
    else
    {
        switch (seid)
        {
            case AV_SEID_SBC_SNK:
                return AV_SEID_SBC_MONO_TWS_SRC;
            case AV_SEID_AAC_SNK:
                return appConfigAacStereoForwarding() ?
                        AV_SEID_AAC_STEREO_TWS_SRC :
                        AV_SEID_SBC_MONO_TWS_SRC;
            case AV_SEID_APTX_SNK:
                return AV_SEID_APTX_MONO_TWS_SRC;
            default:
                return AV_SEID_INVALID;
        }
    }
}

/*! \brief Test if two instances have compatible SEIDs
    \param inst1 The first instance
    \param inst2 The second instance
    \return TRUE if compatible, FALSE if incompatible
    \note To be compatible, there must be one source SEID and one sink SEID that
          match when converted between sink/source.
*/
bool appA2dpSeidsAreCompatible(const avInstanceTaskData *inst1, const avInstanceTaskData *inst2)
{
    if (appA2dpIsSinkCodec(inst1))
    {
        return (appA2dpConvertSeidFromSinkToSource(inst1->a2dp.current_seid) == inst2->a2dp.current_seid);
    }
    else if (appA2dpIsSinkCodec(inst2))
    {
        return (appA2dpConvertSeidFromSinkToSource(inst2->a2dp.current_seid) == inst1->a2dp.current_seid);
    }
    return FALSE;
}

a2dpTaskData * A2dpProfile_GetPeerData(void)
{
    bdaddr address;
    a2dpTaskData * a2dp = NULL;
    if(appDeviceGetPeerBdAddr(&address))
    {
        a2dp = &(appAvInstanceFindFromBdAddr(&address)->a2dp);
    }
    DEBUG_LOG("A2dpProfile_GetPeerData =%p, peer %04x,%02x,%06lx", a2dp, address.lap, address.uap, address.nap);
    return a2dp;
}

static a2dp_codec_settings *appA2dpGetCodecSettings(const avInstanceTaskData *theInst)
{
    a2dp_codec_settings *codec_settings = A2dpCodecGetSettings(theInst->a2dp.device_id,
                                                               theInst->a2dp.stream_id);

    /* If pointer is valid and is_twsp_mode is set then change SEID to virtual
     * SEID AV_SEID_APTX_ADAPTIVE_TWS_SNK */
    if (codec_settings && codec_settings->codecData.aptx_ad_params.is_twsp_mode)
        codec_settings->seid = AV_SEID_APTX_ADAPTIVE_TWS_SNK;

    return codec_settings;
}


/*! \brief Update UI to show streaming state

    This function updates the UI when streaming is active, the current SEID
    is checked to differentiate between the codec type streaming.
*/
static void appA2dpStreamingActiveUi(avInstanceTaskData *theInst)
{
    /* Call appropriate UI function */
    switch (theInst->a2dp.current_seid)
    {
        case AV_SEID_SBC_SNK:
        case AV_SEID_AAC_SNK:
        case AV_SEID_SBC_MONO_TWS_SNK:
        case AV_SEID_AAC_STEREO_TWS_SNK:
            appAvSendUiMessageId(AV_STREAMING_ACTIVE);
            return;

        case AV_SEID_APTX_SNK:
        case AV_SEID_APTXHD_SNK:
        case AV_SEID_APTX_ADAPTIVE_SNK:
        case AV_SEID_APTX_MONO_TWS_SNK:
        case AV_SEID_APTX_ADAPTIVE_TWS_SNK:
            appAvSendUiMessageId(AV_STREAMING_ACTIVE_APTX);
            return;
    }
}

/*! \brief Update UI to show streaming inactive

    This function updates the UI when streaming becomes inactive.
    If just calls the appropriate UI module function.
*/
static void appA2dpStreamingInactiveUi(avInstanceTaskData *theInst)
{
    if (appA2dpIsSinkCodec(theInst))
    {
        appAvSendUiMessageId(AV_STREAMING_INACTIVE);
    }
}

uint8 A2dpProfile_GetDefaultVolume(void)
{
    /* Set default volume as set in configuration */
    const int rangeDb = appConfigMaxVolumedB() - appConfigMinVolumedB();
    return (uint8)((appConfigDefaultVolumedB() - appConfigMinVolumedB()) * VOLUME_MAX / rangeDb);
}

static void connectPeerSource(avInstanceTaskData *theInst)
{
    source_defined_params_t source_params;

    if (A2dpProfile_GetForwardingConnectParameters(&source_params))
    {
        connect_parameters_t connect_params =
            { .source = { .type = source_type_audio, .sourceu = audio_source_a2dp_1 },
               .source_params = source_params};
        KymeraAdaptation_Connect(&connect_params);
        A2dpProfile_FreeForwardingConnectParameters(&source_params);
    }
    else
    {
        DEBUG_LOG("appA2dpStartAudio(%p) no codec_settings not starting kymera", (void *)theInst);
    }
}

static void disconnectPeerSource(avInstanceTaskData *theInst)
{
    source_defined_params_t source_params;

    if (A2dpProfile_GetForwardingDisconnectParameters(&source_params))
    {
        disconnect_parameters_t disconnect_params = {
            .source = { .type = source_type_audio, .sourceu = audio_source_a2dp_1 },
            .source_params = source_params};

        KymeraAdaptation_Disconnect(&disconnect_params);
        A2dpProfile_FreeForwardingDisconnectParameters(&source_params);
    }
    else
    {
        DEBUG_LOG("appA2dpStopAudio(%p) no codec_settings not starting kymera", (void *)theInst);
    }
}

static audio_source_t a2dpProfile_LookupSource(avInstanceTaskData *theInst)
{
    void * ptr_to_source = NULL;
    audio_source_t source = audio_source_none;
    size_t size = sizeof(audio_source_t);
    device_t device = Av_GetDeviceForInstance(theInst);

    PanicFalse(device != NULL);
    PanicFalse(Device_GetProperty(device, device_property_audio_source, &ptr_to_source, &size));

    source = *((audio_source_t *)ptr_to_source);

    DEBUG_LOG_VERBOSE("a2dpProfile_LookupSource device=%04x source=%d", device, source);
    return source;
}

/*! \brief Send a status message indicating audio connection and let the service
           do the connection */
static void appA2dpSendAudioConnectMessage(audio_source_t source)
{
    MESSAGE_MAKE(message, AV_A2DP_AUDIO_CONNECT_MESSAGE_T);
    message->audio_source = source;
    appAvSendStatusMessage(AV_A2DP_AUDIO_CONNECTED, (void *)message, sizeof(audio_source_t));
}

/*! Function to call to start the A2DP audio chains. By default this is the
    internal function, but external entity may override it. */
static A2dpProfile_SendAudioMessage sendAudioConnectMessage = appA2dpSendAudioConnectMessage;

/*! \brief Send a status message indicating audio disconnection and let the service
           do the disconnection */
static void appA2dpSendAudioDisconnectMessage(audio_source_t source)
{
    MESSAGE_MAKE(message, AV_A2DP_AUDIO_CONNECT_MESSAGE_T);
    message->audio_source = source;
    /* Send message with audio_source_a2dp as a parameter and let the service do the disconnection */
    appAvSendStatusMessage(AV_A2DP_AUDIO_DISCONNECTED, (void *)message, sizeof(audio_source_t));
}

/*! Function to call to stop the A2DP audio chains. By default this is the
    internal function, but external entity may override it. */
static A2dpProfile_SendAudioMessage sendAudioDisconnectMessage = appA2dpSendAudioDisconnectMessage;

A2dpProfile_SendAudioMessage A2dpProfile_SetSendAudioConnectMessageFunction(A2dpProfile_SendAudioMessage override)
{
    A2dpProfile_SendAudioMessage prev = sendAudioConnectMessage;
    sendAudioConnectMessage = override;
    return prev;
}

A2dpProfile_SendAudioMessage A2dpProfile_SetSendAudioDisconnectMessageFunction(A2dpProfile_SendAudioMessage override)
{
    A2dpProfile_SendAudioMessage prev = sendAudioDisconnectMessage;
    sendAudioDisconnectMessage = override;
    return prev;
}

/*! \brief Start audio. */
static void appA2dpStartAudio(avInstanceTaskData *theInst)
{
    DEBUG_LOG("appA2dpStartAudio(%p), device_id=%d, stream_id=%d", (void *)theInst, theInst->a2dp.device_id, theInst->a2dp.stream_id);

    appA2dpSetAudioStartLockBit(theInst);

    if (theInst == appAvGetA2dpSource())
    {
        connectPeerSource(theInst);
    }
    else
    {
        audio_source_t source  = Av_GetSourceForInstance(theInst);
        if(source != audio_source_none)
        {
            sendAudioConnectMessage(source);
        }
    }
}

/*! \brief Stop audio. */
static void appA2dpStopAudio(avInstanceTaskData *theInst)
{
    DEBUG_LOG("appA2dpStopAudio(%p), device_id=%d, stream_id=%d", (void *)theInst, theInst->a2dp.device_id, theInst->a2dp.stream_id);
    if (theInst == appAvGetA2dpSource())
    {
        disconnectPeerSource(theInst);
    }
    else
    {
        device_t device = Av_FindDeviceFromInstance(theInst);
        audio_source_t source  = Av_GetSourceForInstance(theInst);

        if (BtDevice_GetDeviceType(device) == DEVICE_TYPE_HANDSET)
        {
            /* Use the lock to prevent a call to appAvInstanceDestroy until cleared */
            appA2dpSetAudioStopLockBit(theInst);
            DEBUG_LOG("appA2dpStopAudio(%p) %p %u", (void *)theInst, &appA2dpGetLock(theInst), appA2dpGetLock(theInst));
        }

        if(source != audio_source_none)
        {
            sendAudioDisconnectMessage(source);
        }
    }
}

/*! \brief Called on exiting a '_SYNC' state.
           The _SYNC state can be exited for reasons other than receiving
           a #AV_INTERNAL_A2DP_INST_SYNC_RES, for example, receiving a disconnect
           message from the A2DP library. In this case, when the
           #AV_INTERNAL_A2DP_INST_SYNC_RES is finally received, the handler
           (#appA2dpHandleInternalA2dpInstSyncResponse) needs to know the message
           should be ignored. Incrementing the a2dp_sync_counter achieves this.
*/
static void appA2dpInstSyncExit(avInstanceTaskData *theInst)
{
    theInst->a2dp.sync_counter++;
}

/*! \brief Enter A2DP_STATE_CONNECTING_LOCAL

    The A2DP state machine has entered 'connecting local' state, set the
    lock to serialise connect attempts and block and other operations on this
    instance.
*/
static void appA2dpEnterConnectingLocal(avInstanceTaskData *theInst)
{
    DEBUG_LOG("appA2dpEnterConnectingLocal(%p)", (void *)theInst);

    /* Set operation lock */
    appA2dpSetTransitionLockBit(theInst);

    /* Clear detach pending flag */
    theInst->detach_pending = FALSE;

    /* Set locally initiated flag */
    theInst->a2dp.bitfields.local_initiated = TRUE;
}

/*! \brief Exit A2DP_STATE_CONNECTING_LOCAL

    The A2DP state machine has exited 'connecting local' state, clear the
    lock to allow pending connection attempts
    and any pending operations on this instance to proceed.
*/
static void appA2dpExitConnectingLocal(avInstanceTaskData *theInst)
{
    DEBUG_LOG("appA2dpExitConnectingLocal(%p)", (void *)theInst);

    /* Clear operation lock */
    appA2dpClearTransitionLockBit(theInst);

    /* We have finished (successfully or not) attempting to connect, so
     * we can relinquish our lock on the ACL.  Bluestack will then close
     * the ACL when there are no more L2CAP connections */
    ConManagerReleaseAcl(&theInst->bd_addr);
}

/*! \brief Enter A2DP_STATE_CONNECTING_REMOTE

    The A2DP state machine has entered 'connecting remote' state, set the
    operation lock to block any other operations on this instance.
*/
static void appA2dpEnterConnectingRemote(avInstanceTaskData *theInst)
{
    DEBUG_LOG("appA2dpEnterConnectingRemote(%p)", (void *)theInst);

    /* Set operation lock */
    appA2dpSetTransitionLockBit(theInst);

    /* Clear detach pending flag */
    theInst->detach_pending = FALSE;

    /* Clear locally initiated flag */
    theInst->a2dp.bitfields.local_initiated = FALSE;
}

/*! \brief Exit A2DP_STATE_CONNECTING_REMOTE

    The A2DP state machine has exited 'connecting remote' state, clear the
    operation lock to allow any pending operations on this instance to proceed.
*/
static void appA2dpExitConnectingRemote(avInstanceTaskData *theInst)
{
    DEBUG_LOG("appA2dpExitConnectingRemote(%p)", (void *)theInst);

    /* Clear operation lock */
    appA2dpClearTransitionLockBit(theInst);
}

/*! \brief Enter A2DP_STATE_CONNECTING_CROSSOVER

    This state is used to track when another connection attempt occurs whilst
    the original connection attempt is still in progress.  We'll stay in this
    state until either one of the connection succeeds or fails.
*/
static void appA2dpEnterConnectingCrossover(avInstanceTaskData *theInst)
{
    DEBUG_LOG("appA2dpEnterConnectingCrossover(%p)", (void *)theInst);

    /* Set operation lock */
    appA2dpSetTransitionLockBit(theInst);
}

/*! \brief Exit A2DP_STATE_CONNECTING_CROSSOVER

    Only one connection is left, so no longer in the crossover state.
*/
static void appA2dpExitConnectingCrossover(avInstanceTaskData *theInst)
{
    DEBUG_LOG("appA2dpExitConnectingCrossover(%p)", (void *)theInst);

    /* Clear operation lock */
    appA2dpClearTransitionLockBit(theInst);
}

/*! \brief Determine if device is PTS tester.

    \param bd_addr Pointer to read-only device BT address to check.
    \return bool TRUE if device is PTES tester, FALSE if it isn't.
*/
static bool a2dpProfile_DeviceIsPts(const bdaddr *bd_addr)
{
    device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);
    if (device)
    {
        uint16 flags = 0;
        Device_GetPropertyU16(device, device_property_flags, &flags);
        return !!(flags & DEVICE_FLAGS_IS_PTS);
    }
    else
        return FALSE;
}

/*! \brief  Enter A2DP_STATE_CONNECTED_SIGNALLING

    The A2DP state machine has entered 'connected signalling' state, this means
    that the A2DP signalling channel has been established.

    Kick the link policy manager to make sure this link is configured correctly
    and to maintain the correct link topology.

    Check if we need to create media channel immediately, either because this
    is a locally initiated connection with SEIDs specified or because there
    is already another A2DP sink with media channel established.
*/
static void appA2dpEnterConnectedSignalling(avInstanceTaskData *theInst)
{
    uint8 seid = AV_SEID_INVALID;
    bool is_handset = appDeviceIsHandset(&theInst->bd_addr);
    bool is_peer = appDeviceIsPeer(&theInst->bd_addr);
    bool handset_media_connected = FALSE;


    DEBUG_LOG("appA2dpEnterConnectedSignalling(%p)(PARENT)", (void *)theInst);

    /* Set PTS test mode if device is PTS tester */
    if (a2dpProfile_DeviceIsPts(&theInst->bd_addr))
    {
        A2dpProfile_SetPtsMode(TRUE);
    }

    /* Mark this device as supporting A2DP */
    BtDevice_SetSupportedProfile(&theInst->bd_addr, DEVICE_PROFILE_A2DP);

    /* Clear current SEID */
    theInst->a2dp.current_seid = AV_SEID_INVALID;
    theInst->a2dp.stream_id = 0;

    /* This is intended to check if the handset a2dp media channel is connected
       and if so, select a peer seid to use based on the handset seid. */
    handset_media_connected = appAvInstanceShouldConnectMediaChannel(theInst, &seid);

    DEBUG_LOG("appA2dpEnterConnectedSignalling connect_media_av %d seid 0x%x connect_flag %d is_handset %d",
              handset_media_connected, seid, appA2dpIsConnectMediaFlagSet(theInst), is_handset);

    /* Request A2DP media connect if:
       * This is the peer A2DP inst and the handset A2DP is connected and has a valid seid.
       * This is the handset A2DP inst.

       When sending the media connect internal msg:
       * If A2DP_CONNECT_MEDIA is set don't use a delay (locally initiated A2DP)
       * Otherwise use a delay (remotely initiated A2DP)
    */
    if (handset_media_connected || is_handset)
    {
        MAKE_AV_MESSAGE(AV_INTERNAL_A2DP_CONNECT_MEDIA_REQ);
        message->seid = seid;
        message->delay_ms = appA2dpIsConnectMediaFlagSet(theInst) ?
                                0 : appConfigA2dpMediaConnectDelayAfterLocalA2dpConnectMs();

        MessageCancelFirst(&theInst->av_task, AV_INTERNAL_A2DP_CONNECT_MEDIA_REQ);
        MessageSendConditionally(&theInst->av_task, AV_INTERNAL_A2DP_CONNECT_MEDIA_REQ, message,
                                 &appA2dpGetLock(theInst));
    }

    /* Play connected UI if not the peer */
    {
        if (!is_peer)
        {
            appAvSendUiMessageId(AV_CONNECTED);
        }
        else
        {
            appAvSendUiMessageId(AV_CONNECTED_PEER);
        }
    }

    /* Update AV instance now that A2DP is connected */
    appAvInstanceA2dpConnected(theInst);
}

/*! \brief Exit A2DP_STATE_CONNECTED_SIGNALLING

    The A2DP state machine has exited 'connected signalling' state, this means
    that the A2DP signalling channel has closed.
*/
static void appA2dpExitConnectedSignalling(avInstanceTaskData *theInst)
{
    DEBUG_LOG("appA2dpExitConnectedSignalling(%p)(PARENT)", (void *)theInst);

    /* Cancel any pending media connect request */
    MessageCancelFirst(&theInst->av_task, AV_INTERNAL_A2DP_CONNECT_MEDIA_REQ);

    /* Clear current SEID */
    theInst->a2dp.current_seid = AV_SEID_INVALID;


    /* Clear PTS test mode */
    A2dpProfile_SetPtsMode(FALSE);
}

/*! \brief Enter A2DP_STATE_CONNECTING_MEDIA_LOCAL

    The A2DP state machine has entered 'connecting media local' state, this means
    that the A2DP media channel is required.  Set the
    operation lock to block any other operations on this instance and initiate
    opening media channel.

*/
static void appA2dpEnterConnectingMediaLocal(avInstanceTaskData *theInst)
{
    DEBUG_LOG("appA2dpEnterConnectingMediaLocal(%p) (%d)", (void *)theInst, theInst->a2dp.current_seid);

    /* Set operation lock */
    appA2dpSetTransitionLockBit(theInst);

    /* Open media channel to peer device */
    if (theInst->a2dp.current_seid != AV_SEID_INVALID)
    {
        A2dpMediaOpenRequest(theInst->a2dp.device_id, 1, &theInst->a2dp.current_seid);
    }
    else
    {
        if(appDeviceIsTwsPlusHandset(&theInst->bd_addr))
        {
            const uint8 sink_seids[] = {AV_SEID_APTX_MONO_TWS_SNK, AV_SEID_APTX_SNK, AV_SEID_AAC_SNK, AV_SEID_SBC_SNK};
            A2dpMediaOpenRequest(theInst->a2dp.device_id, ARRAY_DIM(sink_seids), (uint8 *)sink_seids);
        }
        else
        {
            const uint8 sink_seids[] = {AV_SEID_APTX_ADAPTIVE_SNK, AV_SEID_APTXHD_SNK, AV_SEID_APTX_SNK, AV_SEID_AAC_SNK,
                                        AV_SEID_SBC_SNK};
            A2dpMediaOpenRequest(theInst->a2dp.device_id, ARRAY_DIM(sink_seids), (uint8 *)sink_seids);
        }
    }
}

/*! \brief Exit A2DP_STATE_CONNECTING_MEDIA_LOCAL

    The A2DP state machine has exited 'connecting media' state, clear the
    operation lock to allow any pending operations on this instance to proceed.
*/
static void appA2dpExitConnectingMediaLocal(avInstanceTaskData *theInst)
{
    DEBUG_LOG("appA2dpExitConnectingMediaLocal(%p)", (void *)theInst);

    /* Clear operation lock */
    appA2dpClearTransitionLockBit(theInst);
}

/*! \brief Enter A2DP_STATE_CONNECTING_MEDIA_REMOTE_SYNC

    The A2DP state machine has entered 'connecting media remote' state, this means
    that the A2DP media channel is being opened by the remote device.  Set the
    operation lock to block any other operations on this instance and attempt
    to synchronise the other instance.
*/
static void appA2dpEnterConnectingMediaRemoteSync(avInstanceTaskData *theInst)
{
    DEBUG_LOG("appA2dpEnterConnectingMediaRemoteSync(%p), seid %d", (void *)theInst, theInst->a2dp.current_seid);

    /* Set operation lock */
    appA2dpSetTransitionLockBit(theInst);

    /* Send a request for any registered audio_sync instance to also become
       'connected' and then wait for the response. */
    AudioSync_ConnectIndication(theInst->a2dp.sync_inst, &theInst->av_task,
                                a2dpProfile_LookupSource(theInst),
                                theInst->a2dp.current_seid, theInst->a2dp.sync_counter++);
}

/*! \brief Exit A2DP_STATE_CONNECTING_MEDIA_REMOTE_SYNC

    The A2DP state machine has exited 'connecting media remote' state, clear the
    operation lock to allow any pending operations on this instance to proceed.
*/
static void appA2dpExitConnectingMediaRemoteSync(avInstanceTaskData *theInst)
{
    DEBUG_LOG("appA2dpExitConnectingMediaRemoteSync(%p)", (void *)theInst);

    /* Clear operation lock */
    appA2dpClearTransitionLockBit(theInst);

    appA2dpInstSyncExit(theInst);
}

/*! \brief Enter A2DP_STATE_CONNECTED_MEDIA

    The A2DP state machine has entered 'connected media' state, this means
    that the A2DP media channel has been established.
*/
static void appA2dpEnterConnectedMedia(avInstanceTaskData *theInst)
{
    DEBUG_LOG("appA2dpEnterConnectedMedia(%p)", (void *)theInst);

    appLinkPolicyUpdateRoleFromSink(A2dpMediaGetSink(theInst->a2dp.device_id, theInst->a2dp.stream_id));
}

/*! \brief Exit A2DP_STATE_CONNECTED_MEDIA

    The A2DP state machine has exited 'connected media' state, this means
    that the A2DP media channel has closed.
*/
static void appA2dpExitConnectedMedia(avInstanceTaskData *theInst)
{
    DEBUG_LOG("appA2dpExitConnectedMedia(%p)(PARENT)", (void *)theInst);

    theInst->a2dp.suspend_state &= ~(AV_SUSPEND_REASON_RELAY | AV_SUSPEND_REASON_REMOTE);

    /* Tell the registered audio_sync object that we are now in the 'disconnected' state. */
    AudioSync_StateIndication(theInst->a2dp.sync_inst,
                              a2dpProfile_LookupSource(theInst),
                              AUDIO_SYNC_STATE_DISCONNECTED, theInst->a2dp.current_seid);
}

/*! \brief Enter A2DP_STATE_DISCONNECTING_MEDIA

    The A2DP state machine has entered 'disconnecting media' state, this means
    that we have initiated disconnecting the A2DP media channel.

    Set the operation lock to block any other operations, call
    A2dpMediaCloseRequest() to actually request closing of the media channel.
*/
static void appA2dpEnterDisconnectingMedia(avInstanceTaskData *theInst)
{
    DEBUG_LOG("appA2dpEnterDisconnectingMedia(%p)(PARENT)", (void *)theInst);

    /* Set operation lock */
    appA2dpSetTransitionLockBit(theInst);

    /* Close media channel to peer device */
    A2dpMediaCloseRequest(theInst->a2dp.device_id, theInst->a2dp.stream_id);
}

/*! \brief Exit A2DP_STATE_DISCONNECTING_MEDIA

    The A2DP state machine has exited 'disconnecting media' state, this means
    that we have completed disconnecting the A2DP media channel.

    Clear the operation lock to allow any pending operations on this instance
    to proceed.
*/
static void appA2dpExitDisconnectingMedia(avInstanceTaskData *theInst)
{
    DEBUG_LOG("appA2dpExitDisconnectingMedia(%p)", (void *)theInst);

    /* Clear operation lock */
    appA2dpClearTransitionLockBit(theInst);
}

/*! \brief Enter A2DP_STATE_CONNECTED_MEDIA_STREAMING

    The A2DP state machine has entered 'connected media streaming' state, this means
    that the A2DP media channel is now streaming audio.
*/
static void appA2dpEnterConnectedMediaStreaming(avInstanceTaskData *theInst)
{
    DEBUG_LOG("appA2dpEnterConnectedMediaStreaming(%p)", (void *)theInst);

    /* Prevent role switch when streaming TWS (standard or plus, source or sink) */
    if (appA2dpIsSeidTws(theInst->a2dp.current_seid))
    {
        appLinkPolicyPreventRoleSwitch(&theInst->bd_addr);
    }

    /* Call appropriate UI function */
    appA2dpStreamingActiveUi(theInst);

    /* Update hint */
    appAvHintPlayStatus(avrcp_play_status_playing);

    /* Tell a registered audio_sync object we are 'active' */
    AudioSync_StateIndication(theInst->a2dp.sync_inst,
                              a2dpProfile_LookupSource(theInst),
                              AUDIO_SYNC_STATE_ACTIVE, theInst->a2dp.current_seid);

    /* Tell clients we are streaming */
    appAvSendStatusMessage(AV_STREAMING_ACTIVE_IND, NULL, 0);
}

/*! \brief Exit A2DP_STATE_CONNECTED_MEDIA_STREAMING

    The A2DP state machine has exited 'connected media streaming' state, this means
    that the A2DP media channel has stopped streaming audio state.
*/
static void appA2dpExitConnectedMediaStreaming(avInstanceTaskData *theInst)
{
    DEBUG_LOG("appA2dpExitConnectedMediaStreaming(%p)", (void *)theInst);

    /* Update hint */
    appAvHintPlayStatus(avrcp_play_status_stopped);

    /* Stop UI indication */
    appA2dpStreamingInactiveUi(theInst);

    /* Allow role switch on exit streaming */
    appLinkPolicyAllowRoleSwitch(&theInst->bd_addr);

    /* Tell clients we are not streaming */
    appAvSendStatusMessage(AV_STREAMING_INACTIVE_IND, NULL, 0);
}

/*! \brief Enter A2DP_STATE_CONNECTED_MEDIA_STREAMING_MUTED

    The A2DP state machine has entered 'connected media streaming muted' state, this means
    that the headset has failed to suspend the audio.
*/
static void appA2dpEnterConnectedMediaStreamingMuted(avInstanceTaskData *theInst)
{
    UNUSED(theInst);
    DEBUG_LOG("appA2dpEnterConnectedMediaStreamingMuted(%p)", (void *)theInst);
}

/*! \brief Exit A2DP_STATE_CONNECTED_MEDIA_STREAMING_MUTED

    The A2DP state machine has exited 'connected media streaming muted' state, this means
    that either about to start streaming again or we're disconnecting.
*/
static void appA2dpExitConnectedMediaStreamingMuted(avInstanceTaskData *theInst)
{
    DEBUG_LOG("appA2dpExitConnectedMediaStreamingMuted(%p)", (void *)theInst);
}

/*! \brief Enter A2DP_STATE_CONNECTED_MEDIA_SUSPENDING_LOCAL

    The A2DP state machine has entered 'connected media suspending' state, this means
    that the A2DP media channel needs to be suspended.

    SeF the operation lock to block any other operations, call
    A2dpMediaSuspendRequest() to actually request suspension of the stream.
*/
static void appA2dpEnterConnectedMediaSuspendingLocal(avInstanceTaskData *theInst)
{
    DEBUG_LOG("appA2dpEnterConnectedMediaSuspendingLocal(%p)", (void *)theInst);

    /* Set operation lock */
    appA2dpSetTransitionLockBit(theInst);

    /* Suspend A2DP streaming */
    A2dpMediaSuspendRequest(theInst->a2dp.device_id, theInst->a2dp.stream_id);
}

/*! \brief Exit A2DP_STATE_CONNECTED_MEDIA_SUSPENDING_LOCAL

    The A2DP state machine has exited 'connected media suspending' state, this could
    be for a number of reasons.	 Clear the operation lock to allow other operations to
    proceed.
*/
static void appA2dpExitConnectedMediaSuspendingLocal(avInstanceTaskData *theInst)
{
    DEBUG_LOG("appA2dpExitConnectedMediaSuspendingLocal(%p)", (void *)theInst);

    /* Clear operation lock */
    appA2dpClearTransitionLockBit(theInst);
}

/*! \brief Enter A2DP_STATE_CONNECTED_MEDIA_SUSPENDED

    The A2DP state machine has entered 'connected media suspended' state, this means
    the audio streaming has now actually suspended.
*/
static void appA2dpEnterConnectedMediaSuspended(avInstanceTaskData *theInst)
{
    DEBUG_LOG("appA2dpEnterConnectedMediaSuspended(%p)", (void *)theInst);

    /* Tell the registered audio_sync object that we are now in the 'connected' state. */
    AudioSync_StateIndication(theInst->a2dp.sync_inst,
                              a2dpProfile_LookupSource(theInst),
                              AUDIO_SYNC_STATE_CONNECTED, theInst->a2dp.current_seid);

    appAvInstanceStartMediaPlayback(theInst);
}

/*! \brief Exit A2DP_STATE_CONNECTED_MEDIA_SUSPENDED

    The A2DP state machine has exited 'connected media suspended' state, this could
    be for a number of reasons.
*/
static void appA2dpExitConnectedMediaSuspended(avInstanceTaskData *theInst)
{
    UNUSED(theInst);
    DEBUG_LOG("appA2dpExitConnectedMediaSuspended(%p)", (void *)theInst);
}

/*! \brief Enter A2DP_STATE_CONNECTED_MEDIA_RECONFIGURING

    This means a codec reconfiguration is in progress.
*/
static void appA2dpEnterConnectedMediaReconfiguring(avInstanceTaskData *theInst)
{
    DEBUG_LOG("appA2dpEnterConnectedMediaReconfiguring(%p)", (void *)theInst);
    appA2dpSetTransitionLockBit(theInst);
}

/*! \brief Exit A2DP_STATE_CONNECTED_MEDIA_RECONFIGURING

    This means a codec reconfiguration completed.
*/
static void appA2dpExitConnectedMediaReconfiguring(avInstanceTaskData *theInst)
{
    DEBUG_LOG("appA2dpExitConnectedMediaReconfiguring(%p)", (void *)theInst);
    appA2dpClearTransitionLockBit(theInst);
}

/*! \brief Enter A2DP_STATE_CONNECTED_MEDIA_STARTING_LOCAL_SYNC

    The A2DP state machine has entered 'connected media starting local sync' state,
    this means we should synchronise with the slave.
*/
static void appA2dpEnterConnectedMediaStartingLocalSync(avInstanceTaskData *theInst)
{
    DEBUG_LOG("appA2dpEnterConnectedMediaStartingLocalSync(%p)", (void *)theInst);

    /* Start as quickly as possible */
    appPowerPerformanceProfileRequest();

    /* Set operation locks */
    appA2dpSetTransitionLockBit(theInst);

    /* Reset the flag as sending AUDIO_SYNC_ACTIVATE_IND */
    theInst->a2dp.bitfields.sync_response_rcvd = FALSE;
    DEBUG_LOG("appA2dpEnterConnectedMediaStartingLocalSync: sync_response_rcvd %d", theInst->a2dp.bitfields.sync_response_rcvd);

    /* Send a request for any registered audio_sync instance to also become
       'active' and then wait for the response. */
    AudioSync_ActivateIndication(theInst->a2dp.sync_inst, &theInst->av_task,
                                 a2dpProfile_LookupSource(theInst),
                                 theInst->a2dp.current_seid, theInst->a2dp.sync_counter++);
}

/*! \brief Exit A2DP_STATE_CONNECTED_MEDIA_STARTING_LOCAL_SYNC

    The A2DP state machine has exited 'connected media starting local sync' state,
    this means we have either sucessfully synchronised the slave or we
    failed for some reason.
*/
static void appA2dpExitConnectedMediaStartingLocalSync(avInstanceTaskData *theInst)
{
    DEBUG_LOG("appA2dpExitConnectedMediaStartingLocalSync(%p)", (void *)theInst);

    /* Clear operation lock */
    appA2dpClearTransitionLockBit(theInst);

    appA2dpInstSyncExit(theInst);

    appPowerPerformanceProfileRelinquish();
}

/*! \brief Enter A2DP_STATE_CONNECTED_MEDIA_STARTING_REMOTE_SYNC

    The A2DP state machine has entered 'connected media starting remote' state,
    this means the remote device has requested to start streaming.

    We sync the slave and get a message back that triggers the A2dpStartResponse().

    The operation lock is set to that any other operations are blocked until we
    have exited this state.
*/
static void appA2dpEnterConnectedMediaStartingRemoteSync(avInstanceTaskData *theInst)
{
    DEBUG_LOG("appA2dpEnterConnectedMediaStartingRemoteSync(%p)", (void *)theInst);

    /* Start as quickly as possible */
    appPowerPerformanceProfileRequest();

    /* Set operation lock */
    appA2dpSetTransitionLockBit(theInst);

    /* Send a request for any registered audio_sync instance to also become
       'active' and then wait for the response. */
    AudioSync_ActivateIndication(theInst->a2dp.sync_inst, &theInst->av_task,
                                 a2dpProfile_LookupSource(theInst),
                                 theInst->a2dp.current_seid, theInst->a2dp.sync_counter++);
}

/*! \brief Exit A2DP_STATE_CONNECTED_MEDIA_STARTING_REMOTE_SYNC

    The A2DP state machine has exited 'connected media starting remote' state,
    this means we have either sucessfully started streaming or we failed for some
    reason.

    As we are exiting this state we can clear the operation lock to allow any other
    blocked operations to proceed.
*/
static void appA2dpExitConnectedMediaStartingRemoteSync(avInstanceTaskData *theInst)
{
    DEBUG_LOG("appA2dpExitConnectedMediaStartingRemoteSync(%p)", (void *)theInst);

    /* Clear operation lock */
    appA2dpClearTransitionLockBit(theInst);

    appA2dpInstSyncExit(theInst);

    appPowerPerformanceProfileRelinquish();
}

/*! \brief Enter A2DP_STATE_DISCONNECTING

    The A2DP state machine has entered 'disconnecting' state, this means that
    we have initiated a disconnect.  Set the operation lock to prevent any other
    operations occuring and call A2dpDisconnectAll() to start the disconnection.
*/
static void appA2dpEnterDisconnecting(avInstanceTaskData *theInst)
{
    DEBUG_LOG("appA2dpEnterDisconnecting(%p)", (void *)theInst);

    /* Set operation lock */
    appA2dpSetTransitionLockBit(theInst);
    /* Cancel any pending KYMERA_INTERNAL_A2DP_START message that migh otherwise
       be delivered after the A2DP instance has been destroyed. */
    appKymeraCancelA2dpStart();
    /* Make sure AVRCP isn't doing something important, send internal message blocked on
       AVRCP lock */
    MessageSendConditionally(&theInst->av_task, AV_INTERNAL_AVRCP_UNLOCK_IND, NULL, &appAvrcpGetLock(theInst));
}

/*! \brief Exit A2DP_STATE_DISCONNECTING

    The A2DP state machine has exited 'disconnect' state, this means we have
    completed the disconnect, clear the operation lock so that any blocked
    operations can proceed.
*/
static void appA2dpExitDisconnecting(avInstanceTaskData *theInst)
{
    DEBUG_LOG("appA2dpExitDisconnecting(%p)", (void *)theInst);

    /* Clear operation lock */
    appA2dpClearTransitionLockBit(theInst);
}

/*! \brief Enter A2DP_STATE_DISCONNECTED

    The A2DP state machine has entered 'disconnected' state, this means we
    have completely disconnected.  Generally after entering the
    'disconnected' state we'll received a AV_INTERNAL_A2DP_DESTROY_REQ message
    which will destroy this instance.
*/
static void appA2dpEnterDisconnected(avInstanceTaskData *theInst)
{
    DEBUG_LOG("appA2dpEnterDisconnected(%p) %p %u", (void *)theInst, &appA2dpGetLock(theInst), appA2dpGetLock(theInst));

    /* Clear A2DP device ID */
    theInst->a2dp.device_id = INVALID_DEVICE_ID;
    /* Send ourselves a destroy message so that any other messages waiting on the
       operation lock can be handled */
    MessageSendConditionally(&theInst->av_task, AV_INTERNAL_A2DP_DESTROY_REQ, NULL, &appA2dpGetLock(theInst));

    /* Tell clients we have disconnected */
    appAvInstanceA2dpDisconnected(theInst);
}

/*! \brief Exiting A2DP_STATE_DISCONNECTED

    The A2DP state machine has entered 'disconnected' state, this means we
    are about to connect to the peer device, either for a new connection or
    on a reconnect attempt.
*/
static void appA2dpExitDisconnected(avInstanceTaskData *theInst)
{
    DEBUG_LOG("appA2dpExitDisconnected(%p)", (void *)theInst);

    /* Reset disconnect reason */
    theInst->a2dp.bitfields.disconnect_reason = AV_A2DP_INVALID_REASON;

    /* Clear any queued AV_INTERNAL_A2DP_DESTROY_REQ messages, as we are exiting the
       'destroyed' state, probably due to a incoming connection */
    MessageCancelAll(&theInst->av_task, AV_INTERNAL_A2DP_DESTROY_REQ);
}

static uint16 a2dpProfile_GetContext(avInstanceTaskData *theInst)
{
    uint16 context = BAD_CONTEXT;
    switch (appA2dpGetState(theInst))
    {
    case A2DP_STATE_NULL:
    case A2DP_STATE_DISCONNECTED:
    case A2DP_STATE_CONNECTING_LOCAL:
    case A2DP_STATE_CONNECTING_REMOTE:
    case A2DP_STATE_CONNECTING_CROSSOVER:
        context = context_audio_disconnected;
        break;
    case A2DP_STATE_CONNECTED_SIGNALLING:
    case A2DP_STATE_CONNECTING_MEDIA_LOCAL:
    case A2DP_STATE_CONNECTING_MEDIA_REMOTE_SYNC:
    case A2DP_STATE_CONNECTED_MEDIA:
        context = context_audio_connected;
        break;
    case A2DP_STATE_CONNECTED_MEDIA_STREAMING:
    case A2DP_STATE_CONNECTED_MEDIA_STREAMING_MUTED:
        context = context_audio_is_streaming;
        break;
    case A2DP_STATE_CONNECTED_MEDIA_SUSPENDING_LOCAL:
    case A2DP_STATE_CONNECTED_MEDIA_SUSPENDED:
    case A2DP_STATE_CONNECTED_MEDIA_RECONFIGURING:
    case A2DP_STATE_CONNECTED_MEDIA_STARTING_LOCAL_SYNC:
    case A2DP_STATE_CONNECTED_MEDIA_STARTING_REMOTE_SYNC:
    case A2DP_STATE_DISCONNECTING_MEDIA:
    case A2DP_STATE_DISCONNECTING:
        context = context_audio_connected;
        break;
    default:
        break;
    }
    return context;
}

/*! \brief Set A2DP state

    Called to change state.  Handles calling the state entry and exit functions.
    Note: The entry and exit functions will be called regardless of whether or not
    the state actually changes value.
*/
static void appA2dpSetState(avInstanceTaskData *theInst, avA2dpState a2dp_state)
{
    avA2dpState a2dp_old_state = theInst->a2dp.state;
    DEBUG_LOG("appA2dpSetState(%p) state(%02x)", (void *)theInst, a2dp_state);

    /* Handle state exit functions */
    switch (a2dp_old_state)
    {
        case A2DP_STATE_CONNECTING_LOCAL:
            appA2dpExitConnectingLocal(theInst);
            break;
        case A2DP_STATE_CONNECTING_REMOTE:
            appA2dpExitConnectingRemote(theInst);
            break;
        case A2DP_STATE_CONNECTING_CROSSOVER:
            appA2dpExitConnectingCrossover(theInst);
            break;
        case A2DP_STATE_CONNECTING_MEDIA_LOCAL:
            appA2dpExitConnectingMediaLocal(theInst);
            break;
        case A2DP_STATE_CONNECTING_MEDIA_REMOTE_SYNC:
            appA2dpExitConnectingMediaRemoteSync(theInst);
            break;
        case A2DP_STATE_CONNECTED_MEDIA_STREAMING:
            appA2dpExitConnectedMediaStreaming(theInst);
            break;
        case A2DP_STATE_CONNECTED_MEDIA_STREAMING_MUTED:
            appA2dpExitConnectedMediaStreamingMuted(theInst);
            break;
        case A2DP_STATE_CONNECTED_MEDIA_SUSPENDING_LOCAL:
            appA2dpExitConnectedMediaSuspendingLocal(theInst);
            break;
        case A2DP_STATE_CONNECTED_MEDIA_SUSPENDED:
            appA2dpExitConnectedMediaSuspended(theInst);
            break;
        case A2DP_STATE_CONNECTED_MEDIA_RECONFIGURING:
            appA2dpExitConnectedMediaReconfiguring(theInst);
            break;
        case A2DP_STATE_CONNECTED_MEDIA_STARTING_LOCAL_SYNC:
            appA2dpExitConnectedMediaStartingLocalSync(theInst);
            break;
        case A2DP_STATE_CONNECTED_MEDIA_STARTING_REMOTE_SYNC:
            appA2dpExitConnectedMediaStartingRemoteSync(theInst);
            break;
        case A2DP_STATE_DISCONNECTING_MEDIA:
            appA2dpExitDisconnectingMedia(theInst);
            break;
        case A2DP_STATE_DISCONNECTING:
            appA2dpExitDisconnecting(theInst);
            break;
        case A2DP_STATE_DISCONNECTED:
            appA2dpExitDisconnected(theInst);
            break;
        default:
            break;
    }

    /* Check if exiting 'kymera on' state */
    if (appA2dpIsKymeraOnInState(a2dp_old_state) && !appA2dpIsKymeraOnInState(a2dp_state))
        appA2dpStopAudio(theInst);

    /* Check if exiting 'connected media' parent state */
    if (appA2dpIsStateConnectedMedia(a2dp_old_state) && !appA2dpIsStateConnectedMedia(a2dp_state))
        appA2dpExitConnectedMedia(theInst);

    /* Check if exiting 'connected signalling' parent state */
    if (appA2dpIsStateConnectedSignalling(a2dp_old_state) && !appA2dpIsStateConnectedSignalling(a2dp_state))
        appA2dpExitConnectedSignalling(theInst);

    /* Set new state */
    theInst->a2dp.state = a2dp_state;

    /* Check if entering 'connected signalling' parent state */
    if (!appA2dpIsStateConnectedSignalling(a2dp_old_state) && appA2dpIsStateConnectedSignalling(a2dp_state))
        appA2dpEnterConnectedSignalling(theInst);

    /* Check if entering 'connected media' parent state */
    if (!appA2dpIsStateConnectedMedia(a2dp_old_state) && appA2dpIsStateConnectedMedia(a2dp_state))
        appA2dpEnterConnectedMedia(theInst);

    /* Check if entering 'kymera on' state */
    if (!appA2dpIsKymeraOnInState(a2dp_old_state) && appA2dpIsKymeraOnInState(a2dp_state))
        appA2dpStartAudio(theInst);

    /* Handle state entry functions */
    switch (a2dp_state)
    {
        case A2DP_STATE_CONNECTING_LOCAL:
            appA2dpEnterConnectingLocal(theInst);
            break;
        case A2DP_STATE_CONNECTING_REMOTE:
            appA2dpEnterConnectingRemote(theInst);
            break;
        case A2DP_STATE_CONNECTING_CROSSOVER:
            appA2dpEnterConnectingCrossover(theInst);
            break;
        case A2DP_STATE_CONNECTING_MEDIA_LOCAL:
            appA2dpEnterConnectingMediaLocal(theInst);
            break;
        case A2DP_STATE_CONNECTING_MEDIA_REMOTE_SYNC:
            appA2dpEnterConnectingMediaRemoteSync(theInst);
            break;
        case A2DP_STATE_CONNECTED_MEDIA_STREAMING:
            appA2dpEnterConnectedMediaStreaming(theInst);
            break;
        case A2DP_STATE_CONNECTED_MEDIA_STREAMING_MUTED:
            appA2dpEnterConnectedMediaStreamingMuted(theInst);
            break;
        case A2DP_STATE_CONNECTED_MEDIA_SUSPENDING_LOCAL:
            appA2dpEnterConnectedMediaSuspendingLocal(theInst);
            break;
        case A2DP_STATE_CONNECTED_MEDIA_SUSPENDED:
            appA2dpEnterConnectedMediaSuspended(theInst);
            break;
        case A2DP_STATE_CONNECTED_MEDIA_RECONFIGURING:
            appA2dpEnterConnectedMediaReconfiguring(theInst);
            break;
        case A2DP_STATE_CONNECTED_MEDIA_STARTING_LOCAL_SYNC:
            appA2dpEnterConnectedMediaStartingLocalSync(theInst);
            break;
        case A2DP_STATE_CONNECTED_MEDIA_STARTING_REMOTE_SYNC:
            appA2dpEnterConnectedMediaStartingRemoteSync(theInst);
            break;
        case A2DP_STATE_DISCONNECTING_MEDIA:
            appA2dpEnterDisconnectingMedia(theInst);
            break;
        case A2DP_STATE_DISCONNECTING:
            appA2dpEnterDisconnecting(theInst);
            break;
        case A2DP_STATE_DISCONNECTED:
            appA2dpEnterDisconnected(theInst);
            break;
        default:
            break;
    }

    Ui_InformContextChange(ui_provider_media_player, a2dpProfile_GetContext(theInst));

    /* Update link policy following change in state */
    appLinkPolicyUpdatePowerTable(&theInst->bd_addr);

    if(appDeviceIsPeerConnected())
    {
        bdaddr peer_bd_addr;
        appDeviceGetPeerBdAddr(&peer_bd_addr);
        appLinkPolicyUpdatePowerTable(&peer_bd_addr);
    }
}

/*! \brief Get A2DP state

    \param  theAv   The AV instance for this A2DP link

    \return The current A2DP state.
*/
avA2dpState appA2dpGetState(avInstanceTaskData *theAv)
{
    return theAv->a2dp.state;
}

/*! \brief Handle A2DP error

    Some error occurred in the A2DP state machine.

    To avoid the state machine getting stuck, if instance is connected then
    drop connection and move to 'disconnecting' state.
*/
void appA2dpError(avInstanceTaskData *theInst, MessageId id, Message message)
{
    UNUSED(message); UNUSED(id);

#if defined(AV_DEBUG) || defined(AV_DEBUG_PANIC)
    DEBUG_LOG("appA2dpError(%p), state enum:avA2dpState:0x%x, MESSAGE:0x%x",
                (void *)theInst, theInst->a2dp.state, id);
#endif

    /* Check if we are connected */
    if (appA2dpIsStateConnectedSignalling(appA2dpGetState(theInst)))
    {
        /* Move to 'disconnecting' state */
        appA2dpSetState(theInst, A2DP_STATE_DISCONNECTING);
    }

    /* Destroy ourselves */
    MessageSend(&theInst->av_task, AV_INTERNAL_A2DP_DESTROY_REQ, NULL);
}

/*! \brief Request outgoing A2DP connection

    Handle A2DP connect request from AV parent task, store connection
    parameters and move into the 'connecting local' state.  The state machine
    will handle creating the connection.  If we are not in the 'disconnected'
    state then just ignore the request as it was probably due to a
    race-condition, this can happen as the AV_INTERNAL_A2DP_CONNECT_REQ can be
    blocked by the ACL lock not the operation lock.
*/
static void appA2dpHandleInternalA2dpConnectRequest(avInstanceTaskData *theInst,
                                                    const AV_INTERNAL_A2DP_CONNECT_REQ_T *req)
{
    DEBUG_LOG("appA2dpHandleInternalA2dpConnectRequest(%p), %x,%x,%lx",
                 (void *)theInst, theInst->bd_addr.nap, theInst->bd_addr.uap, theInst->bd_addr.lap);

    /* Handle different states */
    switch (appA2dpGetState(theInst))
    {
        case A2DP_STATE_DISCONNECTED:
        {
            /* Check ACL is connected */
            if (ConManagerIsConnected(&theInst->bd_addr))
            {
                DEBUG_LOG("appA2dpHandleInternalA2dpConnectRequest(%p), ACL connected", theInst);

                /* Store connection parameters */
                theInst->a2dp.bitfields.flags = req->flags;
                theInst->a2dp.bitfields.connect_retries = req->num_retries;

                /* Request outgoing connection */
                A2dpSignallingConnectRequestWithTask(&theInst->bd_addr, &theInst->av_task);

                /* Move to 'connecting local' state */
                appA2dpSetState(theInst, A2DP_STATE_CONNECTING_LOCAL);
            }
            else
            {
                /* Check if we should retry */
                if (req->num_retries)
                {
                    MAKE_AV_MESSAGE(AV_INTERNAL_A2DP_CONNECT_REQ);

                    DEBUG_LOG("appA2dpHandleInternalA2dpConnectRequest(%p), ACL not connected, retrying", theInst);

                    /* Send message to retry connecting this AV instance */
                    message->num_retries = req->num_retries - 1;
                    message->flags = req->flags;
                    MessageCancelFirst(&theInst->av_task, AV_INTERNAL_A2DP_CONNECT_REQ);
                    MessageSendConditionally(&theInst->av_task, AV_INTERNAL_A2DP_CONNECT_REQ, message,
                                             ConManagerCreateAcl(&theInst->bd_addr));

                    /* Move to 'disconnected' state */
                    appA2dpSetState(theInst, A2DP_STATE_DISCONNECTED);
                    MessageCancelFirst(&theInst->av_task, AV_INTERNAL_A2DP_DESTROY_REQ);
                    return;
                }
                else
                {
                    DEBUG_LOG("appA2dpHandleInternalA2dpConnectRequest(%p), ACL not connected", theInst);
                }
            }
        }
        return;

        case A2DP_STATE_DISCONNECTING:
        {
            /* Send AV_INTERNAL_A2DP_CONNECT_REQ to start A2DP connection */
            MAKE_AV_MESSAGE(AV_INTERNAL_A2DP_CONNECT_REQ);

            DEBUG_LOG("appA2dpHandleInternalA2dpConnectRequest(%p) repost connect request", (void*)theInst);

            /* Send message to newly created AV instance to connect A2DP */
            message->num_retries = req->num_retries;
            message->flags = req->flags;
            MessageCancelFirst(&theInst->av_task, AV_INTERNAL_A2DP_CONNECT_REQ);
            MessageSendConditionally(&theInst->av_task, AV_INTERNAL_A2DP_CONNECT_REQ, message,
                                     &appA2dpGetLock(theInst));
        }
        return;

        default:
            return;
    }
}

static inline void a2dpProfile_StoreSeid(avInstanceTaskData *theInst, uint8 seid)
{
    theInst->a2dp.current_seid = seid;
    theInst->a2dp.last_known_seid = seid;
}

/*! \brief Request A2DP media channel

    Handle A2DP open media channel request from AV parent task, or self.
    Only valid in the 'connected signalling' state, should never be received in any of the
    transition states as the operation lock will block the request.
*/
static void appA2dpHandleInternalA2dpConnectMediaRequest(avInstanceTaskData *theInst,
                                                         const AV_INTERNAL_A2DP_CONNECT_MEDIA_REQ_T *req)
{
    DEBUG_LOG("appA2dpHandleInternalA2dpConnectMediaRequest(%p) delay_ms %u", (void *)theInst, req->delay_ms);

    /* Handle different states */
    switch (appA2dpGetState(theInst))
    {
        case A2DP_STATE_CONNECTED_SIGNALLING:
        {
            if (req->delay_ms)
            {
                /* Connect media channel */
                MAKE_AV_MESSAGE(AV_INTERNAL_A2DP_CONNECT_MEDIA_REQ);
                message->seid = req->seid;
                message->delay_ms = 0;
                MessageCancelFirst(&theInst->av_task, AV_INTERNAL_A2DP_CONNECT_MEDIA_REQ);
                MessageSendLater(&theInst->av_task, AV_INTERNAL_A2DP_CONNECT_MEDIA_REQ, message, req->delay_ms);
                return;
            }

            a2dpProfile_StoreSeid(theInst, req->seid);

            /* Move to 'local connecting media' state */
            appA2dpSetState(theInst, A2DP_STATE_CONNECTING_MEDIA_LOCAL);
        }
        return;

        case A2DP_STATE_CONNECTED_MEDIA_STREAMING:
        case A2DP_STATE_CONNECTED_MEDIA_STREAMING_MUTED:
        case A2DP_STATE_CONNECTED_MEDIA_SUSPENDED:
        case A2DP_STATE_DISCONNECTED:
            return;

        default:
            appA2dpError(theInst, AV_INTERNAL_A2DP_CONNECT_MEDIA_REQ, NULL);
            return;
    }
}

/*! \brief Disconnect A2DP media channel

    Handle A2DP close media channel request from AV parent task, or self.
    Only valid in the 'connected media' states, should never be received in any of the
    transition states as the operation lock will block the request.
*/
static void appA2dpHandleInternalA2dpDisconnectMediaRequest(avInstanceTaskData *theInst)
{
    DEBUG_LOG("appA2dpHandleInternalA2dpDisconnectMediaRequest(%p)", (void *)theInst);

    /* Handle different states */
    switch (appA2dpGetState(theInst))
    {
        case A2DP_STATE_CONNECTED_MEDIA_STREAMING:
        case A2DP_STATE_CONNECTED_MEDIA_STREAMING_MUTED:
        case A2DP_STATE_CONNECTED_MEDIA_SUSPENDED:
        {
            /* Move to 'local disconnecting media' state */
            appA2dpSetState(theInst, A2DP_STATE_DISCONNECTING_MEDIA);
        }
        return;

        case A2DP_STATE_CONNECTED_SIGNALLING:
        case A2DP_STATE_DISCONNECTED:
            return;

        default:
            appA2dpError(theInst, AV_INTERNAL_A2DP_DISCONNECT_MEDIA_REQ, NULL);
            return;
    }
}

/*! \brief Request A2DP disconnection

    Handle A2DP disconnect request from AV parent task.  Move into the
    'disconnecting' state, this will initiate the disconnect.
*/
static void appA2dpHandleInternalA2dpDisconnectRequest(avInstanceTaskData *theInst,
                                                       const AV_INTERNAL_A2DP_DISCONNECT_REQ_T *req)
{
    DEBUG_LOG("appA2dpHandleInternalA2dpDisconnectRequest(%p)", (void *)theInst);

    /* Handle different states */
    switch (appA2dpGetState(theInst))
    {
        case A2DP_STATE_CONNECTED_SIGNALLING:
        case A2DP_STATE_CONNECTED_MEDIA_STREAMING:
        case A2DP_STATE_CONNECTED_MEDIA_STREAMING_MUTED:
        case A2DP_STATE_CONNECTED_MEDIA_SUSPENDED:
        {
            /* Store flags */
            theInst->a2dp.bitfields.flags |= req->flags;

            /* Move to 'disconnecting' state */
            appA2dpSetState(theInst, A2DP_STATE_DISCONNECTING);
        }
        return;

        case A2DP_STATE_DISCONNECTED:
            /* Ignore as instance already disconnected */
            return;

        default:
            appA2dpError(theInst, AV_INTERNAL_A2DP_DISCONNECT_REQ, req);
            return;
    }
}

/*! \brief Request suspend A2DP streaming

    Handle A2DP suspend request from AV parent task, move into the
    'suspending local' state, this will initate the suspend.

    Record the suspend reason, to prevent resuming if there are outstanding
    suspend reasons.
*/
static void appA2dpHandleInternalA2dpSuspendRequest(avInstanceTaskData *theInst,
                                                    const AV_INTERNAL_A2DP_SUSPEND_MEDIA_REQ_T *req)
{
    DEBUG_LOG("appA2dpHandleInternalA2dpSuspendRequest(%p) suspend_state(0x%x) reason(0x%x)",
                (void *)theInst, theInst->a2dp.suspend_state, req->reason);

    /* Record suspend reason */
    theInst->a2dp.suspend_state |= req->reason;

    /* Handle different states */
    switch (appA2dpGetState(theInst))
    {
        case A2DP_STATE_CONNECTING_LOCAL:
        case A2DP_STATE_CONNECTING_REMOTE:
        case A2DP_STATE_CONNECTED_SIGNALLING:
        case A2DP_STATE_CONNECTING_MEDIA_LOCAL:
        case A2DP_STATE_CONNECTING_MEDIA_REMOTE_SYNC:
        case A2DP_STATE_CONNECTED_MEDIA_SUSPENDING_LOCAL:
        case A2DP_STATE_CONNECTED_MEDIA_SUSPENDED:
        case A2DP_STATE_CONNECTED_MEDIA_STARTING_LOCAL_SYNC:
        case A2DP_STATE_CONNECTED_MEDIA_STARTING_REMOTE_SYNC:
        case A2DP_STATE_DISCONNECTING_MEDIA:
        case A2DP_STATE_DISCONNECTING:
        case A2DP_STATE_DISCONNECTED:
            return;

        case A2DP_STATE_CONNECTED_MEDIA_STREAMING:
        case A2DP_STATE_CONNECTED_MEDIA_STREAMING_MUTED:
        {
            /* Suspend if this instance is an AV sink, or if AV source and the
               other instance sync sent the suspend request */
            if (appA2dpIsSinkCodec(theInst) || (req->reason == AV_SUSPEND_REASON_RELAY))
            {
                appA2dpSetState(theInst, A2DP_STATE_CONNECTED_MEDIA_SUSPENDING_LOCAL);
            }
        }
        return;

        default:
            appA2dpError(theInst, AV_INTERNAL_A2DP_SUSPEND_MEDIA_REQ, req);
            return;
    }
}

/*! \brief Request start A2DP streaming

    Handle A2DP resume request from AV parent task.  Clear the suspend reason,
    if there are no suspend reasons left then we can attempt to initiate A2DP
    streaming.
*/
static void appA2dpHandleInternalA2dpResumeRequest(avInstanceTaskData *theInst,
                                                   const AV_INTERNAL_A2DP_RESUME_MEDIA_REQ_T *req)
{
    DEBUG_LOG("appA2dpHandleInternalA2dpResumeRequest(%p) suspend_state(0x%x) reason(0x%x)",
                (void *)theInst, theInst->a2dp.suspend_state, req->reason);

    /* Clear suspend reason */
    theInst->a2dp.suspend_state &= ~req->reason;

    /* Immediately return if suspend is not cleared. */
    if (theInst->a2dp.suspend_state)
    {
        return;
    }

    switch (appA2dpGetState(theInst))
    {
        case A2DP_STATE_CONNECTED_MEDIA_SUSPENDED:
        {
            /* Resume if this instance is an AV sink, or if AV source and the
               other instance sync sent the resume request */
            if (appA2dpIsSinkCodec(theInst) || (req->reason == AV_SUSPEND_REASON_RELAY))
            {
                appA2dpSetState(theInst, A2DP_STATE_CONNECTED_MEDIA_STARTING_LOCAL_SYNC);
            }
        }
        return;

        case A2DP_STATE_CONNECTED_MEDIA_STREAMING_MUTED:
        {
            appA2dpSetState(theInst, A2DP_STATE_CONNECTED_MEDIA_STREAMING);
        }
        return;

        case A2DP_STATE_CONNECTING_LOCAL:
        case A2DP_STATE_CONNECTING_REMOTE:
        case A2DP_STATE_CONNECTED_SIGNALLING:
        case A2DP_STATE_CONNECTING_MEDIA_LOCAL:
        case A2DP_STATE_CONNECTING_MEDIA_REMOTE_SYNC:
        case A2DP_STATE_CONNECTED_MEDIA_STREAMING:
        case A2DP_STATE_CONNECTED_MEDIA_SUSPENDING_LOCAL:
        case A2DP_STATE_CONNECTED_MEDIA_STARTING_LOCAL_SYNC:
        case A2DP_STATE_CONNECTED_MEDIA_STARTING_REMOTE_SYNC:
        case A2DP_STATE_DISCONNECTING_MEDIA:
        case A2DP_STATE_DISCONNECTING:
        case A2DP_STATE_DISCONNECTED:
            return;

        default:
            appA2dpError(theInst, AV_INTERNAL_A2DP_RESUME_MEDIA_REQ, req);
            return;
    }
}

/*! \brief Accept incoming A2DP connection

    A2DP Library has indicated an incoming A2DP connection for a new instance,
    the incoming connection was originally handled by the parent AV task which
    created a new A2DP instance and then sent this instance an internal message
    indicating a incoming A2DP connection.  Due to cross-over/race conditions
    it is also possible for this message to arrive on an existing instance, so
    we have to check that the instance is in the 'disconnect' state before
    accepting the connection, otherwise the connection is rejected.
*/
static void appA2dpHandleInternalA2dpSignallingConnectIndication(avInstanceTaskData *theInst,
                                                                 const AV_INTERNAL_A2DP_SIGNALLING_CONNECT_IND_T *ind)
{
    DEBUG_LOG("appA2dpHandleInternalA2dpSignallingConnectIndication(%p)", (void *)theInst);

    /* Handle different states */
    switch (appA2dpGetState(theInst))
    {
        case A2DP_STATE_DISCONNECTED:
        {
            /* Store A2DP device ID */
            theInst->a2dp.device_id = ind->device_id;

            /* Store connection flags */
            theInst->a2dp.bitfields.flags = ind->flags;

            /* Accept incoming connection */
            A2dpSignallingConnectResponseWithTask(theInst->a2dp.device_id, TRUE, &theInst->av_task);

            /* Move to 'connecting remote' state */
            appA2dpSetState(theInst, A2DP_STATE_CONNECTING_REMOTE);
        }
        return;


        case A2DP_STATE_CONNECTING_LOCAL:
        {
            DEBUG_LOG("appA2dpHandleInternalA2dpSignallingConnectIndication(%p), crossover", (void *)theInst);

            /* Store A2DP device ID */
            theInst->a2dp.device_id = ind->device_id;

            /* Accept incoming connection */
            A2dpSignallingConnectResponseWithTask(ind->device_id, TRUE, &theInst->av_task);

            /* Move to 'connecting crossover' state */
            appA2dpSetState(theInst, A2DP_STATE_CONNECTING_CROSSOVER);
        }
        return;


        default:
        {
            /* Reject incoming connection */
            A2dpSignallingConnectResponseWithTask(theInst->a2dp.device_id, FALSE, &theInst->av_task);
        }
        return;
    }
}

/*! \brief Handle incoming A2DP connection.
    \param theAv The AV.
    \param ind The A2DP signalling connect indication.

    A2DP Library has indicating an incoming A2DP connection.
    Check if we can create or use an existing AV instance, if so, send a
    AV_INTERNAL_A2DP_SIGNALLING_CONNECT_IND to the AV instance which will
    respond to the A2DP library, otherwise reject it.
*/
void appA2dpSignallingConnectIndicationNew(avTaskData *theAv, const A2DP_SIGNALLING_CONNECT_IND_T *ind)
{
    avInstanceTaskData *av_inst;

    UNUSED(theAv);
    DEBUG_LOG("appA2dpSignallingConnectIndicationNew");

    /* Check there isn't already an A2DP instance for this device */
    av_inst = appAvInstanceFindFromBdAddr(&ind->addr);
    if (av_inst == NULL)
        av_inst = appAvInstanceCreate(&ind->addr);
    else
    {
        /* Make sure there's no pending destroy message */
        MessageCancelAll(&av_inst->av_task, AV_INTERNAL_A2DP_DESTROY_REQ);
        MessageCancelAll(&av_inst->av_task, AV_INTERNAL_AVRCP_DESTROY_REQ);
    }

    if (av_inst != NULL)
    {
        /* Create message to send to AV instance */
        MAKE_AV_MESSAGE(AV_INTERNAL_A2DP_SIGNALLING_CONNECT_IND);

        /* Make sure instance isn't about to be destroyed */
        MessageCancelAll(&av_inst->av_task, AV_INTERNAL_A2DP_DESTROY_REQ);

        /* Send message to AV instance */
        message->device_id = ind->device_id;
        message->flags = 0;
        MessageSend(&av_inst->av_task, AV_INTERNAL_A2DP_SIGNALLING_CONNECT_IND, message);

        /* Finished so return */
        return;
    }

    /* Reject incoming connection, either there is an existing instance, or we failed to create a new instance */
    DEBUG_LOG("appA2dpSignallingConnectIndicationNew, rejecting");
    A2dpSignallingConnectResponse(ind->device_id, FALSE);
}

/*! \brief Reject incoming A2DP connection.
    \param theAv The AV.
    \param ind The signalling connect indication.

    A2DP Library has indicating an incoming A2DP connection, we're not in
    the 'Active' state, so just reject the connection.
*/
void appA2dpRejectA2dpSignallingConnectIndicationNew(avTaskData *theAv, const A2DP_SIGNALLING_CONNECT_IND_T *ind)
{
    UNUSED(theAv);
    DEBUG_LOG("appA2dpRejectA2dpSignallingConnectIndicationNew");

    /* Reject incoming connection */
    A2dpSignallingConnectResponse(ind->device_id, FALSE);
}

/*! \brief Accept incoming A2DP connection

    A2DP Library has indicated an incoming A2DP connection for an existing
    instance.  Due to cross-over/race conditions it is possible for this
    message to arrive is various states, so we have to check that the instance
    is in the 'disconnect' state before accepting the connection, otherwise
    the connection is rejected.
*/
static void appA2dpHandleA2dpSignallingConnectIndication(avInstanceTaskData *theInst,
                                                         const A2DP_SIGNALLING_CONNECT_IND_T *ind)
{
    DEBUG_LOG("appA2dpHandleA2dpSignallingConnectIndication(%p)", (void *)theInst);

    /* Handle different states */
    switch (appA2dpGetState(theInst))
    {
        case A2DP_STATE_DISCONNECTED:
        {
            /* Store A2DP device ID */
            theInst->a2dp.device_id = ind->device_id;

            /* Accept incoming connection */
            A2dpSignallingConnectResponseWithTask(ind->device_id, TRUE, &theInst->av_task);

            /* Move to 'connecting remote' state */
            appA2dpSetState(theInst, A2DP_STATE_CONNECTING_REMOTE);
        }
        return;

        default:
        {
            /* Reject incoming connection */
            A2dpSignallingConnectResponseWithTask(ind->device_id, FALSE, &theInst->av_task);
        }
        return;
    }
}

/*! \brief A2DP signalling channel confirmation

    A2DP library has confirmed signalling channel connect request.
    First of all check if the request was successful, if it was then we should
    store the pointer to the newly created A2DP instance, also obtain the
    address of the remote device from the Sink.  After this move into the
    'connect signalling' state as we now have an active A2DP signalling channel.

    If the request was unsuccessful, move back to the 'disconnected' state and
    play an error tone if this connection request was silent.  Note: Moving to
    the 'disconnected' state may result in this AV instance being free'd.
*/
static void appA2dpHandleA2dpSignallingConnectConfirm(avInstanceTaskData *theInst,
                                                      const A2DP_SIGNALLING_CONNECT_CFM_T *cfm)
{
    DEBUG_LOG("appA2dpHandleA2dpSignallingConnectConfirm(%p) status %u, local %u detach_pending %d",
              (void *)theInst, cfm->status, cfm->locally_initiated, theInst->detach_pending);

    /* Handle different states */
    switch (appA2dpGetState(theInst))
    {
        case A2DP_STATE_CONNECTING_LOCAL:
        case A2DP_STATE_CONNECTING_REMOTE:
        case A2DP_STATE_CONNECTING_CROSSOVER:
        {
            /* Check if signalling channel created successfully */
            if (cfm->status == a2dp_success)
            {
                /* Store the A2DP device ID to use in controlling A2DP library */
                theInst->a2dp.device_id = cfm->device_id;

                /* Reset suspend reasons */
                theInst->a2dp.suspend_state &= ~(AV_SUSPEND_REASON_RELAY | AV_SUSPEND_REASON_REMOTE);

                /* Move to 'connected signalling' state */
                appA2dpSetState(theInst, A2DP_STATE_CONNECTED_SIGNALLING);
            }
            else
            {
                if (appA2dpGetState(theInst) == A2DP_STATE_CONNECTING_CROSSOVER)
                {
                    if (cfm->locally_initiated)
                        appA2dpSetState(theInst, A2DP_STATE_CONNECTING_REMOTE);
                    else
                        appA2dpSetState(theInst, A2DP_STATE_CONNECTING_LOCAL);
                }
                else
                {
                    /* Set disconnect reason */
                    theInst->a2dp.bitfields.disconnect_reason = AV_A2DP_CONNECT_FAILED;

                    /* Check if we should retry
                       - Don't retry if the ACL has already been disconnected.
                    */
                    if (   !theInst->detach_pending
                        && theInst->a2dp.bitfields.connect_retries)
                    {
                        MAKE_AV_MESSAGE(AV_INTERNAL_A2DP_CONNECT_REQ);

                        /* Send message to retry connecting this AV instance */
                        message->num_retries = theInst->a2dp.bitfields.connect_retries - 1;
                        message->flags = theInst->a2dp.bitfields.flags;
                        MessageCancelFirst(&theInst->av_task, AV_INTERNAL_A2DP_CONNECT_REQ);
                        MessageSendConditionally(&theInst->av_task, AV_INTERNAL_A2DP_CONNECT_REQ, message,
                                                 ConManagerCreateAcl(&theInst->bd_addr));

                        /* Move to 'disconnected' state */
                        appA2dpSetState(theInst, A2DP_STATE_DISCONNECTED);
                        MessageCancelFirst(&theInst->av_task, AV_INTERNAL_A2DP_DESTROY_REQ);
                        return;
                    }
                    else
                    {
                        appAvSendUiMessageId(AV_ERROR);

                        /* Notify any clients that made an a2dp connect request */
                        appAvA2dpSendConnectCfm(theInst, FALSE);

                        /* Move to 'disconnected' state */
                        appA2dpSetState(theInst, A2DP_STATE_DISCONNECTED);
                    }
                }
            }
        }
        return;

        default:
            appA2dpError(theInst, A2DP_SIGNALLING_CONNECT_CFM, cfm);
            return;
    }
}

/*! \brief A2DP connection disconnected

    A2DP Library has indicated that the signalling channel for A2DP
    has been disconnected, move to the 'disconnected' state, this will
    result in this AV instance being destroyed.
*/
static void appA2dpHandleA2dpSignallingDisconnectInd(avInstanceTaskData *theInst,
                                                     const A2DP_SIGNALLING_DISCONNECT_IND_T *ind)
{
    DEBUG_LOG("appA2dpHandleA2dpSignallingDisconnectInd(%p), status %u, device_id %u, inst_device_id %u",
               (void *)theInst, ind->status, ind->device_id, theInst->a2dp.device_id);
    assert(theInst->a2dp.device_id == ind->device_id);

    /* Handle different states */
    switch (appA2dpGetState(theInst))
    {
        case A2DP_STATE_CONNECTING_LOCAL:
        case A2DP_STATE_CONNECTING_REMOTE:
        case A2DP_STATE_CONNECTED_SIGNALLING:
        case A2DP_STATE_CONNECTING_MEDIA_LOCAL:
        case A2DP_STATE_CONNECTING_MEDIA_REMOTE_SYNC:
        case A2DP_STATE_CONNECTED_MEDIA_STREAMING:
        case A2DP_STATE_CONNECTED_MEDIA_STREAMING_MUTED:
        case A2DP_STATE_CONNECTED_MEDIA_SUSPENDING_LOCAL:
        case A2DP_STATE_CONNECTED_MEDIA_SUSPENDED:
        case A2DP_STATE_CONNECTED_MEDIA_STARTING_LOCAL_SYNC:
        case A2DP_STATE_CONNECTED_MEDIA_STARTING_REMOTE_SYNC:
        {
            /* Check if disconnected due to link-loss */
            if (ind->status == a2dp_disconnect_link_loss && !theInst->detach_pending)
            {
                appAvSendUiMessageId(AV_LINK_LOSS);

                /* Set disconnect reason */
                theInst->a2dp.bitfields.disconnect_reason = AV_A2DP_DISCONNECT_LINKLOSS;
            }
            else if (ind->status == a2dp_success)
            {
                /* Play disconnected UI if not the peer */
                if (!appDeviceIsPeer(&theInst->bd_addr))
                {
                    appAvSendUiMessageId(AV_DISCONNECTED);
                }

                /* Set disconnect reason */
                theInst->a2dp.bitfields.disconnect_reason = AV_A2DP_DISCONNECT_NORMAL;
            }
            else
            {
                /* Play disconnected UI if not the peer */
                if (!appDeviceIsPeer(&theInst->bd_addr))
                {
                    appAvSendUiMessageId(AV_DISCONNECTED);
                }

                /* Set disconnect reason */
                theInst->a2dp.bitfields.disconnect_reason = AV_A2DP_DISCONNECT_ERROR;
            }

            /* Move to 'disconnected' state */
            appA2dpSetState(theInst, A2DP_STATE_DISCONNECTED);
        }
        break;

        case A2DP_STATE_DISCONNECTING_MEDIA:
        case A2DP_STATE_DISCONNECTING:
        case A2DP_STATE_DISCONNECTED:
        {
            /* Play disconnected UI if not the peer */
            if (!appDeviceIsPeer(&theInst->bd_addr) &&
                ind->status != a2dp_disconnect_transferred)
            {
                appAvSendUiMessageId(AV_DISCONNECTED);
            }

            if(ind->status == a2dp_disconnect_transferred)
            {
                /* Set disconnect reason for link transferred during handover */
                theInst->a2dp.bitfields.disconnect_reason = AV_A2DP_DISCONNECT_LINK_TRANSFERRED;
            }
            else
            {
                /* Set disconnect reason for normal disconnection */
                theInst->a2dp.bitfields.disconnect_reason = AV_A2DP_DISCONNECT_NORMAL;
            }

            /* Move to 'disconnected' state */
            appA2dpSetState(theInst, A2DP_STATE_DISCONNECTED);
        }
        return;

        default:
            appA2dpError(theInst, A2DP_SIGNALLING_DISCONNECT_IND, ind);
            return;
    }
}

/*! \brief A2DP media channel open indication

    A2DP Library has indicated that the A2DP media channel has been opened by
    peer device.

    The SEID for this channel should be stored as this will be required
    later to connect the Kalimba to the A2DP media channel.

    Move into the 'connected media suspended' state, the peer device is
    responsible for starting streaming.
*/
static void appA2dpHandleA2dpMediaOpenIndication(avInstanceTaskData *theInst,
                                                 const A2DP_MEDIA_OPEN_IND_T *ind)
{
    assert(theInst->a2dp.device_id == ind->device_id);
    DEBUG_LOG("appA2dpHandleA2dpMediaOpenIndication(%p) seid(%d)", (void *)theInst, ind->seid);

    MessageCancelFirst(&theInst->av_task, AV_INTERNAL_A2DP_CONNECT_MEDIA_REQ);

    /* Handle different states */
    switch (appA2dpGetState(theInst))
    {
        case A2DP_STATE_CONNECTED_SIGNALLING:
        case A2DP_STATE_CONNECTING_MEDIA_LOCAL:
        case A2DP_STATE_CONNECTING_MEDIA_REMOTE_SYNC:
        {
            a2dpProfile_StoreSeid(theInst, ind->seid);

            /* Mark media channel as suspended by remote */
            theInst->a2dp.suspend_state |= AV_SUSPEND_REASON_REMOTE;

            /* The A2dpMediaOpenResponse will be sent on successful sync */

            /* Move to 'connecting media remote' state, wait for confirmation */
            appA2dpSetState(theInst, A2DP_STATE_CONNECTING_MEDIA_REMOTE_SYNC);
        }
        return;

        case A2DP_STATE_CONNECTED_MEDIA_STREAMING:
        case A2DP_STATE_CONNECTED_MEDIA_STREAMING_MUTED:
        case A2DP_STATE_CONNECTED_MEDIA_SUSPENDING_LOCAL:
        case A2DP_STATE_CONNECTED_MEDIA_SUSPENDED:
        case A2DP_STATE_CONNECTED_MEDIA_STARTING_LOCAL_SYNC:
        case A2DP_STATE_CONNECTED_MEDIA_STARTING_REMOTE_SYNC:
        case A2DP_STATE_DISCONNECTING_MEDIA:
        case A2DP_STATE_DISCONNECTING:
        case A2DP_STATE_DISCONNECTED:
        {
            DEBUG_LOG("appA2dpHandleA2dpMediaOpenIndication(%p) rejecting, seid(%d)", (void *)theInst, ind->seid);

            /* Reject media connection */
            A2dpMediaOpenResponse(ind->device_id, FALSE);
        }
        return;

        default:
            appA2dpError(theInst, A2DP_MEDIA_OPEN_IND, ind);
            return;
    }
}

static void A2dpProfile_sendErrorIndication(void)
{
    appAvSendUiMessageId(AV_ERROR);
}

/*! \brief A2DP media channel open confirmation

    A2DP Library has confirmed opening of the media channel, if the channel we opened
    successfully then store the SEID for use later one.

    Check if the channel should be suspended or streaming and move into the appropriate
    state.

    The state entry functions will handle resuming/suspending the channel.
    If the channel open failed then move back to 'connected signalling' state and play
    error tone.
*/
static void appA2dpHandleA2dpMediaOpenConfirm(avInstanceTaskData *theInst,
                                              const A2DP_MEDIA_OPEN_CFM_T *cfm)
{
    DEBUG_LOG("appA2dpHandleA2dpMediaOpenConfirm(%p) status(%d) seid(%d)", (void *)theInst, cfm->status, cfm->seid);

    if (cfm->status == a2dp_success)
    {
        a2dpProfile_StoreSeid(theInst, cfm->seid);
        theInst->a2dp.stream_id = cfm->stream_id;

        /* At this point check if SEID is for aptX adaptive, if it is
         * check the configuration, if it's TWS+ then use virtual SEID */
        if (theInst->a2dp.current_seid == AV_SEID_APTX_ADAPTIVE_SNK)
        {
            /* Attempt to get CODEC settings */
            a2dp_codec_settings *codec_settings = appA2dpGetCodecSettings(theInst);

            /* If pointer is valid then change SEID to virtual
             * SEID AV_SEID_APTX_ADAPTIVE_TWS_SNK */
            if (codec_settings)
            {
                a2dpProfile_StoreSeid(theInst, codec_settings->seid);
                free(codec_settings);
            }
        }
    }

    /* Handle different states */
    switch (appA2dpGetState(theInst))
    {
        case A2DP_STATE_CONNECTING_MEDIA_LOCAL:
        {
            assert(theInst->a2dp.device_id == cfm->device_id);

            /* Check if open was successful */
            if (cfm->status == a2dp_success)
            {
                avA2dpState next_state = A2DP_STATE_CONNECTED_MEDIA_SUSPENDED;

                if (appA2dpIsSeidSink(theInst->a2dp.current_seid))
                {
                    /* A sink instance should never initiate a start. Set the
                       reason to avoid starts until the source initiates the
                       start, which will clear the reason from the state. */
                    theInst->a2dp.suspend_state |= AV_SUSPEND_REASON_REMOTE;
                }
                if (!theInst->a2dp.suspend_state)
                {
                    if (appAvInstanceShouldStartMediaStreamingOnMediaOpenCfm(theInst))
                    {
                        next_state = A2DP_STATE_CONNECTED_MEDIA_STARTING_LOCAL_SYNC;
                    }
                }

                appA2dpSetState(theInst, next_state);
            }
            else
            {
                appA2dpSetState(theInst, A2DP_STATE_CONNECTED_SIGNALLING);

                A2dpProfile_sendErrorIndication();
            }
        }
        return;

        case A2DP_STATE_CONNECTING_MEDIA_REMOTE_SYNC:
        {
            assert(theInst->a2dp.device_id == cfm->device_id);

            /* Check if open was successful */
            if (cfm->status == a2dp_success)
            {
                /* Remote initiate media channel defaults to suspended */
                appA2dpSetState(theInst, A2DP_STATE_CONNECTED_MEDIA_SUSPENDED);
            }
            else
            {
                if (cfm->status == a2dp_aborted)
                {
                    DEBUG_LOG("appA2dpHandleA2dpMediaOpenConfirm aborted; disconnecting A2DP signalling");
                    appA2dpSetState(theInst, A2DP_STATE_DISCONNECTING);
                }
                /* Receiving A2DP_MEDIA_OPEN_CFM with a2dp_wrong_state in this
                state means a A2DP MEDIA OPEN cross-over occurred.
                Stay in CONNECTING_MEDIA_REMOTE_SYNC as There is no need to
                perform a state transition. */
                else if (cfm->status != a2dp_wrong_state )
                {
                    appA2dpSetState(theInst, A2DP_STATE_CONNECTED_SIGNALLING);
                }
             }
        }
        return;

        case A2DP_STATE_DISCONNECTED:
            return;

        default:
            if (cfm->status == a2dp_success)
                appA2dpError(theInst, A2DP_MEDIA_OPEN_CFM, cfm);
            return;
    }
}

/*! \brief Handle A2DP streaming start indication

    A2DP Library has indicated streaming of the media channel, accept the
    streaming request and move into the appropriate state.  If there is still
    a suspend reason active move into the 'streaming muted'.
*/
static void appA2dpHandleA2dpMediaStartIndication(avInstanceTaskData *theInst,
                                                  const A2DP_MEDIA_START_IND_T *ind)
{
    assert(theInst->a2dp.device_id == ind->device_id);

    /* Record the fact that remote device has request start */
    theInst->a2dp.suspend_state &= ~AV_SUSPEND_REASON_REMOTE;

    DEBUG_LOG("appA2dpHandleA2dpMediaStartIndication(%p) state(0x%x) suspend_state(0x%x)",
                (void *)theInst, appA2dpGetState(theInst), theInst->a2dp.suspend_state);

    /* Handle different states */
    switch (appA2dpGetState(theInst))
    {
        case A2DP_STATE_CONNECTED_MEDIA_SUSPENDED:
        case A2DP_STATE_CONNECTED_MEDIA_STREAMING_MUTED:
        case A2DP_STATE_CONNECTED_MEDIA_STARTING_LOCAL_SYNC:
        {
            /* Check if we should still be suspended */
            if ((theInst->a2dp.suspend_state == 0) &&
                appAvInstanceShouldStartMediaStreamingOnMediaStartInd(theInst))
            {
                if (appA2dpGetState(theInst) == A2DP_STATE_CONNECTED_MEDIA_STARTING_LOCAL_SYNC)
                {
                    /* As apps already in the process of starting the MEDIA and
                       this flag suggets synchronisation is completed so need to
                       respond to AG. */
                    DEBUG_LOG("appA2dpHandleA2dpMediaStartIndication: sync_response_rcvd %d", theInst->a2dp.bitfields.sync_response_rcvd);
                    if (theInst->a2dp.bitfields.sync_response_rcvd)
                    {
                        DEBUG_LOG("appA2dpHandleA2dpMediaStartIndication accepting A2dpMediaStart device_id %u", theInst->a2dp.device_id);
                        TimestampEvent(TIMESTAMP_EVENT_A2DP_START_RSP);
                        PanicFalse(A2dpMediaStartResponse(theInst->a2dp.device_id, theInst->a2dp.stream_id, TRUE));
                        /* The sync is complete, waiting for the A2DP_MEDIA_START_CFM. */
                    }
                    /* Receiving A2DP_MEDIA_START_IND_T in state
                       A2DP_STATE_CONNECTED_MEDIA_STARTING_LOCAL_SYNC means a
                       start cross-over occurred. When entering state
                       STARTING_LOCAL_SYNC the same actions are performed as
                       required when entering STARTING_REMOVE_SYNC, so there is
                       no need to perform a state transition - instead just
                       change the local state without going through a state
                       transition. */
                   theInst->a2dp.state = A2DP_STATE_CONNECTED_MEDIA_STARTING_REMOTE_SYNC;
                }
                else
                {
                    /* Move to 'connected media starting remote' state to wait for
                       the other instance to start streaming */
                    appA2dpSetState(theInst, A2DP_STATE_CONNECTED_MEDIA_STARTING_REMOTE_SYNC);
                }
                TimestampEvent(TIMESTAMP_EVENT_A2DP_START_IND);

            }
            else
            {
                /* Note: We could reject the AVDTP_START at this point, but this
                   seems to upset some AV sources, so instead we'll accept
                   but just drop the audio data */

                /* Accept streaming start request */
                A2dpMediaStartResponse(theInst->a2dp.device_id, ind->stream_id, TRUE);

                /* Move into 'streaming muted' state */
                appA2dpSetState(theInst, A2DP_STATE_CONNECTED_MEDIA_STREAMING_MUTED);
            }
        }
        return;

        case A2DP_STATE_CONNECTED_MEDIA_SUSPENDING_LOCAL:
        {
            /* Accept streaming start request */
            A2dpMediaStartResponse(theInst->a2dp.device_id, ind->stream_id, TRUE);

            /* Not ready to start streaming, so enter the 'streaming muted' state */
            appA2dpSetState(theInst, A2DP_STATE_CONNECTED_MEDIA_STREAMING_MUTED);
        }
        return;

        case A2DP_STATE_CONNECTED_MEDIA_STREAMING:
        {
            /* Accept streaming start request */
            A2dpMediaStartResponse(theInst->a2dp.device_id, ind->stream_id, TRUE);
        }
        return;

        case A2DP_STATE_CONNECTED_MEDIA_STARTING_REMOTE_SYNC:
        {
            /* Received duplicate, just ignore and hope for the best... */
        }
        return;

        default:
            appA2dpError(theInst, A2DP_MEDIA_START_IND, ind);
            return;
    }
}

/*! \brief Handle A2DP streaming start confirmation

    A2DP Library has confirmed streaming of the media channel, if successful
    and in the 'suspended' or 'starting local' state then move into the
    'streaming' state if there is no suspend reasons pending otherwise
    move into the 'suspending local' state.  If streaming failed then move
    into the 'suspended' state and wait for the peer device to start streaming.
*/
static void appA2dpHandleA2dpMediaStartConfirmation(avInstanceTaskData *theInst,
                                                    const A2DP_MEDIA_START_CFM_T *cfm)
{
    uint32 start_time;

    assert(theInst->a2dp.device_id == cfm->device_id);

    TimestampEvent(TIMESTAMP_EVENT_A2DP_START_CFM);

    start_time = TimestampEvent_Delta(TIMESTAMP_EVENT_A2DP_START_IND,
                                      TIMESTAMP_EVENT_A2DP_START_CFM);
    DEBUG_LOG("appA2dpHandleA2dpMediaStartConfirmation(%p) status(%d) start_time(%d)",
               (void *)theInst, cfm->status, start_time);

    /* Handle different states */
    switch (appA2dpGetState(theInst))
    {
        case A2DP_STATE_CONNECTED_MEDIA_STREAMING_MUTED:
        case A2DP_STATE_CONNECTED_MEDIA_STREAMING:
        {
            /* Ignore, we're already streaming */
        }
        return;

        case A2DP_STATE_CONNECTED_MEDIA_SUSPENDED:
        case A2DP_STATE_CONNECTED_MEDIA_STARTING_LOCAL_SYNC:
        case A2DP_STATE_CONNECTED_MEDIA_STARTING_REMOTE_SYNC:
        {
            /* Check confirmation is success */
            if (cfm->status == a2dp_success)
            {
                /* Check if we should start or suspend streaming */
                if (theInst->a2dp.suspend_state != 0)
                    appA2dpSetState(theInst, A2DP_STATE_CONNECTED_MEDIA_SUSPENDING_LOCAL);
                else
                    appA2dpSetState(theInst, A2DP_STATE_CONNECTED_MEDIA_STREAMING);
            }
            /* Receiving A2DP_MEDIA_START_CFM with a2dp_wrong_state in these
            states means a A2DP MEDIA START cross-over occurred.
            There is no need to perform a state transition. */
            else if (cfm->status != a2dp_wrong_state )
            {
                /* Move to suspended state */
                appA2dpSetState(theInst, A2DP_STATE_CONNECTED_MEDIA_SUSPENDED);
            }
        }
        return;

        default:
            appA2dpError(theInst, A2DP_MEDIA_START_CFM, cfm);
            return;
    }
}

/*! \brief Handle A2DP streaming suspend indication

    The peer device has suspended the media channel.  Move into the
    'connected suspended' state, the state entry function will handle
    turning off the DACs and powering down the DSP.
*/
static void appA2dpHandleA2dpMediaSuspendIndication(avInstanceTaskData *theInst,
                                                    const A2DP_MEDIA_SUSPEND_IND_T *ind)
{
    assert(theInst->a2dp.device_id == ind->device_id);
    DEBUG_LOG("appA2dpHandleA2dpMediaSuspendIndication(%p)", (void *)theInst);

    if(appA2dpIsSourceCodec(theInst) && appA2dpGetState(theInst) == A2DP_STATE_CONNECTED_MEDIA_SUSPENDING_LOCAL)
    {
        /* if the above condition is TRUE a suspend crossover has been detected.
                To avoid a stalemate, we (master) don't set the AV_SUSPEND_REASON_REMOTE.
                The slave (that is remotely suspended) will be resumed by us later. */
        DEBUG_LOG("appA2dpHandleA2dpMediaSuspendIndication, crossover detected by master");
    }
    else
    {
        /* Record the fact that remote device has request suspend */
        theInst->a2dp.suspend_state |= AV_SUSPEND_REASON_REMOTE;
    }

    /* Handle different states */
    switch (appA2dpGetState(theInst))
    {
        case A2DP_STATE_CONNECTED_MEDIA_STARTING_LOCAL_SYNC:
        case A2DP_STATE_CONNECTED_MEDIA_STARTING_REMOTE_SYNC:
        case A2DP_STATE_CONNECTED_MEDIA_STREAMING:
        case A2DP_STATE_CONNECTED_MEDIA_STREAMING_MUTED:
        case A2DP_STATE_CONNECTED_MEDIA_SUSPENDED:
        case A2DP_STATE_CONNECTED_MEDIA_SUSPENDING_LOCAL:
        {
            /* Move to 'connect media suspended' state */
            appA2dpSetState(theInst, A2DP_STATE_CONNECTED_MEDIA_SUSPENDED);
        }
        return;

        default:
            appA2dpError(theInst, A2DP_MEDIA_SUSPEND_IND, ind);
            return;
    }
}

/*! \brief Handle A2DP streaming suspend confirmation

    Confirmation of request to suspend streaming.  If our request to suspend
    streaming was sucessful move to the 'suspended' or 'resuming' state depending
    on whether a resume was pending.

    If the suspend request was rejected then move to the 'streaming muted' or
    'streaming' state depending on whether a resume was pending.  If a resume was
    pending we can go straight to the 'streaming' state and the suspend never
    actually happened.
*/
static void appA2dpHandleA2dpMediaSuspendConfirmation(avInstanceTaskData *theInst,
                                                      const A2DP_MEDIA_SUSPEND_CFM_T *cfm)
{
    assert(theInst->a2dp.device_id == cfm->device_id);
    DEBUG_LOG("appA2dpHandleA2dpMediaSuspendConfirmation(%p) status %d", (void *)theInst, cfm->status);

    /* Handle different states */
    switch (appA2dpGetState(theInst))
    {
        case A2DP_STATE_CONNECTED_MEDIA_SUSPENDED:
            /* Already suspended, so just ignore */
            return;

        case A2DP_STATE_CONNECTED_MEDIA_STREAMING:
        case A2DP_STATE_CONNECTED_MEDIA_SUSPENDING_LOCAL:
        case A2DP_STATE_CONNECTED_MEDIA_STARTING_LOCAL_SYNC:
        case A2DP_STATE_CONNECTED_MEDIA_STARTING_REMOTE_SYNC:
        {
            /* Check if we suspended */
            if (cfm->status == a2dp_success)
            {
                /* Check if we should start or suspend streaming */
                if (theInst->a2dp.suspend_state != 0)
                    appA2dpSetState(theInst, A2DP_STATE_CONNECTED_MEDIA_SUSPENDED);
                else
                    appA2dpSetState(theInst, A2DP_STATE_CONNECTED_MEDIA_STARTING_LOCAL_SYNC);
            }
            else
            {
                /* Check if we should start or mute streaming */
                if (theInst->a2dp.suspend_state != 0)
                    appA2dpSetState(theInst, A2DP_STATE_CONNECTED_MEDIA_STREAMING_MUTED);
                else
                    appA2dpSetState(theInst, A2DP_STATE_CONNECTED_MEDIA_STARTING_LOCAL_SYNC);
            }
        }
        return;

        default:
            appA2dpError(theInst, A2DP_MEDIA_SUSPEND_CFM, cfm);
            return;
    }
}

/*! \brief Find the start of the service category in CODEC capabilities.

    \param          service_category The service category to search for.
    \param[in,out]  caps_ptr Must be set to point to a service category definition
    (followed by the service category length). The function will modify the pointer
    to the category header if service_category is found, or NULL if not found/malformed.
    \param[out]     size_caps_ptr Set to the total size of the category, or 0 if not
    found/malformed.
*/
static void appA2dpFindServiceCategory(uint8 service_category, const uint8 **caps_ptr, uint16 *size_caps_ptr)
{
    const uint8 *caps = *caps_ptr;
    signed size_caps = *size_caps_ptr;

    while (size_caps > 0)
    {
        if (service_category == caps[0])
        {
            break;
        }

        size_caps -= 2 + caps[1];
        caps += 2 + caps[1];
    }
    if (size_caps >= 2)
    {
        *caps_ptr = caps;
        *size_caps_ptr = size_caps;
    }
    else
    {
        /* Not found or malformed caps */
        *caps_ptr = NULL;
        *size_caps_ptr = 0;
    }
}

/*! \brief Copy service category

    Searchs the CODEC capabilities for the required service category and
    copies it from the from_caps capabilities to the to_caps capabilities.
*/
static bool appA2dpCopyServiceCategory(uint8 service_category,
                                       const uint8 *from_caps, uint16 size_from_caps,
                                       uint8 *to_caps, uint16 size_to_caps)
{
    appA2dpFindServiceCategory(service_category, &from_caps, &size_from_caps);
    appA2dpFindServiceCategory(service_category, &to_caps, &size_to_caps);
    if (from_caps && to_caps)
    {
        memcpy(to_caps, from_caps, size_from_caps);
        return TRUE;
    }
    else
        return FALSE;
}

/*! \brief The offset in bytes between the main AVDTP_SERVICE_MEDIA_CODEC and
           the embedded AVDTP_SERVICE_MEDIA_CODEC definitions in AAC/SBC TWS
           capabilities see aac_tws_caps/sbc_tws_caps in a2dp_profile_caps.c */
#define TWS_CAPS_OFFSET_FROM_MEDIA_CODEC_TO_EMBEDDED_MEDIA_CODEC 10

static void appA2dpConfigureCodecCapabilities(uint8 *src_caps, uint16 size_src_caps,
                                              const uint8 *snk_caps, uint16 size_snk_caps,
                                              uint8 src_seid, uint32 rate)
{
    /* Find AVDTP_SERVICE_MEDIA_CODEC service category in source CODEC capabilites */
    appA2dpFindServiceCategory(AVDTP_SERVICE_MEDIA_CODEC, &src_caps, &size_src_caps);
    PanicFalse(src_caps);
    switch (src_seid)
    {
        case AV_SEID_APTX_MONO_TWS_SRC:
        {
            /* Configure sample rate */
            appAvUpdateAptxMonoTwsCapabilities(src_caps, rate);
        }
        break;


        case AV_SEID_SBC_MONO_TWS_SRC:
        {
            /* Configure sample rate */
            appAvUpdateSbcMonoTwsCapabilities(src_caps, rate);
        }
        break;

        case AV_SEID_SBC_SRC:
        {
            /* Configure sample rate */
            appAvUpdateSbcCapabilities(src_caps, rate);
        }
        break;

        case AV_SEID_AAC_STEREO_TWS_SRC:
        {
            PanicFalse(size_src_caps >= TWS_CAPS_OFFSET_FROM_MEDIA_CODEC_TO_EMBEDDED_MEDIA_CODEC);
            src_caps += TWS_CAPS_OFFSET_FROM_MEDIA_CODEC_TO_EMBEDDED_MEDIA_CODEC;
            size_src_caps -= TWS_CAPS_OFFSET_FROM_MEDIA_CODEC_TO_EMBEDDED_MEDIA_CODEC;
            /* Copy AVDTP_SERVICE_MEDIA_CODEC from sink capabilties to embedded
               source capabilities */
            appA2dpCopyServiceCategory(AVDTP_SERVICE_MEDIA_CODEC,
                                       snk_caps, size_snk_caps,
                                       src_caps, size_src_caps);
        }
        break;

        default:
            Panic();
        return;
    }
}

/*! \brief Handle request for CODEC configuration

    The A2DP profile library has requested the media CODEC configuration for
    the specified media codec.

    Check if there is another A2DP instance and query it's media CODEC
    configuration to build up a vendor CODEC.
*/
static void appA2dpHandleA2dpCodecConfigureIndication(avInstanceTaskData *theInst,
                                                      A2DP_CODEC_CONFIGURE_IND_T *ind)
{
    avInstanceTaskData *peerInst;

    assert(theInst->a2dp.device_id == ind->device_id);
    DEBUG_LOG("appA2dpHandleA2dpCodecConfigureIndication(%p)", (void *)theInst);

    /* Check if there is another connected media A2DP instance */
    peerInst = appAvInstanceFindA2dpState(theInst, A2DP_STATE_MASK_CONNECTED_MEDIA,
                                          A2DP_STATE_CONNECTED_MEDIA);
    if (peerInst == NULL)
    {
        peerInst = appAvInstanceFindA2dpState(theInst, ~0,
                                              A2DP_STATE_CONNECTING_MEDIA_REMOTE_SYNC);
    }

    if (peerInst != NULL)
    {
        /* Check instance is configured as a sink */
        if (appA2dpIsSinkCodec(peerInst))
        {
            /* Get CODEC capabilities of other A2DP instance (sink).
               The stream ID just set to 0, because it is not known at this point */
            a2dp_codec_settings *codec_settings = A2dpCodecGetSettings(peerInst->a2dp.device_id, 0);

            /* Check SEID of source is compatible with non-TWS sink */
            if (codec_settings && appA2dpConvertSeidFromSinkToSource(codec_settings->seid) == ind->local_seid)
            {
                const uint8 *sink_caps = codec_settings->configured_codec_caps;
                uint16 size_sink_caps = codec_settings->size_configured_codec_caps;

                DEBUG_LOG("appA2dpHandleA2dpCodecConfigureIndication(%p), local SEID %d, sink SEID %d %d",
                      (void *)theInst, ind->local_seid, codec_settings->seid, codec_settings->rate);

                /* Get CODEC capabilities of the TWS source */
                uint8 *src_caps = ind->codec_service_caps;
                uint16 size_src_caps = ind->size_codec_service_caps;

                appA2dpConfigureCodecCapabilities(src_caps, size_src_caps,
                                                  sink_caps, size_sink_caps,
                                                  ind->local_seid, codec_settings->rate);

                /* Pass updated CODEC capabilites to A2DP library */
                PanicFalse(A2dpCodecConfigureResponse(ind->device_id, TRUE, ind->local_seid,
                                                      ind->size_codec_service_caps, ind->codec_service_caps));
            }
            else
            {
                DEBUG_LOG("appA2dpHandleA2dpCodecConfigureIndication(%p) rejecting, local SEID %d",
                            (void *)theInst, ind->local_seid);

                /* SEID of source and sink incompatible, reject */
                A2dpCodecConfigureResponse(theInst->a2dp.device_id, FALSE, ind->local_seid, 0, NULL);
            }

            /* Free structure now that we're done with it */
            free(codec_settings);
        }
        else
        {
            /* No streaming on sink instance, keep rejecting to prevent media channel being setup */
            A2dpCodecConfigureResponse(theInst->a2dp.device_id, FALSE, ind->local_seid, 0, NULL);
        }
    }
    else
    {
        /* No sink instance, keep rejecting to prevent media channel being setup */
        A2dpCodecConfigureResponse(theInst->a2dp.device_id, FALSE, ind->local_seid, 0, NULL);
    }
}

/*! \brief Handle media channel closed remotely, or in reponse to a  local call
    to A2dpMediaCloseRequest().

    The peer device has disconnected the media channel, move back to the
    'connected signalling' state, the state machine entry & exit functions
    will automatically disconnect the DACs & DSP if required.
*/
static void appA2dpHandleA2dpMediaCloseIndication(avInstanceTaskData *theInst,
                                                  const A2DP_MEDIA_CLOSE_IND_T *ind)
{
    assert(theInst->a2dp.device_id == ind->device_id);
    DEBUG_LOG("appA2dpHandleA2dpMediaCloseIndication(%p), status(%d)", (void *)theInst, ind->status);

    /* Handle different states */
    switch (appA2dpGetState(theInst))
    {
        case A2DP_STATE_CONNECTED_SIGNALLING:
        case A2DP_STATE_CONNECTING_MEDIA_LOCAL:
        case A2DP_STATE_CONNECTING_MEDIA_REMOTE_SYNC:
        case A2DP_STATE_CONNECTED_MEDIA_STREAMING:
        case A2DP_STATE_CONNECTED_MEDIA_STREAMING_MUTED:
        case A2DP_STATE_CONNECTED_MEDIA_SUSPENDING_LOCAL:
        case A2DP_STATE_CONNECTED_MEDIA_SUSPENDED:
        case A2DP_STATE_CONNECTED_MEDIA_STARTING_LOCAL_SYNC:
        case A2DP_STATE_CONNECTED_MEDIA_STARTING_REMOTE_SYNC:
        case A2DP_STATE_DISCONNECTING_MEDIA:
        {
            /* Move to 'connected signalling' */
            appA2dpSetState(theInst, A2DP_STATE_CONNECTED_SIGNALLING);
        }
        return;

        case A2DP_STATE_CONNECTING_LOCAL:
        case A2DP_STATE_CONNECTING_REMOTE:
        case A2DP_STATE_DISCONNECTED:
        {
            /* Probably late message from A2DP profile library, just ignore */
        }
        return;

        case A2DP_STATE_DISCONNECTING:
        {
            /* Media channel disconnected, wait for signalling channel to disconnect */
            if(ind->status == a2dp_disconnect_transferred)
            {
                DEBUG_LOG("appA2dpHandleA2dpMediaCloseIndication Media closed due to link transfer");
            }
        }
        return;

        default:
            appA2dpError(theInst, A2DP_MEDIA_CLOSE_IND, ind);
            return;
    }
}

/*! \brief Handle media av sync delay indication

    Causes a delay report to be issued to a connected Source.
*/
static void appA2dpHandleA2dpMediaAvSyncDelayIndication(avInstanceTaskData *theInst,
                                                        const A2DP_MEDIA_AV_SYNC_DELAY_IND_T *ind)
{
    assert(theInst->a2dp.device_id == ind->device_id);
    DEBUG_LOG("appA2dpHandleA2dpMediaAvSyncDelayIndication(%p) seid(%d)", (void *)theInst, ind->seid);

    if (appA2dpIsConnected(theInst))
    {
        uint16 delay;
        if (appA2dpIsSeidTwsSink(ind->seid))
        {
            delay = TWS_SLAVE_LATENCY_MS * 10;
        }
        else
        {
            delay = Kymera_LatencyManagerGetLatencyForSeidInUs(ind->seid) / 100;
        }
        A2dpMediaAvSyncDelayResponse(ind->device_id, ind->seid, delay);
    }
    else
        appA2dpError(theInst, A2DP_MEDIA_AV_SYNC_DELAY_IND, ind);
}

static void appA2dpHandleA2dpMediaReconfigureIndication(avInstanceTaskData *theInst,
                                                        const A2DP_MEDIA_RECONFIGURE_IND_T *ind)
{
    theInst->a2dp.device_id = ind->device_id;
    theInst->a2dp.stream_id = ind->stream_id;

    a2dp_codec_settings *codec_settings = appA2dpGetCodecSettings(theInst);
    if (codec_settings)
    {
        DEBUG_LOG("appA2dpHandleA2dpMediaReconfigureIndication(%p) seid(%d) rate(%d)",
                    (void *)theInst, codec_settings->seid, codec_settings->rate);
        free(codec_settings);

        /* Tell an registered audio_sync object of the changed codec parameters. */
        AudioSync_CodecReconfiguredIndication(theInst->a2dp.sync_inst,
                                              a2dpProfile_LookupSource(theInst),
                                              theInst->a2dp.current_seid,
                                              theInst->a2dp.device_id, theInst->a2dp.stream_id);
    }
}

static void appA2dpHandleA2dpMediaReconfigureConfirm(avInstanceTaskData *theInst,
                                                     const A2DP_MEDIA_RECONFIGURE_CFM_T *message)
{
    avA2dpState local_state = appA2dpGetState(theInst);
    DEBUG_LOG("appA2dpHandleA2dpMediaReconfigureConfirm(%p) state(0x%x) status(%d)", theInst, local_state, message->status);

    if (a2dp_success == message->status)
    {
        switch (local_state)
        {
            case A2DP_STATE_CONNECTED_MEDIA_RECONFIGURING:
                appA2dpSetState(theInst, A2DP_STATE_CONNECTED_MEDIA_SUSPENDED);
            break;
            case A2DP_STATE_CONNECTED_MEDIA_SUSPENDED:
            break;
            default:
                appA2dpError(theInst, A2DP_MEDIA_RECONFIGURE_CFM, message);
            break;
        }
    }
    else
    {
        appA2dpSetState(theInst, A2DP_STATE_CONNECTED_SIGNALLING);
    }
}


/*! \brief Handle internal indication that AVRCP is unlocked

    This function is called when we have determined that the AVRCP library is no
    longer locked, we need to make sure AVRCP is unlocked in case there is
    a passthrough command in the process of being sent.
*/
static void appA2dpHandleInternalAvrcpUnlockInd(avInstanceTaskData *theInst)
{
    /* AVRCP is now unlocked, we can proceed with the disconnect */
    A2dpSignallingDisconnectRequest(theInst->a2dp.device_id);
}

static void appA2dpHandleInternalA2dpCodecReconfigInd(avInstanceTaskData *theInst,
                                                      const AV_INTERNAL_A2DP_CODEC_RECONFIG_IND_T *ind)
{
    avA2dpState local_state = appA2dpGetState(theInst);
    DEBUG_LOG("appA2dpHandleInternalA2dpCodecReconfigInd(%p) state(0x%x)", (void *)theInst, local_state);
    switch (local_state)
    {
        case A2DP_STATE_CONNECTED_MEDIA_SUSPENDED:
        {
            DEBUG_LOG("appA2dpHandleInternalA2dpCodecReconfigInd(%p) %d %d %d %d", (void *)theInst,
                       theInst->a2dp.device_id, theInst->a2dp.stream_id, ind->device_id, ind->stream_id);

            a2dp_codec_settings *src_codec_settings = A2dpCodecGetSettings(theInst->a2dp.device_id, theInst->a2dp.stream_id);
            a2dp_codec_settings *snk_codec_settings = A2dpCodecGetSettings(ind->device_id, ind->stream_id);

            DEBUG_LOG("appA2dpHandleInternalA2dpCodecReconfigInd(%p) %p %p %d %d %d %d", (void *)theInst,
                src_codec_settings, snk_codec_settings, src_codec_settings->seid, snk_codec_settings->seid,
                src_codec_settings->rate, snk_codec_settings->rate);

            /* Check codec settings are compatible */
            if (src_codec_settings && snk_codec_settings &&
                appA2dpConvertSeidFromSinkToSource(snk_codec_settings->seid) == src_codec_settings->seid)
            {
                uint8 *media_codec_cap = src_codec_settings->configured_codec_caps;
                uint16 media_codec_cap_size = src_codec_settings->size_configured_codec_caps;
                appA2dpConfigureCodecCapabilities(src_codec_settings->configured_codec_caps,
                                                  src_codec_settings->size_configured_codec_caps,
                                                  snk_codec_settings->configured_codec_caps,
                                                  snk_codec_settings->size_configured_codec_caps,
                                                  src_codec_settings->seid, snk_codec_settings->rate);

                /* Modify the pointer/size to address the media codec within the codec caps */
                appA2dpFindServiceCategory(AVDTP_SERVICE_MEDIA_CODEC, &media_codec_cap, &media_codec_cap_size);
                PanicNull(media_codec_cap);
                PanicZero(media_codec_cap_size);
                PanicFalse(AVDTP_SERVICE_MEDIA_CODEC == media_codec_cap[0]);

                PanicFalse(A2dpMediaReconfigureRequest(theInst->a2dp.device_id, theInst->a2dp.stream_id,
                                                       media_codec_cap_size, media_codec_cap));
                appA2dpSetState(theInst, A2DP_STATE_CONNECTED_MEDIA_RECONFIGURING);
            }
            else
            {
                appA2dpSetState(theInst, A2DP_STATE_CONNECTED_SIGNALLING);
            }
            free(src_codec_settings);
            free(snk_codec_settings);
        }
        break;
        default:
            appA2dpError(theInst, AV_INTERNAL_A2DP_CODEC_RECONFIG_IND, NULL);
        break;
    }
}

/*! \brief Initialise AV instance

    This function initialises the specified AV instance, all state variables are
    set to defaults, with the exception of the streaming state which is
    initialised with the value supplied. If non-zero this has the effect of
    blocking streaming initially.

    \note This function should only be called on a newly created
    instance.

    \param theAv            The AV that has the A2DP instance to initialise
    \param suspend_state    The initial suspend_state to apply
 */
void appA2dpInstanceInit(avInstanceTaskData *theAv, uint8 suspend_state)
{
    theAv->a2dp.state = A2DP_STATE_DISCONNECTED;
    theAv->a2dp.current_seid = AV_SEID_INVALID;
    theAv->a2dp.bitfields.flags = 0;
    theAv->a2dp.lock = 0;
    theAv->a2dp.sync_counter = 0;
    theAv->a2dp.suspend_state = suspend_state;
    theAv->a2dp.bitfields.local_initiated = FALSE;
    theAv->a2dp.bitfields.disconnect_reason = AV_A2DP_DISCONNECT_NORMAL;
    theAv->a2dp.source_state = audio_source_disconnected;

    /* No profile instance yet */
    theAv->a2dp.device_id = INVALID_DEVICE_ID;
}

/*! \brief Set A2DP volume

    Update the playback volume if the supplied AV has an active A2DP Sink.

    Initiate saving of the playback volume for use on subsequent connections
    or after the earbud restarts. The volume is not saved immediately
    as volume is often adjusted several times consecutively.

    \param theInst  The AV instance with an A2DP link
    \param volume   The volume to use
*/
void appA2dpVolumeSet(avInstanceTaskData *theInst, uint16 volume)
{
    PanicFalse(appAvIsValidInst(theInst));

    /* Check if this instance is a A2DP sink */
    if (appA2dpIsSinkCodec(theInst))
    {
        /* We're a sink so just set the CODEC volume immediately */
        appKymeraA2dpSetVolume(volume);
    }

}





/*! \brief Message Handler

    This function is the main message handler for an A2DP instance, every
    message is handled in it's own seperate handler function.  The switch
    statement is broken into seperate blocks to reduce code size, if execution
    reaches the end of the function then it is assumed that the message is
    unhandled.

    \param theInst      The instance data for the AV for this A2DP connection
    \param id           Message identifier. For internal messages, see \ref av_internal_messages
    \param[in] message  Message content, potentially NULL.

*/
void appA2dpInstanceHandleMessage(avInstanceTaskData *theInst, MessageId id, Message message)
{
    /* Handle internal messages */
    switch (id)
    {
        case AV_INTERNAL_A2DP_SIGNALLING_CONNECT_IND:
            appA2dpHandleInternalA2dpSignallingConnectIndication(theInst, (AV_INTERNAL_A2DP_SIGNALLING_CONNECT_IND_T *)message);
            return;

        case AV_INTERNAL_A2DP_CONNECT_REQ:
            appA2dpHandleInternalA2dpConnectRequest(theInst, (AV_INTERNAL_A2DP_CONNECT_REQ_T *)message);
            return;

        case AV_INTERNAL_A2DP_CONNECT_MEDIA_REQ:
            appA2dpHandleInternalA2dpConnectMediaRequest(theInst, (AV_INTERNAL_A2DP_CONNECT_MEDIA_REQ_T *)message);
            return;

        case AV_INTERNAL_A2DP_DISCONNECT_MEDIA_REQ:
            appA2dpHandleInternalA2dpDisconnectMediaRequest(theInst);
            return;

        case AV_INTERNAL_A2DP_DISCONNECT_REQ:
            appA2dpHandleInternalA2dpDisconnectRequest(theInst, (AV_INTERNAL_A2DP_DISCONNECT_REQ_T *)message);
            return;

        case AV_INTERNAL_A2DP_SUSPEND_MEDIA_REQ:
            appA2dpHandleInternalA2dpSuspendRequest(theInst, (AV_INTERNAL_A2DP_SUSPEND_MEDIA_REQ_T *)message);
            return;

        case AV_INTERNAL_A2DP_RESUME_MEDIA_REQ:
            appA2dpHandleInternalA2dpResumeRequest(theInst, (AV_INTERNAL_A2DP_RESUME_MEDIA_REQ_T *)message);
            return;

        case AV_INTERNAL_AVRCP_UNLOCK_IND:
            appA2dpHandleInternalAvrcpUnlockInd(theInst);
            return;

        case AV_INTERNAL_A2DP_DESTROY_REQ:
            appAvInstanceDestroy(theInst);
            return;

        case AV_INTERNAL_A2DP_CODEC_RECONFIG_IND:
            appA2dpHandleInternalA2dpCodecReconfigInd(theInst, message);
            return;
    }

    /* Handle A2DP library messages */
    switch (id)
    {
        case A2DP_SIGNALLING_CONNECT_IND:
            appA2dpHandleA2dpSignallingConnectIndication(theInst, (A2DP_SIGNALLING_CONNECT_IND_T *)message);
            return;

        case A2DP_SIGNALLING_CONNECT_CFM:
            appA2dpHandleA2dpSignallingConnectConfirm(theInst, (A2DP_SIGNALLING_CONNECT_CFM_T *)message);
            return;

        case A2DP_SIGNALLING_DISCONNECT_IND:
            appA2dpHandleA2dpSignallingDisconnectInd(theInst, (A2DP_SIGNALLING_DISCONNECT_IND_T *)message);
            return;

        case A2DP_MEDIA_OPEN_IND:
            appA2dpHandleA2dpMediaOpenIndication(theInst, (A2DP_MEDIA_OPEN_IND_T *)message);
            return;

        case A2DP_MEDIA_OPEN_CFM:
            appA2dpHandleA2dpMediaOpenConfirm(theInst, (A2DP_MEDIA_OPEN_CFM_T *)message);
            return;

        case A2DP_MEDIA_START_IND:
            appA2dpHandleA2dpMediaStartIndication(theInst, (A2DP_MEDIA_START_IND_T *)message);
            return;

        case A2DP_MEDIA_START_CFM:
            appA2dpHandleA2dpMediaStartConfirmation(theInst, (A2DP_MEDIA_START_CFM_T *)message);
            return;

        case A2DP_MEDIA_SUSPEND_IND:
            appA2dpHandleA2dpMediaSuspendIndication(theInst, (A2DP_MEDIA_SUSPEND_IND_T *)message);
            return;

        case A2DP_MEDIA_SUSPEND_CFM:
            appA2dpHandleA2dpMediaSuspendConfirmation(theInst, (A2DP_MEDIA_SUSPEND_CFM_T *)message);
            return;

        case A2DP_CODEC_CONFIGURE_IND:
            appA2dpHandleA2dpCodecConfigureIndication(theInst, (A2DP_CODEC_CONFIGURE_IND_T *)message);
            return;

        case A2DP_MEDIA_CLOSE_IND:
            appA2dpHandleA2dpMediaCloseIndication(theInst, (A2DP_MEDIA_CLOSE_IND_T *)message);
            return;

        case A2DP_ENCRYPTION_CHANGE_IND:
            return;

        case A2DP_MEDIA_RECONFIGURE_IND:
            appA2dpHandleA2dpMediaReconfigureIndication(theInst, (A2DP_MEDIA_RECONFIGURE_IND_T *)message);
            return;
        case A2DP_MEDIA_RECONFIGURE_CFM:
            appA2dpHandleA2dpMediaReconfigureConfirm(theInst, (A2DP_MEDIA_RECONFIGURE_CFM_T *)message);
            return;

        case A2DP_MEDIA_AV_SYNC_DELAY_IND:
            appA2dpHandleA2dpMediaAvSyncDelayIndication(theInst, (A2DP_MEDIA_AV_SYNC_DELAY_IND_T *)message);
            return;

        case A2DP_MEDIA_AV_SYNC_DELAY_CFM:
        case A2DP_MEDIA_AV_SYNC_DELAY_UPDATED_IND:
            return;
    }

    /* Unhandled message */
    appA2dpError(theInst, id, message);
}

void A2dpProfile_SetPtsMode(bool is_enabled)
{
    pts_mode_enabled = is_enabled;
}

bool A2dpProfile_IsPtsMode(void)
{
    return pts_mode_enabled;
}

static void a2dpProfile_SendPauseToInstance(avInstanceTaskData *av_instance)
{
    DEBUG_LOG_FN_ENTRY("a2dpProfile_SendPauseToInstance");

    MessageCancelAll(&av_instance->av_task, AV_INTERNAL_AVRCP_PLAY_REQ);
    MessageCancelAll(&av_instance->av_task, AV_INTERNAL_AVRCP_PAUSE_REQ);
    MessageSendConditionally(&av_instance->av_task, AV_INTERNAL_AVRCP_PAUSE_REQ, NULL,
                                &av_instance->avrcp.playback_lock);
}

void A2dpProfile_SetSourceState(avInstanceTaskData *av_instance, audio_source_state_t state)
{
    avA2dpState a2dp_state = appA2dpGetState(av_instance);

    DEBUG_LOG_FN_ENTRY("A2dpProfile_SetSourceState new_state=%d, old_state=%d, a2dp_state=0x%02x", state,
                                av_instance->a2dp.source_state, a2dp_state);

    switch(av_instance->a2dp.source_state)
    {
        case audio_source_connected:
            if(state == audio_source_disconnecting)
            {
                switch(a2dp_state)
                {
                    case A2DP_STATE_CONNECTED_MEDIA_STREAMING:
                    case A2DP_STATE_CONNECTED_MEDIA_STREAMING_MUTED:
                        a2dpProfile_SendPauseToInstance(av_instance);
                        break;

                    default:
                        break;
                }
            }
            break;

        case audio_source_disconnected:
        case audio_source_connecting:
        case audio_source_disconnecting:
            break;

        default:
            Panic();
            break;
    }
    av_instance->a2dp.source_state = state;
}

#else
static const int compiler_happy;
#endif
