/****************************************************************************
 * Copyright (c) 2015 - 2020 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \defgroup lib_private\aanc
 *
 * \file  fxlms100_public.h
 * \ingroup lib_private\aanc
 *
 * FXLMS100 library public header file.
 *
 */
#ifndef _FXLMS100_LIB_PUBLIC_H_
#define _FXLMS100_LIB_PUBLIC_H_

/* Imports FXLMS100 structures */
#include "fxlms100_struct_public.h"
#include "fxlms100_defs_public.h"

/******************************************************************************
Public Function Definitions
*/

/**
 * \brief  Create the FXLMS100 data object.
 *
 * \param  pp_fxlms  Address of pointer to FXLMS100 object to be created.
 * \param  p_params  Pointer to previously allocated FXLMS100_PARAMETERS object.
 *                   This is used to control the behaviour of the fxlms library.
 * \param  p_stats  Pointer to previously allocated FXLMS100_STATISTICS object.
 *                  This is used to return information from the fxlms library.
 * \param  num_bp_coeffs  The number of coefficients to use in the bandpass
 *                        filters that preceed the LMS calculation.
 *
 * \return  boolean indicating success or failure.
 */
extern bool aanc_fxlms100_create(void **pp_fxlms,
                                 FXLMS100_PARAMETERS *p_params,
                                 FXLMS100_STATISTICS *p_stats,
                                 int num_bp_coeffs);

/**
 * \brief  Initialize the FXLMS100 data object.
 *
 * \param  p_fxlms  Pointer to the fxlms object created by `aanc_fxlms100_create`.
 * \param  p_bp_coeffs_num  Pointer to bandpass filter numerator coefficients.
 *                          The number of coefficients should match the argument
 *                          passed to `aanc_fxlms100_create`.
 * \param  p_bp_coeffs_den  Pointer to bandpass filter denominator coefficients.
 *                          The number of coefficients should match the argument
 *                          passed to `aanc_fxlms100_create`.
 * \param  initial_gain  Value to initialize the gain calculation to (0-255).
 * \param  p_int_ip  Pointer to cbuffer for internal mic input data.
 * \param  p_ext_ip  Pointer to cbuffer for external mic input data.
 * \param  p_int_op  Pointer to cbuffer for internal mic output data.
 * \param  p_ext_op  Pointer to cbuffer for external mic output data.
 * \param  reset_gain  Boolean indicating whether to reset the gain calculation
 *
 * \return  boolean indicating success or failure.
 */
extern bool aanc_fxlms100_initialize(void *p_fxlms, tCbuffer *p_int_ip,
                                     tCbuffer *p_ext_ip, tCbuffer *p_int_op,
                                     tCbuffer *p_ext_op, bool reset_gain);

/**
 * \brief  Destroy the FXLMS100 data object.
 *
 * \param  pP_fxlms  Address of pointer to the fxlms object created in
 *                  `aanc_fxlms100_create`.
 *
 * \return  boolean indicating success or failure.
 */
extern bool aanc_fxlms100_destroy(void **Pp_fxlms);

/**
 * \brief  Process data with FXLMS100.
 *
 * \param  p_fxlms  Pointer to the fxlms object created in
 *                  `aanc_fxlms100_create`.
 *
 * \return  boolean indicating success or failure.
 */
extern bool aanc_fxlms100_process_data(void *p_fxlms);

/**
 * \brief  Update FxLMS algorithm gain
 *
 * \param  p_fxlms   Pointer to the fxlms object created in
 *                   `aanc_fxlms100_create`.
 * \param  p_stats   Pointer to previously allocated FXLMS100_STATISTICS object.
 *                   This is used to return information from the fxlms library.
 * \param  new_gain  Gain value used to update the FxLMS algorithm gain
 *
 * Note that the gain update will only be made if the new value is within the
 * parameter bounds for the algorithm.
 *
 * \return boolean indicating success or failure.
 */
extern bool aanc_fxlms100_update_gain(void *p_fxlms,
                                      FXLMS100_STATISTICS *p_stats,
                                      uint16 new_gain);

/**
 * \brief  Set the plant model coefficients for FXLMS100.
 *
 * \param  p_fxlms  Pointer to the fxlms object created in
 *                  `aanc_fxlms100_create`.
 * \param  p_msg  Pointer to the OPMSG data received from the framework.
 *
 * \return  boolean indicating success or failure.
 */
extern bool aanc_fxlms100_set_plant_model(void *p_fxlms, void *p_msg);

/**
 * \brief  Set the control model coefficients for FXLMS100.
 *
 * \param  p_fxlms  Pointer to the fxlms object created in
 *                  `aanc_fxlms100_create`.
 * \param  p_msg  Pointer to the OPMSG data received from the framework.
 *
 * \return  boolean indicating success or failure.
 */
extern bool aanc_fxlms100_set_control_model(void *p_fxlms, void *p_msg);

/**
 * \brief  Convolve filter coefficients to produce a composite filter.
 *
 * \param  p_coeffs1  Pointer to the first coefficient array.
 * \param  p_coeffs2  Pointer to the second coefficient array.
 * \param  p_output   Pointer to the output coefficient array.
 * \param  n_input_coeffs  Number of coefficients to convolve.
 *
 * \return  boolean indicating success or failure.
 *
 * This operation convolves the two sets of coefficients in a tight loop. It
 * assumes that the length of the input coefficients is matched between the
 * first and second arrays and that the output array has been preallocated to
 * the correct length, i.e. 2 * n - 1.
 */
extern bool aanc_fxlms100_convolve_coeffs(int *p_coeffs1,
                                          int *p_coeffs2,
                                          int *p_output,
                                          unsigned n_input_coeffs);

#endif /* _FXLMS100_LIB_PUBLIC_H_ */