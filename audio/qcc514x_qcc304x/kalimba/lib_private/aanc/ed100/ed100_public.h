/****************************************************************************
 * Copyright (c) 2015 - 2019 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \defgroup AANC
 * \ingroup lib_private\aanc
 * \file  ed100_public.h
 * \ingroup AANC
 *
 * ED100 library public header file.
 *
 */
#ifndef _ED100_LIB_PUBLIC_H_
#define _ED100_LIB_PUBLIC_H_

/* Imports Kalimba type definitions */
#include "types.h"

/* Imports ED100 data types */
#include "ed100_struct_public.h"

/******************************************************************************
Public Function Definitions
*/

/**
 * \brief  Create the ED100 data object.
 *
 * \param  pp_ed  Address of pointer to allocate to ED100 data object.
 * \param  p_params  Pointer to previously allocated ED100_PARAMETERS object.
 *                   This is used to control the behaviour of the ed100 library.
 * \param  p_stats  Pointer to previously allocated ED100_STATISTICS object.
 *                  This is used to return information from the ed100 library.
 * \param  sample_rate  Sample rate for the EC module (Hz).
 *
 * \return  boolean indicating success or failure.
 */
extern bool aanc_ed100_create(void **pp_ed, ED100_PARAMETERS *p_params,
                              ED100_STATISTICS *p_stats, unsigned sample_rate);

/**
 * \brief  Initialize the ED100 data object.
 *
 * \param  p_ed  Pointer to the ed100 object created in `aanc_ed100_create`.
 *
 * \return  boolean indicating success or failure.
 */
extern bool aanc_ed100_initialize(void *p_ed);

/**
 * \brief  Process data with ED100.
 *
 * \param  p_input  Pointer to cbuffer with input data to process.
 * \param  p_ed  Pointer to the ed100 object created in `aanc_ed100_create`.
 *
 * \return  boolean indicating success or failure.
 */
extern bool aanc_ed100_process_data(tCbuffer *p_input, void *p_ed);

/**
 * \brief  Destroy the ED100 data object.
 *
 * \param  pp_ed  Address of the pointer to the ed100 object created in
 *                `aanc_ed100_create`.
 *
 * \return  boolean indicating success or failure.
 */
extern bool aanc_ed100_destroy(void **pp_ed);

/**
 * \brief  Do self-speech detection.
 *
 * \param  p_ed_int  Pointer to the internal ed100 object.
 * \param  p_ed_ext  Pointer to the external ed100 object.
 * \param  threshold  Threshold for self-speech detection
 *
 * \return  boolean indicating self-speech detection.
 */
extern bool aanc_ed100_self_speech_detect(void *p_ed_int, void *p_ed_ext,
                                          int threshold);

#endif /* _ED100_LIB_PUBLIC_H_ */