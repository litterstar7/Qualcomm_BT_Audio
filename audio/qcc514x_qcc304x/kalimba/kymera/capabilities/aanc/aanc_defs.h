/****************************************************************************
 * Copyright (c) 2014 - 2019 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \defgroup AANC
 * \ingroup capabilities
 * \file  aanc_defs.h
 * \ingroup AANC
 *
 * Adaptive ANC (AANC) operator shared definitions and include files
 *
 */

#ifndef _AANC_DEFS_H_
#define _AANC_DEFS_H_

#include "fxlms100_public.h"
#include "ed100_public.h"

#define AANC_DEFAULT_FRAME_SIZE   64   /* 4 ms at 16k */
#define AANC_DEFAULT_BLOCK_SIZE  0.5 * AANC_DEFAULT_FRAME_SIZE
#define AANC_DEFAULT_BUFFER_SIZE   2 * AANC_DEFAULT_FRAME_SIZE
#define AANC_FRAME_RATE          250   /* Fs = 16kHz, frame size = 64 */ 

#endif /* _AANC_DEFS_H_ */