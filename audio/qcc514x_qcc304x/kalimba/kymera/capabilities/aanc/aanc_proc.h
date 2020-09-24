/****************************************************************************
 * Copyright (c) 2015 - 2019 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \defgroup AANC
 * \ingroup capabilities
 * \file  aanc_proc.h
 * \ingroup AANC
 *
 * AANC processing library header file.
 *
 */

#ifndef _AANC_PROC_H_
#define _AANC_PROC_H_

/* Imports Kalimba type definitions */
#include "types.h"

/* Imports macros for working with fractional numbers */
#include "platform/pl_fractional.h"

/* Imports logging functions */
#include "audio_log/audio_log.h"

/* Imports memory management functions */
#include "pmalloc/pl_malloc.h"
/* Imports cbuffer management functions */
#include "buffer/cbuffer_c.h"
/* Imports ADDR_PER_WORD definition */
#include "portability_macros.h"

/* Imports AANC private library functions */
#include "aanc_defs.h"
/* Imports AANC parameter/statistic definitions. */
#include "aanc_gen_c.h"

/******************************************************************************
Public Constant Definitions
*/
#define AANC_PROC_CLIPPING_THRESHOLD        0x3FFFFFFF
#define AANC_PROC_RESET_INT_MIC_CLIP_FLAG   0x7FFFFEFF
#define AANC_PROC_RESET_EXT_MIC_CLIP_FLAG   0x7FFFFDFF
#define AANC_PROC_RESET_PLAYBACK_CLIP_FLAG  0x7FFFFBFF
#define AANC_PROC_NUM_TAPS_BP               5
#define AANC_PROC_QUIET_MODE_RESET_FLAG     0x7FEFFFFF

#define AANC_ED_FLAG_MASK           (AANC_FLAGS_ED_INT | \
                                     AANC_FLAGS_ED_EXT | \
                                     AANC_FLAGS_ED_PLAYBACK)

#define AANC_CLIPPING_FLAG_MASK      (AANC_FLAGS_CLIPPING_INT | \
                                      AANC_FLAGS_CLIPPING_EXT | \
                                      AANC_FLAGS_CLIPPING_PLAYBACK)

#define AANC_SATURATION_FLAG_MASK    (AANC_FLAGS_SATURATION_INT | \
                                      AANC_FLAGS_SATURATION_EXT | \
                                      AANC_FLAGS_SATURATION_PLAYBACK | \
                                      AANC_FLAGS_SATURATION_AFTER_MODEL)

#define AANC_MODEL_LOADED            (AANC_FLAGS_STATIC_GAIN_LOADED | \
                                      AANC_FLAGS_PLANT_MODEL_LOADED | \
                                      AANC_FLAGS_CONTROL_MODEL_LOADED)

/******************************************************************************
Public Type Definitions
*/

typedef struct _ADAPTIVE_GAIN
{
    unsigned *p_aanc_reinit_flag;

    tCbuffer *p_tmp_int_ip;              /* Pointer to temp int mic ip (DM1) */
    tCbuffer *p_tmp_int_op;              /* Pointer to temp int mic op (DM2) */
    ED100_PARAMETERS *p_ed_int_params;   /* Pointer to int mic EC parameters */
    ED100_STATISTICS *p_ed_int_stats;    /* Pointer to int mic EC statistics */
    void *p_ed_int;                      /* Pointer to int mic EC object */

    /* Note that temp int/ext mic input buffers are in different memory banks
     * to facilitate efficient clipping and peak detection. Output buffers
     * are in DM2 to facilitate efficient FXLMS processing. */
    tCbuffer *p_tmp_ext_ip;              /* Pointer to temp ext mic ip (DM2) */
    tCbuffer *p_tmp_ext_op;              /* Pointer to temp ext mic op (DM2) */
    ED100_PARAMETERS *p_ed_ext_params;   /* Pointer to ext mic EC parameters */
    ED100_STATISTICS *p_ed_ext_stats;    /* Pointer to ext mic EC statistics */
    void *p_ed_ext;                      /* Pointer to ext mic EC object */

    tCbuffer *p_tmp_pb_ip;               /* Pointer to temp playback buffer */
    ED100_PARAMETERS *p_ed_pb_params;    /* Pointer to playback EC parameters */
    ED100_STATISTICS *p_ed_pb_stats;     /* Pointer to playback EC statistics */
    void *p_ed_pb;                       /* Pointer to playback EC object */

    FXLMS100_PARAMETERS *p_fxlms_params; /* Pointer to fxlms parameters */
    FXLMS100_STATISTICS *p_fxlms_stats;  /* Pointer to fxlms statistics */
    void *p_fxlms;                       /* Pointer to fxlms object */

    unsigned clip_threshold;             /* Threshold for clipping detection */
    unsigned clipping_detection;         /* Clipping detection */

    unsigned ext_peak_value;             /* Peak int mic signal value */
    unsigned int_peak_value;             /* Peak ext mic signal value */
    unsigned pb_peak_value;              /* Peak playback signal value */

    uint16 clip_duration_ext;            /* Duration for ext mic clipping timer */
    uint16 clip_duration_int;            /* Duration for int mic clipping timer */
    uint16 clip_duration_pb;             /* Duration for playback clipping timer */

    uint16 clip_counter_ext;             /* Clipping counter on ext mic */
    uint16 clip_counter_int;             /* Clipping counter on int mic */
    uint16 clip_counter_pb;              /* Clipping counter on playback */

    /* Pointers to cap data parameters */
    AANC_PARAMETERS *p_aanc_params;      /* Pointer to AANC parameters */
    unsigned *p_aanc_flags;              /* Pointer to AANC flags */

    /* Input/Output buffer pointers from terminals */
    tCbuffer *p_playback_op;
    tCbuffer *p_fbmon_ip;
    tCbuffer *p_mic_int_ip;
    tCbuffer *p_mic_ext_ip;

    tCbuffer *p_playback_ip;
    tCbuffer *p_fbmon_op;
    tCbuffer *p_mic_int_op;
    tCbuffer *p_mic_ext_op;

} ADAPTIVE_GAIN;


/******************************************************************************
Public Function Definitions
*/

/**
 * \brief  Create the ADAPTIVE_GAIN data object.
 *
 * \param  pp_ag  Address of the pointer to the object to be created.
 * \param  sample_rate  The sample rate of the operator. Used for the AANC
 *                      EC module.
 *
 * \return  boolean indicating success or failure.
 */
bool aanc_proc_create(ADAPTIVE_GAIN **pp_ag, unsigned sample_rate);

/**
 * \brief  Destroy the ADAPTIVE_GAIN data object.
 *
 * \param  pp_ag  Address of the pointer to the adaptive gain object created in
 *                `aanc_proc_create`.
 *
 * \return  boolean indicating success or failure.
 */
bool aanc_proc_destroy(ADAPTIVE_GAIN **pp_ag);

/**
 * \brief  Initialize the ADAPTIVE_GAIN data object.
 *
 * \param  p_params  Pointer to the AANC parameters.
 * \param  p_ag  Pointer to the adaptive gain object created in
 *               `aanc_proc_create`.
 * \param  ag_start  Value to initialize the gain calculation to (0-255).
 * \param  p_flags  Pointer to aanc flags.
 * \param  hard_initialize  Boolean indicating whether to hard reset the
 *                          algorithm.
 *
 * \return  boolean indicating success or failure.
 */
bool aanc_proc_initialize(AANC_PARAMETERS *p_params, ADAPTIVE_GAIN *p_ag,
                          unsigned ag_start, unsigned *p_flags,
                          bool hard_initialize);

/**
 * \brief  Process data with the adaptive gain algorithm.
 *
 * \param  p_ag  Pointer to the adaptive gain object created in
 *                  `aanc_proc_create`.
 * \param calculate_gain  Boolean indicating whether the gain calculation step
 *                        should be performed. All other adaptive gain
 *                        processing will continue.
 *
 * \return  boolean indicating success or failure.
 */
bool aanc_proc_process_data(ADAPTIVE_GAIN *p_ag, bool calculate_gain);

/**
 * \brief  ASM function to do clipping and peak detection.
 *
 * \param  p_ag  Pointer to the adaptive gain object created in
 *                  `aanc_proc_create`.
 *
 * \return unsigned int flag field indicating clipping events on mic channels
 */
extern unsigned aanc_proc_clipping_peak_detect(ADAPTIVE_GAIN *p_ag);

/**
 * \brief  ASM function to calculate dB representation of gain
 *
 * \param  fine_gain    Fine gain value
 * \param  coarse_gain  Coarse gain value
 *
 * \return int value of calculated gain in dB in Q12.20
 */
extern int aanc_proc_calc_gain_db(unsigned fine_gain, unsigned coarse_gain);

#endif /* _AANC_PROC_H_ */
