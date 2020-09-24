/****************************************************************************
 * Copyright (c) 2015 - 2019 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \defgroup AANC
 * \ingroup capabilities
 * \file  aanc_proc.c
 * \ingroup AANC
 *
 * AANC processing library.
 */

#include "aanc_proc.h"

/******************************************************************************
Private Function Definitions
*/

/**
 * \brief  Create ED100 library objects.
 *
 * \param  pp_params  Address of the pointer to ED100 parameters to be
 *                    allocated.
 * \param  pp_stats  Address of the pointer to ED100 statistics to be
 *                   allocated.
 * \param  pp_ed  Address of the pointer to ED100 object to be
 *                allocated.
  * \param  sample_rate  The sample rate for the ED object.
 *
 * \return  boolean indicating success or failure.
 */
static bool aanc_proc_create_ed(ED100_PARAMETERS **pp_params,
                                ED100_STATISTICS **pp_stats,
                                void **pp_ed,
                                unsigned sample_rate)
{
    /* Allocate parameters */
    *pp_params = xzpnew(ED100_PARAMETERS);
    if (*pp_params == NULL)
    {
        return FALSE;
    }

    /* Allocate statistics */
    *pp_stats = xzpnew(ED100_STATISTICS);
    if (*pp_stats == NULL)
    {
        pdelete(*pp_params);
        *pp_params = NULL;

        return FALSE;
    }

    /* Create the ED object */
    if (!aanc_ed100_create(pp_ed, *pp_params, *pp_stats, sample_rate))
    {
        pdelete(*pp_stats);
        *pp_stats = NULL;
        pdelete(*pp_params);
        *pp_params = NULL;

        return FALSE;
    }

    return TRUE;
}

/**
 * \brief  Destroy ED100 library objects.
 *
 * \param  pp_params  Address of the pointer to ED100 parameters.
 * \param  pp_stats  Address of the pointer to ED100 statistics.
 * \param  pp_ed  Address of the pointer to ED100 object.
 *
 * \return  boolean indicating success or failure.
 */
static void aanc_proc_destroy_ed(ED100_PARAMETERS **pp_params,
                                 ED100_STATISTICS **pp_stats,
                                 void **pp_ed)
{
    pdelete(*pp_params);
    pdelete(*pp_stats);

    aanc_ed100_destroy(pp_ed);

    return;
}

/**
 * \brief  Create cbuffers with a particular malloc preference.
 *
 * \param  pp_buffer  Address of the pointer to Cbuffer to be allocated.
 * \param  malloc_pref  Malloc preference.
 *
 * \return  boolean indicating success or failure.
 */
static bool aanc_proc_create_cbuffer(tCbuffer **pp_buffer, unsigned malloc_pref)
{
    /* Allocate buffer memory explicitly */
    int *ptr = xzppnewn(AANC_DEFAULT_BUFFER_SIZE, int, malloc_pref);

    if (ptr == NULL)
    {
        return FALSE;
    }

    /* Wrap allocated memory in a cbuffer */
    *pp_buffer = cbuffer_create(ptr, AANC_DEFAULT_BUFFER_SIZE,
                               BUF_DESC_SW_BUFFER);
    if (*pp_buffer == NULL)
    {
        pdelete(ptr);
        ptr = NULL;

        return FALSE;
    }

    return TRUE;
}

/******************************************************************************
Public Function Implementations
*/

bool aanc_proc_create(ADAPTIVE_GAIN **pp_ag, unsigned sample_rate)
{

    ADAPTIVE_GAIN *p_ag = xzpnew(ADAPTIVE_GAIN);
    if (p_ag == NULL)
    {
        *pp_ag = NULL;
        L2_DBG_MSG("AANC_PROC failed to allocate adaptive gain");
        return FALSE;
    }

    *pp_ag = p_ag;

    p_ag->p_aanc_reinit_flag = NULL;

    /* Allocate internal input cbuffer in DM1 */
    if (!aanc_proc_create_cbuffer(&p_ag->p_tmp_int_ip, MALLOC_PREFERENCE_DM1))
    {
        aanc_proc_destroy(pp_ag);
        L2_DBG_MSG("AANC_PROC failed to allocate int mic input buffer");
        return FALSE;
    }

    /* Allocate external input cbuffer in DM2 */
    if (!aanc_proc_create_cbuffer(&p_ag->p_tmp_ext_ip, MALLOC_PREFERENCE_DM2))
    {
        aanc_proc_destroy(pp_ag);
        L2_DBG_MSG("AANC_PROC failed to allocate ext mic input buffer");
        return FALSE;
    }

    /* Create playback cbuffer without specific bank allocation */
    p_ag->p_tmp_pb_ip = cbuffer_create_with_malloc(AANC_DEFAULT_BUFFER_SIZE,
                                                   BUF_DESC_SW_BUFFER);
    if (p_ag->p_tmp_pb_ip == NULL)
    {
        aanc_proc_destroy(pp_ag);
        L2_DBG_MSG("AANC_PROC failed to allocate playback cbuffer");
        return FALSE;
    }

    /* Create EDs */
    if (!aanc_proc_create_ed(&p_ag->p_ed_int_params, &p_ag->p_ed_int_stats,
                             &p_ag->p_ed_int, sample_rate))
    {
        aanc_proc_destroy(pp_ag);
        L2_DBG_MSG("AANC_PROC failed to create int mic ED");
        return FALSE;
    }

    if (!aanc_proc_create_ed(&p_ag->p_ed_ext_params, &p_ag->p_ed_ext_stats,
                             &p_ag->p_ed_ext, sample_rate))
    {
        aanc_proc_destroy(pp_ag);
        L2_DBG_MSG("AANC_PROC failed to create ext mic ED");
        return FALSE;
    }

    if (!aanc_proc_create_ed(&p_ag->p_ed_pb_params, &p_ag->p_ed_pb_stats,
                             &p_ag->p_ed_pb, sample_rate))
    {
        aanc_proc_destroy(pp_ag);
        L2_DBG_MSG("AANC_PROC failed to create playback mic ED");
        return FALSE;
    }

    /* Allocate int mic output cbuffer in DM2 */
    if (!aanc_proc_create_cbuffer(&p_ag->p_tmp_int_op, MALLOC_PREFERENCE_DM2))
    {
        aanc_proc_destroy(pp_ag);
        L2_DBG_MSG("AANC_PROC failed to allocate int mic output buffer");
        return FALSE;
    }

    /* Allocate ext mic output cbuffer in DM2 */
    if (!aanc_proc_create_cbuffer(&p_ag->p_tmp_ext_op, MALLOC_PREFERENCE_DM2))
    {
        aanc_proc_destroy(pp_ag);
        L2_DBG_MSG("AANC_PROC failed to allocate ext mic output buffer");
        return FALSE;
    }

    /* Allocate and zero FXLMS parameters */
    p_ag->p_fxlms_params = xzpnew(FXLMS100_PARAMETERS);
    if (p_ag->p_fxlms_params == NULL)
    {
        aanc_proc_destroy(pp_ag);
        L2_DBG_MSG("AANC_PROC failed to create fxlms parameters");
        return FALSE;
    }

    /* Allocate and zero FXLMS statistics */
    p_ag->p_fxlms_stats = xzpnew(FXLMS100_STATISTICS);
    if (p_ag->p_fxlms_stats == NULL)
    {
        aanc_proc_destroy(pp_ag);
        L2_DBG_MSG("AANC_PROC failed to create fxlms statistics");
        return FALSE;
    }

    /* Create FXLMS data structure */
    if (!aanc_fxlms100_create(&p_ag->p_fxlms, p_ag->p_fxlms_params,
                              p_ag->p_fxlms_stats, AANC_PROC_NUM_TAPS_BP))
    {
        aanc_proc_destroy(pp_ag);
        L2_DBG_MSG("AANC_PROC failed to create fxlms");
        return FALSE;
    }

    p_ag->clip_threshold = AANC_PROC_CLIPPING_THRESHOLD;

    return TRUE;
}

bool aanc_proc_destroy(ADAPTIVE_GAIN **pp_ag)
{
    if (*pp_ag == NULL)
    {
        return TRUE;
    }

    ADAPTIVE_GAIN *p_ag = *pp_ag;

    aanc_fxlms100_destroy(&p_ag->p_fxlms);
    pdelete(p_ag->p_fxlms_params);
    pdelete(p_ag->p_fxlms_stats);

    aanc_proc_destroy_ed(&p_ag->p_ed_int_params, &p_ag->p_ed_int_stats,
                         &p_ag->p_ed_int);
    aanc_proc_destroy_ed(&p_ag->p_ed_ext_params, &p_ag->p_ed_ext_stats,
                         &p_ag->p_ed_ext);
    aanc_proc_destroy_ed(&p_ag->p_ed_pb_params, &p_ag->p_ed_pb_stats,
                         &p_ag->p_ed_pb);

    cbuffer_destroy(p_ag->p_tmp_int_ip);
    cbuffer_destroy(p_ag->p_tmp_ext_ip);
    cbuffer_destroy(p_ag->p_tmp_pb_ip);

    cbuffer_destroy(p_ag->p_tmp_int_op);
    cbuffer_destroy(p_ag->p_tmp_ext_op);

    pdelete(p_ag);
    *pp_ag = NULL;

    return TRUE;
}

bool aanc_proc_initialize(AANC_PARAMETERS *p_params, ADAPTIVE_GAIN *p_ag,
                          unsigned ag_start, unsigned *p_flags,
                          bool hard_initialize)
{
    /* Initialize pointers to parameters and flags */
    p_ag->p_aanc_params = p_params;
    p_ag->p_aanc_flags = p_flags;

    /* Initialize the FXLMS */
    FXLMS100_PARAMETERS *p_fxlms_params = p_ag->p_fxlms_params;

    p_fxlms_params->target_nr = 0;
    p_fxlms_params->mu = p_params->OFFSET_MU;
    p_fxlms_params->gamma = p_params->OFFSET_GAMMA;
    p_fxlms_params->lambda = p_params->OFFSET_LAMBDA;
    p_fxlms_params->frame_size = AANC_DEFAULT_FRAME_SIZE;
    p_fxlms_params->min_bound = p_params->OFFSET_FXLMS_MIN_BOUND;
    p_fxlms_params->max_bound = p_params->OFFSET_FXLMS_MAX_BOUND;
    p_fxlms_params->max_delta = p_params->OFFSET_FXLMS_MAX_DELTA;

    if (hard_initialize)
    {
        p_fxlms_params->initial_gain = ag_start;
    }

    int bp_num_coeffs_int[] = {
        p_params->OFFSET_BPF_NUMERATOR_COEFF_INT_0,
        p_params->OFFSET_BPF_NUMERATOR_COEFF_INT_1,
        p_params->OFFSET_BPF_NUMERATOR_COEFF_INT_2,
        p_params->OFFSET_BPF_NUMERATOR_COEFF_INT_3,
        p_params->OFFSET_BPF_NUMERATOR_COEFF_INT_4
    };
    p_fxlms_params->p_bp_coeffs_num_int = bp_num_coeffs_int;

    int bp_den_coeffs_int[] = {
        p_params->OFFSET_BPF_DENOMINATOR_COEFF_INT_0,
        p_params->OFFSET_BPF_DENOMINATOR_COEFF_INT_1,
        p_params->OFFSET_BPF_DENOMINATOR_COEFF_INT_2,
        p_params->OFFSET_BPF_DENOMINATOR_COEFF_INT_3,
        p_params->OFFSET_BPF_DENOMINATOR_COEFF_INT_4
    };
    p_fxlms_params->p_bp_coeffs_den_int = bp_den_coeffs_int;

    int bp_num_coeffs_ext[] = {
        p_params->OFFSET_BPF_NUMERATOR_COEFF_EXT_0,
        p_params->OFFSET_BPF_NUMERATOR_COEFF_EXT_1,
        p_params->OFFSET_BPF_NUMERATOR_COEFF_EXT_2,
        p_params->OFFSET_BPF_NUMERATOR_COEFF_EXT_3,
        p_params->OFFSET_BPF_NUMERATOR_COEFF_EXT_4
    };
    p_fxlms_params->p_bp_coeffs_num_ext = bp_num_coeffs_ext;

    int bp_den_coeffs_ext[] = {
        p_params->OFFSET_BPF_DENOMINATOR_COEFF_EXT_0,
        p_params->OFFSET_BPF_DENOMINATOR_COEFF_EXT_1,
        p_params->OFFSET_BPF_DENOMINATOR_COEFF_EXT_2,
        p_params->OFFSET_BPF_DENOMINATOR_COEFF_EXT_3,
        p_params->OFFSET_BPF_DENOMINATOR_COEFF_EXT_4
    };
    p_fxlms_params->p_bp_coeffs_den_ext = bp_den_coeffs_ext;

    aanc_fxlms100_initialize(p_ag->p_fxlms, p_ag->p_tmp_int_ip,
                             p_ag->p_tmp_ext_ip, p_ag->p_tmp_int_op,
                             p_ag->p_tmp_ext_op, hard_initialize);

    /* Initialize EDs */
    bool ext_ed_disable_e_filter_check = FALSE;
    if (p_params->OFFSET_AANC_DEBUG & \
        AANC_CONFIG_AANC_DEBUG_DISABLE_ED_EXT_E_FILTER_CHECK)
    {
        ext_ed_disable_e_filter_check = TRUE;
    }
    bool int_ed_disable_e_filter_check = FALSE;
    if (p_params->OFFSET_AANC_DEBUG & \
        AANC_CONFIG_AANC_DEBUG_DISABLE_ED_INT_E_FILTER_CHECK)
    {
        int_ed_disable_e_filter_check = TRUE;
    }
    bool pb_ed_disable_e_filter_check = FALSE;
    if (p_params->OFFSET_AANC_DEBUG & \
        AANC_CONFIG_AANC_DEBUG_DISABLE_ED_PB_E_FILTER_CHECK)
    {
        pb_ed_disable_e_filter_check = TRUE;
    }

    ED100_PARAMETERS *p_ed_int_params = p_ag->p_ed_int_params;
    p_ed_int_params->frame_size = AANC_DEFAULT_FRAME_SIZE;
    p_ed_int_params->attack_time = p_params->OFFSET_ED_INT_ATTACK;
    p_ed_int_params->decay_time = p_params->OFFSET_ED_INT_DECAY;
    p_ed_int_params->envelope_time = p_params->OFFSET_ED_INT_ENVELOPE;
    p_ed_int_params->init_frame_time = p_params->OFFSET_ED_INT_INIT_FRAME;
    p_ed_int_params->ratio = p_params->OFFSET_ED_INT_RATIO;
    p_ed_int_params->min_signal = p_params->OFFSET_ED_INT_MIN_SIGNAL;
    p_ed_int_params->min_max_envelope = p_params->OFFSET_ED_INT_MIN_MAX_ENVELOPE;
    p_ed_int_params->delta_th = p_params->OFFSET_ED_INT_DELTA_TH;
    p_ed_int_params->count_th = p_params->OFFSET_ED_INT_COUNT_TH;
    p_ed_int_params->hold_frames = p_params->OFFSET_ED_INT_HOLD_FRAMES;
    p_ed_int_params->e_min_threshold = p_params->OFFSET_ED_INT_E_FILTER_MIN_THRESHOLD;
    p_ed_int_params->e_min_counter_threshold = p_params->OFFSET_ED_INT_E_FILTER_MIN_COUNTER_THRESHOLD;
    p_ed_int_params->e_min_check_disabled = int_ed_disable_e_filter_check;
    aanc_ed100_initialize(p_ag->p_ed_int);

    ED100_PARAMETERS *p_ed_ext_params = p_ag->p_ed_ext_params;
    p_ed_ext_params->frame_size = AANC_DEFAULT_FRAME_SIZE;
    p_ed_ext_params->attack_time = p_params->OFFSET_ED_EXT_ATTACK;
    p_ed_ext_params->decay_time = p_params->OFFSET_ED_EXT_DECAY;
    p_ed_ext_params->envelope_time = p_params->OFFSET_ED_EXT_ENVELOPE;
    p_ed_ext_params->init_frame_time = p_params->OFFSET_ED_EXT_INIT_FRAME;
    p_ed_ext_params->ratio = p_params->OFFSET_ED_EXT_RATIO;
    p_ed_ext_params->min_signal = p_params->OFFSET_ED_EXT_MIN_SIGNAL;
    p_ed_ext_params->min_max_envelope = p_params->OFFSET_ED_EXT_MIN_MAX_ENVELOPE;
    p_ed_ext_params->delta_th = p_params->OFFSET_ED_EXT_DELTA_TH;
    p_ed_ext_params->count_th = p_params->OFFSET_ED_EXT_COUNT_TH;
    p_ed_ext_params->hold_frames = p_params->OFFSET_ED_EXT_HOLD_FRAMES;
    p_ed_ext_params->e_min_threshold = p_params->OFFSET_ED_EXT_E_FILTER_MIN_THRESHOLD;
    p_ed_ext_params->e_min_counter_threshold = p_params->OFFSET_ED_EXT_E_FILTER_MIN_COUNTER_THRESHOLD;
    p_ed_ext_params->e_min_check_disabled = ext_ed_disable_e_filter_check;
    aanc_ed100_initialize(p_ag->p_ed_ext);

    ED100_PARAMETERS *p_ed_pb_params = p_ag->p_ed_pb_params;
    p_ed_pb_params->frame_size = AANC_DEFAULT_FRAME_SIZE;
    p_ed_pb_params->attack_time = p_params->OFFSET_ED_PB_ATTACK;
    p_ed_pb_params->decay_time = p_params->OFFSET_ED_PB_DECAY;
    p_ed_pb_params->envelope_time = p_params->OFFSET_ED_PB_ENVELOPE;
    p_ed_pb_params->init_frame_time = p_params->OFFSET_ED_PB_INIT_FRAME;
    p_ed_pb_params->ratio = p_params->OFFSET_ED_PB_RATIO;
    p_ed_pb_params->min_signal = p_params->OFFSET_ED_PB_MIN_SIGNAL;
    p_ed_pb_params->min_max_envelope = p_params->OFFSET_ED_PB_MIN_MAX_ENVELOPE;
    p_ed_pb_params->delta_th = p_params->OFFSET_ED_PB_DELTA_TH;
    p_ed_pb_params->count_th = p_params->OFFSET_ED_PB_COUNT_TH;
    p_ed_pb_params->hold_frames = p_params->OFFSET_ED_PB_HOLD_FRAMES;
    p_ed_pb_params->e_min_threshold = p_params->OFFSET_ED_PB_E_FILTER_MIN_THRESHOLD;
    p_ed_pb_params->e_min_counter_threshold = p_params->OFFSET_ED_PB_E_FILTER_MIN_COUNTER_THRESHOLD;
    p_ed_pb_params->e_min_check_disabled = pb_ed_disable_e_filter_check;
    aanc_ed100_initialize(p_ag->p_ed_pb);

    /* Initialize clip counter (ext and int), duration (in frames) and clip detection*/
    p_ag->clip_duration_ext = (uint16) ((p_params->OFFSET_CLIPPING_DURATION_EXT * AANC_FRAME_RATE) >> 20);
    p_ag->clip_duration_int = (uint16) ((p_params->OFFSET_CLIPPING_DURATION_INT * AANC_FRAME_RATE) >> 20);
    p_ag->clip_duration_pb = (uint16) ((p_params->OFFSET_CLIPPING_DURATION_PB * AANC_FRAME_RATE) >> 20);
    p_ag->clip_counter_ext = 0;
    p_ag->clip_counter_int = 0;
    p_ag->clip_counter_pb = 0;
    p_ag->clipping_detection = 0;

    return TRUE;
}

bool aanc_proc_process_data(ADAPTIVE_GAIN *p_ag, bool calculate_gain)
{
    unsigned clipping_detection = 0;
    bool self_speech = FALSE;

    // Clear all flags connected with processing data but persist quiet mode
    unsigned flags_pre_proc = *p_ag->p_aanc_flags & (AANC_MODEL_LOADED | AANC_FLAGS_QUIET_MODE);

    // Get config flags
    unsigned config = p_ag->p_aanc_params->OFFSET_AANC_CONFIG;
    unsigned debug_config = p_ag->p_aanc_params->OFFSET_AANC_DEBUG;
    bool clip_int_disable = debug_config & AANC_CONFIG_AANC_DEBUG_DISABLE_CLIPPING_DETECT_INT;
    bool clip_ext_disable = debug_config & AANC_CONFIG_AANC_DEBUG_DISABLE_CLIPPING_DETECT_EXT;
    bool clip_pb_disable = debug_config & AANC_CONFIG_AANC_DEBUG_DISABLE_CLIPPING_DETECT_PB;
    bool clip_disable = clip_int_disable && clip_ext_disable && clip_pb_disable;

    // Get control for whether the read pointer is updated or not
    // If MUX_SEL_ALGORITHM we update the read pointer because the input
    // buffer is not copied later. If not we don't update it so that the
    // input buffer is correctly copied to the output.
    unsigned mux_sel_algorithm = \
        debug_config & AANC_CONFIG_AANC_DEBUG_MUX_SEL_ALGORITHM;
    p_ag->p_fxlms_params->read_ptr_upd = mux_sel_algorithm;

    // Get ED pointer handles
    ED100_STATISTICS *p_ed_int_stats = p_ag->p_ed_int_stats;
    ED100_STATISTICS *p_ed_ext_stats = p_ag->p_ed_ext_stats;
    ED100_STATISTICS *p_ed_pb_stats = p_ag->p_ed_pb_stats;

    int quiet_mode_lo_threshold = p_ag->p_aanc_params->OFFSET_QUIET_MODE_LO_THRESHOLD;
    int quiet_mode_hi_threshold = p_ag->p_aanc_params->OFFSET_QUIET_MODE_HI_THRESHOLD;

    /* Copy input data to internal data buffers */
    cbuffer_copy(p_ag->p_tmp_int_ip, p_ag->p_mic_int_ip, AANC_DEFAULT_FRAME_SIZE);
    cbuffer_copy(p_ag->p_tmp_ext_ip, p_ag->p_mic_ext_ip, AANC_DEFAULT_FRAME_SIZE);

    /* Copy playback data to internal data buffers if connected */
    if (p_ag->p_playback_ip != NULL)
    {
        cbuffer_copy(p_ag->p_tmp_pb_ip, p_ag->p_playback_ip,
                     AANC_DEFAULT_FRAME_SIZE);
    }

    /* Copy fbmon data through if connected */
    if (p_ag->p_fbmon_ip != NULL)
    {
        if (p_ag->p_fbmon_op != NULL)
        {
            cbuffer_copy(p_ag->p_fbmon_op, p_ag->p_fbmon_ip,
                         AANC_DEFAULT_FRAME_SIZE);
        }
        else
        {
            cbuffer_discard_data(p_ag->p_fbmon_ip, AANC_DEFAULT_FRAME_SIZE);
        }
    }

    /* Clipping detection on the input mics */
    if (!(clip_disable))
    {
        clipping_detection = aanc_proc_clipping_peak_detect(p_ag);

        /* Set timer on int mic if clipping event detected  */
        if ((clipping_detection & AANC_FLAGS_CLIPPING_INT) && !clip_int_disable)
        {
            p_ag->clip_counter_int = p_ag->clip_duration_int;
            p_ag->clipping_detection |= AANC_FLAGS_CLIPPING_INT;
        }
        /* Set timer on ext mic if clipping event detected  */
        if ((clipping_detection & AANC_FLAGS_CLIPPING_EXT) && !clip_ext_disable)
        {
            p_ag->clip_counter_ext = p_ag->clip_duration_ext;
            p_ag->clipping_detection |= AANC_FLAGS_CLIPPING_EXT;
        }

        /* Set timer on playback if clipping event detected  */
        if ((clipping_detection & AANC_FLAGS_CLIPPING_PLAYBACK) && !clip_pb_disable)
        {
            p_ag->clip_counter_pb = p_ag->clip_duration_pb;
            p_ag->clipping_detection |= AANC_FLAGS_CLIPPING_PLAYBACK;
        }

        /* Reset int mic clipping flag if timer expires and no clipping event detected */
        if (!p_ag->clip_counter_int && !(clipping_detection & AANC_FLAGS_CLIPPING_INT))
        {
            p_ag->clipping_detection &= AANC_PROC_RESET_INT_MIC_CLIP_FLAG;
        }
        /* Reset ext mic clipping flag if timer expires and no clipping event detected */
        if (!p_ag->clip_counter_ext && !(clipping_detection & AANC_FLAGS_CLIPPING_EXT))
        {
            p_ag->clipping_detection &= AANC_PROC_RESET_EXT_MIC_CLIP_FLAG;
        }
        /* Reset playback clipping flag if timer expires and no clipping event detected */
        if (!p_ag->clip_counter_pb && !(clipping_detection & AANC_FLAGS_CLIPPING_PLAYBACK))
        {
            p_ag->clipping_detection &= AANC_PROC_RESET_PLAYBACK_CLIP_FLAG;
        }

        if (p_ag->clipping_detection)
        {
            /* Copy input data to output if terminals are connected otherwise
             * discard data.
             */
            if (p_ag->p_mic_int_op != NULL && p_ag->p_mic_ext_op != NULL)
            {
                cbuffer_copy(p_ag->p_mic_int_op, p_ag->p_tmp_int_ip,
                             AANC_DEFAULT_FRAME_SIZE);
                cbuffer_copy(p_ag->p_mic_ext_op, p_ag->p_tmp_ext_ip,
                             AANC_DEFAULT_FRAME_SIZE);
            }
            else
            {
                cbuffer_discard_data(p_ag->p_tmp_int_ip,
                                     AANC_DEFAULT_FRAME_SIZE);
                cbuffer_discard_data(p_ag->p_tmp_ext_ip,
                                     AANC_DEFAULT_FRAME_SIZE);
            }

            /* Copy or discard data on the playback stream */
            if (p_ag->p_playback_ip != NULL) {
                if (p_ag->p_playback_op != NULL)
                {
                    cbuffer_copy(p_ag->p_playback_op, p_ag->p_tmp_pb_ip,
                                AANC_DEFAULT_FRAME_SIZE);
                }
                else
                {
                    cbuffer_discard_data(p_ag->p_tmp_pb_ip,
                                        AANC_DEFAULT_FRAME_SIZE);
                }
            }

            /* Decrement int mic clip counter and update flag */
            if (p_ag->clip_counter_int && !(clipping_detection & AANC_FLAGS_CLIPPING_INT))
            {
                p_ag->clip_counter_int--;
                flags_pre_proc |= AANC_FLAGS_CLIPPING_INT;
            }
            /* Decrement ext mic clip counter and update flag */
            if (p_ag->clip_counter_ext && !(clipping_detection & AANC_FLAGS_CLIPPING_EXT))
            {
                p_ag->clip_counter_ext--;
                flags_pre_proc |= AANC_FLAGS_CLIPPING_EXT;
            }
            /* Decrement playback clip counter and update flag */
            if (p_ag->clip_counter_pb && !(clipping_detection & AANC_FLAGS_CLIPPING_PLAYBACK))
            {
                p_ag->clip_counter_pb--;
                flags_pre_proc |= AANC_FLAGS_CLIPPING_PLAYBACK;
            }

            flags_pre_proc |= p_ag->clipping_detection;
            *p_ag->p_aanc_flags = flags_pre_proc;
            return FALSE;
        }
    }

    /* ED process ext mic */
    if (!(config & AANC_CONFIG_AANC_CONFIG_DISABLE_ED_EXT))
    {
        aanc_ed100_process_data(p_ag->p_tmp_ext_ip, p_ag->p_ed_ext);

        /* Catch external ED detection */
        if (p_ed_ext_stats->detection)
        {
            flags_pre_proc |= AANC_FLAGS_ED_EXT;
            L4_DBG_MSG("AANC_PROC ED Ext Detection");
        }

        /* Threshold detect on external ED */
        if (p_ed_ext_stats->spl < quiet_mode_lo_threshold)
        {
            L4_DBG_MSG("AANC_PROC ED Ext below quiet mode low threshold");
            /* Set quiet mode flag */
            flags_pre_proc |= AANC_FLAGS_QUIET_MODE;
        }
        else if (p_ed_ext_stats->spl > quiet_mode_hi_threshold)
        {
            /* Reset quiet mode flag */
            flags_pre_proc &= AANC_PROC_QUIET_MODE_RESET_FLAG;
        }
    }

    /* ED process int mic */
    if (!(config & AANC_CONFIG_AANC_CONFIG_DISABLE_ED_INT))
    {
        aanc_ed100_process_data(p_ag->p_tmp_int_ip, p_ag->p_ed_int);
        if (p_ed_int_stats->detection)
        {
            flags_pre_proc |= AANC_FLAGS_ED_INT;
            L4_DBG_MSG("AANC_PROC: ED Int Detection");
        }
    }

    if (!(config & AANC_CONFIG_AANC_CONFIG_DISABLE_SELF_SPEECH))
    {
        /* ED process self-speech */
        self_speech = aanc_ed100_self_speech_detect(
            p_ag->p_ed_int, p_ag->p_ed_ext,
            p_ag->p_aanc_params->OFFSET_SELF_SPEECH_THRESHOLD);
        if (self_speech)
        {
            flags_pre_proc |= AANC_FLAGS_SELF_SPEECH;
            L4_DBG_MSG("AANC_PROC: Self Speech Detection");
        }
    }

    /* ED process playback */
    if (p_ag->p_playback_ip != NULL &&
        !(config & AANC_CONFIG_AANC_CONFIG_DISABLE_ED_PB))
    {
        aanc_ed100_process_data(p_ag->p_tmp_pb_ip, p_ag->p_ed_pb);
        if (p_ed_pb_stats->detection)
        {
            flags_pre_proc |= AANC_FLAGS_ED_PLAYBACK;
            L4_DBG_MSG("AANC_PROC: ED Playback Detection");
        }
    }

    /* Update flags */
    *p_ag->p_aanc_flags = flags_pre_proc;

    /* Reference the working buffer used at the end to copy or discard data.
     * If adaptive gain calculation runs this is updated to the temporary output
     * buffers.
     */
    tCbuffer *p_int_working_buffer = p_ag->p_tmp_int_ip;
    tCbuffer *p_ext_working_buffer = p_ag->p_tmp_ext_ip;

    /* Call adaptive ANC function */
    if (!p_ed_ext_stats->detection && !p_ed_int_stats->detection &&
        !p_ed_pb_stats->detection  && !self_speech && calculate_gain)
    {
        L5_DBG_MSG("AANC_PROC: Calculate new gain");
        if (aanc_fxlms100_process_data(p_ag->p_fxlms))
        {
            *p_ag->p_aanc_flags |= p_ag->p_fxlms_stats->flags;
            if (mux_sel_algorithm)
            {
                p_int_working_buffer = p_ag->p_tmp_int_op;
                p_ext_working_buffer = p_ag->p_tmp_ext_op;
            }
        }
    }

    /* Copy internal buffers to the output buffers if they are connected
    * otherwise discard the data.
    */
    if (p_ag->p_mic_int_op != NULL && p_ag->p_mic_ext_op != NULL)
    {
        cbuffer_copy(p_ag->p_mic_int_op, p_int_working_buffer,
                        AANC_DEFAULT_FRAME_SIZE);
        cbuffer_copy(p_ag->p_mic_ext_op, p_ext_working_buffer,
                        AANC_DEFAULT_FRAME_SIZE);
    }
    else
    {
        cbuffer_discard_data(p_int_working_buffer, AANC_DEFAULT_FRAME_SIZE);
        cbuffer_discard_data(p_ext_working_buffer, AANC_DEFAULT_FRAME_SIZE);
    }

    /* Copy or discard data on the internal playback stream buffer */
    if (p_ag->p_playback_ip != NULL) {
        if (p_ag->p_playback_op != NULL)
        {
            cbuffer_copy(p_ag->p_playback_op, p_ag->p_tmp_pb_ip,
                        AANC_DEFAULT_FRAME_SIZE);
        }
        else
        {
            cbuffer_discard_data(p_ag->p_tmp_pb_ip,
                                 AANC_DEFAULT_FRAME_SIZE);
        }
    }

    return TRUE;
}
