/****************************************************************************
 * Copyright (c) 2015 - 2020 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \defgroup lib_private\aanc
 *
 * \file  ed100_struct_public.h
 * \ingroup lib_private\aanc
 *
 * ED100 library header file providing public data structures.
 *
 */
#ifndef _ED100_LIB_STRUCT_PUBLIC_H_
#define _ED100_LIB_STRUCT_PUBLIC_H_

/* Imports Kalimba type definitions */
#include "types.h"
#include "opmgr/opmgr_for_ops.h"

/******************************************************************************
Public Constant Definitions
*/

/******************************************************************************
Public Variable Definitions.

There provide a means to pass data and variables into and out of the ED100
library.
*/

typedef struct _ED100_PARAMETERS
{
    int frame_size;                   /* No. samples to process per frame */
    int attack_time;                  /* Attach time (Q7.N, s) */
    int decay_time;                   /* Decay time (Q7.N, s) */
    int envelope_time;                /* Envelope time (Q7.N, s) */
    int init_frame_time;              /* Initial frame time (Q7.N, s) */
    int ratio;                        /* Ratio */
    int min_signal;                   /* Min signal (Q12.N, dB) */
    int min_max_envelope;             /* Min-Max envelope (Q12.N, dB) */
    int delta_th;                     /* Delta threshold (Q12.B, dB) */
    int count_th;                     /* Count threshold (Q7.N) */
    unsigned hold_frames;             /* No. frames to hold a trigger */
    unsigned e_min_threshold;         /* e_min threshold (12.N, dB) */
    unsigned e_min_counter_threshold; /* e_min counter threshold (frames) */
    bool e_min_check_disabled;        /* Control e_min check */
} ED100_PARAMETERS;

typedef struct _ED100_STATISTICS
{
    int spl;        /* SPL value calculated during ED processing */
    int spl_max;    /* Maximum SPL calculated during ED processing */
    int spl_mid;    /* Mid-point SPL calculated during ED processing */
    bool detection; /* Whether a detection occurred during ED processing */
    bool licensed;  /* Flag to indicate license status */
} ED100_STATISTICS;

#endif /* _ED100_LIB_STRUCT_PUBLIC_H_ */