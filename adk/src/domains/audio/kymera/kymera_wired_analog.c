/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       kymera_wired_analog.c
\brief      Kymera Wired Analog file to create, configure and destroy the wired analog chain
*/

#include <vmal.h>

#include "kymera_config.h"
#include "kymera_private.h"
#include "kymera_music_processing.h"

#ifdef INCLUDE_WIRED_ANALOG_AUDIO
/**************************************************************************************

                                               UTILITY FUNCTIONS
**************************************************************************************/
static void kymeraWiredAnalog_ConfigureChain(kymeraTaskData *theKymera, uint32 rate, 
                                                                                uint32 min_latency, uint32 max_latency, uint32 latency)
{
    kymera_chain_handle_t chain_handle = theKymera->chain_input_handle;
    DEBUG_LOG("kymeraWiredAnalog_ConfigureChain");

    Operator ttp_passthrough = ChainGetOperatorByRole(chain_handle, OPR_LATENCY_BUFFER);
    if(ttp_passthrough)
    {
        OperatorsStandardSetLatencyLimits(ttp_passthrough,
                                          TTP_LATENCY_IN_US(min_latency),
                                          TTP_LATENCY_IN_US(max_latency));
    
        OperatorsConfigureTtpPassthrough(ttp_passthrough, TTP_LATENCY_IN_US(latency), rate, operator_data_format_pcm);
        OperatorsStandardSetBufferSizeWithFormat(ttp_passthrough, TTP_BUFFER_SIZE, operator_data_format_pcm);
    }

    ChainConnect(chain_handle);
}

static Source SourcekymeraWiredAnalog_GetSource(audio_channel channel, uint8 inst, uint32 rate)
{
#define SAMPLE_SIZE 16 /* only if 24 bit resolution is supported this can be 24 */
    Source source;
    analogue_input_params params = {
        .pre_amp = FALSE,
        .gain = 0x09, /* for line-in set to 0dB */
        .instance = 0, /* Place holder */
        .enable_24_bit_resolution = FALSE
        };

    DEBUG_LOG("SourcekymeraWiredAnalog_GetSource, Get source for Channel: %u, Instance: %u and Sample Rate: %u", channel, inst, rate);
    params.instance = inst;
    source = AudioPluginAnalogueInputSetup(channel, params, rate);
    PanicFalse(SourceConfigure(source, STREAM_AUDIO_SAMPLE_SIZE, SAMPLE_SIZE));

    return source;
}

static void kymeraWiredAnalog_StartChains(kymeraTaskData *theKymera)
{
    bool connected;

    Source line_in_l = SourcekymeraWiredAnalog_GetSource(appConfigLeftAudioChannel(), appConfigLeftAudioInstance(), theKymera->output_rate /* for now input/output rate are same */);
    Source line_in_r = SourcekymeraWiredAnalog_GetSource(appConfigRightAudioChannel(), appConfigRightAudioInstance(), theKymera->output_rate /* for now input/output rate are same */);
    /* if stereo, then synchronize */
    if(line_in_r)
        SourceSynchronise(line_in_l, line_in_r);

    DEBUG_LOG("kymeraWiredAnalog_StartChains");
    /* The media source may fail to connect to the input chain if the source
    disconnects between the time wired analog audio asks Kymera to start and this
    function being called. wired analog audio will subsequently ask Kymera to stop. */
    connected = ChainConnectInput(theKymera->chain_input_handle, line_in_l, EPR_WIRED_STEREO_INPUT_L);
    if(line_in_r)
        connected = ChainConnectInput(theKymera->chain_input_handle, line_in_r, EPR_WIRED_STEREO_INPUT_R);

    /* Start the output chain regardless of whether the source was connected
    to the input chain. Failing to do so would mean audio would be unable
    to play a tone. This would cause kymera to lock, since it would never
    receive a KYMERA_OP_MSG_ID_TONE_END and the kymera lock would never
    be cleared. */
    KymeraOutput_ChainStart();
    Kymera_StartMusicProcessingChain();

    if (connected)
        ChainStart(theKymera->chain_input_handle);
}

static void kymeraWiredAnalog_CreateChain(const KYMERA_INTERNAL_WIRED_ANALOG_AUDIO_START_T *msg)
{

    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOG("kymeraWiredAnalog_CreateChain, creating output chain, completing startup");

    theKymera->output_rate = msg->rate;
    KymeraOutput_CreateChain(KICK_PERIOD_WIRED_ANALOG, 0, msg->volume_in_db);
    KymeraOutput_SetOperatorUcids(FALSE);
    appKymeraExternalAmpControl(TRUE);
    /* Create the wired analog chain */
    theKymera->chain_input_handle = PanicNull(ChainCreate(Kymera_GetChainConfigs()->chain_input_wired_analog_stereo_config));
    /* configure it */
    kymeraWiredAnalog_ConfigureChain(theKymera, msg->rate, msg->min_latency, msg->max_latency, msg->target_latency);
}

static void kymeraWiredAnalog_DestroyChain(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    PanicNull(theKymera->chain_input_handle);
    PanicNull(KymeraOutput_GetOutputHandle());

    Sink to_ttp_l = ChainGetInput(theKymera->chain_input_handle, EPR_WIRED_STEREO_INPUT_L);
    Sink to_ttp_r = ChainGetInput(theKymera->chain_input_handle, EPR_WIRED_STEREO_INPUT_R);

    Source from_ttp_l = ChainGetOutput(theKymera->chain_input_handle, EPR_SOURCE_DECODED_PCM);
    Source from_ttp_r = ChainGetOutput(theKymera->chain_input_handle, EPR_SOURCE_DECODED_PCM_RIGHT);
    

    DEBUG_LOG("kymeraWiredAnalog_DestroyChain, l-source(%p), r-source(%p)", from_ttp_l, from_ttp_r);
    DEBUG_LOG("kymeraWiredAnalog_DestroyChain, l-sink(%p), r-sink(%p)", to_ttp_l, to_ttp_r);

    /* A tone still playing at this point must be interruptable */
    appKymeraTonePromptStop();

    /* Stop chains before disconnecting */
    ChainStop(theKymera->chain_input_handle);
    /* Disable external amplifier if required */
    appKymeraExternalAmpControl(FALSE);

    /* Disconnect codec source from chain */
    StreamDisconnect(NULL, to_ttp_l);
    StreamDisconnect(NULL, to_ttp_r);

    /* Disconnect the chain output */
    StreamDisconnect(from_ttp_l, NULL);
    StreamDisconnect(from_ttp_r, NULL);

    Kymera_StopMusicProcessingChain();
    /* Stop and destroy the output chain */
    KymeraOutput_DestroyChain();
    Kymera_DestroyMusicProcessingChain();

    /* Destroy chains now that input has been disconnected */
    ChainDestroy(theKymera->chain_input_handle);
    theKymera->chain_input_handle = NULL;

}


/**************************************************************************************

                                               INTERFACE FUNCTIONS
**************************************************************************************/

/*************************************************************************/
void KymeraWiredAnalog_StartPlayingAudio(const KYMERA_INTERNAL_WIRED_ANALOG_AUDIO_START_T *msg)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOG("KymeraWiredAnalog_StartPlayingAudio, state %u, rate %u, latency %u", appKymeraGetState(), msg->rate, msg->target_latency);

    /* If there is a tone still playing at this point,
      * it must be an interruptable tone, so cut it off */
    if (appKymeraGetState() == KYMERA_STATE_TONE_PLAYING)
        appKymeraTonePromptStop();

    /* Can only start streaming if we're currently idle */
    PanicFalse(appKymeraGetState() == KYMERA_STATE_IDLE);
    /* Ensure there are no audio chains already */
    PanicNotNull(theKymera->chain_input_handle);
    PanicNotNull(KymeraOutput_GetOutputHandle());

    kymeraWiredAnalog_CreateChain(msg);
    Kymera_CreateMusicProcessingChain();
    Kymera_ConfigureMusicProcessing(msg->rate);
    appKymeraJoinInputOutputChains(theKymera);
    appKymeraSetState(KYMERA_STATE_WIRED_AUDIO_PLAYING);

    /* Set the DSP clock to low-power mode */
    appKymeraConfigureDspPowerMode();
    kymeraWiredAnalog_StartChains(theKymera);    
}

/*************************************************************************/
void KymeraWiredAnalog_StopPlayingAudio(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();

    DEBUG_LOG("KymeraWiredAnalog_StopPlayingAudio, state %u", appKymeraGetState());
    switch (appKymeraGetState())
    {
        case KYMERA_STATE_WIRED_AUDIO_PLAYING:
            kymeraWiredAnalog_DestroyChain();
            theKymera->output_rate = 0;
            appKymeraSetState(KYMERA_STATE_IDLE);
        break;

        case KYMERA_STATE_IDLE:
        break;

        default:
            // Report, but ignore attempts to stop in invalid states
            DEBUG_LOG("KymeraWiredAnalog_StopPlayingAudio, invalid state %u", appKymeraGetState());
        break;
    }
}

#endif /* INCLUDE_WIRED_ANALOG_AUDIO*/
