/*!
\copyright  Copyright (c) 2017-2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera.c
\brief      Kymera Manager
*/

#include "kymera_private.h"
#include "kymera_config.h"
#include "kymera_adaptive_anc.h"
#include "kymera_aec.h"
#include "av.h"
#include "a2dp_profile.h"
#include "scofwd_profile_config.h"
#include "kymera.h"
#include "usb_common.h"
#include "power_manager.h"
#include "kymera_latency_manager.h"
#include "kymera_dynamic_latency.h"
#include "mirror_profile.h"
#include "anc_state_manager.h"
#include <vmal.h>
#include "kymera_usb_audio.h"
#include "kymera_usb_voice.h"
#include "kymera_va.h"
#include <task_list.h>
#include <logging.h>

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_ENUM(app_kymera_internal_message_ids)
LOGGING_PRESERVE_MESSAGE_ENUM(kymera_messages)
LOGGING_PRESERVE_MESSAGE_TYPE(kymera_msg_t)



/*!< State data for the DSP configuration */
kymeraTaskData  app_kymera;

/*! Macro for creating messages */
#define MAKE_KYMERA_MESSAGE(TYPE) \
    TYPE##_T *message = PanicUnlessNew(TYPE##_T);

static const appKymeraScoChainInfo *appKymeraScoChainTable = NULL;
static const appKymeraScoChainInfo *appKymeraScoSlaveChainTable = NULL;

static const capability_bundle_config_t *bundle_config = NULL;

static const kymera_chain_configs_t *chain_configs = NULL;

#define GetKymeraClients() TaskList_GetBaseTaskList(&KymeraGetTaskData()->client_tasks)


static void appKymeraSetConcurrentState(appKymeraConcurrentState state)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOG("appKymeraSetConcurrentState, kymera state %u", theKymera->state);
    DEBUG_LOG("appKymeraSetConcurrentState, kymera concurrent state %u", state);
    theKymera->concurrent_state = state;
}

static void appKymeraAncImplicitEnableOnScoStart(void)
{
    if (appConfigImplicitEnableAANCDuringSCO())
    {
        AncStateManager_ImplicitEnableReq();
    }   
}

static void appKymeraAncImplicitDisableOnScoStop(void)
{
    if (appConfigImplicitEnableAANCDuringSCO())
    {
        AncStateManager_ImplicitDisableReq();
    }   
}

static void appKymeraSetupForConcurrentCase(MessageId msg_id)
{

        switch (msg_id)
        {

            case KYMERA_INTERNAL_AANC_ENABLE:
                if(appKymeraIsBusy())
                {
                    /* We are entering concurreny state */
                    appKymeraSetConcurrentState(KYMERA_CON_STATE_WITH_ADAPTIVE_ANC);
                }
                break;

            case KYMERA_INTERNAL_AANC_DISABLE:
                /* donest matter if kymera is busy with anything else,we are no more in concurrent state */
                appKymeraSetConcurrentState(KYMERA_CON_STATE_NONE);
                break;
                
            /* AANC ->SCO ON */
            /* AANC -> Start Voice Capture */
            case KYMERA_INTERNAL_SCO_START:
            case KYMERA_INTERNAL_SCO_SLAVE_START:
            {
                /* If Adaptive Anc is On, then we have concurrenct state */
                if(appKymeraAdaptiveAncStarted())
                {
                    KymeraAdaptiveAnc_StopAancChainAndDisconnectAec();
                    /* If Adaptive ANC is already running, we need to update the concurrent state */
                    appKymeraSetConcurrentState(KYMERA_CON_STATE_WITH_ADAPTIVE_ANC);
                }
            }
            break;

            case KYMERA_INTERNAL_SCO_STOP:
            case KYMERA_INTERNAL_SCOFWD_RX_STOP:
            {
                /*Reconfigure the AEC chain in case Adaptive ANC is enabled*/
                KymeraAdaptiveAnc_ReconfigureAecForStandAlone();
                /* donest matter if kymera is busy with anything else,we are no more in concurrent state */
                appKymeraSetConcurrentState(KYMERA_CON_STATE_NONE);
                appKymeraAncImplicitDisableOnScoStop();
            }
            break;

            case KYMERA_INTERNAL_A2DP_STARTING:
            {
                /* If Adaptive Anc is On, then we have concurrenct state. We might hit this
                     case multiple times because A2DP starting message is split into mulitple
                     steps. So, need to disable adaptivity only once */
                if(appKymeraAdaptiveAncStarted() && !appKymeraInConcurrency())
                {
                    Kymera_DisableAdaptiveAncAdaptivity();
                    /* Probably Adaptive ANC is active, so update the concurrent state */
                    appKymeraSetConcurrentState(KYMERA_CON_STATE_WITH_ADAPTIVE_ANC);
                }
            }
            break;
            
            case KYMERA_INTERNAL_A2DP_STOP:
            {
                Kymera_EnableAdaptiveAncAdaptivity();
                /* donest matter if kymera is busy with anything else,we are no more in concurrent state */
                appKymeraSetConcurrentState(KYMERA_CON_STATE_NONE);
            }
            break;

            case KYMERA_INTERNAL_TONE_PROMPT_PLAY:
            {
                /* It could so happen that either standalone ANC or in concurrency with
                     SCO/A2DP, and there is tone/prompty play */
                if(appKymeraAdaptiveAncStarted() || appKymeraInConcurrency())
                {
                    Kymera_DisableAdaptiveAncAdaptivity();
                    /* Probably Adaptive ANC is active, so update the concurrent state */
                    appKymeraSetConcurrentState(KYMERA_CON_STATE_WITH_ADAPTIVE_ANC);
                }
            }
            break;

            case MESSAGE_STREAM_DISCONNECT:
            {
                /* This message comes for Tone/Prompt Stop, so we need to start adaptivity */
                
                /* Tone/Prompts could come in conjunction with A2DP/SCO */
                /* However in case of A2DP we should not enable adaptivity, this will be taken care
                    once A2DP stops */
                if(!appKymeraIsBusy() || !appKymeraIsBusyStreaming())
                {
                    Kymera_EnableAdaptiveAncAdaptivity();
                    appKymeraSetConcurrentState(KYMERA_CON_STATE_NONE);
                }
            }
            break;

            default:
                break;
        }
}

void appKymeraPromptPlay(FILE_INDEX prompt, promptFormat format, uint32 rate, rtime_t ttp,
                         bool interruptible, uint16 *client_lock, uint16 client_lock_mask)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    DEBUG_LOG("appKymeraPromptPlay, queue prompt %d, int %u", prompt, interruptible);

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_TONE_PROMPT_PLAY);
    message->tone = NULL;
    message->prompt = prompt;
    message->prompt_format = format;
    message->rate = rate;
    message->time_to_play = ttp;
    message->interruptible = interruptible;
    message->client_lock = client_lock;
    message->client_lock_mask = client_lock_mask;

    MessageCancelFirst(&theKymera->task, KYMERA_INTERNAL_PREPARE_FOR_PROMPT_TIMEOUT);
    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_TONE_PROMPT_PLAY, message, &theKymera->lock);
    theKymera->tone_count++;
}

void appKymeraTonePlay(const ringtone_note *tone, rtime_t ttp, bool interruptible,
                       uint16 *client_lock, uint16 client_lock_mask)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    DEBUG_LOG("appKymeraTonePlay, queue tone %p, int %u", tone, interruptible);

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_TONE_PROMPT_PLAY);
    message->tone = tone;
    message->prompt = FILE_NONE;
    message->rate = KYMERA_TONE_GEN_RATE;
    message->time_to_play = ttp;
    message->interruptible = interruptible;
    message->client_lock = client_lock;
    message->client_lock_mask = client_lock_mask;

    MessageCancelFirst(&theKymera->task, KYMERA_INTERNAL_PREPARE_FOR_PROMPT_TIMEOUT);
    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_TONE_PROMPT_PLAY, message, &theKymera->lock);
    theKymera->tone_count++;
}

void appKymeraTonePromptCancel(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    DEBUG_LOG("appKymeraTonePromptCancel");

    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_TONE_PROMPT_STOP, NULL, &theKymera->lock);
}

void appKymeraCancelA2dpStart(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    MessageCancelAll(&theKymera->task, KYMERA_INTERNAL_A2DP_START);
}

void appKymeraA2dpStart(uint16 *client_lock, uint16 client_lock_mask,
                        const a2dp_codec_settings *codec_settings,
                        uint32 max_bitrate,
                        int16 volume_in_db, uint8 master_pre_start_delay,
                        uint8 q2q_mode, aptx_adaptive_ttp_latencies_t nq2q_ttp)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOG("appKymeraA2dpStart, seid %u, lock %u, busy_lock %u, q2q %u", codec_settings->seid, theKymera->lock, theKymera->busy_lock, q2q_mode);

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_A2DP_START);
    message->lock = client_lock;
    message->lock_mask = client_lock_mask;
    message->codec_settings = *codec_settings;
    message->volume_in_db = volume_in_db;
    message->master_pre_start_delay = master_pre_start_delay;
    message->q2q_mode = q2q_mode;
    message->nq2q_ttp = nq2q_ttp;
    message->max_bitrate = max_bitrate;
    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_A2DP_START,
                             message,
                             &theKymera->lock);
}

void appKymeraA2dpStop(uint8 seid, Source source)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    MessageId mid = appA2dpIsSeidSource(seid) ? KYMERA_INTERNAL_A2DP_STOP_FORWARDING :
                                                KYMERA_INTERNAL_A2DP_STOP;
    DEBUG_LOG("appKymeraA2dpStop, seid %u", seid);
    
    /*Cancel any pending KYMERA_INTERNAL_A2DP_AUDIO_SYNCHRONISED message.
      A2DP might have been stopped while Audio Synchronization is still incomplete, 
      in which case this timed message needs to be cancelled.
     */
    MessageCancelAll(&theKymera->task, KYMERA_INTERNAL_A2DP_AUDIO_SYNCHRONISED);

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_A2DP_STOP);
    message->seid = seid;
    message->source = source;
    MessageSendConditionally(&theKymera->task, mid, message, &theKymera->lock);
}

void appKymeraA2dpSetVolume(int16 volume_in_db)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOG("appKymeraA2dpSetVolume, volume %d", volume_in_db);

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_A2DP_SET_VOL);
    message->volume_in_db = volume_in_db;
    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_A2DP_SET_VOL, message, &theKymera->lock);
}

void appKymeraScoStartForwarding(Sink forwarding_sink, bool enable_mic_fwd)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOG("appKymeraScoStartForwarding, queue sink %p, state %u", forwarding_sink, appKymeraGetState());
    PanicNull(forwarding_sink);
    
    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_SCO_START_FORWARDING_TX);
    message->forwarding_sink = forwarding_sink;
    message->enable_mic_fwd = enable_mic_fwd;
    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_SCO_START_FORWARDING_TX, message, &theKymera->lock);
}

void appKymeraScoStopForwarding(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOG("appKymeraScoStopForwarding, state %u", appKymeraGetState());

    if (!appKymeraHandleInternalScoForwardingStopTx())
        MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_SCO_STOP_FORWARDING_TX, NULL, &theKymera->lock);
}

kymera_chain_handle_t appKymeraScoCreateChain(const appKymeraScoChainInfo *info)
{    
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOG("appKymeraCreateScoChain, mode %u, sco_fwd %u, mic_cfg %u, cvc_2_mic %u, rate %u",
               info->mode, info->sco_fwd, info->mic_fwd, info->mic_cfg, info->rate);

    theKymera->sco_info = info;

    /* Create chain and return handle */
    theKymera->chainu.sco_handle = ChainCreate(info->chain);

    /* Configure DSP power mode appropriately for SCO chain */
    appKymeraConfigureDspPowerMode();
    
    return theKymera->chainu.sco_handle;
}

static void appKymeraScoStartHelper(Sink audio_sink, const appKymeraScoChainInfo *info, uint8 wesco,
                                    int16 volume_in_db, uint8 pre_start_delay, bool conditionally)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_SCO_START);
    PanicNull(audio_sink);

    message->audio_sink = audio_sink;
    message->wesco      = wesco;
    message->volume_in_db     = volume_in_db;
    message->pre_start_delay = pre_start_delay;
    message->sco_info   = info;
    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_SCO_START, message, conditionally ? &theKymera->lock : NULL);
}

bool appKymeraScoStart(Sink audio_sink, appKymeraScoMode mode, bool *allow_scofwd, bool *allow_micfwd,
                       uint8 wesco, int16 volume_in_db, uint8 pre_start_delay)
{     
    uint8 mic_cfg = Kymera_GetNumberOfMics();
    const appKymeraScoChainInfo *info = appKymeraScoFindChain(appKymeraScoChainTable,
                                                              mode, *allow_scofwd, *allow_micfwd,
                                                              mic_cfg);
    if (!info)
        info = appKymeraScoFindChain(appKymeraScoChainTable,
                                     mode, FALSE, FALSE, mic_cfg);
    
    if (info)
    {
        DEBUG_LOG("appKymeraScoStart, queue sink 0x%x", audio_sink);
        *allow_scofwd = info->sco_fwd;
        *allow_micfwd = info->mic_fwd;

        if (audio_sink)
        {
            DEBUG_LOG("appKymeraScoStart, queue sink 0x%x", audio_sink);
            appKymeraScoStartHelper(audio_sink, info, wesco, volume_in_db, pre_start_delay, TRUE);
            return TRUE;
        }
        else
        {
            DEBUG_LOG("appKymeraScoStart, invalid sink");
            return FALSE;
        }
    }
    else
    {
        DEBUG_LOG("appKymeraScoStart, failed to find suitable SCO chain");
        return FALSE;
    }
}

void appKymeraScoStop(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOG("appKymeraScoStop");

    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_SCO_STOP, NULL, &theKymera->lock);
}

void appKymeraScoSlaveStartHelper(Source link_source, int16 volume_in_db, const appKymeraScoChainInfo *info, uint16 delay)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOG("appKymeraScoSlaveStartHelper, delay %u", delay);

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_SCO_SLAVE_START);
    message->link_source = link_source;
    message->volume_in_db = volume_in_db;
    message->sco_info = info;
    message->pre_start_delay = delay;
    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_SCO_SLAVE_START, message, &theKymera->lock);
}

void appKymeraScoSlaveStart(Source link_source, int16 volume_in_db, bool allow_micfwd, uint16 pre_start_delay)
{
    DEBUG_LOG("appKymeraScoSlaveStart, source 0x%x", link_source);
    uint8 mic_cfg = Kymera_GetNumberOfMics();
    const appKymeraScoChainInfo *info = appKymeraScoFindChain(appKymeraScoSlaveChainTable,
                                                              SCO_WB, FALSE, allow_micfwd,
                                                              mic_cfg);

    
    PanicNull(link_source);
    appKymeraScoSlaveStartHelper(link_source, volume_in_db, info, pre_start_delay);
}

void appKymeraScoSlaveStop(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOG("appKymeraScoSlaveStop");

    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_SCOFWD_RX_STOP, NULL, &theKymera->lock);
}

void appKymeraScoSetVolume(int16 volume_in_db)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    DEBUG_LOG("appKymeraScoSetVolume msg, vol %u", volume_in_db);

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_SCO_SET_VOL);
    message->volume_in_db = volume_in_db;

    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_SCO_SET_VOL, message, &theKymera->lock);
}

void appKymeraScoMicMute(bool mute)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    DEBUG_LOG("appKymeraScoMicMute msg, mute %u", mute);

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_SCO_MIC_MUTE);
    message->mute = mute;
    MessageSend(&theKymera->task, KYMERA_INTERNAL_SCO_MIC_MUTE, message);
}

void appKymeraScoUseLocalMic(void)
{
    /* Only do something if both EBs support MIC forwarding */
    if (appConfigMicForwardingEnabled())
    {
        kymeraTaskData *theKymera = KymeraGetTaskData();

        DEBUG_LOG("appKymeraScoUseLocalMic");

        MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_MICFWD_LOCAL_MIC, NULL, &theKymera->lock);
    }
}

void appKymeraScoUseRemoteMic(void)
{
    /* Only do something if both EBs support MIC forwarding */
    if (appConfigMicForwardingEnabled())
    {
        kymeraTaskData *theKymera = KymeraGetTaskData();

        DEBUG_LOG("appKymeraScoUseRemoteMic");

        MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_MICFWD_REMOTE_MIC, NULL, &theKymera->lock);
    }
}

/*! \brief Notify Kymera to update registered clients. */
static void appKymera_MsgRegisteredClients( MessageId id, uint16 info )
{    
    kymeraTaskData *theKymera = KymeraGetTaskData();

    if(theKymera->client_tasks) /* Check if any of client registered */
    {
        MESSAGE_MAKE(ind, kymera_aanc_event_msg_t);
        ind->info = info;

        TaskList_MessageSendWithSize(theKymera->client_tasks, id, ind, sizeof(ind));
                      
    }
}

static void kymera_dsp_msg_handler(MessageFromOperator *op_msg)
{
    uint16 msg_id, event_id;
       

    msg_id = op_msg->message[KYMERA_OP_MSG_WORD_MSG_ID];
    event_id = op_msg->message[KYMERA_OP_MSG_WORD_EVENT_ID];
    
    switch (msg_id)
    {
        case KYMERA_OP_MSG_ID_TONE_END:
        {
            DEBUG_LOG("KYMERA_OP_MSG_ID_TONE_END");
            

            PanicFalse(op_msg->len == KYMERA_OP_MSG_LEN);

            appKymeraTonePromptStop();
            Kymera_LatencyManagerHandleToneEnd();
            /* In case of concurrency, the ringtone gerenator tone end message can be considered
                        same as MESSAGE_STREAM_DISCONNECT, so that we can enable adaptivity */
            appKymeraSetupForConcurrentCase(MESSAGE_STREAM_DISCONNECT);
        }
        break;

        case KYMERA_OP_MSG_ID_AANC_EVENT_TRIGGER:
        {
            DEBUG_LOG("KYMERA_OP_MSG_ID_AANC_EVENT_TRIGGER Event %d",event_id);
            if (event_id ==  KYMERA_AANC_EVENT_ED_ACTIVE )
            {
                DEBUG_LOG("KYMERA_AANC_ED_ACTIVE_TRIGGER_IND");
                appKymera_MsgRegisteredClients(KYMERA_AANC_ED_ACTIVE_TRIGGER_IND,
                                               op_msg->message[KYMERA_OP_MSG_WORD_PAYLOAD_0]);
            }
            else if (event_id ==  KYMERA_AANC_EVENT_ED_INACTIVE_GAIN_UNCHANGED )
            {
                DEBUG_LOG("KYMERA_AANC_ED_INACTIVE_TRIGGER_IND");
                appKymera_MsgRegisteredClients(KYMERA_AANC_ED_INACTIVE_TRIGGER_IND,
                                               op_msg->message[KYMERA_OP_MSG_WORD_PAYLOAD_0]);
            }
            else if (event_id ==  KYMERA_AANC_EVENT_QUIET_MODE )
            {
                DEBUG_LOG("KYMERA_AANC_QUIET_MODE_TRIGGER_IND");
                appKymera_MsgRegisteredClients(KYMERA_AANC_QUIET_MODE_TRIGGER_IND,
                                               KYMERA_OP_MSG_WORD_PAYLOAD_NA);
            }
            else
            {
                /* ignore */
            }
        }
        break;

        case KYMERA_OP_MSG_ID_AANC_EVENT_CLEAR:
        {
            DEBUG_LOG("KYMERA_OP_MSG_ID_AANC_EVENT_CLEAR Event %d", event_id);
 
            if (event_id ==  KYMERA_AANC_EVENT_ED_ACTIVE )
            {
                DEBUG_LOG("KYMERA_AANC_ED_ACTIVE_CLEAR_IND");
                appKymera_MsgRegisteredClients(KYMERA_AANC_ED_ACTIVE_CLEAR_IND,
                                               op_msg->message[KYMERA_OP_MSG_WORD_PAYLOAD_0]);
            }
            else if (event_id ==  KYMERA_AANC_EVENT_ED_INACTIVE_GAIN_UNCHANGED )
            {
                DEBUG_LOG("KYMERA_AANC_ED_INACTIVE_CLEAR_IND");
                appKymera_MsgRegisteredClients(KYMERA_AANC_ED_INACTIVE_CLEAR_IND,
                                               op_msg->message[KYMERA_OP_MSG_WORD_PAYLOAD_0]);
            }
            else if (event_id ==  KYMERA_AANC_EVENT_QUIET_MODE )
            {
                DEBUG_LOG("KYMERA_AANC_QUIET_MODE_CLEAR_IND");
                appKymera_MsgRegisteredClients(KYMERA_AANC_QUIET_MODE_CLEAR_IND,
                                               KYMERA_OP_MSG_WORD_PAYLOAD_NA);
                
            }
            else
            {
                /* ignore */
            }
        }
        break;

        default:
        break;
    }
}

void appKymeraProspectiveDspPowerOn(void)
{
    switch (appKymeraGetState())
    {
        case KYMERA_STATE_IDLE:
        case KYMERA_STATE_A2DP_STARTING_A:
        case KYMERA_STATE_A2DP_STARTING_B:
        case KYMERA_STATE_A2DP_STARTING_C:
        case KYMERA_STATE_A2DP_STARTING_SLAVE:
        case KYMERA_STATE_A2DP_STREAMING:
        case KYMERA_STATE_A2DP_STREAMING_WITH_FORWARDING:
        case KYMERA_STATE_SCO_ACTIVE:
        case KYMERA_STATE_SCO_ACTIVE_WITH_FORWARDING:
        case KYMERA_STATE_SCO_SLAVE_ACTIVE:
        case KYMERA_STATE_TONE_PLAYING:
        case KYMERA_STATE_USB_AUDIO_ACTIVE:
        case KYMERA_STATE_USB_VOICE_ACTIVE:
            if (MessageCancelFirst(KymeraGetTask(), KYMERA_INTERNAL_PROSPECTIVE_POWER_OFF))
            {
                /* Already prospectively on, just re-start off timer */
                DEBUG_LOG("appKymeraProspectiveDspPowerOn already on, restart timer");
            }
            else
            {
                DEBUG_LOG("appKymeraProspectiveDspPowerOn starting");
                appPowerPerformanceProfileRequest();
                OperatorsFrameworkEnable();
                appPowerPerformanceProfileRelinquish();
            }
            MessageSendLater(KymeraGetTask(), KYMERA_INTERNAL_PROSPECTIVE_POWER_OFF, NULL,
                             appConfigProspectiveAudioOffTimeout());
        break;

        default:
        break;
    }
}

/* Handle KYMERA_INTERNAL_PROSPECTIVE_POWER_OFF - switch off DSP again */
static void appKymeraHandleProspectivePowerOff(void)
{
    DEBUG_LOG("appKymeraHandleProspectivePowerOff");
    OperatorsFrameworkDisable();
}

static void kymera_msg_handler(Task task, MessageId id, Message msg)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    UNUSED(task);
    
    switch (id)
    {
        case MESSAGE_FROM_OPERATOR:
            kymera_dsp_msg_handler((MessageFromOperator *)msg);
        break;

        case MESSAGE_SOURCE_EMPTY:
        break;

        case MESSAGE_STREAM_DISCONNECT:
        {
            DEBUG_LOG("appKymera MESSAGE_STREAM_DISCONNECT");
#ifdef INCLUDE_MIRRORING
            const MessageStreamDisconnect *msd = msg;
            if (msd->source == theKymera->sync_info.source ||
                msd->source == MirrorProfile_GetA2dpAudioSyncTransportSource())
            {
                /* This is the stream associated with the TWM audio sync stream,
                   not the tone stream, do not stop playing the tone. */
                break;
            }
#endif
            appKymeraTonePromptStop();
            appKymeraSetupForConcurrentCase(MESSAGE_STREAM_DISCONNECT);
        }
        break;

        case KYMERA_INTERNAL_A2DP_START:
        {
            const KYMERA_INTERNAL_A2DP_START_T *m = (const KYMERA_INTERNAL_A2DP_START_T *)msg;
            uint8 seid = m->codec_settings.seid;

            /* Check if we are busy (due to other chain in use) */
            if (!appA2dpIsSeidSource(seid) && theKymera->busy_lock)
            {
               /* Re-send message blocked on busy_lock */
                MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_A2DP_START);
                *message = *m;
                MessageSendConditionally(&theKymera->task, id, message, &theKymera->busy_lock);
                break;
            }

            /* If there is no pre-start delay, or during the pre-start delay, the
            start can be cancelled if there is a stop on the message queue */
            MessageId mid = appA2dpIsSeidSource(seid) ? KYMERA_INTERNAL_A2DP_STOP_FORWARDING :
                                                        KYMERA_INTERNAL_A2DP_STOP;
            if (MessageCancelFirst(&theKymera->task, mid))
            {
                /* A stop on the queue was cancelled, clear the starter's lock
                and stop starting */
                DEBUG_LOG("appKymera not starting due to queued stop, seid=%u", seid);
                if (m->lock)
                {
                    *m->lock &= ~m->lock_mask;
                }
                /* Also clear kymera's lock, since no longer starting */
                appKymeraClearStartingLock(theKymera);
                break;
            }
            if (m->master_pre_start_delay)
            {
                /* Send another message before starting kymera. */
                MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_A2DP_START);
                *message = *m;
                --message->master_pre_start_delay;
                MessageSend(&theKymera->task, id, message);
                appKymeraSetStartingLock(theKymera);
                break;
            }
        }
        // fallthrough (no message cancelled, zero master_pre_start_delay)
        case KYMERA_INTERNAL_A2DP_STARTING:
        {
            const KYMERA_INTERNAL_A2DP_START_T *m = (const KYMERA_INTERNAL_A2DP_START_T *)msg;
            appKymeraSetupForConcurrentCase(KYMERA_INTERNAL_A2DP_STARTING);
            if (appKymeraHandleInternalA2dpStart(m))
            {
                /* Start complete, clear locks. */
                appKymeraClearStartingLock(theKymera);
                if (m->lock)
                {
                    *m->lock &= ~m->lock_mask;
                }
            }
            else
            {
                /* Start incomplete, send another message. */
                MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_A2DP_START);
                *message = *m;
                MessageSend(&theKymera->task, KYMERA_INTERNAL_A2DP_STARTING, message);
                appKymeraSetStartingLock(theKymera);
            }
        }
        break;

        case KYMERA_INTERNAL_A2DP_STOP:
        case KYMERA_INTERNAL_A2DP_STOP_FORWARDING:
            appKymeraHandleInternalA2dpStop(msg);
            appKymeraSetupForConcurrentCase(KYMERA_INTERNAL_A2DP_STOP);
        break;

        case KYMERA_INTERNAL_A2DP_SET_VOL:
        {
            KYMERA_INTERNAL_A2DP_SET_VOL_T *m = (KYMERA_INTERNAL_A2DP_SET_VOL_T *)msg;
            appKymeraHandleInternalA2dpSetVolume(m->volume_in_db);
        }
        break;

        case KYMERA_INTERNAL_SCO_START:
        {
            const KYMERA_INTERNAL_SCO_START_T *m = (const KYMERA_INTERNAL_SCO_START_T *)msg;

            if (theKymera->busy_lock)
            {
                MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_SCO_START);
                *message = *m;
               /* Another audio chain is active, re-send message blocked on busy_lock */
                MessageSendConditionally(&theKymera->task, id, message, &theKymera->busy_lock);
                break;
            }

            if (m->pre_start_delay)
            {
                /* Resends are sent unconditonally, but the lock is set blocking
                   other new messages */
                appKymeraSetStartingLock(KymeraGetTaskData());
                appKymeraScoStartHelper(m->audio_sink, m->sco_info, m->wesco, m->volume_in_db,
                                        m->pre_start_delay - 1, FALSE);
            }
            else
            {
                /* Check if there is already a concurrent use-case active. If yes, take appropriate step */
                appKymeraSetupForConcurrentCase(KYMERA_INTERNAL_SCO_START);
                if(appKymeraHandleInternalScoStart(m->audio_sink, m->sco_info, m->wesco, m->volume_in_db))
                {
                    appKymeraAncImplicitEnableOnScoStart();
                }
                else
                {
                    /* SCO failed, so exit from concurrenct usecase */
                    appKymeraSetupForConcurrentCase(KYMERA_INTERNAL_SCO_STOP);
                }
                appKymeraClearStartingLock(KymeraGetTaskData());
            }
        }
        break;

        case KYMERA_INTERNAL_SCO_START_FORWARDING_TX:
        {
            const KYMERA_INTERNAL_SCO_START_FORWARDING_TX_T *m =
                    (const KYMERA_INTERNAL_SCO_START_FORWARDING_TX_T*)msg;
            appKymeraHandleInternalScoForwardingStartTx(m->forwarding_sink);
        }
        break;

        case KYMERA_INTERNAL_SCO_STOP_FORWARDING_TX:
        {
            appKymeraHandleInternalScoForwardingStopTx();
        }
        break;

        case KYMERA_INTERNAL_SCO_SET_VOL:
        {
            KYMERA_INTERNAL_SCO_SET_VOL_T *m = (KYMERA_INTERNAL_SCO_SET_VOL_T *)msg;
            appKymeraHandleInternalScoSetVolume(m->volume_in_db);
        }
        break;

        case KYMERA_INTERNAL_SCO_MIC_MUTE:
        {
            KYMERA_INTERNAL_SCO_MIC_MUTE_T *m = (KYMERA_INTERNAL_SCO_MIC_MUTE_T *)msg;
            appKymeraHandleInternalScoMicMute(m->mute);
        }
        break;


        case KYMERA_INTERNAL_SCO_STOP:
        {
            appKymeraHandleInternalScoStop();
            appKymeraSetupForConcurrentCase(KYMERA_INTERNAL_SCO_STOP);            
        }
        break;

        case KYMERA_INTERNAL_SCO_SLAVE_START:
        {
            const KYMERA_INTERNAL_SCO_SLAVE_START_T *m = (const KYMERA_INTERNAL_SCO_SLAVE_START_T *)msg;
            if (theKymera->busy_lock)
            {
               /* Re-send message blocked on busy_lock */
                MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_SCO_SLAVE_START);
                *message = *m;
                MessageSendConditionally(&theKymera->task, id, message, &theKymera->busy_lock);
            }

#if 0
            /* If we are not idle (a pre-requisite) and this message can be delayed,
               then re-send it. The normal situation is message delays when stopping
               A2DP/AV. That is calls were issued in the right order to stop A2DP then
               start SCO receive but the number of messages required for each were
               different, leading the 2nd action to complete 1st. */
            if (   start_req->pre_start_delay
                && appKymeraGetState() != KYMERA_STATE_IDLE)
            {
                DEBUG_LOG("appKymeraHandleInternalScoForwardingStartRx, re-queueing.");
                appKymeraScoFwdStartReceiveHelper(start_req->link_source, start_req->volume,
                                                  start_req->sco_info,
                                                  start_req->pre_start_delay - 1);
                return;
            }
#endif

            else
            {
                /* Check if there is already a concurrent use-case active. If yes, take appropriate step */
                appKymeraSetupForConcurrentCase(KYMERA_INTERNAL_SCO_SLAVE_START);
                if(!appKymeraHandleInternalScoSlaveStart(m->link_source, m->sco_info, m->volume_in_db))
                {
                    /* Slave failed to start SCO, so we dont have any concurrent use-case */
                    appKymeraSetupForConcurrentCase(KYMERA_INTERNAL_SCOFWD_RX_STOP);
                }
            }
        }
        break;

        case KYMERA_INTERNAL_SCOFWD_RX_STOP:
        {
            appKymeraHandleInternalScoSlaveStop();
            /*Reconfigure the AEC chain in case Adaptive ANC is enabled*/
            appKymeraSetupForConcurrentCase(KYMERA_INTERNAL_SCOFWD_RX_STOP);
        }
        break;

        case KYMERA_INTERNAL_TONE_PROMPT_PLAY:
            appKymeraSetupForConcurrentCase(KYMERA_INTERNAL_TONE_PROMPT_PLAY);
            appKymeraHandleInternalTonePromptPlay(msg);
        break;

        case KYMERA_INTERNAL_TONE_PROMPT_STOP:
        case KYMERA_INTERNAL_PREPARE_FOR_PROMPT_TIMEOUT:
            appKymeraTonePromptStop();
        break;

        case KYMERA_INTERNAL_MICFWD_LOCAL_MIC:
            appKymeraSwitchSelectMic(MIC_SELECTION_LOCAL);
            break;

        case KYMERA_INTERNAL_MICFWD_REMOTE_MIC:
            appKymeraSwitchSelectMic(MIC_SELECTION_REMOTE);
            break;

        case KYMERA_INTERNAL_ANC_TUNING_START:
        {
            const KYMERA_INTERNAL_ANC_TUNING_START_T *m = (const KYMERA_INTERNAL_ANC_TUNING_START_T *)msg;
            KymeraAnc_TuningCreateChain(m->usb_rate);
        }
        break;

        case KYMERA_INTERNAL_ANC_TUNING_STOP:
            KymeraAnc_TuningDestroyChain();
        break;

        case KYMERA_INTERNAL_ADAPTIVE_ANC_TUNING_START:
        {
            const KYMERA_INTERNAL_ADAPTIVE_ANC_TUNING_START_T *m = (const KYMERA_INTERNAL_ADAPTIVE_ANC_TUNING_START_T *)msg;
            kymeraAdaptiveAnc_CreateAdaptiveAncTuningChain(m->usb_rate);
        }
        break;

        case KYMERA_INTERNAL_ADAPTIVE_ANC_TUNING_STOP:
            kymeraAdaptiveAnc_DestroyAdaptiveAncTuningChain();
        break;

        case KYMERA_INTERNAL_AANC_ENABLE:
            appKymeraSetupForConcurrentCase(KYMERA_INTERNAL_AANC_ENABLE);
            KymeraAdaptiveAnc_Enable((const KYMERA_INTERNAL_AANC_ENABLE_T *)msg);
        break;

        case KYMERA_INTERNAL_AANC_DISABLE:
            KymeraAdaptiveAnc_Disable();
            appKymeraSetupForConcurrentCase(KYMERA_INTERNAL_AANC_DISABLE);
        break;

        case KYMERA_INTERNAL_PROSPECTIVE_POWER_OFF:
            appKymeraHandleProspectivePowerOff();
        break;

        case KYMERA_INTERNAL_AUDIO_SS_DISABLE:
            DEBUG_LOG("appKymera KYMERA_INTERNAL_AUDIO_SS_DISABLE");
            OperatorsFrameworkDisable();
            break;

#ifdef INCLUDE_MIRRORING
        case MESSAGE_SINK_AUDIO_SYNCHRONISED:
        case MESSAGE_SOURCE_AUDIO_SYNCHRONISED:
            appKymeraA2dpHandleAudioSyncStreamInd(id, msg);
        break;

        case KYMERA_INTERNAL_A2DP_DATA_SYNC_IND_TIMEOUT:
            appKymeraA2dpHandleDataSyncIndTimeout();
        break;

        case KYMERA_INTERNAL_A2DP_MESSAGE_MORE_DATA_TIMEOUT:
            appKymeraA2dpHandleMessageMoreDataTimeout();
        break;

        case KYMERA_INTERNAL_A2DP_AUDIO_SYNCHRONISED:
             appKymeraA2dpHandleAudioSynchronisedInd();
        break;

        case MESSAGE_MORE_DATA:
            appKymeraA2dpHandleMessageMoreData((const MessageMoreData *)msg);
        break;

        case KYMERA_INTERNAL_LATENCY_CHECK_TIMEOUT:
            Kymera_DynamicLatencyHandleLatencyTimeout();
        break;

        case KYMERA_INTERNAL_LATENCY_MANAGER_MUTE_COMPLETE:
            Kymera_LatencyManagerHandleMuteComplete();
        break;

        case KYMERA_INTERNAL_LATENCY_MANAGER_MUTE:
            Kymera_LatencyManagerHandleMute();
        break;

        case MIRROR_PROFILE_A2DP_STREAM_ACTIVE_IND:
            Kymera_LatencyManagerHandleMirrorA2dpStreamActive();
        break;

        case MIRROR_PROFILE_A2DP_STREAM_INACTIVE_IND:
            Kymera_LatencyManagerHandleMirrorA2dpStreamInactive();
        break;

#endif /* INCLUDE_MIRRORING */

        case KYMERA_INTERNAL_AEC_LEAKTHROUGH_CREATE_STANDALONE_CHAIN:
        {
            appKymeraSetState(KYMERA_STATE_STANDALONE_LEAKTHROUGH);
            Kymera_CreateLeakthroughChain();
        }
        break;
        case KYMERA_INTERNAL_AEC_LEAKTHROUGH_DESTROY_STANDALONE_CHAIN:
        {
            Kymera_DestroyLeakthroughChain();
            appKymeraSetState(KYMERA_STATE_IDLE);
        }
        break;
        case KYMERA_INTERNAL_LEAKTHROUGH_SIDETONE_ENABLE:
        {
            if(AecLeakthrough_IsLeakthroughEnabled())
            {
                Kymera_LeakthroughSetupSTGain();
            }
        }
        break;
        case KYMERA_INTERNAL_LEAKTHROUGH_SIDETONE_GAIN_RAMPUP:
        {
            Kymera_LeakthroughStepupSTGain();
        }
        break;
        case KYMERA_INTERNAL_WIRED_ANALOG_AUDIO_START:
        {
            const KYMERA_INTERNAL_WIRED_ANALOG_AUDIO_START_T *m = (const KYMERA_INTERNAL_WIRED_ANALOG_AUDIO_START_T *)msg;

            if (theKymera->busy_lock)
            {
                MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_WIRED_ANALOG_AUDIO_START);
                *message = *m;
                /* Another audio chain is active, re-send message blocked on busy_lock */
                MessageSendConditionally(&theKymera->task, id, message, &theKymera->busy_lock);
                break;
            }
            /* Call the function in Kymera_Wired_Analog_Audio to start the audio chain */
            KymeraWiredAnalog_StartPlayingAudio(m);
        }
        break;
        case KYMERA_INTERNAL_WIRED_ANALOG_AUDIO_STOP:
        {
            /*Call the function in Kymera_Wired_Analog_Audio to stop the audio chain */
            KymeraWiredAnalog_StopPlayingAudio();
        }
        break;
        case KYMERA_INTERNAL_USB_AUDIO_START:
        {
           KymeraUsbAudio_Start((KYMERA_INTERNAL_USB_AUDIO_START_T *)msg);
        }
        break;
        case KYMERA_INTERNAL_USB_AUDIO_STOP:
        {
            KymeraUsbAudio_Stop((KYMERA_INTERNAL_USB_AUDIO_STOP_T *)msg);
        }
        break;
        case KYMERA_INTERNAL_USB_AUDIO_SET_VOL:
        {
            KYMERA_INTERNAL_USB_AUDIO_SET_VOL_T *m = (KYMERA_INTERNAL_USB_AUDIO_SET_VOL_T *)msg;
            KymeraUsbAudio_SetVolume(m->volume_in_db);
        }
        break;
        case KYMERA_INTERNAL_USB_VOICE_START:
        {
           KymeraUsbVoice_Start((KYMERA_INTERNAL_USB_VOICE_START_T *)msg);
        }
        break;
        case KYMERA_INTERNAL_USB_VOICE_STOP:
        {
            KymeraUsbVoice_Stop((KYMERA_INTERNAL_USB_VOICE_STOP_T *)msg);
        }
        break;
        case KYMERA_INTERNAL_USB_VOICE_SET_VOL:
        {
            KYMERA_INTERNAL_USB_VOICE_SET_VOL_T *m = (KYMERA_INTERNAL_USB_VOICE_SET_VOL_T *)msg;
            KymeraUsbVoice_SetVolume(m->volume_in_db);
        }
        break;
        case KYMERA_INTERNAL_USB_VOICE_MIC_MUTE:
        {
            KYMERA_INTERNAL_USB_VOICE_MIC_MUTE_T *m = (KYMERA_INTERNAL_USB_VOICE_MIC_MUTE_T *)msg;
            KymeraUsbVoice_MicMute(m->mute);
        }
        break;

        default:
            break;
    }
}

bool appKymeraInit(Task init_task)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    memset(theKymera, 0, sizeof(*theKymera));
    theKymera->task.handler = kymera_msg_handler;
    theKymera->state = KYMERA_STATE_IDLE;
    theKymera->output_rate = 0;
    theKymera->lock = theKymera->busy_lock = 0;
    theKymera->a2dp_seid = AV_SEID_INVALID;
    theKymera->tone_count = 0;
    theKymera->q2q_mode = 0;
    theKymera->enable_left_right_mix = TRUE;
    appKymeraExternalAmpSetup();
    DEBUG_LOG("appKymeraInit number of bundles %d", bundle_config->number_of_capability_bundles);
    if (bundle_config && bundle_config->number_of_capability_bundles > 0)
        ChainSetDownloadableCapabilityBundleConfig(bundle_config);
    theKymera->mic = MIC_SELECTION_LOCAL;
    Microphones_Init();

#ifdef INCLUDE_ANC_PASSTHROUGH_SUPPORT_CHAIN
    theKymera->anc_passthough_operator = INVALID_OPERATOR;
#endif

#ifdef ENABLE_AEC_LEAKTHROUGH
    Kymera_LeakthroughInit();
#endif
    theKymera->client_tasks = TaskList_Create();

#ifdef INCLUDE_MIC_CONCURRENCY
    Kymera_ScoInit();
    Kymera_ScoForwardingInit();
#endif
    Kymera_VaInit();

    Kymera_LatencyManagerInit(FALSE, 0);
    Kymera_DynamicLatencyInit();
    MirrorProfile_ClientRegister(&theKymera->task);
    UNUSED(init_task);
    return TRUE;
}

bool Kymera_IsIdle(void)
{
    return (KymeraGetTaskData()->state == KYMERA_STATE_IDLE);
}

void Kymera_ClientRegister(Task client_task)
{
    DEBUG_LOG("Kymera_ClientRegister");
    kymeraTaskData *kymera_sm = KymeraGetTaskData();
    TaskList_AddTask(kymera_sm->client_tasks, client_task);
}

void Kymera_ClientUnregister(Task client_task)
{
    DEBUG_LOG("Kymera_ClientRegister");
    kymeraTaskData *kymera_sm = KymeraGetTaskData();
    TaskList_RemoveTask(kymera_sm->client_tasks, client_task);
}


void Kymera_SetBundleConfig(const capability_bundle_config_t *config)
{
    bundle_config = config;
}

void Kymera_SetChainConfigs(const kymera_chain_configs_t *configs)
{
    chain_configs = configs;
}

const kymera_chain_configs_t *Kymera_GetChainConfigs(void)
{
    PanicNull((void *)chain_configs);
    return chain_configs;
}

void Kymera_SetScoChainTable(const appKymeraScoChainInfo *info)
{
    appKymeraScoChainTable = info;
}

void Kymera_SetScoSlaveChainTable(const appKymeraScoChainInfo *info)
{
    appKymeraScoSlaveChainTable = info;
}

void Kymera_RegisterNotificationListener(Task task)
{
    kymeraTaskData *the_kymera = KymeraGetTaskData();
    /* Only one listener is supported, if more is needed change implementation to use the task list*/
    PanicNotNull(the_kymera->listener);
    the_kymera->listener = task;
}

void Kymera_StartWiredAnalogAudio(int16 volume_in_db, uint32 rate, uint32 min_latency, uint32 max_latency, uint32 target_latency)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_WIRED_ANALOG_AUDIO_START);
    DEBUG_LOG("Kymera_StartWiredAnalogAudio");
    
    message->rate      = rate;
    message->volume_in_db     = volume_in_db;
    message->min_latency = min_latency;
    message->max_latency = max_latency;
    message->target_latency = target_latency;
    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_WIRED_ANALOG_AUDIO_START, message, &theKymera->lock);
}

void Kymera_StopWiredAnalogAudio(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOG("Kymera_StopWiredAnalogAudio");
    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_WIRED_ANALOG_AUDIO_STOP, NULL, &theKymera->lock);
}
void appKymeraUsbAudioStart(uint8 channels, uint8 frame_size, Source src,
                            int16 volume_in_db, uint32 rate, uint32 min_latency,
                            uint32 max_latency, uint32 target_latency)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_USB_AUDIO_START);
    message->channels = channels;
    message->frame_size = frame_size;
    message->sample_freq = rate;
    message->spkr_src = src;
    message->volume_in_db = volume_in_db;
    message->min_latency_ms = min_latency;
    message->max_latency_ms = max_latency;
    message->target_latency_ms = target_latency;

    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_USB_AUDIO_START, message, &theKymera->lock);
}

void appKymeraUsbAudioStop(Source usb_src)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_USB_AUDIO_STOP);
    message->source = usb_src;

    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_USB_AUDIO_STOP, message, &theKymera->lock);
}

void appKymeraUsbAudioSetVolume(int16 volume_in_db)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOG("appKymeraUsbAudioSetVolume, volume %u", volume_in_db);

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_USB_AUDIO_SET_VOL);
    message->volume_in_db = volume_in_db;
    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_USB_AUDIO_SET_VOL, message, &theKymera->lock);
}

void appKymeraUsbVoiceStart(Sink mic_sink, usb_voice_mode_t mode,
                            uint32 sample_rate, Source spkr_src,
                            int16 volume_in_db, uint32 min_latency,
                            uint32 max_latency, uint32 target_latency)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_USB_VOICE_START);
    message->mic_sink = mic_sink;
    message->mode = mode;
    message->sampleFreq = sample_rate;
    message->spkr_src = spkr_src;
    message->volume = volume_in_db;
    message->min_latency_ms = min_latency;
    message->max_latency_ms = max_latency;
    message->target_latency_ms = target_latency;

    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_USB_VOICE_START, message, &theKymera->lock);
}

void appKymeraUsbVoiceStop(Source spkr_src, Sink mic_sink)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_USB_VOICE_STOP);
    message->spkr_src = spkr_src;
    message->mic_sink = mic_sink;

    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_USB_VOICE_STOP, message, &theKymera->lock);
}

void appKymeraUsbVoiceSetVolume(int16 volume_in_db)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_USB_VOICE_SET_VOL);
    message->volume_in_db = volume_in_db;

    MessageSendConditionally(&theKymera->task, KYMERA_INTERNAL_USB_VOICE_SET_VOL, message, &theKymera->lock);
}

void appKymeraUsbVoiceMicMute(bool mute)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_USB_VOICE_MIC_MUTE);
    message->mute = mute;
    MessageSend(&theKymera->task, KYMERA_INTERNAL_USB_VOICE_MIC_MUTE, message);
}

Transform Kymera_GetA2dpMediaStreamTransform(void)
{
    Transform trans = (Transform)0;

#ifdef INCLUDE_MIRRORING
    kymeraTaskData *theKymera = KymeraGetTaskData();

    if (theKymera->state == KYMERA_STATE_A2DP_STREAMING ||
        theKymera->state == KYMERA_STATE_A2DP_STREAMING_WITH_FORWARDING)
    {
        trans = theKymera->hashu.hash;
    }
#endif

    return trans;
}

void Kymera_EnableAdaptiveAnc(bool in_ear, anc_path_enable path, adaptive_anc_hw_channel_t hw_channel, anc_mode_t mode)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    MAKE_KYMERA_MESSAGE(KYMERA_INTERNAL_AANC_ENABLE);

    message->in_ear = in_ear;
    message->control_path      = path;
    message->hw_channel     = hw_channel;
    message->current_mode = mode;
    MessageSend(&theKymera->task, KYMERA_INTERNAL_AANC_ENABLE, message);
}

void Kymera_DisableAdaptiveAnc(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    MessageSend(&theKymera->task, KYMERA_INTERNAL_AANC_DISABLE, NULL);
}

bool Kymera_IsAdaptiveAncEnabled(void)
{
    return (appKymeraAdaptiveAncStarted() || appKymeraInConcurrency());
}

void Kymera_UpdateAdaptiveAncInEarStatus(void)
{
    KymeraAdaptiveAnc_UpdateInEarStatus();
}

void Kymera_UpdateAdaptiveAncOutOfEarStatus(void)
{
    KymeraAdaptiveAnc_UpdateOutOfEarStatus();
}

void Kymera_EnableAdaptiveAncAdaptivity(void)
{
    KymeraAdaptiveAnc_EnableAdaptivity();
}

void Kymera_DisableAdaptiveAncAdaptivity(void)
{
    KymeraAdaptiveAnc_DisableAdaptivity();
}

void Kymera_GetApdativeAncFFGain(uint8 *gain)
{
    PanicNull(gain);
    KymeraAdaptiveAnc_GetFFGain(gain);
} 

void Kymera_SetApdativeAncGainValues(uint32 mantissa, uint32 exponent)
{
    /* Just to keep compiler happy */
    UNUSED(mantissa);
    UNUSED(exponent);
    KymeraAdaptiveAnc_SetGainValues(mantissa, exponent);
}
void Kymera_EnableAdaptiveAncGentleMute(void)
{
    KymeraAdaptiveAnc_EnableGentleMute();
}

void Kymera_AdaptiveAncSetUcid(anc_mode_t new_mode)
{
    KymeraAdaptiveAnc_SetUcid(new_mode);
}

void Kymera_AdaptiveAncApplyModeChange(anc_mode_t new_mode, audio_anc_path_id feedforward_anc_path, adaptive_anc_hw_channel_t hw_channel)
{
    KymeraAdaptiveAnc_ApplyModeChange(new_mode, feedforward_anc_path, hw_channel);
    
    if ((new_mode==anc_mode_1) && appKymeraIsBusyStreaming())
    {
        /*Disable Adaptivity if A2DP is streaming*/
        Kymera_DisableAdaptiveAncAdaptivity();
    }
}

