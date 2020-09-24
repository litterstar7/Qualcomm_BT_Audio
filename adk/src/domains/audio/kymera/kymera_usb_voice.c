/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_usb_voice.c
\brief      Kymera USB Voice Driver
*/

#include "kymera_config.h"
#include "kymera_aec.h"
#include "kymera_cvc.h"
#include "kymera_usb_voice.h"

#include <operators.h>
#include <logging.h>

#define USB_VOICE_CHANNEL_MONO          (1)
#define USB_VOICE_FRAME_SIZE            (2) /* 16 Bits */

#define USB_VOICE_NUM_OF_MICS           (2)

#define USB_VOICE_INVALID_NUM_OF_MICS   (3)

/* TODO: Once generic name (without "SCO") is used, below macros
 * can be removed
 */
#ifdef KYMERA_SCO_USE_2MIC_BINAURAL
#define KYMERA_USB_VOICE_USE_2MIC_BINAURAL
#elif KYMERA_SCO_USE_2MIC
#define KYMERA_USB_VOICE_USE_2MIC
#endif /* KYMERA_SCO_USE_2MIC_BINAURAL */

/* Reference data */
#define AEC_USB_TX_BUFFER_SIZE_MS   15
#define AEC_USB_TTP_DELAY_MS    50

static kymera_chain_handle_t kymeraUsbVoice_GetChain(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    /* Create input chain */
    return theKymera->chainu.usb_voice_handle;
}

static kymera_chain_handle_t kymeraUsbVoice_CreateChain(usb_voice_mode_t mode)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    const chain_config_t *config = NULL;

    /* USB Voice does not support 3-mic cVc. So lets panic is user selected this
     * option.
     */
    if (Kymera_GetNumberOfMics() == USB_VOICE_INVALID_NUM_OF_MICS)
    {
        DEBUG_LOG("kymeraUsbVoice_CreateChain invalid no of mics %x", Kymera_GetNumberOfMics());
        return NULL;
    }

    switch(mode)
    {
        case usb_voice_mode_nb:
        {
            if(USB_VOICE_NUM_OF_MICS == Kymera_GetNumberOfMics())
            {
#ifdef KYMERA_USB_VOICE_USE_2MIC_BINAURAL
                config = Kymera_GetChainConfigs()->chain_usb_voice_nb_2mic_binaural_config;
#else
                config = Kymera_GetChainConfigs()->chain_usb_voice_nb_2mic_config;
#endif /* KYMERA_USB_VOICE_USE_2MIC_BINAURAL */
            }
            else
            {
                config = Kymera_GetChainConfigs()->chain_usb_voice_nb_config;
            }
            break;
        }
        case usb_voice_mode_wb:
        {
            if(USB_VOICE_NUM_OF_MICS == Kymera_GetNumberOfMics())
            {
#ifdef KYMERA_USB_VOICE_USE_2MIC_BINAURAL
                config = Kymera_GetChainConfigs()->chain_usb_voice_wb_2mic_binaural_config;
#else
                config = Kymera_GetChainConfigs()->chain_usb_voice_wb_2mic_config;
#endif /* KYMERA_USB_VOICE_USE_2MIC_BINAURAL */
            }
            else
            {
                config = Kymera_GetChainConfigs()->chain_usb_voice_wb_config;
            }
            break;
        }
        default:
            DEBUG_LOG("USB Voice: Invalid configuration mode%x", mode);
            break;
    }

    /* Create input chain */
    theKymera->chainu.usb_voice_handle = PanicNull(ChainCreate(config));

    /* Configure DSP power mode appropriately for USB chain */
    appKymeraConfigureDspPowerMode();

    return theKymera->chainu.usb_voice_handle;
}

static void kymeraUsbVoice_ConfigureChain(KYMERA_INTERNAL_USB_VOICE_START_T *usb_voice)
{
    usb_config_t config;
    Operator usb_audio_rx_op = ChainGetOperatorByRole(kymeraUsbVoice_GetChain(), OPR_USB_AUDIO_RX);
    Operator usb_audio_tx_op = ChainGetOperatorByRole(kymeraUsbVoice_GetChain(), OPR_USB_AUDIO_TX);

    config.sample_rate = usb_voice->sampleFreq;
    config.sample_size = USB_VOICE_FRAME_SIZE;
    config.number_of_channels = USB_VOICE_CHANNEL_MONO;

    DEBUG_LOG("kymeraUsbVoice_ConfigureChain: Operators rx %x, tx %x", usb_audio_rx_op, usb_audio_tx_op);

    OperatorsConfigureUsbAudio(usb_audio_rx_op, config);

    OperatorsStandardSetLatencyLimits(usb_audio_rx_op,
                                      TTP_LATENCY_IN_US(usb_voice->min_latency_ms),
                                      TTP_LATENCY_IN_US(usb_voice->max_latency_ms));

    OperatorsStandardSetTimeToPlayLatency(usb_audio_rx_op, TTP_LATENCY_IN_US(usb_voice->target_latency_ms));
    OperatorsStandardSetBufferSizeWithFormat(usb_audio_rx_op, TTP_BUFFER_SIZE,
                                                     operator_data_format_pcm);

    OperatorsConfigureUsbAudio(usb_audio_tx_op, config);
    OperatorsStandardSetBufferSizeWithFormat(usb_audio_tx_op, TTP_BUFFER_SIZE,
                                                     operator_data_format_pcm);

    KymeraOutput_ConfigureChainOperators(kymeraUsbVoice_GetChain(),
              usb_voice->sampleFreq, KICK_PERIOD_VOICE, 0, usb_voice->volume);

    KymeraOutput_SetOperatorUcids(TRUE);

    /* Enable external amplifier if required */
    appKymeraExternalAmpControl(TRUE);
}

static void kymeraUsbVoice_ConnectToAecChain(const aec_connect_audio_input_t* input,
                        const aec_connect_audio_output_t* output,
                        const aec_audio_config_t *config)
{
    Kymera_SetAecUseCase(aec_usecase_voice_call);
    /* Connect AEC Input */
    Kymera_ConnectAudioInputToAec(input, config);
    /* Connect AEC Ouput */
    Kymera_ConnectAudioOutputToAec(output, config);
}

static void kymeraUsbVoice_DisconnectFromAecChain(void)
{
    /* Disconnect AEC Input */
    Kymera_DisconnectAudioInputFromAec();

    /* Disconnect AEC Ouput */
    Kymera_DisconnectAudioOutputFromAec();

    /* Reset the AEC ref usecase*/
    Kymera_SetAecUseCase(aec_usecase_default);
}

void KymeraUsbVoice_Start(KYMERA_INTERNAL_USB_VOICE_START_T *usb_voice)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    aec_connect_audio_output_t aec_output_param;
    aec_connect_audio_input_t aec_input_param;
    aec_audio_config_t aec_config = {0};

    DEBUG_LOG("USB Voice: KymeraUsbVoice_Start Sink %x", usb_voice->mic_sink);

    if (appKymeraGetState() == KYMERA_STATE_TONE_PLAYING)
    {
        /* If there is a tone still playing at this point,
         * it must be an interruptible tone, so cut it off */
        appKymeraTonePromptStop();
    }

    /* Can't start voice chain if we're not idle */
    PanicFalse(appKymeraGetState() == KYMERA_STATE_IDLE);

    /* USB chain must be destroyed if we get here */
    PanicNotNull(kymeraUsbVoice_GetChain());

    /* Move to USB active state now, what ever happens we end up in this state
      (even if it's temporary) */
    appKymeraSetState(KYMERA_STATE_USB_VOICE_ACTIVE);

    /* Create appropriate USB chain */
    kymera_chain_handle_t usb_voice_chain = PanicNull(kymeraUsbVoice_CreateChain(usb_voice->mode));

    uint8 nr_mics = Kymera_GetNumberOfMics();
    Source mic_src_1a = Kymera_GetMicrophoneSource(appConfigMicVoice(),
                        NULL,appKymeraGetOptimalMicSampleRate(usb_voice->sampleFreq),
                        high_priority_user);

    Source mic_src_1b = Kymera_GetMicrophoneSource((nr_mics > 1) ? appConfigMicExternal() : microphone_none,
                         mic_src_1a, appKymeraGetOptimalMicSampleRate(usb_voice->sampleFreq),
                         high_priority_user);

    aec_output_param.input_1 = ChainGetOutput(usb_voice_chain, EPR_SCO_VOL_OUT);
    aec_output_param.input_2 = NULL;

    DEBUG_LOG("USB Voice: mic 1 and 2 are %x, %x", mic_src_1a, mic_src_1b);

    /* Fill the AEC Ref Input params */
    aec_input_param.mic_input_1 = mic_src_1a;
    aec_input_param.mic_input_2 = mic_src_1b;
    aec_input_param.mic_input_3 = NULL;
    aec_input_param.mic_output_1 = ChainGetInput(usb_voice_chain, EPR_CVC_SEND_IN1);
    aec_input_param.mic_output_2 = (nr_mics > 1) ? ChainGetInput(usb_voice_chain, EPR_CVC_SEND_IN2) : NULL;
    aec_input_param.mic_output_3 = NULL;
    aec_input_param.reference_output = ChainGetInput(usb_voice_chain, EPR_CVC_SEND_REF_IN);

    /* sample rate */
    aec_config.spk_sample_rate = usb_voice->sampleFreq;
    aec_config.mic_sample_rate = usb_voice->sampleFreq;
    /* terminal buffer size */
    aec_config.buffer_size = AEC_USB_TX_BUFFER_SIZE_MS;
    /* TTP Gate */
    aec_config.ttp_gate_delay = AEC_USB_TTP_DELAY_MS;

     /* Get sources and sinks for chain endpoints */
     Source usb_ep_src  = ChainGetOutput(usb_voice_chain, EPR_USB_TO_HOST);
     Sink usb_ep_snk    = ChainGetInput(usb_voice_chain, EPR_USB_FROM_HOST);

     PanicFalse(OperatorsFrameworkSetKickPeriod(KICK_PERIOD_VOICE));

    theKymera->output_rate = usb_voice->sampleFreq;

    /* Connect to AEC Reference Chain */
    kymeraUsbVoice_ConnectToAecChain(&aec_input_param, &aec_output_param,
                                    &aec_config);

    /* Configure chain specific operators */
    kymeraUsbVoice_ConfigureChain(usb_voice);

    StreamDisconnect(usb_voice->spkr_src, NULL);
    StreamDisconnect(usb_ep_src, NULL);

    /* Disconnect USB ISO in endpoint */
    StreamDisconnect(usb_voice->spkr_src, NULL);

    /* Disconnect USB ISO out endpoint */
    StreamDisconnect(NULL, usb_voice->mic_sink);

    /* Connect USB chain to USB endpoints */
    StreamConnect(usb_voice->spkr_src, usb_ep_snk);
    StreamConnect(usb_ep_src, usb_voice->mic_sink);

    /* Connect chain */
    ChainConnect(usb_voice_chain);

    /* The chain can fail to start if the USB source disconnects whilst kymera
    is queuing the USB start request or starting the chain. If the attempt fails,
    ChainStartAttempt will stop (but not destroy) any operators it started in the chain. */
    if (ChainStartAttempt(usb_voice_chain))
    {
        KymeraUsbVoice_SetVolume(usb_voice->volume);

    }
    else
    {
        KYMERA_INTERNAL_USB_VOICE_STOP_T disconnect_params;
        DEBUG_LOG("USB Voice: KymeraUsbVoiceStart, could not start chain");
        disconnect_params.mic_sink = usb_voice->mic_sink;
        disconnect_params.spkr_src = usb_voice->spkr_src;
        KymeraUsbVoice_Stop(&disconnect_params);
    }
}

void KymeraUsbVoice_Stop(KYMERA_INTERNAL_USB_VOICE_STOP_T *usb_voice)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    DEBUG_LOG("USB Voice: KymeraUsbVoice_Stop, mic sink %u", usb_voice->mic_sink);

    /* Get current USB chain */
    kymera_chain_handle_t usb_voice_chain = kymeraUsbVoice_GetChain();
    if (appKymeraGetState() != KYMERA_STATE_USB_VOICE_ACTIVE)
    {
        if (usb_voice_chain == NULL)
        {
            /* Attempting to stop a USB Voice chain when not ACTIVE.*/
            DEBUG_LOG("USB Voice: KymeraUsbVoice_Stop, not stopping - already idle");
            return;
        }
        else
        {
            Panic();
        }
    }

    /* Get sources and sinks for chain endpoints */
    Source usb_ep_src  = ChainGetOutput(usb_voice_chain, EPR_USB_TO_HOST);
    Sink usb_ep_snk    = ChainGetInput(usb_voice_chain, EPR_USB_FROM_HOST);

    Operator op = ChainGetOperatorByRole(usb_voice_chain, OPR_VOLUME_CONTROL);
    uint16 volume = volTo60thDbGain(0);
    OperatorsVolumeSetAuxGain(op, volume);

    appKymeraTonePromptStop();

    /* Stop chains */
    ChainStop(usb_voice_chain);

    /* Disconnect USB ISO in endpoint */
    StreamDisconnect(usb_voice->spkr_src, NULL);

    /* Disconnect USB ISO out endpoint */
    StreamDisconnect(NULL, usb_voice->mic_sink);

    /* Disconnect from AEC chain */
    kymeraUsbVoice_DisconnectFromAecChain();

    Kymera_CloseMicrophone(appConfigMicVoice(), high_priority_user);
    uint8 nr_mics = Kymera_GetNumberOfMics();
    Kymera_CloseMicrophone((nr_mics > 1) ? appConfigMicExternal() : microphone_none, high_priority_user);

    StreamDisconnect(ChainGetOutput(usb_voice_chain, EPR_SCO_VOL_OUT), NULL);

    /* Disconnect USB from chain USB endpoints */
    StreamDisconnect(usb_ep_src, NULL);
    StreamDisconnect(NULL, usb_ep_snk);

    /* Destroy chains */
    ChainDestroy(usb_voice_chain);
    usb_voice_chain = NULL;
    theKymera->chainu.usb_voice_handle = NULL;

    /* Disable external amplifier if required */
    appKymeraExternalAmpControl(FALSE);

    /* Update state variables */
    appKymeraSetState(KYMERA_STATE_IDLE);
    theKymera->output_rate = 0;
}

void KymeraUsbVoice_SetVolume(int16 volume_in_db)
{
    kymera_chain_handle_t usbChain = kymeraUsbVoice_GetChain();

    DEBUG_LOG("KymeraUsbVoice_SetVolume, vol %d", volume_in_db);

    switch (KymeraGetTaskData()->state)
    {
        case KYMERA_STATE_USB_VOICE_ACTIVE:
        {
            appKymeraSetMainVolume(usbChain, volume_in_db);
        }
        break;
        default:
            break;
    }
}

void KymeraUsbVoice_MicMute(bool mute)
{
    DEBUG_LOG("KymeraUsbVoice_MicMute, mute %u", mute);

    switch (KymeraGetTaskData()->state)
    {
        case KYMERA_STATE_USB_VOICE_ACTIVE:
        {
            Operator aec_op = Kymera_GetAecOperator();
            if (aec_op != INVALID_OPERATOR)
            {
                OperatorsAecMuteMicOutput(aec_op, mute);
            }
        }
        break;

        default:
            break;
    }
}

