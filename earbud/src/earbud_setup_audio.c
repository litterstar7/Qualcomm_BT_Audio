/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Pre and post init audio setup.

*/

#include "earbud_cap_ids.h"

#include "chains/chain_aptx_ad_tws_plus_decoder.h"
#include "chains/chain_aptx_mono_no_autosync_decoder.h"
#include "chains/chain_sbc_mono_no_autosync_decoder.h"
#include "chains/chain_aac_stereo_decoder_left.h"
#include "chains/chain_aac_stereo_decoder_right.h"

#include "chains/chain_forwarding_input_sbc_left.h"
#include "chains/chain_forwarding_input_sbc_right.h"
#include "chains/chain_forwarding_input_sbc_left_no_pcm_buffer.h"
#include "chains/chain_forwarding_input_sbc_right_no_pcm_buffer.h"
#include "chains/chain_forwarding_input_aac_left.h"
#include "chains/chain_forwarding_input_aac_right.h"
#include "chains/chain_forwarding_input_aac_stereo_left.h"
#include "chains/chain_forwarding_input_aac_stereo_right.h"
#include "chains/chain_forwarding_input_aptx_left.h"
#include "chains/chain_forwarding_input_aptx_right.h"
#include "chains/chain_forwarding_input_aptx_left_no_pcm_buffer.h"
#include "chains/chain_forwarding_input_aptx_right_no_pcm_buffer.h"

#include "chains/chain_input_aac_stereo_mix.h"
#include "chains/chain_input_sbc_stereo_mix.h"
#include "chains/chain_input_aptx_stereo_mix.h"

#ifdef APTX_ADAPTIVE_ON_P0
#include "chain_input_aptx_adaptive_stereo_mix_p0.h"
#include "chain_input_aptx_adaptive_stereo_mix_q2q_p0.h"
#else
#include "chain_input_aptx_adaptive_stereo_mix.h"
#include "chain_input_aptx_adaptive_stereo_mix_q2q.h"
#endif
#include "chains/chain_aec.h"
#ifdef INCLUDE_MIRRORING
#include "chains/chain_output_volume_mirror.h"
#define CHAIN_OUTPUT_VOLUME_CONFIG chain_output_volume_mirror_config
#else
#include "chains/chain_output_volume.h"
#define CHAIN_OUTPUT_VOLUME_CONFIG chain_output_volume_config
#endif

#include "chains/chain_tone_gen.h"
#include "chains/chain_tone_gen_no_iir.h"
#include "chains/chain_prompt_decoder.h"
#include "chains/chain_prompt_decoder_no_iir.h"
#include "chains/chain_prompt_pcm.h"

#include "chains/chain_sco_nb.h"
#include "chains/chain_sco_wb.h"
#include "chains/chain_sco_swb.h"
#include "chains/chain_micfwd_wb.h"
#include "chains/chain_micfwd_nb.h"
#include "chains/chain_scofwd_wb.h"
#include "chains/chain_scofwd_nb.h"

#include "chains/chain_sco_nb_2mic.h"
#include "chains/chain_sco_wb_2mic.h"
#include "chains/chain_sco_swb_2mic.h"
#include "chains/chain_sco_nb_3mic.h"
#include "chains/chain_sco_wb_3mic.h"
#include "chains/chain_sco_swb_3mic.h"
#include "chains/chain_micfwd_wb_2mic.h"
#include "chains/chain_micfwd_nb_2mic.h"
#include "chains/chain_scofwd_wb_2mic.h"
#include "chains/chain_scofwd_nb_2mic.h"

#include "chains/chain_micfwd_send.h"
#include "chains/chain_scofwd_recv.h"
#include "chains/chain_micfwd_send_2mic.h"
#include "chains/chain_scofwd_recv_2mic.h"

#include "chains/chain_va_encode_msbc.h"
#include "chains/chain_va_encode_opus.h"
#include "chains/chain_va_encode_sbc.h"
#include "chains/chain_va_mic_1mic_cvc.h"
#include "chains/chain_va_mic_1mic_cvc_wuw.h"
#include "chains/chain_va_mic_2mic_cvc.h"
#include "chains/chain_va_mic_2mic_cvc_wuw.h"
#include "chains/chain_va_wuw_qva.h"
#include "chains/chain_va_wuw_gva.h"
#include "chains/chain_va_wuw_apva.h"

#include "chains/chain_music_processing.h"
#include "chains/chain_aanc.h"
#include "chains/chain_aanc_resampler_sco_tx_path.h"
#include "chains/chain_aanc_resampler_sco_rx_path.h"
#include "chains/chain_aanc_splitter_sco_rx_path.h"
#include "chains/chain_aanc_splitter_sco_tx_path.h"
#include "chains/chain_aanc_consumer.h"

#include "chains/chain_mic_resampler.h"

#include "earbud_setup_audio.h"

#include "kymera.h"

static const capability_bundle_t capability_bundle[] =
{
#ifdef DOWNLOAD_SWITCHED_PASSTHROUGH
    {
        "download_switched_passthrough_consumer.edkcs",
        capability_bundle_available_p0
    },
#endif
#ifdef DOWNLOAD_APTX_CLASSIC_DEMUX
    {
        "download_aptx_demux.edkcs",
        capability_bundle_available_p0
    },
#endif
#ifdef DOWNLOAD_AEC_REF
    {
#ifdef CORVUS_YD300
        "download_aec_reference.dkcs",
#else
        "download_aec_reference.edkcs",
#endif
        capability_bundle_available_p0
    },
#endif
#ifdef DOWNLOAD_ADAPTIVE_ANC
    {
        "download_aanc.edkcs",
        capability_bundle_available_p0
    },
#endif
#ifdef DOWNLOAD_APTX_ADAPTIVE_DECODE
    {
        "download_aptx_adaptive_decode.edkcs",
        capability_bundle_available_p0
    },
#endif
#if defined(DOWNLOAD_ASYNC_WBS_DEC) || defined(DOWNLOAD_ASYNC_WBS_ENC)
    /*  Chains for SCO forwarding.
        Likely to update to use the downloadable AEC regardless
        as offers better TTP support (synchronisation) and other
        extensions */
    {
        "download_async_wbs.edkcs",
        capability_bundle_available_p0
    },
#endif
#ifdef DOWNLOAD_VOLUME_CONTROL
    {
        "download_volume_control.edkcs",
        capability_bundle_available_p0
    },
#endif
#ifdef DOWNLOAD_VA_GRAPH_MANAGER
    {
        "download_va_graph_manager.edkcs",
        capability_bundle_available_p0
    },
#endif
#ifdef DOWNLOAD_CVC_FBC
    {
        "download_cvc_fbc.edkcs",
        capability_bundle_available_p0
    },
#endif
#ifdef DOWNLOAD_GVA
    {
        "download_gva.dkcs",
        capability_bundle_available_p0
    },
#endif
#ifdef DOWNLOAD_APVA
    {
        "download_apva.dkcs",
        capability_bundle_available_p0
    },
#endif
#ifdef DOWNLOAD_CVC_3MIC
    {
        "download_cvc_send_internal_mic.dkcs",
        capability_bundle_available_p0
    },
#endif
#ifdef DOWNLOAD_OPUS_CELT_ENCODE
    {
        "download_opus_celt_encode.edkcs",
        capability_bundle_available_p0
    },
#endif
    {
        0, 0
    }
};

static const capability_bundle_config_t bundle_config = {capability_bundle, ARRAY_DIM(capability_bundle) - 1};

static const kymera_chain_configs_t chain_configs = {
        .chain_aptx_ad_tws_plus_decoder_config = &chain_aptx_ad_tws_plus_decoder_config,
        .chain_aptx_mono_no_autosync_decoder_config = &chain_aptx_mono_no_autosync_decoder_config,
        .chain_sbc_mono_no_autosync_decoder_config = &chain_sbc_mono_no_autosync_decoder_config,
        .chain_aac_stereo_decoder_left_config = &chain_aac_stereo_decoder_left_config,
        .chain_aac_stereo_decoder_right_config = &chain_aac_stereo_decoder_right_config,
        .chain_forwarding_input_sbc_left_config = &chain_forwarding_input_sbc_left_config,
        .chain_forwarding_input_sbc_right_config = &chain_forwarding_input_sbc_right_config,
        .chain_forwarding_input_sbc_left_no_pcm_buffer_config = &chain_forwarding_input_sbc_left_no_pcm_buffer_config,
        .chain_forwarding_input_sbc_right_no_pcm_buffer_config = &chain_forwarding_input_sbc_right_no_pcm_buffer_config,
        .chain_forwarding_input_aptx_left_config = &chain_forwarding_input_aptx_left_config,
        .chain_forwarding_input_aptx_right_config = &chain_forwarding_input_aptx_right_config,
        .chain_forwarding_input_aptx_left_no_pcm_buffer_config = &chain_forwarding_input_aptx_left_no_pcm_buffer_config,
        .chain_forwarding_input_aptx_right_no_pcm_buffer_config = &chain_forwarding_input_aptx_right_no_pcm_buffer_config,
        .chain_forwarding_input_aac_left_config = &chain_forwarding_input_aac_left_config,
        .chain_forwarding_input_aac_right_config = &chain_forwarding_input_aac_right_config,
        .chain_forwarding_input_aac_stereo_left_config = &chain_forwarding_input_aac_stereo_left_config,
        .chain_forwarding_input_aac_stereo_right_config = &chain_forwarding_input_aac_stereo_right_config,
        .chain_input_aac_stereo_mix_config = &chain_input_aac_stereo_mix_config,
        .chain_input_sbc_stereo_mix_config = &chain_input_sbc_stereo_mix_config,
        .chain_input_aptx_stereo_mix_config = &chain_input_aptx_stereo_mix_config,
        .chain_aec_config = &chain_aec_config,
        .chain_output_volume_config = &CHAIN_OUTPUT_VOLUME_CONFIG,
        .chain_tone_gen_config = &chain_tone_gen_config,
        .chain_tone_gen_no_iir_config = &chain_tone_gen_no_iir_config,
        .chain_prompt_decoder_config = &chain_prompt_decoder_config,
        .chain_prompt_decoder_no_iir_config = &chain_prompt_decoder_no_iir_config,
        .chain_prompt_pcm_config = &chain_prompt_pcm_config,
        .chain_aanc_config = &chain_aanc_config,
        .chain_aanc_resampler_sco_tx_path_config = &chain_aanc_resampler_sco_tx_path_config,
        .chain_aanc_resampler_sco_rx_path_config = &chain_aanc_resampler_sco_rx_path_config,
        .chain_aanc_splitter_sco_rx_path_config = &chain_aanc_splitter_sco_rx_path_config,
        .chain_aanc_splitter_sco_tx_path_config = &chain_aanc_splitter_sco_tx_path_config,
        .chain_aanc_consumer  = &chain_aanc_consumer_config,
#ifdef APTX_ADAPTIVE_ON_P0
        .chain_input_aptx_adaptive_stereo_mix_config = &chain_input_aptx_adaptive_stereo_mix_p0_config,
        .chain_input_aptx_adaptive_stereo_mix_q2q_config = &chain_input_aptx_adaptive_stereo_mix_q2q_p0_config,
#else
        .chain_input_aptx_adaptive_stereo_mix_config = &chain_input_aptx_adaptive_stereo_mix_config,
        .chain_input_aptx_adaptive_stereo_mix_q2q_config = &chain_input_aptx_adaptive_stereo_mix_q2q_config,   
#endif
#ifdef INCLUDE_SPEAKER_EQ
        .chain_music_processing_config = &chain_music_processing_config,
#endif
#ifdef INCLUDE_MIC_CONCURRENCY
        .chain_mic_resampler_config = &chain_mic_resampler_config,
#endif
};


const appKymeraVaEncodeChainInfo va_encode_chain_info[] =
{
    {va_audio_codec_sbc, &chain_va_encode_sbc_config},
    {va_audio_codec_msbc, &chain_va_encode_msbc_config},
    {va_audio_codec_opus, &chain_va_encode_opus_config}
};

static const appKymeraVaEncodeChainTable va_encode_chain_table =
{
    .chain_table = va_encode_chain_info,
    .table_length = ARRAY_DIM(va_encode_chain_info)
};

static const appKymeraVaMicChainInfo va_mic_chain_info[] =
{
  /*{  WuW,   CVC, mics, chain_to_use}*/
#ifdef INCLUDE_WUW
    { TRUE,  TRUE,    1, &chain_va_mic_1mic_cvc_wuw_config},
    { TRUE,  TRUE,    2, &chain_va_mic_2mic_cvc_wuw_config},
#endif /* INCLUDE_WUW */
    {FALSE,  TRUE,    1, &chain_va_mic_1mic_cvc_config},
    {FALSE,  TRUE,    2, &chain_va_mic_2mic_cvc_config}
};

static const appKymeraVaMicChainTable va_mic_chain_table =
{
    .chain_table = va_mic_chain_info,
    .table_length = ARRAY_DIM(va_mic_chain_info)
};

#ifdef INCLUDE_WUW
static const appKymeraVaWuwChainInfo va_wuw_chain_info[] =
{
    {va_wuw_engine_qva, &chain_va_wuw_qva_config},
    {va_wuw_engine_gva, &chain_va_wuw_gva_config},
    {va_wuw_engine_apva, &chain_va_wuw_apva_config}
};

static const appKymeraVaWuwChainTable va_wuw_chain_table =
{
    .chain_table = va_wuw_chain_info,
    .table_length = ARRAY_DIM(va_wuw_chain_info)
};
#endif /* INCLUDE_WUW */

const appKymeraScoChainInfo kymera_sco_chain_table[] =
{
  /* sco_mode sco_fwd mic_fwd mic_cfg   chain                        rate */
  { SCO_NB,   FALSE,  FALSE,  1,        &chain_sco_nb_config,          8000 },
  { SCO_WB,   FALSE,  FALSE,  1,        &chain_sco_wb_config,         16000 },
  { SCO_SWB,  FALSE,  FALSE,  1,        &chain_sco_swb_config,        32000 },
  { SCO_NB,   TRUE,   FALSE,  1,        &chain_scofwd_nb_config,       8000 },
  { SCO_WB,   TRUE,   FALSE,  1,        &chain_scofwd_wb_config,      16000 },
  { SCO_NB,   TRUE,   TRUE,   1,        &chain_micfwd_nb_config,       8000 },
  { SCO_WB,   TRUE,   TRUE,   1,        &chain_micfwd_wb_config,      16000 },

  { SCO_NB,   FALSE,  FALSE,  2,        &chain_sco_nb_2mic_config,     8000 },
  { SCO_WB,   FALSE,  FALSE,  2,        &chain_sco_wb_2mic_config,    16000 },
  { SCO_SWB,  FALSE,  FALSE,  2,        &chain_sco_swb_2mic_config,   32000 },
  { SCO_NB,   TRUE,   FALSE,  2,        &chain_scofwd_nb_2mic_config,  8000 },
  { SCO_WB,   TRUE,   FALSE,  2,        &chain_scofwd_wb_2mic_config, 16000 },
  { SCO_NB,   TRUE,   TRUE,   2,        &chain_micfwd_nb_2mic_config,  8000 },
  { SCO_WB,   TRUE,   TRUE,   2,        &chain_micfwd_wb_2mic_config, 16000 },

  { SCO_NB,   FALSE,  FALSE,  3,        &chain_sco_nb_3mic_config,     8000 },
  { SCO_WB,   FALSE,  FALSE,  3,        &chain_sco_wb_3mic_config,    16000 },
  { SCO_SWB,  FALSE,  FALSE,  3,        &chain_sco_swb_3mic_config,   32000 },

  { NO_SCO }
};

const appKymeraScoChainInfo kymera_sco_slave_chain_table[] =
{
  /* sco_mode sco_fwd mic_fwd mic_cfg   chain                              rate */
  { SCO_WB,   FALSE,  FALSE,  1,        &chain_scofwd_recv_config,         16000 },
  { SCO_WB,   FALSE,  TRUE,   1,        &chain_micfwd_send_config,         16000 },
  { SCO_WB,   FALSE,  FALSE,  2,        &chain_scofwd_recv_2mic_config,    16000 },
  { SCO_WB,   FALSE,  TRUE,   2,        &chain_micfwd_send_2mic_config,    16000 },
  { NO_SCO }
};

const audio_output_config_t audio_hw_output_config =
{
    .mapping = {
        {0,audio_output_type_dac,audio_output_hardware_instance_0, audio_output_channel_a }
    },
    .output_resolution_mode = audio_output_16_bit
};

void Earbud_SetBundlesConfig(void)
{
    Kymera_SetBundleConfig(&bundle_config);
}

void Earbud_SetupAudio(void)
{
    Kymera_SetChainConfigs(&chain_configs);

    Kymera_SetScoChainTable(kymera_sco_chain_table);
    Kymera_SetScoSlaveChainTable(kymera_sco_slave_chain_table);
    Kymera_SetVaMicChainTable(&va_mic_chain_table);
    Kymera_SetVaEncodeChainTable(&va_encode_chain_table);
#ifdef INCLUDE_WUW
    Kymera_SetVaWuwChainTable(&va_wuw_chain_table);
#endif /* INCLUDE_WUW */
    AudioOutputInit(&audio_hw_output_config);

}
