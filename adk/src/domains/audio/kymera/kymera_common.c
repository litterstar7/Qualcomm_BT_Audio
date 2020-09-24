/*!
\copyright  Copyright (c) 2017-2020  Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\brief      Kymera common code
*/

#include <audio_clock.h>
#include <audio_power.h>
#include <pio_common.h>
#include <vmal.h>
#include <anc_state_manager.h>
#include <audio_output.h>

#include "kymera_private.h"
#include "kymera_ucid.h"
#include "kymera_anc.h"
#include "kymera_aec.h"
#include "kymera_config.h"
#include "kymera_va.h"
#include "kymera_latency_manager.h"
#include "av.h"
#include "microphones.h"
#include "microphones_config.h"
#include "pio_common.h"
#include "kymera_music_processing.h"
#if defined(INCLUDE_KYMERA_AEC) || defined(ENABLE_ADAPTIVE_ANC)
#define isAecUsedInOutputChain() TRUE
#else
#define isAecUsedInOutputChain() FALSE
#endif

static uint8 audio_ss_client_count = 0;

static audio_dsp_clock_type appKymeraGetNbWbScoDspClockType(void)
{
#if defined(ENABLE_ADAPTIVE_ANC)
    return AUDIO_DSP_TURBO_CLOCK;
#else
    return AUDIO_DSP_BASE_CLOCK;
#endif
}

void Kymera_ConnectIfValid(Source source, Sink sink)
{
    if (source && sink)
    {
        PanicNull(StreamConnect(source, sink));
    }
}

void Kymera_DisconnectIfValid(Source source, Sink sink)
{
    if (source || sink)
    {
        StreamDisconnect(source, sink);
    }
}

int32 volTo60thDbGain(int16 volume_in_db)
{
    int32 gain = VOLUME_MUTE_IN_DB;
    if (volume_in_db > appConfigMinVolumedB())
    {
        gain = volume_in_db;
        if(gain > appConfigMaxVolumedB())
        {
            gain = appConfigMaxVolumedB();
        }
    }
    return (gain * KYMERA_DB_SCALE);
}

void appKymeraSetState(appKymeraState state)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOG_STATE("appKymeraSetState, state %u -> %u", theKymera->state, state);
    theKymera->state = state;
    KymeraAnc_PreStateTransition(state);
    /* Set busy lock if not in idle or tone state */
    theKymera->busy_lock = (state != KYMERA_STATE_IDLE) && (state != KYMERA_STATE_TONE_PLAYING) && (state != KYMERA_STATE_STANDALONE_LEAKTHROUGH) && (state != KYMERA_STATE_ADAPTIVE_ANC_STARTED);
}

bool appKymeraSetActiveDspClock(audio_dsp_clock_type type)
{
    audio_dsp_clock_configuration cconfig =
    {
        .active_mode = type,
        .low_power_mode =  AUDIO_DSP_CLOCK_NO_CHANGE,
        .trigger_mode = AUDIO_DSP_CLOCK_NO_CHANGE
    };
    return AudioDspClockConfigure(&cconfig);
}

void appKymeraConfigureDspPowerMode(void)
{
#if ! defined(__CSRA68100_APP__)
    kymeraTaskData *theKymera = KymeraGetTaskData();
    bool tone_playing = appKymeraIsPlayingPrompt();

    DEBUG_LOG("appKymeraConfigureDspPowerMode, tone %u, state %u, a2dp seid %u", tone_playing, appKymeraGetState(), theKymera->a2dp_seid);
    
    /* Assume we are switching to the low power slow clock unless one of the
     * special cases below applies */
    audio_dsp_clock_configuration cconfig =
    {
        .active_mode = AUDIO_DSP_SLOW_CLOCK,
        .low_power_mode =  AUDIO_DSP_SLOW_CLOCK,
        .trigger_mode = AUDIO_DSP_CLOCK_NO_CHANGE
    };
    
    audio_dsp_clock kclocks;
    audio_power_save_mode mode = AUDIO_POWER_SAVE_MODE_3;

    switch (appKymeraGetState())
    {
        case KYMERA_STATE_A2DP_STARTING_A:
        case KYMERA_STATE_A2DP_STARTING_B:
        case KYMERA_STATE_A2DP_STARTING_C:
        case KYMERA_STATE_A2DP_STARTING_SLAVE:
        case KYMERA_STATE_A2DP_STREAMING:
        case KYMERA_STATE_A2DP_STREAMING_WITH_FORWARDING:
        case KYMERA_STATE_STANDALONE_LEAKTHROUGH:
        {
            if(AncStateManager_CheckIfDspClockBoostUpRequired())
            {
                cconfig.active_mode = AUDIO_DSP_TURBO_CLOCK;
                mode = AUDIO_POWER_SAVE_MODE_1;
            }
            else if(Kymera_IsVaActive())
            {
                cconfig.active_mode = AUDIO_DSP_TURBO_CLOCK;
                mode = AUDIO_POWER_SAVE_MODE_1;
            }
            else if(tone_playing)
            {
                /* Always jump up to normal clock for tones - for most codecs there is
                * not enough MIPs when running on a slow clock to also play a tone */
                cconfig.active_mode = AUDIO_DSP_BASE_CLOCK;
                mode = AUDIO_POWER_SAVE_MODE_1;
            }
            else
            {
                /* Either setting up for the first time or returning from a tone, in
                * either case return to the default clock rate for the codec in use */
                switch(theKymera->a2dp_seid)
                {
                    case AV_SEID_APTX_SNK:
                    case AV_SEID_APTXHD_SNK:
                    case AV_SEID_APTX_ADAPTIVE_SNK:
                    case AV_SEID_APTX_ADAPTIVE_TWS_SNK:
                    {
                        /* Not enough MIPs to run aptX master (TWS standard) or
                        * aptX adaptive (TWS standard and TWS+) on slow clock */
                        cconfig.active_mode = AUDIO_DSP_BASE_CLOCK;
                        mode = AUDIO_POWER_SAVE_MODE_1;
                    }
                    break;

                    case AV_SEID_SBC_SNK:
                    {
                        if (isAecUsedInOutputChain() || appConfigSbcNoPcmLatencyBuffer())
                        {
                            cconfig.active_mode = AUDIO_DSP_BASE_CLOCK;
                            mode = AUDIO_POWER_SAVE_MODE_1;
                        }
                    }
                    break;

                    case AV_SEID_APTX_MONO_TWS_SNK:
                    {
                        if (isAecUsedInOutputChain())
                        {
                            cconfig.active_mode = AUDIO_DSP_BASE_CLOCK;
                            mode = AUDIO_POWER_SAVE_MODE_1;
                        }
                    }
                    break;

                    default:
                    break;
                }
            }
            if (Kymera_BoostClockInGamingMode() && Kymera_LatencyManagerIsGamingModeEnabled())
            {
                cconfig.active_mode += 1;
                cconfig.active_mode = MIN(cconfig.active_mode, AUDIO_DSP_TURBO_CLOCK);
            }
        }
        break;

        case KYMERA_STATE_SCO_ACTIVE:
        case KYMERA_STATE_SCO_ACTIVE_WITH_FORWARDING:
        case KYMERA_STATE_SCO_SLAVE_ACTIVE:
        {
            DEBUG_LOG("appKymeraConfigureDspPowerMode, sco_info %u, mode %u", theKymera->sco_info, theKymera->sco_info->mode);
         if(AncStateManager_CheckIfDspClockBoostUpRequired())
         {
            cconfig.active_mode = AUDIO_DSP_TURBO_CLOCK;
            mode = AUDIO_POWER_SAVE_MODE_1;
          }
         else if (theKymera->sco_info)
           {
                switch (theKymera->sco_info->mode)
                {
                    case SCO_NB:
                    case SCO_WB:
                    {
                        /* Always jump up to normal clock (80Mhz) for NB or WB CVC in standard build */
                        cconfig.active_mode = appKymeraGetNbWbScoDspClockType();
                        mode = AUDIO_POWER_SAVE_MODE_1;
                    }
                    break;

                    case SCO_SWB:
                    case SCO_UWB:
                    {
                        /* Always jump up to turbo clock (120Mhz) for SWB or UWB CVC */
                        cconfig.active_mode = AUDIO_DSP_TURBO_CLOCK;
                        mode = AUDIO_POWER_SAVE_MODE_1;
                    }
                    break;

                    default:
                        break;
                }
            }
        }
        break;

        case KYMERA_STATE_ANC_TUNING:
        {
            /* Always jump up to turbo clock (120Mhz) for ANC tuning */
            cconfig.active_mode = AUDIO_DSP_TURBO_CLOCK;
            mode = AUDIO_POWER_SAVE_MODE_1;
        }
        break;

        case KYMERA_STATE_MIC_LOOPBACK:
        case KYMERA_STATE_TONE_PLAYING:
        {
            if(AncStateManager_CheckIfDspClockBoostUpRequired())
            {
                cconfig.active_mode = AUDIO_DSP_TURBO_CLOCK;
                mode = AUDIO_POWER_SAVE_MODE_1;
            }
            else if(Kymera_IsVaActive())
            {
                cconfig.active_mode = AUDIO_DSP_TURBO_CLOCK;
                mode = AUDIO_POWER_SAVE_MODE_1;
            }
        }
        break;

        /* All other states default to slow */
        case KYMERA_STATE_WIRED_AUDIO_PLAYING:
        case KYMERA_STATE_IDLE:
        case KYMERA_STATE_ADAPTIVE_ANC_STARTED:
			if(AncStateManager_CheckIfDspClockBoostUpRequired())
            {
                cconfig.active_mode = AUDIO_DSP_TURBO_CLOCK;
                mode = AUDIO_POWER_SAVE_MODE_1;
            }
            else if(Kymera_IsVaActive())
            {
                cconfig.active_mode = Kymera_VaGetMinDspClock();
                mode = AUDIO_POWER_SAVE_MODE_1;
            }
            break;

        case KYMERA_STATE_USB_AUDIO_ACTIVE:
        case KYMERA_STATE_USB_VOICE_ACTIVE:
            cconfig.active_mode = AUDIO_DSP_TURBO_CLOCK;
            mode = AUDIO_POWER_SAVE_MODE_1;
            break;
    }

#ifdef AUDIO_IN_SQIF
    /* Make clock faster when running from SQIF */
    cconfig.active_mode += 1;
#endif


    PanicFalse(AudioDspClockConfigure(&cconfig));
    PanicFalse(AudioPowerSaveModeSet(mode));

    PanicFalse(AudioDspGetClock(&kclocks));
    mode = AudioPowerSaveModeGet();
    DEBUG_LOG("appKymeraConfigureDspPowerMode, kymera clocks %d %d %d, mode %d", kclocks.active_mode, kclocks.low_power_mode, kclocks.trigger_mode, mode);
#else
    /* No DSP clock control on CSRA68100 */
#endif
}

void appKymeraExternalAmpSetup(void)
{
    if (appConfigExternalAmpControlRequired())
    {
        kymeraTaskData *theKymera = KymeraGetTaskData();
        int pio_mask = PioCommonPioMask(appConfigExternalAmpControlPio());
        int pio_bank = PioCommonPioBank(appConfigExternalAmpControlPio());

        /* Reset usage count */
        theKymera->dac_amp_usage = 0;

        /* map in PIO */
        PioSetMapPins32Bank(pio_bank, pio_mask, pio_mask);
        /* set as output */
        PioSetDir32Bank(pio_bank, pio_mask, pio_mask);
        /* start disabled */
        PioSet32Bank(pio_bank, pio_mask,
                     appConfigExternalAmpControlDisableMask());
    }
}

void appKymeraExternalAmpControl(bool enable)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    if (appConfigExternalAmpControlRequired())
    {
        theKymera->dac_amp_usage += enable ? 1 : - 1;

        /* Drive PIO high if enabling AMP and usage has gone from 0 to 1,
         * Drive PIO low if disabling AMP and usage has gone from 1 to 0 */
        if ((enable && theKymera->dac_amp_usage == 1) ||
            (!enable && theKymera->dac_amp_usage == 0))
        {
            int pio_mask = PioCommonPioMask(appConfigExternalAmpControlPio());
            int pio_bank = PioCommonPioBank(appConfigExternalAmpControlPio());

            PioSet32Bank(pio_bank, pio_mask,
                         enable ? appConfigExternalAmpControlEnableMask() :
                                  appConfigExternalAmpControlDisableMask());
        }
    }

    if(enable)
    {
        /* If we're enabling the amp then also call OperatorFrameworkEnable() so that the audio S/S will
           remain on even if the audio chain is destroyed, this allows us to control the timing of when the audio S/S
           and DACs are powered off to mitigate audio pops and clicks.*/

        /* Cancel any pending audio s/s disable message since we're enabling.  If message was cancelled no need
           to call OperatorFrameworkEnable() as audio S/S is still powered on from previous time */
        if(MessageCancelFirst(&theKymera->task, KYMERA_INTERNAL_AUDIO_SS_DISABLE))
        {
            DEBUG_LOG("appKymeraExternalAmpControl, there is already a client for the audio SS");
        }
        else
        {
            DEBUG_LOG("appKymeraExternalAmpControl, adding a client to the audio SS");
            OperatorsFrameworkEnable();
        }

        audio_ss_client_count++;
    }
    else
    {
        if (audio_ss_client_count > 1)
        {
            OperatorsFrameworkDisable();
            audio_ss_client_count--;
            DEBUG_LOG("appKymeraExternalAmpControl, removed audio source, count is %d", audio_ss_client_count);
        }
        else
        {
            /* If we're disabling the amp then send a timed message that will turn off the audio s/s later rather than 
            immediately */
            DEBUG_LOG("appKymeraExternalAmpControl, sending later KYMERA_INTERNAL_AUDIO_SS_DISABLE, count is %d", audio_ss_client_count);
            MessageSendLater(&theKymera->task, KYMERA_INTERNAL_AUDIO_SS_DISABLE, NULL, appKymeraDacDisconnectionDelayMs());
            audio_ss_client_count = 0;
        }
    }
}

void appKymeraConfigureSpcMode(Operator op, bool is_consumer)
{
    uint16 msg[2];
    msg[0] = OPMSG_SPC_ID_TRANSITION; /* MSG ID to set SPC mode transition */
    msg[1] = is_consumer;
    PanicFalse(OperatorMessage(op, msg, 2, NULL, 0));
}

void appKymeraConfigureScoSpcDataFormat(Operator op, audio_data_format format)
{
    uint16 msg[2];
    msg[0] = OPMSG_OP_TERMINAL_DATA_FORMAT; /* MSG ID to set SPC data type */
    msg[1] = format;
    PanicFalse(OperatorMessage(op, msg, 2, NULL, 0));
}

void appKymeraSetMainVolume(kymera_chain_handle_t chain, int16 volume_in_db)
{
    Operator volop;

    if (GET_OP_FROM_CHAIN(volop, chain, OPR_VOLUME_CONTROL))
    {
        OperatorsVolumeSetMainGain(volop, volTo60thDbGain(volume_in_db));
    }
}

void appKymeraSetVolume(kymera_chain_handle_t chain, int16 volume_in_db)
{
    Operator volop;

    if (GET_OP_FROM_CHAIN(volop, chain, OPR_VOLUME_CONTROL))
    {
        OperatorsVolumeSetMainAndAuxGain(volop, volTo60thDbGain(volume_in_db));
    }
}

Source Kymera_GetMicrophoneSource(microphone_number_t microphone_number, Source source_to_synchronise_with, uint32 sample_rate, microphone_user_type_t microphone_user_type)
{
    Source mic_source = NULL;
    if(microphone_number != microphone_none)
    {
        mic_source = Microphones_TurnOnMicrophone(microphone_number, sample_rate, microphone_user_type);
    }
    if(mic_source && source_to_synchronise_with)
    {
        SourceSynchronise(source_to_synchronise_with, mic_source);
    }
    return mic_source;
}

void Kymera_CloseMicrophone(microphone_number_t microphone_number, microphone_user_type_t microphone_user_type)
{
    if(microphone_number != microphone_none)
    {
        Microphones_TurnOffMicrophone(microphone_number, microphone_user_type);
    }
}

unsigned AudioConfigGetMicrophoneBiasVoltage(mic_bias_id id)
{
    unsigned bias = 0;
    if (id == MIC_BIAS_0)
    {
        if (appConfigMic0Bias() == BIAS_CONFIG_MIC_BIAS_0)
            bias =  appConfigMic0BiasVoltage();
        else if (appConfigMic1Bias() == BIAS_CONFIG_MIC_BIAS_0)
            bias = appConfigMic1BiasVoltage();
        else
            Panic();
    }
    else if (id == MIC_BIAS_1)
    {
        if (appConfigMic0Bias() == BIAS_CONFIG_MIC_BIAS_1)
            bias = appConfigMic0BiasVoltage();
        else if (appConfigMic1Bias() == BIAS_CONFIG_MIC_BIAS_1)
            bias = appConfigMic1BiasVoltage();
        else
            Panic();
    }
    else
        Panic();

    DEBUG_LOG("AudioConfigGetMicrophoneBiasVoltage, id %u, bias %u", id, bias);
    return bias;
}

void AudioConfigSetRawDacGain(audio_output_t channel, uint32 raw_gain)
{
    DEBUG_LOG("AudioConfigSetRawDacGain, channel %u, gain %u", channel, raw_gain);
    if (channel == audio_output_primary_left)
    {
        Sink sink = StreamAudioSink(appConfigLeftAudioHardware(), appConfigLeftAudioInstance(), appConfigLeftAudioChannel());
        PanicFalse(SinkConfigure(sink, STREAM_CODEC_RAW_OUTPUT_GAIN, raw_gain));
    }
    else if (channel == audio_output_primary_right)
    {
        Sink sink = StreamAudioSink(appConfigRightAudioHardware(), appConfigRightAudioInstance(), appConfigRightAudioChannel());
        PanicFalse(SinkConfigure(sink, STREAM_CODEC_RAW_OUTPUT_GAIN, raw_gain));
    }
    else
        Panic();
}

uint32 appKymeraGetOptimalMicSampleRate(uint32 sample_rate)
{

#ifdef ENABLE_ADAPTIVE_ANC
    sample_rate = ADAPTIVE_ANC_MIC_SAMPLE_RATE;
#endif

    return sample_rate;
}

uint8 Kymera_GetNumberOfMics(void)
{
    uint8 nr_used_microphones = 1;

#if defined(KYMERA_SCO_USE_2MIC) && defined(KYMERA_SCO_USE_3MIC)
    #error Defining KYMERA_SCO_USE_2MIC and defining KYMERA_SCO_USE_3MIC is not allowed
#endif

#if defined(KYMERA_SCO_USE_3MIC)
    nr_used_microphones = 3;
#elif defined (KYMERA_SCO_USE_2MIC)
    nr_used_microphones = 2;
#endif
    return(nr_used_microphones);
}

void appKymeraCreateLoopBackAudioChain(microphone_number_t mic_number, uint32 sample_rate)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    DEBUG_LOG_FN_ENTRY("appKymeraCreateLoopBackAudioChain, mic %u, sample rate %u ",
                        mic_number, sample_rate);

    if (Kymera_IsIdle())
    {
        Source mic = Kymera_GetMicrophoneSource(mic_number, NULL, sample_rate, high_priority_user);
        /* Need to set up audio output chain to play microphone source from scratch */
        theKymera->output_rate = sample_rate;
        KymeraOutput_CreateChain(KICK_PERIOD_SLOW, 0, 0);
        appKymeraExternalAmpControl(TRUE);
        KymeraOutput_ChainStart();
        Operator op = KymeraOutput_ChainGetOperatorByRole(OPR_VOLUME_CONTROL);
        OperatorsVolumeMute(op, FALSE);

        appKymeraSetState(KYMERA_STATE_MIC_LOOPBACK);
        KymeraOutput_ConnectSource(mic,EPR_VOLUME_AUX);
        /* Set output chain volume */
        op = KymeraOutput_ChainGetOperatorByRole(OPR_VOLUME_CONTROL);
        OperatorsVolumeSetAuxGain(op, -10);
    }
}

void appKymeraDestroyLoopbackAudioChain(microphone_number_t mic_number)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    DEBUG_LOG_FN_ENTRY("appKymeraDestroyLoopbackAudioChain, mic %u", mic_number);

    if (appKymeraGetState() == KYMERA_STATE_MIC_LOOPBACK)
    {
        Operator op = KymeraOutput_ChainGetOperatorByRole(OPR_VOLUME_CONTROL);
        uint16 volume = volTo60thDbGain(0);
        OperatorsVolumeSetMainGain(op, volume);
        OperatorsVolumeMute(op, TRUE);

        Kymera_CloseMicrophone(mic_number, high_priority_user);
        appKymeraExternalAmpControl(FALSE);
        KymeraOutput_DestroyChain();
        theKymera->output_rate = 0;

        appKymeraSetState(KYMERA_STATE_IDLE);
    }
}

void appKymeraJoinInputOutputChains(kymeraTaskData *theKymera)
{
    Source left_src = ChainGetOutput(theKymera->chain_input_handle, EPR_SOURCE_DECODED_PCM);
    Source right_src = appConfigOutputIsStereo() ? ChainGetOutput(theKymera->chain_input_handle, EPR_SOURCE_DECODED_PCM_RIGHT) : NULL;

    if(Kymera_IsMusicProcessingPresent())
    {
        /* If music processing is present then connect it with the input chain */
        PanicFalse(ChainConnectInput(theKymera->chain_music_processing_handle,
                        left_src,
                        EPR_MUSIC_PROCESSING_IN_L));
        /* Now take the output of music processing chain to be connected to the output chain */
        left_src = ChainGetOutput(theKymera->chain_music_processing_handle, EPR_MUSIC_PROCESSING_OUT_L);

        if(right_src)
        {
            PanicFalse(ChainConnectInput(theKymera->chain_music_processing_handle,
                            right_src,
                            EPR_MUSIC_PROCESSING_IN_R));
            /* Now take the output of music processing chain to be connected to the output chain */    
            right_src = ChainGetOutput(theKymera->chain_music_processing_handle, EPR_MUSIC_PROCESSING_OUT_R);
        }
     }

    /* Connect front of the chain to the output chain. */
    PanicFalse(KymeraOutput_ConnectSource(left_src,EPR_SINK_STEREO_MIXER_L));
    if(right_src)
    {
        PanicFalse(KymeraOutput_ConnectSource(right_src,EPR_SINK_STEREO_MIXER_R));
    }
}

aec_usecase_t appKymeraGetAecUseCase(void)
{
    aec_usecase_t aec_usecase = Kymera_GetAecUseCase();
    /* The only concurrency use-case could be with either Adaptive ANC or Leak-thru */
    if(Kymera_IsAdaptiveAncEnabled())
        aec_usecase = aec_usecase_adaptive_anc;
    return aec_usecase;
}

void Kymera_ConnectOutputSource(Source left, Source right, uint32 output_sample_rate)
{
    audio_output_params_t output_params;
	
    memset(&output_params, 0, sizeof(audio_output_params_t));
    output_params.sample_rate = output_sample_rate;
    output_params.transform = audio_output_tansform_connect;

    AudioOutputAddSource(left, audio_output_primary_left);

    if(appConfigOutputIsStereo())
    {
        AudioOutputAddSource(right, audio_output_primary_right);
    }
    /* Connect the sources to their appropriate hardware outputs. */
    AudioOutputConnect(&output_params);
}

