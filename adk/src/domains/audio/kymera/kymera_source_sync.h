/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\ingroup    kymera
\file
\brief      Private (internal) kymera header file for source sync capability.
*/

#ifndef KYMERA_SOURCE_SYNC_H
#define KYMERA_SOURCE_SYNC_H

#include "kymera_output_chain_config.h"
#include "chain.h"

/*! \brief Calculate and set in config the source sync input buffer size in samples.
    \param config The kick period and rate must be set.
    \param codec_block_size The maximum size block of PCM samples produced by the decoder per kick.
    \note This calculation is suitable for chains where any bulk latency is upstream
    of the decoder and the buffer between the decoder and the source sync is only
    required to hold sufficient samples to contain the codec processing block size.
*/
void appKymeraSetSourceSyncConfigInputBufferSize(kymera_output_chain_config *config, unsigned codec_block_size);

/*! \brief Calculate and set in config the source sync output buffer size in samples.
    \param config The kick period and rate must be set.
    \param kp_multiply The mulitple factor of the kick period.
    \param kp_divide The division factor of the kick period.
    \note The calculation is (kick_period * multiply) / divide microseconds
    converted to number of samples (at the defined rate).
*/
void appKymeraSetSourceSyncConfigOutputBufferSize(kymera_output_chain_config *config, unsigned kp_multiply, unsigned kp_divide);

/*! \brief Configure the source sync operator.
    \param chain The audio chain containing the source sync operator.
    \param config The audio output chain configuration.
    \param set_input_buffer Select whether the source sync input terminal buffer will be set.
*/
void appKymeraConfigureSourceSync(kymera_chain_handle_t chain, const kymera_output_chain_config *config, bool set_input_buffer);

#endif
