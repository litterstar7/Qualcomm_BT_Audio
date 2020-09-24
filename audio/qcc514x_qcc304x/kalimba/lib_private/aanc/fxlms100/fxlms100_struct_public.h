/****************************************************************************
 * Copyright (c) 2015 - 2020 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \defgroup lib_private\aanc
 *
 * \file  fxlms100_struct_public.h
 * \ingroup lib_private\aanc
 *
 * FXLMS100 library header file providing public data structures.
 *
 */
#ifndef _FXLMS100_LIB_STRUCT_PUBLIC_H_
#define _FXLMS100_LIB_STRUCT_PUBLIC_H_

/* Imports Kalimba type definitions */
#include "types.h"
#include "opmgr/opmgr_for_ops.h"

/******************************************************************************
Public Constant Definitions
*/

/******************************************************************************
Public Variable Definitions.

There provide a means to pass data and variables into and out of the FXLMS100
library.
*/

typedef struct _FXLMS100_PARAMETERS
{
    int target_nr;             /* Target Noise Reduction */
    int mu;                    /* Mu (convergence rate control) */
    int gamma;                 /* Gamma (leak control) */
    int frame_size;            /* Number of samples to process per frame */
    int lambda;                /* Lambda (fine tuning)  */
    int *p_bp_coeffs_num_int;  /* Pointer to internal mic bandpass filter numerator coeffs */
    int *p_bp_coeffs_den_int;  /* Pointer to internal mic bandpass filter denominator coeffs */
    int *p_bp_coeffs_num_ext;  /* Pointer to external mic bandpass filter numerator coeffs */
    int *p_bp_coeffs_den_ext;  /* Pointer to external mic bandpass filter denominator coeffs */
    unsigned initial_gain;     /* Initial gain value for algorithm */
    unsigned read_ptr_upd;     /* Control read pointer update in process_data */
    unsigned min_bound;        /* Minimum bound for FxLMS gain */
    unsigned max_bound;        /* Maximum bound for FxLMS gain */
    unsigned max_delta;        /* Maximum gain step for FxLMS algorithm */
} FXLMS100_PARAMETERS;

typedef struct _FXLMS100_STATISTICS
{
    unsigned flags;         /* Flags set during data processing */
    unsigned adaptive_gain; /* Calculated adaptive gain value for the ANC */
    bool licensed;          /* Flag to indicate license status */
    int *p_cp_num_coeffs;   /* Pointer to control-plant numerator coefficients */
    int *p_cp_den_coeffs;   /* Pointer to control-plant denominator coefficients */
    unsigned num_cp_coeffs; /* Number of control-plant coefficients */
} FXLMS100_STATISTICS;

#endif /* _FXLMS100_LIB_STRUCT_PUBLIC_H_ */