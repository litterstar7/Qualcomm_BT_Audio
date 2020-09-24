/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Kymera A2DP for stereo
*/

#ifdef INCLUDE_STEREO
#include <operators.h>

#include "kymera_private.h"
#include "kymera_a2dp_private.h"
#include "kymera_music_processing.h"
#include "kymera_config.h"
#include "av.h"
#include "a2dp_profile_config.h"

static void appKymeraPreStartSanity(kymeraTaskData *theKymera)
{
    /* Can only start streaming if we're currently idle */
    PanicFalse(appKymeraGetState() == KYMERA_STATE_IDLE);

    /* Ensure there are no audio chains already */
    PanicNotNull(theKymera->chain_input_handle);
    PanicNotNull(KymeraOutput_GetOutputHandle());
}

static void appKymeraCreateInputChain(kymeraTaskData *theKymera, uint8 seid)
{
    const chain_config_t *config = NULL;
    DEBUG_LOG("appKymeraCreateInputChain");

    switch (seid)
    {
        case AV_SEID_SBC_SNK:
            DEBUG_LOG("Create SBC input chain");
            config = Kymera_GetChainConfigs()->chain_input_sbc_stereo_config;
        break;

        case AV_SEID_AAC_SNK:
            DEBUG_LOG("Create AAC input chain");
            config = Kymera_GetChainConfigs()->chain_input_aac_stereo_config;
        break;

        case AV_SEID_APTX_SNK:
            DEBUG_LOG("Create aptX Classic input chain");
            config = Kymera_GetChainConfigs()->chain_input_aptx_stereo_config;
        break;

        case AV_SEID_APTXHD_SNK:
            DEBUG_LOG("Create aptX HD input chain");
            config = Kymera_GetChainConfigs()->chain_input_aptxhd_stereo_config;

        break;

#ifdef INCLUDE_APTX_ADAPTIVE
        case AV_SEID_APTX_ADAPTIVE_SNK:
             DEBUG_LOG("Create aptX Adaptive input chain");
             if (theKymera->q2q_mode)
                 config =  Kymera_GetChainConfigs()->chain_input_aptx_adaptive_stereo_q2q_config;
             else
                 config =  Kymera_GetChainConfigs()->chain_input_aptx_adaptive_stereo_config;
        break;
#endif
        default:
            Panic();
        break;
    }

    /* Create input chain */
    theKymera->chain_input_handle = PanicNull(ChainCreate(config));
}

static void appKymeraConfigureInputChain(kymeraTaskData *theKymera,
                                         uint8 seid, uint32 rate,
                                         bool cp_header_enabled,
                                         aptx_adaptive_ttp_latencies_t nq2q_ttp)
{
    kymera_chain_handle_t chain_handle = theKymera->chain_input_handle;
    rtp_codec_type_t rtp_codec = -1;
    rtp_working_mode_t mode = rtp_decode;
    Operator op_aac_decoder;
#ifdef INCLUDE_APTX_ADAPTIVE
    Operator op;
#endif
    Operator op_rtp_decoder = ChainGetOperatorByRole(chain_handle, OPR_RTP_DECODER);
    DEBUG_LOG("appKymeraConfigureInputChain");

#ifndef INCLUDE_APTX_ADAPTIVE
    UNUSED(nq2q_ttp);
#endif

    switch (seid)
    {
        case AV_SEID_SBC_SNK:
            DEBUG_LOG("configure SBC input chain");
            rtp_codec = rtp_codec_type_sbc;
        break;

        case AV_SEID_AAC_SNK:
            DEBUG_LOG("configure AAC input chain");
            rtp_codec = rtp_codec_type_aac;
            op_aac_decoder = PanicZero(ChainGetOperatorByRole(chain_handle, OPR_AAC_DECODER));
            OperatorsRtpSetAacCodec(op_rtp_decoder, op_aac_decoder);
        break;
		
        case AV_SEID_APTX_SNK:
            DEBUG_LOG("configure aptX Classic input chain");
            rtp_codec = rtp_codec_type_aptx;
            if (!cp_header_enabled)
            {
                mode = rtp_ttp_only;
            }
        break;

        case AV_SEID_APTXHD_SNK:
            DEBUG_LOG("configure aptX HD input chain");
            rtp_codec = rtp_codec_type_aptx_hd;
        break;

#ifdef INCLUDE_APTX_ADAPTIVE
        case AV_SEID_APTX_ADAPTIVE_SNK:
            DEBUG_LOG("configure aptX adaptive input chain");
            aptx_adaptive_ttp_in_ms_t aptx_ad_ttp;

            if (theKymera->q2q_mode)
            {
                op = PanicZero(ChainGetOperatorByRole(chain_handle, OPR_SWITCHED_PASSTHROUGH_CONSUMER));
                OperatorsSetSwitchedPassthruEncoding(op, spc_op_format_encoded);
                OperatorsStandardSetBufferSizeWithFormat(op, PRE_DECODER_BUFFER_SIZE, operator_data_format_encoded);
                OperatorsSetSwitchedPassthruMode(op, spc_op_mode_passthrough);
            }
            else
            {
                convertAptxAdaptiveTtpToOperatorsFormat(nq2q_ttp, &aptx_ad_ttp);
                getAdjustedAptxAdaptiveTtpLatencies(&aptx_ad_ttp);

                OperatorsRtpSetAptxAdaptiveTTPLatency(op_rtp_decoder, aptx_ad_ttp);
                rtp_codec = rtp_codec_type_aptx_ad;
            }
        break;
#endif

        default:
            Panic();
        break;
    }

    if (!theKymera->q2q_mode) /* We don't use rtp decoder for Q2Q mode */
        appKymeraConfigureRtpDecoder(op_rtp_decoder, rtp_codec, mode, rate, cp_header_enabled, PRE_DECODER_BUFFER_SIZE);
    ChainConnect(chain_handle);
}

static void appKymeraCreateAndConfigureOutputChain(uint8 seid, uint32 rate,
                                                   int16 volume_in_db)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    unsigned kick_period = KICK_PERIOD_FAST;
    DEBUG_LOG("appKymeraCreateAndConfigureOutputChain, creating output chain, completing startup");

    switch (seid)
    {
        case AV_SEID_SBC_SNK:
            kick_period = KICK_PERIOD_MASTER_SBC;
            break;

        case AV_SEID_AAC_SNK:
            kick_period = KICK_PERIOD_MASTER_AAC;
            break;

        case AV_SEID_APTX_SNK:
        case AV_SEID_APTXHD_SNK:
            kick_period = KICK_PERIOD_MASTER_APTX;
        break;
#ifdef INCLUDE_APTX_ADAPTIVE
        case AV_SEID_APTX_ADAPTIVE_SNK:
            kick_period = KICK_PERIOD_MASTER_APTX_ADAPTIVE;
        break;
#endif

        default :
            Panic();
            break;
    }

    theKymera->output_rate = rate;
    KymeraOutput_CreateChain(kick_period, outputLatencyBuffer(), volume_in_db);
    KymeraOutput_SetOperatorUcids(FALSE);
    appKymeraExternalAmpControl(TRUE);
}

static void appKymeraStartChains(kymeraTaskData *theKymera, Source media_source)
{
    bool connected;

    DEBUG_LOG("appKymeraStartChains");
    /* Start the output chain regardless of whether the source was connected
    to the input chain. Failing to do so would mean audio would be unable
    to play a tone. This would cause kymera to lock, since it would never
    receive a KYMERA_OP_MSG_ID_TONE_END and the kymera lock would never
    be cleared. */
    KymeraOutput_ChainStart();
    Kymera_StartMusicProcessingChain();
    /* In Q2Q mode the media source has already been connected to the input
    chain by the TransformPacketise so the chain can be started immediately */
    if (theKymera->q2q_mode)
    {
        ChainStart(theKymera->chain_input_handle);
    }
    else
    {
        /* The media source may fail to connect to the input chain if the source
        disconnects between the time A2DP asks Kymera to start and this
        function being called. A2DP will subsequently ask Kymera to stop. */
        connected = ChainConnectInput(theKymera->chain_input_handle, media_source, EPR_SINK_MEDIA);
        if (connected)
        {
            ChainStart(theKymera->chain_input_handle);
        }
    }
}

static bool appKymeraA2dpStartStereo(const a2dp_codec_settings *codec_settings, uint32 max_bitrate, int16 volume_in_db,
                                     aptx_adaptive_ttp_latencies_t nq2q_ttp)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    UNUSED(max_bitrate);
    bool cp_header_enabled;
    uint32 rate;
    uint8 seid;
    Source media_source;
    uint16 mtu;

    DEBUG_LOG("appKymeraA2dpStartStereo");
    appKymeraGetA2dpCodecSettingsCore(codec_settings, &seid, &media_source, &rate, &cp_header_enabled, &mtu);

    switch (appKymeraGetState())
    {
        /* Headset audio chains are started in one step */
        case KYMERA_STATE_A2DP_STARTING_A:
        {
            /* If the DSP is already running, set turbo clock to reduce startup time.
            If the DSP is not running this call will fail. That is ignored since
            the DSP will subsequently be started when the first chain is created
            and it starts by default at turbo clock */
            appKymeraSetActiveDspClock(AUDIO_DSP_TURBO_CLOCK);

            appKymeraCreateAndConfigureOutputChain(seid, rate, volume_in_db);
            
            appKymeraCreateInputChain(theKymera, seid);
            appKymeraConfigureInputChain(theKymera, seid,
                                         rate, cp_header_enabled,
                                         nq2q_ttp);
            Kymera_CreateMusicProcessingChain();
            Kymera_ConfigureMusicProcessing(rate);
            appKymeraJoinInputOutputChains(theKymera);
            appKymeraConfigureDspPowerMode();
            /* Connect media source to chain */
            StreamDisconnect(media_source, 0);

            if (theKymera->q2q_mode)
            {
                Sink sink = ChainGetInput(theKymera->chain_input_handle, EPR_SINK_MEDIA);
                Transform packetiser = PanicNull(TransformPacketise(media_source, sink));
                if (packetiser)
                {
                    PanicFalse(TransformConfigure(packetiser, VM_TRANSFORM_PACKETISE_CODEC, VM_TRANSFORM_PACKETISE_CODEC_APTX));
                    PanicFalse(TransformConfigure(packetiser, VM_TRANSFORM_PACKETISE_MODE, VM_TRANSFORM_PACKETISE_MODE_TWSPLUS));
                    PanicFalse(TransformConfigure(packetiser, VM_TRANSFORM_PACKETISE_SAMPLE_RATE, (uint16) rate));
                    PanicFalse(TransformConfigure(packetiser, VM_TRANSFORM_PACKETISE_CPENABLE, (uint16) cp_header_enabled));
                    PanicFalse(TransformStart(packetiser));
                    theKymera->packetiser = packetiser;
                 }
            }

            appKymeraStartChains(theKymera, media_source);

            if(AecLeakthrough_IsLeakthroughEnabled())
            {
                /* Setting the usecase for AEC_REF to a2dp_leakthrough */
                Kymera_SetAecUseCase(aec_usecase_a2dp_leakthrough);
                Kymera_LeakthroughConnectWithTone();
            }
        }
        return TRUE;

        default:
            Panic();
        return FALSE;
    }
}

void appKymeraA2dpCommonStop(Source source)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    DEBUG_LOG("appKymeraA2dpCommonStop, source(%p)", source);

    PanicNull(theKymera->chain_input_handle);
    PanicNull(KymeraOutput_GetOutputHandle());

    if(AecLeakthrough_IsLeakthroughEnabled())
    {
        /*Setting the sidetone gain to minimum value to avoid pop during A2dp pause*/
        Kymera_LeakthroughMuteDisconnect(TRUE);
        Kymera_SetAecUseCase(aec_usecase_default);
    }

    /* A tone still playing at this point must be interruptable */
    appKymeraTonePromptStop();

    /* Stop chains before disconnecting */
    ChainStop(theKymera->chain_input_handle);

    /* Disable external amplifier if required */
    appKymeraExternalAmpControl(FALSE);

    /* Disconnect A2DP source from the RTP operator then dispose */
    StreamDisconnect(source, 0);
    StreamConnectDispose(source);

    Kymera_StopMusicProcessingChain();

    /* Stop and destroy the output chain */
    KymeraOutput_DestroyChain();

    Kymera_DestroyMusicProcessingChain();

    /* Destroy chains now that input has been disconnected */
    ChainDestroy(theKymera->chain_input_handle);
    theKymera->chain_input_handle = NULL;

}

bool appKymeraHandleInternalA2dpStart(const KYMERA_INTERNAL_A2DP_START_T *msg)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    uint8 seid = msg->codec_settings.seid;
    uint32 rate = msg->codec_settings.rate;
    uint8 q2q = msg->q2q_mode;

    DEBUG_LOG("appKymeraHandleInternalA2dpStart, state %u, seid %u, rate %u", appKymeraGetState(), seid, rate);

    if (appKymeraGetState() == KYMERA_STATE_TONE_PLAYING)
    {
        /* If there is a tone still playing at this point,
         * it must be an interruptable tone, so cut it off */
        appKymeraTonePromptStop();
    }

    if(appKymeraGetState() == KYMERA_STATE_STANDALONE_LEAKTHROUGH)
    {
        Kymera_LeakthroughStopChainIfRunning();
        appKymeraSetState(KYMERA_STATE_IDLE);
    }

    if (appA2dpIsSeidNonTwsSink(seid))
    {
        switch (appKymeraGetState())
        {
            case KYMERA_STATE_IDLE:
            {
                appKymeraPreStartSanity(theKymera);
                theKymera->output_rate = rate;
                theKymera->a2dp_seid = seid;
                theKymera->q2q_mode = q2q;
                appKymeraSetState(KYMERA_STATE_A2DP_STARTING_A);
            }
            // fall-through
            case KYMERA_STATE_A2DP_STARTING_A:
            {
                if (!appKymeraA2dpStartStereo(&msg->codec_settings, msg->max_bitrate, msg->volume_in_db,
                                              msg->nq2q_ttp))
                {
                    DEBUG_LOG("appKymeraHandleInternalA2dpStart, state %u, seid %u, rate %u", appKymeraGetState(), seid, rate);
                    Panic();
                }
                /* Startup is complete, now streaming */
                appKymeraSetState(KYMERA_STATE_A2DP_STREAMING);
            }
            break;

            default:
                Panic();
            break;
        }
    }
    else
    {
        /* Unsupported SEID, control should never reach here */
        Panic();
    }
    return TRUE;
}

void appKymeraHandleInternalA2dpStop(const KYMERA_INTERNAL_A2DP_STOP_T *msg)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    uint8 seid = msg->seid;

    DEBUG_LOG("appKymeraHandleInternalA2dpStop, state %u, seid %u", appKymeraGetState(), seid);

    if (appA2dpIsSeidNonTwsSink(seid))
    {
        switch (appKymeraGetState())
        {
            case KYMERA_STATE_A2DP_STREAMING:
                appKymeraA2dpCommonStop(msg->source);
                theKymera->output_rate = 0;
                theKymera->a2dp_seid = AV_SEID_INVALID;
                appKymeraSetState(KYMERA_STATE_IDLE);
                Kymera_LeakthroughResumeChainIfSuspended();
            break;

            case KYMERA_STATE_IDLE:
            break;

            default:
                // Report, but ignore attempts to stop in invalid states
                DEBUG_LOG("appKymeraHandleInternalA2dpStop, invalid state %u", appKymeraGetState());
            break;
        }
    }
    else
    {
        /* Unsupported SEID, control should never reach here */
        Panic();
    }
}

void appKymeraHandleInternalA2dpSetVolume(int16 volume_in_db)
{
    DEBUG_LOG("appKymeraHandleInternalA2dpSetVolume, vol %d", volume_in_db);

    switch (appKymeraGetState())
    {
        case KYMERA_STATE_A2DP_STREAMING:
            KymeraOutput_SetMainVolume(volume_in_db);
            break;

        default:
            break;
    }
}

#endif /* INCLUDE_STEREO */
