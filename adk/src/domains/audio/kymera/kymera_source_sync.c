/*!
\copyright  Copyright (c) 2017-2020  Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       kymera_source_sync.c
\brief      Kymera source sync configuration.
*/

#include "kymera_source_sync.h"
#include "kymera_private.h"
#include "stdlib.h"

/*! Convert a channel ID to a bit mask */
#define CHANNEL_TO_MASK(channel) ((uint32)1 << channel)

/*!@{ \name Port numbers for the Source Sync operator */
#define KYMERA_SOURCE_SYNC_INPUT_PORT (0)
#define KYMERA_SOURCE_SYNC_OUTPUT_PORT (0)

#define KYMERA_SOURCE_SYNC_INPUT_PORT_1 (1)
#define KYMERA_SOURCE_SYNC_OUTPUT_PORT_1 (1)
/*!@} */

/* Minimum source sync version that supports setting input terminal buffer size */
#define SET_TERMINAL_BUFFER_SIZE_MIN_VERSION 0x00030004
/* Minimum source sync version that supports setting kick back threshold */
#define SET_KICK_BACK_THRESHOLD_MIN_VERSION 0x00030004

/* Configuration of source sync groups and routes */
static const source_sync_sink_group_t sink_groups[] =
{
    {
        .meta_data_required = TRUE,
        .rate_match = FALSE,
        .channel_mask = CHANNEL_TO_MASK(KYMERA_SOURCE_SYNC_INPUT_PORT)
    }
#if INCLUDE_STEREO
    ,
    {
        .meta_data_required = TRUE,
        .rate_match = FALSE,
        .channel_mask = CHANNEL_TO_MASK(KYMERA_SOURCE_SYNC_INPUT_PORT_1)
    }
#endif
};

static const source_sync_source_group_t source_groups[] =
{
    {
        .meta_data_required = TRUE,
        .ttp_required = TRUE,
        .channel_mask = CHANNEL_TO_MASK(KYMERA_SOURCE_SYNC_OUTPUT_PORT)
    }
#if INCLUDE_STEREO
    ,
    {
        .meta_data_required = TRUE,
        .ttp_required = TRUE,
        .channel_mask = CHANNEL_TO_MASK(KYMERA_SOURCE_SYNC_OUTPUT_PORT_1)
    }
#endif
};

static source_sync_route_t routes[] =
{
    {
        .input_terminal = KYMERA_SOURCE_SYNC_INPUT_PORT,
        .output_terminal = KYMERA_SOURCE_SYNC_OUTPUT_PORT,
        .transition_samples = 0,
        .sample_rate = 0, /* Overridden later */
        .gain = 0
    }
#if INCLUDE_STEREO
    ,
    {
        .input_terminal = KYMERA_SOURCE_SYNC_INPUT_PORT_1,
        .output_terminal = KYMERA_SOURCE_SYNC_OUTPUT_PORT_1,
        .transition_samples = 0,
        .sample_rate = 0, /* Overridden later */
        .gain = 0
    }
#endif
};

static const standard_param_id_t sosy_min_period_id = 0;
static const standard_param_id_t sosy_max_period_id = 1;

static void appKymeraSetSourceSyncParameter(Operator op, standard_param_id_t id, standard_param_value_t value)
{
    set_params_data_t* set_params_data = OperatorsCreateSetParamsData(1);

    set_params_data->number_of_params = 1;
    set_params_data->standard_params[0].id = id;
    set_params_data->standard_params[0].value = value;

    OperatorsStandardSetParameters(op, set_params_data);
    free(set_params_data);
}

void appKymeraSetSourceSyncConfigInputBufferSize(kymera_output_chain_config *config, unsigned codec_block_size)
{
    /* This is the buffer size for a single kick period time. */
    unsigned unit_buffer_size = US_TO_BUFFER_SIZE_MONO_PCM(config->kick_period, config->rate);
    /* Note the +1 is due to Source Sync input quirk */
    config->source_sync_input_buffer_size_samples = unit_buffer_size + codec_block_size + 1;
}

void appKymeraSetSourceSyncConfigOutputBufferSize(kymera_output_chain_config *config,
                                                  unsigned kp_multiply, unsigned kp_divide)
{
    unsigned output_buffer_size_us = config->kick_period * kp_multiply;
    if (kp_divide > 1)
    {
        output_buffer_size_us /= kp_divide;
    }
    config->source_sync_output_buffer_size_samples = US_TO_BUFFER_SIZE_MONO_PCM(output_buffer_size_us, config->rate);
}

void appKymeraConfigureSourceSync(kymera_chain_handle_t chain,
                                  const kymera_output_chain_config *config,
                                  bool set_input_buffer)
{
    Operator op;
    /* Override sample rate in routes config */
    routes[0].sample_rate = config->rate;
#if INCLUDE_STEREO
    routes[1].sample_rate = config->rate;
#endif

    if (GET_OP_FROM_CHAIN(op, chain, OPR_SOURCE_SYNC))
    {
        capablity_version_t version_bits;
        uint32 version;


        /* Send operator configuration messages */
        OperatorsStandardSetSampleRate(op, config->rate);
        OperatorsSourceSyncSetSinkGroups(op, DIMENSION_AND_ADDR_OF(sink_groups));
        OperatorsSourceSyncSetSourceGroups(op, DIMENSION_AND_ADDR_OF(source_groups));
        OperatorsSourceSyncSetRoutes(op, DIMENSION_AND_ADDR_OF(routes));
        OperatorsStandardSetBufferSize(op, config->source_sync_output_buffer_size_samples);

        version_bits = OperatorGetCapabilityVersion(op);
        version = UINT32_BUILD(version_bits.version_msb, version_bits.version_lsb);

        if (set_input_buffer)
        {
            if (version >= SET_TERMINAL_BUFFER_SIZE_MIN_VERSION)
            {
                /* SourceSync can set its input buffer size as a latency buffer. */
                OperatorsStandardSetTerminalBufferSize(op,
                    config->source_sync_input_buffer_size_samples, 0xFFFF, 0);
            }
            else
            {
                DEBUG_LOG_ERROR("appKymeraConfigureSourceSync version 0x%x cannot set term buf size", version);
            }
        }
        if (config->set_source_sync_max_period)
        {
            appKymeraSetSourceSyncParameter(op, sosy_max_period_id, config->source_sync_max_period);
        }
        if (config->set_source_sync_min_period)
        {
            appKymeraSetSourceSyncParameter(op, sosy_min_period_id, config->source_sync_min_period);
        }
        if (config->set_source_sync_kick_back_threshold)
        {
            if (version >= SET_KICK_BACK_THRESHOLD_MIN_VERSION)
            {
                OperatorsStandardSetBackKickThreshold(op, -(int)config->source_sync_kick_back_threshold,
                                                      common_back_kick_mode_level,
                                                      (unsigned)-1);
            }
            else
            {
                DEBUG_LOG_ERROR("appKymeraConfigureSourceSync version 0x%x cannot set kick back threshold", version);
            }
        }
    }
}
