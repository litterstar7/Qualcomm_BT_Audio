/*!
\copyright  Copyright (c) 2017 - 2020  Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_prompts.c
\brief      Kymera tones / prompts
*/

#include "kymera_private.h"
#include "kymera_config.h"
#include "kymera_aec.h"
#include "system_clock.h"

#define VOLUME_CONTROL_SET_AUX_TTP_VERSION_MSB 0x2
#define BUFFER_SIZE_FACTOR 4

#define PREPARE_FOR_PROMPT_TIMEOUT (1000)

static enum
{
    kymera_tone_idle,
    kymera_tone_ready_tone,
    kymera_tone_ready_prompt,
    kymera_tone_playing
} kymera_tone_state = kymera_tone_idle;

static kymera_chain_handle_t kymera_GetTonePromptChain(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    return theKymera->chain_tone_handle;
}

/*! \brief Setup the prompt audio source.
    \param source The prompt audio source.
*/
static void appKymeraPromptSourceSetup(Source source)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    MessageStreamTaskFromSource(source, &theKymera->task);
}

/*! \brief Close the prompt audio source.
    \param source The prompt audio source.
*/
static void appKymeraPromptSourceClose(Source source)
{
    if (source)
    {
        MessageStreamTaskFromSource(source, NULL);
        StreamDisconnect(source, NULL);
        SourceClose(source);
    }
}

/*! \brief Calculates buffer size for tone/vp chain.

    Pessimistic buffer calculation, it assumes slow
    clock rare which used most often. Also BUFFER_SIZE_FACTOR = 4
    is conservative, 2 may also work.

    \param output_rate Kymera output rate in Hz.

 */
static unsigned appKymeraCalculateBufferSize(unsigned output_rate)
{
    unsigned scaled_rate = output_rate / 1000;
    return (KICK_PERIOD_SLOW * scaled_rate * BUFFER_SIZE_FACTOR) / 1000;
}

static void kymera_CreateTonePromptOutputChain(uint16 sample_rate)
{
    PanicFalse(KymeraOutput_GetOutputHandle() == NULL);

    kymeraTaskData *theKymera = KymeraGetTaskData();
    theKymera->output_rate = sample_rate;

    /* If the DSP is already running, set turbo clock to reduce startup time.
    If the DSP is not running this call will fail. That is ignored since
    the DSP will subsequently be started when the first chain is created
    and it starts by default at turbo clock */
    appKymeraSetActiveDspClock(AUDIO_DSP_TURBO_CLOCK);

    /* Need to set up audio output chain to play tone from scratch */
    KymeraOutput_CreateChain(KICK_PERIOD_TONES, 0, 0);
}

static const chain_config_t * kymera_GetPromptChainConfig(promptFormat prompt_format, uint16 sample_rate)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    const chain_config_t *config = NULL;
    const bool has_resampler = (theKymera->output_rate != sample_rate);

    if (prompt_format == PROMPT_FORMAT_SBC)
    {
        config = has_resampler ? Kymera_GetChainConfigs()->chain_prompt_decoder_config : Kymera_GetChainConfigs()->chain_prompt_decoder_no_iir_config;
    }
    else if (prompt_format == PROMPT_FORMAT_PCM)
    {
        /* If PCM at the correct rate, no chain required at all. */
        config = has_resampler ? Kymera_GetChainConfigs()->chain_prompt_pcm_config : NULL;
    }

    return config;

}

static const chain_config_t * kymera_GetToneChainConfig(uint16 sample_rate)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    const chain_config_t *config = NULL;
    const bool has_resampler = (theKymera->output_rate != sample_rate);

    config = has_resampler ? Kymera_GetChainConfigs()->chain_tone_gen_config : Kymera_GetChainConfigs()->chain_tone_gen_no_iir_config;

    return config;
}

static void kymera_CreateChain(const chain_config_t *config)
{
    PanicFalse(kymera_GetTonePromptChain() == NULL);

    kymeraTaskData *theKymera = KymeraGetTaskData();
    kymera_chain_handle_t chain = NULL;
    chain = ChainCreate(config);
    /* Configure pass-through buffer */
    Operator op = ChainGetOperatorByRole(chain, OPR_TONE_PROMPT_BUFFER);
    DEBUG_LOG("kymera_CreateTonePromptChain prompt buffer %u", op);
    unsigned buffer_size = appKymeraCalculateBufferSize(theKymera->output_rate);
    OperatorsStandardSetBufferSize(op, buffer_size);
    ChainConnect(chain);
    theKymera->chain_tone_handle = chain;
}

static void kymera_CreatePromptChain(promptFormat prompt_format, uint16 sample_rate)
{
    const chain_config_t *config = kymera_GetPromptChainConfig(prompt_format, sample_rate);

    /* NULL is a valid config for PCM prompt with no resampler */

    if (config)
    {
        kymera_CreateChain(config);
    }

    kymera_tone_state = kymera_tone_ready_prompt;
}

static void kymera_CreateToneChain(uint16 sample_rate)
{
    const chain_config_t *config = kymera_GetToneChainConfig(sample_rate);
    PanicNull((void *)config);
    kymera_CreateChain(config);
    kymera_tone_state = kymera_tone_ready_tone;
}

static void kymera_ConfigureTonePromptResampler(uint16 output_rate, uint16 tone_prompt_rate)
{
    Operator op = ChainGetOperatorByRole(kymera_GetTonePromptChain(), OPR_TONE_PROMPT_RESAMPLER);
    DEBUG_LOG("kymera_ConfigureTonePromptResampler resampler %u", op);
    OperatorsResamplerSetConversionRate(op, tone_prompt_rate, output_rate);
}

static Source kymera_ConfigurePromptChain(const KYMERA_INTERNAL_TONE_PROMPT_PLAY_T *msg)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    if (theKymera->output_rate != msg->rate)
    {
        kymera_ConfigureTonePromptResampler(theKymera->output_rate, msg->rate);
    }

    /* Configure prompt file source */
    theKymera->prompt_source = PanicNull(StreamFileSource(msg->prompt));
    DEBUG_LOG("kymera_ConfigurePromptChain prompt %u", msg->prompt);
    appKymeraPromptSourceSetup(theKymera->prompt_source);

    if (kymera_GetTonePromptChain())
    {
        PanicFalse(ChainConnectInput(kymera_GetTonePromptChain(), theKymera->prompt_source, EPR_PROMPT_IN));
    }
    else
    {
        /* No chain (prompt is PCM at the correct sample rate) so the source
        is just the file */
        return theKymera->prompt_source;
    }
    return ChainGetOutput(kymera_GetTonePromptChain(), EPR_TONE_PROMPT_CHAIN_OUT);
}

static Source kymera_ConfigureToneChain(const KYMERA_INTERNAL_TONE_PROMPT_PLAY_T *msg)
{
    Operator op;
    kymeraTaskData *theKymera = KymeraGetTaskData();

    if (theKymera->output_rate != msg->rate)
    {
         kymera_ConfigureTonePromptResampler(theKymera->output_rate, msg->rate);
    }

    /* Configure ringtone generator */
    op = ChainGetOperatorByRole(kymera_GetTonePromptChain(), OPR_TONE_GEN);
    DEBUG_LOG("kymera_ConfigureToneChain tone gen %u", op);
    OperatorsStandardSetSampleRate(op, msg->rate);
    OperatorsConfigureToneGenerator(op, msg->tone, &theKymera->task);

    return ChainGetOutput(kymera_GetTonePromptChain(), EPR_TONE_PROMPT_CHAIN_OUT);
}

static bool kymeraTonesPrompts_isAuxTtpSupported(capablity_version_t cap_version)
{
    return cap_version.version_msb >= VOLUME_CONTROL_SET_AUX_TTP_VERSION_MSB ? TRUE : FALSE;
}

bool appKymeraIsPlayingPrompt(void)
{
    return (kymera_tone_state == kymera_tone_playing);
}

static bool kymera_TonePromptIsReady(void)
{
    return ((kymera_tone_state == kymera_tone_ready_prompt) || (kymera_tone_state == kymera_tone_ready_tone));
}

bool Kymera_TonePromptActiveAlone(void)
{
    return ((appKymeraGetState() == KYMERA_STATE_TONE_PLAYING) || ((appKymeraGetState() == KYMERA_STATE_IDLE) && kymera_TonePromptIsReady()));
}

static bool kymera_CorrectPromptChainReady(promptFormat format)
{
    Operator sbc_decoder = ChainGetOperatorByRole(kymera_GetTonePromptChain(), OPR_PROMPT_DECODER);
    bool sbc_prompt_ready = ((format == PROMPT_FORMAT_SBC) && sbc_decoder);
    bool pcm_prompt_ready = ((format == PROMPT_FORMAT_PCM) && !sbc_decoder);

    return sbc_prompt_ready || pcm_prompt_ready;
}

static bool kymera_ResamplingNeedsMet(uint16 rate)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    Operator resampler = ChainGetOperatorByRole(kymera_GetTonePromptChain(), OPR_TONE_PROMPT_RESAMPLER);
    bool needs_resampler = theKymera->output_rate != rate;
    bool resampling_needs_met = (resampler && needs_resampler) || (!resampler && !needs_resampler);

    return resampling_needs_met;
}

static bool kymera_CorrectTonePromptChainReady(const KYMERA_INTERNAL_TONE_PROMPT_PLAY_T *msg)
{
    bool tone_chain_ready_for_tone_play = ((kymera_tone_state == kymera_tone_ready_tone) && msg->tone != NULL);
    bool prompt_chain_ready_for_prompt_play = (((kymera_tone_state == kymera_tone_ready_prompt) && msg->prompt != FILE_NONE) &&
                                               kymera_CorrectPromptChainReady(msg->prompt_format));

    bool resampling_needs_met = kymera_ResamplingNeedsMet(msg->rate);

    DEBUG_LOG("kymera_CorrectTonePromptChainReady %u, tone ready %u, prompt ready %u, resampling needs met %u",
              ((tone_chain_ready_for_tone_play || prompt_chain_ready_for_prompt_play) && resampling_needs_met),
              tone_chain_ready_for_tone_play, prompt_chain_ready_for_prompt_play, resampling_needs_met);

    return((tone_chain_ready_for_tone_play || prompt_chain_ready_for_prompt_play) && resampling_needs_met);
}

void appKymeraHandleInternalTonePromptPlay(const KYMERA_INTERNAL_TONE_PROMPT_PLAY_T *msg)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    Operator op;
    bool is_tone = (msg->tone != NULL);
    bool is_prompt = (msg->prompt != FILE_NONE);
    PanicFalse(is_tone != is_prompt);

    DEBUG_LOG("appKymeraHandleInternalTonePromptPlay, prompt %x, tone %p, ttp %d, int %u, lock 0x%x, mask 0x%x",
                msg->prompt, msg->tone, msg->time_to_play, msg->interruptible, msg->client_lock, msg->client_lock_mask);

    if(theKymera->listener)
    {
        if(msg->tone)
        {
            KYMERA_NOTIFICATION_TONE_STARTED_T* message = PanicNull(malloc(sizeof(KYMERA_NOTIFICATION_TONE_STARTED_T)));
            message->tone = msg->tone;
            MessageSend(theKymera->listener, KYMERA_NOTIFICATION_TONE_STARTED, message);
        }
        else
        {
            KYMERA_NOTIFICATION_PROMPT_STARTED_T* message = PanicNull(malloc(sizeof(KYMERA_NOTIFICATION_PROMPT_STARTED_T)));
            message->id = msg->prompt;
            MessageSend(theKymera->listener, KYMERA_NOTIFICATION_PROMPT_STARTED, message);
        }
    }

    /* If there is a tone still playing at this point, it must be an interruptable tone, so cut it off */
    if(appKymeraIsPlayingPrompt() || (!kymera_CorrectTonePromptChainReady(msg) && kymera_TonePromptIsReady()))
    {
        appKymeraTonePromptStop();
    }

    switch (appKymeraGetState())
    {
        case KYMERA_STATE_IDLE:
        case KYMERA_STATE_ADAPTIVE_ANC_STARTED:
            if(!kymera_TonePromptIsReady())
            {
                kymera_CreateTonePromptOutputChain(msg->rate);
            }

            appKymeraSetState(KYMERA_STATE_TONE_PLAYING);
            appKymeraExternalAmpControl(TRUE);
            KymeraOutput_ChainStart();
            op = KymeraOutput_ChainGetOperatorByRole(OPR_VOLUME_CONTROL);
            OperatorsVolumeMute(op, FALSE);

        // fall-through
        case KYMERA_STATE_SCO_ACTIVE:
        case KYMERA_STATE_SCO_ACTIVE_WITH_FORWARDING:
        case KYMERA_STATE_SCO_SLAVE_ACTIVE:
        case KYMERA_STATE_A2DP_STREAMING:
        case KYMERA_STATE_A2DP_STREAMING_WITH_FORWARDING:
        case KYMERA_STATE_STANDALONE_LEAKTHROUGH:
        case KYMERA_STATE_MIC_LOOPBACK:
        case KYMERA_STATE_WIRED_AUDIO_PLAYING:
        case KYMERA_STATE_USB_AUDIO_ACTIVE:
        case KYMERA_STATE_USB_VOICE_ACTIVE:
        {
            capablity_version_t vol_op_version;
            kymera_chain_handle_t output_chain = KymeraOutput_GetOutputHandle();

            Source aux_source = NULL;

            if(is_tone)
            {
                if(!kymera_TonePromptIsReady())
                {
                    kymera_CreateToneChain(msg->rate);
                }
                aux_source = kymera_ConfigureToneChain(msg);
            }

            if(is_prompt)
            {
                if(!kymera_TonePromptIsReady())
                {
                    kymera_CreatePromptChain(msg->prompt_format, msg->rate);
                }
                aux_source = kymera_ConfigurePromptChain(msg);
            }

            int16 volume_db = (msg->tone != NULL) ? (KYMERA_DB_SCALE * KYMERA_CONFIG_TONE_VOLUME) :
                                                    (KYMERA_DB_SCALE * KYMERA_CONFIG_PROMPT_VOLUME);

            /* Connect tone/prompt chain to output */
            ChainConnectInput(output_chain, aux_source, EPR_VOLUME_AUX);
            /* Set tone/prompt volume */
            op = KymeraOutput_ChainGetOperatorByRole(OPR_VOLUME_CONTROL);
            OperatorsVolumeSetAuxGain(op, volume_db);

            vol_op_version = OperatorGetCapabilityVersion(op);
            if (kymeraTonesPrompts_isAuxTtpSupported(vol_op_version))
            {
                rtime_t now = SystemClockGetTimerTime();
                rtime_t delta = rtime_sub(msg->time_to_play, now);

                DEBUG_LOG("appKymeraHandleInternalTonePromptPlay now=%u, ttp=%u, left=%d", now, msg->time_to_play, delta);

                OperatorsVolumeSetAuxTimeToPlay(op, msg->time_to_play,  0);
            }

            /* Start tone */
            if (theKymera->chain_tone_handle)
            {
                ChainStart(theKymera->chain_tone_handle);
            }

            kymera_tone_state = kymera_tone_playing;
            /* May need to exit low power mode to play tone simultaneously */
            appKymeraConfigureDspPowerMode();

            if (!msg->interruptible)
            {
                appKymeraSetToneLock(theKymera);
            }
            theKymera->tone_client_lock = msg->client_lock;
            theKymera->tone_client_lock_mask = msg->client_lock_mask;
        }
        break;

        case KYMERA_STATE_TONE_PLAYING:
        default:
            /* Unknown state / not supported */
            DEBUG_LOG("appKymeraHandleInternalTonePromptPlay, unsupported state %u", appKymeraGetState());
            Panic();
            break;
    }
}

static void kymera_DestroyChain(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    if(theKymera->chain_tone_handle)
    {
        ChainDestroy(theKymera->chain_tone_handle);
        theKymera->chain_tone_handle = NULL;
    }

    if(Kymera_TonePromptActiveAlone())
    {
        if(appKymeraIsPlayingPrompt())
        {
            Operator op = KymeraOutput_ChainGetOperatorByRole(OPR_VOLUME_CONTROL);
            uint16 volume = volTo60thDbGain(0);

            OperatorsVolumeSetMainGain(op, volume);
            OperatorsVolumeMute(op, TRUE);

            /* Disable external amplifier if required */
            appKymeraExternalAmpControl(FALSE);
        }

#ifdef ENABLE_AEC_LEAKTHROUGH
        if(!Kymera_IsStandaloneLeakthroughActive())
        {
            KymeraOutput_DestroyChain();
            /* Move back to idle state if standalone leak-through is not active */
            appKymeraSetState(KYMERA_STATE_IDLE);
            theKymera->output_rate = 0;
        }
#else
        KymeraOutput_DestroyChain();
        /* Move back to idle state */
        appKymeraSetState(KYMERA_STATE_IDLE);
        theKymera->output_rate = 0;
#endif
    }
}

void appKymeraTonePromptStop(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
   
    /* Exit if there isn't a tone or prompt playing */
    if ((!theKymera->chain_tone_handle) && (!theKymera->prompt_source) && (!kymera_TonePromptIsReady()))
    {
        return;
    }

    DEBUG_LOG("appKymeraTonePromptStop, state %u", appKymeraGetState());

    switch (appKymeraGetState())
    {
        case KYMERA_STATE_IDLE:
        case KYMERA_STATE_ADAPTIVE_ANC_STARTED:
        case KYMERA_STATE_SCO_ACTIVE:
        case KYMERA_STATE_SCO_SLAVE_ACTIVE:
        case KYMERA_STATE_SCO_ACTIVE_WITH_FORWARDING:
        case KYMERA_STATE_A2DP_STREAMING:
        case KYMERA_STATE_A2DP_STREAMING_WITH_FORWARDING:
        case KYMERA_STATE_TONE_PLAYING:
        case KYMERA_STATE_STANDALONE_LEAKTHROUGH:
        case KYMERA_STATE_MIC_LOOPBACK:
        case KYMERA_STATE_WIRED_AUDIO_PLAYING:
        case KYMERA_STATE_USB_AUDIO_ACTIVE:
        case KYMERA_STATE_USB_VOICE_ACTIVE:
        {
            if(appKymeraIsPlayingPrompt())
            {
                Operator op = KymeraOutput_ChainGetOperatorByRole(OPR_VOLUME_CONTROL);
                uint16 volume = volTo60thDbGain(0);
                OperatorsVolumeSetAuxGain(op, volume);

                if (theKymera->prompt_source)
                {
                    appKymeraPromptSourceClose(theKymera->prompt_source);
                    theKymera->prompt_source = NULL;
                }

                if (theKymera->chain_tone_handle)
                {
                    ChainStop(theKymera->chain_tone_handle);
                }
            }

            kymera_DestroyChain();

            appKymeraClearToneLock(theKymera);

            if(appKymeraIsPlayingPrompt())
            {
                PanicZero(theKymera->tone_count);
                theKymera->tone_count--;
            }

            kymera_tone_state = kymera_tone_idle;

            /* Return to low power mode (if applicable) */
            appKymeraConfigureDspPowerMode();

            /* Tone now stopped, clear the client's lock */
            if (theKymera->tone_client_lock)
            {
                *theKymera->tone_client_lock &= ~theKymera->tone_client_lock_mask;
                theKymera->tone_client_lock = 0;
                theKymera->tone_client_lock_mask = 0;
            }
        }
        break;

        default:
            /* Unknown state / not supported */
            DEBUG_LOG("appKymeraTonePromptStop, unsupported state %u", appKymeraGetState());
            Panic();
            break;
    }
}

bool Kymera_PrepareForPrompt(promptFormat format, uint16 sample_rate)
{
    bool prepared = FALSE;

    if(kymera_tone_state == kymera_tone_idle)
    {
        kymeraTaskData *theKymera = KymeraGetTaskData();

        switch(appKymeraGetState())
        {
            case KYMERA_STATE_IDLE:
            case KYMERA_STATE_ADAPTIVE_ANC_STARTED:
                kymera_CreateTonePromptOutputChain(sample_rate);

            // fall-through
            case KYMERA_STATE_SCO_ACTIVE:
            case KYMERA_STATE_SCO_ACTIVE_WITH_FORWARDING:
            case KYMERA_STATE_SCO_SLAVE_ACTIVE:
            case KYMERA_STATE_A2DP_STREAMING:
            case KYMERA_STATE_A2DP_STREAMING_WITH_FORWARDING:
            case KYMERA_STATE_STANDALONE_LEAKTHROUGH:
            case KYMERA_STATE_MIC_LOOPBACK:
            case KYMERA_STATE_WIRED_AUDIO_PLAYING:
                kymera_CreatePromptChain(format, sample_rate);
                MessageSendLater(&theKymera->task, KYMERA_INTERNAL_PREPARE_FOR_PROMPT_TIMEOUT, NULL, PREPARE_FOR_PROMPT_TIMEOUT);
                prepared = TRUE;
                break;

            case KYMERA_STATE_TONE_PLAYING:
            default:
                UNUSED(theKymera);
                break;
        }
    }

    DEBUG_LOG("Kymera_PrepareForPrompt prepared %u, format %u rate %u", prepared, format, sample_rate);

    return prepared;
}

bool Kymera_IsReadyForPrompt(promptFormat format, uint16 sample_rate)
{
    bool resampling_needs_met = kymera_ResamplingNeedsMet(sample_rate);
    bool is_ready_for_prompt = ((kymera_tone_state == kymera_tone_ready_prompt) && kymera_CorrectPromptChainReady(format) && resampling_needs_met);

    DEBUG_LOG("Kymera_IsReadyForPrompt %u, format %u rate %u", is_ready_for_prompt, format, sample_rate);

    return is_ready_for_prompt;
}
