/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_usb_audio.c
\brief      Kymera USB Audio Driver
*/

#include <operators.h>
#include <logging.h>
#include "kymera_usb_audio.h"

#include "kymera_config.h"
#include "kymera_music_processing.h"

static void kymeraUSbAudio_ConfigureInputChain(KYMERA_INTERNAL_USB_AUDIO_START_T *usb_audio)
{
    usb_config_t config;
    kymeraTaskData *theKymera = KymeraGetTaskData();

    Operator usb_audio_rx_op = ChainGetOperatorByRole(theKymera->chain_input_handle, OPR_USB_AUDIO_RX);

    config.sample_rate = usb_audio->sample_freq;
    config.sample_size = usb_audio->frame_size;
    config.number_of_channels = usb_audio->channels;

    OperatorsConfigureUsbAudio(usb_audio_rx_op, config);

    OperatorsStandardSetLatencyLimits(usb_audio_rx_op,
                                      TTP_LATENCY_IN_US(usb_audio->min_latency_ms),
                                      TTP_LATENCY_IN_US(usb_audio->max_latency_ms));

    OperatorsStandardSetTimeToPlayLatency(usb_audio_rx_op,TTP_LATENCY_IN_US(usb_audio->target_latency_ms));
    OperatorsStandardSetBufferSizeWithFormat(usb_audio_rx_op, TTP_BUFFER_SIZE,
                                                     operator_data_format_pcm);
}

static void kymeraUSbAudio_CreateInputChain(kymeraTaskData *theKymera)
{

    const chain_config_t *config =
        Kymera_GetChainConfigs()->chain_input_usb_stereo_config;

    /* Create input chain */
    theKymera->chain_input_handle = PanicNull(ChainCreate(config));
}

static void kymeraUSbAudio_CreateAndConfigureOutputChain(uint32 rate,
                                                   int16 volume_in_db)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    unsigned kick_period = KICK_PERIOD_FAST;

    theKymera->output_rate = rate;
    KymeraOutput_CreateChain(kick_period, outputLatencyBuffer(), volume_in_db);
    KymeraOutput_SetOperatorUcids(FALSE);
    appKymeraExternalAmpControl(TRUE);
}

static void kymeraUSbAudio_StartChains(kymeraTaskData *theKymera, Source media_source)
{
    bool connected;

    DEBUG_LOG("kymeraUSbAudio_StartChains");
    /* Start the output chain regardless of whether the source was connected
    to the input chain. Failing to do so would mean audio would be unable
    to play a tone. This would cause kymera to lock, since it would never
    receive a KYMERA_OP_MSG_ID_TONE_END and the kymera lock would never
    be cleared. */
    KymeraOutput_ChainStart();
    Kymera_StartMusicProcessingChain();
    /* The media source may fail to connect to the input chain if the source
    disconnects between the time A2DP asks Kymera to start and this
    function being called. A2DP will subsequently ask Kymera to stop. */
    connected = ChainConnectInput(theKymera->chain_input_handle, media_source, EPR_USB_FROM_HOST);
    if (connected)
    {
        ChainStart(theKymera->chain_input_handle);
    }
}

void KymeraUsbAudio_Start(KYMERA_INTERNAL_USB_AUDIO_START_T *usb_audio)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    DEBUG_LOG("KymeraUsbAudio_Start");

    if (appKymeraGetState() == KYMERA_STATE_TONE_PLAYING)
    {
        /* If there is a tone still playing at this point,
         * it must be an interruptable tone, so cut it off */
        appKymeraTonePromptStop();
    }

    switch (appKymeraGetState())
    {
        /* Headset audio chains are started in one step */
        case KYMERA_STATE_IDLE:
        {
            /* Ensure there are no audio chains already */
            PanicNotNull(theKymera->chain_input_handle);
            PanicNotNull(KymeraOutput_GetOutputHandle());

            theKymera->output_rate = usb_audio->sample_freq;
            kymeraUSbAudio_CreateAndConfigureOutputChain(usb_audio->sample_freq,
                                                    usb_audio->volume_in_db);

            kymeraUSbAudio_CreateInputChain(theKymera);
            kymeraUSbAudio_ConfigureInputChain(usb_audio);
            Kymera_CreateMusicProcessingChain();
            Kymera_ConfigureMusicProcessing(theKymera->output_rate);
    
            StreamDisconnect(usb_audio->spkr_src, NULL);

            ChainConnect(theKymera->chain_input_handle);

            appKymeraJoinInputOutputChains(theKymera);

            appKymeraSetState(KYMERA_STATE_USB_AUDIO_ACTIVE);

            appKymeraConfigureDspPowerMode();

            kymeraUSbAudio_StartChains(theKymera, usb_audio->spkr_src);

        }

        default:
            // Report, but ignore attempts to stop in invalid states
            DEBUG_LOG("KymeraUsbAudio_Start, invalid state %u", appKymeraGetState());
            break;
    }
}

void KymeraUsbAudio_Stop(KYMERA_INTERNAL_USB_AUDIO_STOP_T *usb_audio)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    DEBUG_LOG("KymeraUsbAudio_Stop");

    switch (appKymeraGetState())
    {
        case KYMERA_STATE_USB_AUDIO_ACTIVE:
        {

            PanicNull(theKymera->chain_input_handle);
            PanicNull(theKymera->chainu.output_vol_handle);

            /* A tone still playing at this point must be interruptable */
            appKymeraTonePromptStop();

            /* Stop chains before disconnecting */
            ChainStop(theKymera->chain_input_handle);

            /* Disable external amplifier if required */
            appKymeraExternalAmpControl(FALSE);

            /* Disconnect USB source from the USB_AUDIO_RX operator then dispose */
            StreamDisconnect(usb_audio->source, 0);
            StreamConnectDispose(usb_audio->source);
            SourceClose(usb_audio->source);

            StreamDisconnect(ChainGetOutput(theKymera->chain_input_handle,
                                 EPR_SOURCE_DECODED_PCM), NULL);
            StreamDisconnect(ChainGetOutput(theKymera->chain_input_handle,
                                 EPR_SOURCE_DECODED_PCM_RIGHT),NULL);

            Sink usb_ep_snk = ChainGetInput(theKymera->chain_input_handle, EPR_USB_FROM_HOST);
            StreamDisconnect(NULL, usb_ep_snk);

            Kymera_StopMusicProcessingChain();
            /* Stop and destroy the output chain */
            KymeraOutput_DestroyChain();
            Kymera_DestroyMusicProcessingChain();

            /* Destroy chains now that input has been disconnected */
            ChainDestroy(theKymera->chain_input_handle);
            theKymera->chain_input_handle = NULL;
            theKymera->output_rate = 0;
            theKymera->usb_rx = 0;
            appKymeraSetState(KYMERA_STATE_IDLE);

            break;
        }

        case KYMERA_STATE_IDLE:
            break;

        default:
            /* Report, but ignore attempts to stop in invalid states */
            DEBUG_LOG("KymeraUsbAudio_Stop, invalid state %u", appKymeraGetState());
            break;
    }
}

void KymeraUsbAudio_SetVolume(int16 volume_in_db)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    DEBUG_LOG("KymeraUsbAudio_SetVolume, vol %d", volume_in_db);

    switch (appKymeraGetState())
    {
        case KYMERA_STATE_USB_AUDIO_ACTIVE:
            appKymeraSetMainVolume(theKymera->chainu.output_vol_handle, volume_in_db);
            break;

        default:
            break;
    }
}
