/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       kymera_output.c
\brief      Kymera module with audio output chain definitions

*/

#include <audio_output.h>

#include "kymera_private.h"
#include "kymera_aec.h"
#include "kymera_config.h"
#include "kymera_source_sync.h"

/*!@} */
#define DEFAULT_AEC_REF_TERMINAL_BUFFER_SIZE 15

/*! \brief The chain output channels */
typedef enum
{
    output_right_channel = 0,
    output_left_channel
}output_channel_t;

static void kymeraOutput_ConnectChainToAudioSink(const connect_audio_output_t *params)
{
#if defined(INCLUDE_KYMERA_AEC) || defined(ENABLE_ADAPTIVE_ANC)
    aec_connect_audio_output_t aec_connect_params = {0};
    aec_audio_config_t config = {0};
    aec_connect_params.input_1 = params->input_1;
    aec_connect_params.input_2 = params->input_2;

    config.spk_sample_rate = KymeraGetTaskData()->output_rate;
    config.ttp_delay = VA_MIC_TTP_LATENCY;

#ifdef ENABLE_AEC_LEAKTHROUGH
    config.is_source_clock_same = TRUE; //Same clock source for speaker and mic path for AEC-leakthrough.
                                        //Should have no implication on normal aec operation.
    config.buffer_size = DEFAULT_AEC_REF_TERMINAL_BUFFER_SIZE;
#endif

    Kymera_ConnectAudioOutputToAec(&aec_connect_params, &config);
#else
    Kymera_ConnectOutputSource(params->input_1,params->input_2,KymeraGetTaskData()->output_rate);
#endif
}

static void kymeraOutput_DisconnectChainFromAudioSink(const connect_audio_output_t *params)
{
#if defined(INCLUDE_KYMERA_AEC) || defined(ENABLE_ADAPTIVE_ANC)
    UNUSED(params);
    Kymera_DisconnectAudioOutputFromAec();
#else
    Kymera_DisconnectIfValid(params->input_1, NULL);
    Kymera_DisconnectIfValid(params->input_2, NULL);
    AudioOutputDisconnect();
#endif
}

static chain_endpoint_role_t kymeraOutput_GetOuputRole(output_channel_t is_left)
{
    chain_endpoint_role_t output_role;

    if(appConfigOutputIsStereo())
    {
        output_role = is_left ? EPR_SOURCE_STEREO_OUTPUT_L : EPR_SOURCE_STEREO_OUTPUT_R;
    }
    else
    {
        output_role = EPR_SOURCE_MIXER_OUT;
    }

    return output_role;
}

static void kymeraOutput_ConfigureOutputChainOperatorsWithConfig(kymera_chain_handle_t chain,
                                                             const kymera_output_chain_config *config)
{
    Operator volume_op;
    bool input_buffer_set = FALSE;

    if (config->source_sync_input_buffer_size_samples)
    {
        /* Not all chains have a separate latency buffer operator but if present
           then set the buffer size. Source Sync version X.X allows its input
           buffer size to be set, so chains using that version of source sync
           typically do not have a seperate latency buffer and the source sync
           input buffer size is set instead in appKymeraConfigureSourceSync(). */
        Operator op;
        if (GET_OP_FROM_CHAIN(op, chain, OPR_LATENCY_BUFFER))
        {
            OperatorsStandardSetBufferSize(op, config->source_sync_input_buffer_size_samples);
            /* Mark buffer size as done */
            input_buffer_set = TRUE;
        }
    }
    appKymeraConfigureSourceSync(chain, config, !input_buffer_set);

    volume_op = ChainGetOperatorByRole(chain, OPR_VOLUME_CONTROL);
    OperatorsStandardSetSampleRate(volume_op, config->rate);
    appKymeraSetVolume(chain, config->volume_in_db);
}

static void kymeraOutput_SetDefaultOutputChainConfig(kymera_output_chain_config *config,
                                                 uint32 rate, unsigned kick_period,
                                                 unsigned buffer_size, int16 volume_in_db)
{
    memset(config, 0, sizeof(*config));
    config->rate = rate;
    config->kick_period = kick_period;
    config->volume_in_db = volume_in_db;
    config->source_sync_input_buffer_size_samples = buffer_size;
    /* By default or for source sync version <=3.3 the output buffer needs to
       be able to hold at least SS_MAX_PERIOD worth  of audio (default = 2 *
       Kp), but be less than SS_MAX_LATENCY (5 * Kp). The recommendation is 2 Kp
       more than SS_MAX_PERIOD, so 4 * Kp. */
    appKymeraSetSourceSyncConfigOutputBufferSize(config, 4, 0);
}

/*! \brief Create only the output audio chain */
static void kymeraOutput_CreateOutputChainOnly(const kymera_output_chain_config *config)
{
    kymera_chain_handle_t chain;

    DEBUG_LOG("kymeraOutput_CreateOutputChainOnly");

    /* Setting leakthrough mic path sample rate same as speaker path sampling rate */
    Kymera_setLeakthroughMicSampleRate(config->rate);

    /* Create chain */
    chain = ChainCreate(Kymera_GetChainConfigs()->chain_output_volume_config);
    KymeraGetTaskData()->chainu.output_vol_handle = chain;
    kymeraOutput_ConfigureOutputChainOperatorsWithConfig(chain, config);
    PanicFalse(OperatorsFrameworkSetKickPeriod(config->kick_period));

    ChainConnect(chain);
}

/*! \brief Create only the audio output - e.g. the DACs */
static void KymeraOutput_CreateOutput(void)
{
    connect_audio_output_t connect_params = {0};

    DEBUG_LOG_FN_ENTRY("KymeraOutput_CreateOutput");

    connect_params.input_1 = ChainGetOutput(KymeraGetTaskData()->chainu.output_vol_handle, kymeraOutput_GetOuputRole(output_left_channel));

    if(appConfigOutputIsStereo())
    {
        connect_params.input_2 = ChainGetOutput(KymeraGetTaskData()->chainu.output_vol_handle, kymeraOutput_GetOuputRole(output_right_channel));
    }

    kymeraOutput_ConnectChainToAudioSink(&connect_params);
}

void KymeraOutput_ConfigureChainOperators(kymera_chain_handle_t chain,
                                            uint32 sample_rate, unsigned kick_period,
                                            unsigned buffer_size, int16 volume_in_db)
{
    kymera_output_chain_config config;
    kymeraOutput_SetDefaultOutputChainConfig(&config, sample_rate, kick_period, buffer_size, volume_in_db);
    kymeraOutput_ConfigureOutputChainOperatorsWithConfig(chain, &config);
}

void KymeraOutput_CreateChainWithConfig(const kymera_output_chain_config *config)
{
    kymeraOutput_CreateOutputChainOnly(config);
    KymeraOutput_CreateOutput();
}

void KymeraOutput_CreateChain(unsigned kick_period, unsigned buffer_size, int16 volume_in_db)
{
    kymera_output_chain_config config;
    kymeraOutput_SetDefaultOutputChainConfig(&config, KymeraGetTaskData()->output_rate, kick_period, buffer_size, volume_in_db);
    kymeraOutput_CreateOutputChainOnly(&config);
    KymeraOutput_CreateOutput();
}

void KymeraOutput_DestroyChain(void)
{
    kymera_chain_handle_t chain = KymeraGetTaskData()->chainu.output_vol_handle;
    Kymera_setLeakthroughMicSampleRate(DEFAULT_MIC_RATE);

    DEBUG_LOG("KymeraOutput_DestroyChain");

    if (chain)
    {
#ifdef INCLUDE_MIRRORING
        /* Destroying the output chain powers-off the DSP, if another
        prompt or activity is pending, the DSP has to start all over again
        which takes a long time. Therefore prospectively power on the DSP
        before destroying the output chain, which will avoid an unneccesary
        power-off/on */
        appKymeraProspectiveDspPowerOn();
#endif
        connect_audio_output_t disconnect_params = {0};
        disconnect_params.input_1 = ChainGetOutput(chain, kymeraOutput_GetOuputRole(output_left_channel));
        disconnect_params.input_2 = ChainGetOutput(chain, kymeraOutput_GetOuputRole(output_right_channel));

        ChainStop(chain);
        kymeraOutput_DisconnectChainFromAudioSink(&disconnect_params);

        ChainDestroy(chain);
        KymeraGetTaskData()->chainu.output_vol_handle = NULL;
    }
}

void KymeraOutput_SetOperatorUcids(bool is_sco)
{
    /* Operators that have UCID set either reside in sco_handle or output_vol_handle
       which are both in the chainu union. */
    kymera_chain_handle_t chain = KymeraGetTaskData()->chainu.output_vol_handle;

    if (is_sco)
    {
        /* SCO/MIC forwarding RX chains do not have CVC send or receive */
        Kymera_SetOperatorUcid(chain, OPR_CVC_SEND, UCID_CVC_SEND);
        #ifdef INCLUDE_SPEAKER_EQ
            Kymera_SetOperatorUcid(chain, OPR_CVC_RECEIVE, UCID_CVC_RECEIVE_EQ);
        #else
            Kymera_SetOperatorUcid(chain, OPR_CVC_RECEIVE, UCID_CVC_RECEIVE);
        #endif
    }

    /* Operators common to SCO/A2DP chains */
    PanicFalse(Kymera_SetOperatorUcid(chain, OPR_VOLUME_CONTROL, UCID_VOLUME_CONTROL));
    PanicFalse(Kymera_SetOperatorUcid(chain, OPR_SOURCE_SYNC, UCID_SOURCE_SYNC));
}

Operator KymeraOutput_ChainGetOperatorByRole(unsigned operator_role)
{
    return ChainGetOperatorByRole(KymeraGetTaskData()->chainu.output_vol_handle, operator_role);
}

void  KymeraOutput_ChainStart(void)
{
    ChainStart(KymeraGetTaskData()->chainu.output_vol_handle);
}

kymera_chain_handle_t KymeraOutput_GetOutputHandle(void)
{
    return KymeraGetTaskData()->chainu.output_vol_handle;
}

void KymeraOutput_SetVolume(int16 volume_in_db)
{
    appKymeraSetVolume(KymeraGetTaskData()->chainu.output_vol_handle, volume_in_db);
}

void KymeraOutput_SetMainVolume(int16 volume_in_db)
{
    appKymeraSetMainVolume(KymeraGetTaskData()->chainu.output_vol_handle, volume_in_db);
}

Source KymeraOutput_ChainGetOutput(unsigned output_role)
{
    return ChainGetOutput(KymeraGetTaskData()->chain_input_handle, output_role);
}

bool KymeraOutput_ConnectSource(Source source, unsigned input_role)
{
    return ChainConnectInput(KymeraGetTaskData()->chainu.output_vol_handle,source, input_role);
}

