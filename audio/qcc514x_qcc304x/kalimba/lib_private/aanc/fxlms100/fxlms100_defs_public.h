/****************************************************************************
 * Copyright (c) 2015 - 2020 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \defgroup lib_private\aanc
 *
 * \file  fxlms100_defs_public.h
 * \ingroup lib_private\aanc
 *
 * FXLMS100 library header file providing public definitions common to C and
 * ASM code.
 */
#ifndef _FXLMS100_LIB_DEFS_PUBLIC_H_
#define _FXLMS100_LIB_DEFS_PUBLIC_H_

/******************************************************************************
Public Constant Definitions
*/

/* Enable saturation detection in the build */
#define DETECT_SATURATION

/* Flag definitions that are set during processing */
#define FXLMS100_FLAGS_SATURATION_INT_SHIFT      12
#define FXLMS100_FLAGS_SATURATION_EXT_SHIFT      13
#define FXLMS100_FLAGS_SATURATION_PLAYBACK_SHIFT 14
#define FXLMS100_FLAGS_SATURATION_MODEL_SHIFT    15

#define FXLMS100_BANDPASS_SHIFT                   3
#define FXLMS100_PLANT_CONTROL_SHIFT              4

#define FXLMS100_GAIN_SHIFT                      23

/* Frame size used to allocate temporary buffer. */
#define FXLMS100_FRAME_SIZE                      64

#endif /* _FXLMS100_LIB_DEFS_PUBLIC_H_ */
