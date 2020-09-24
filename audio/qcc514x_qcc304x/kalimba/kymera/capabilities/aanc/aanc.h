/****************************************************************************
 * Copyright (c) 2015 - 2019 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \defgroup AANC
 * \ingroup capabilities
 * \file  aanc.h
 * \ingroup AANC
 *
 * AANC operator public header file.
 *
 */

#ifndef _AANC_H_
#define _AANC_H_

#include "capabilities.h"

/****************************************************************************
Public Variable Definitions
*/
/** The capability data structure for AANC */
#ifdef INSTALL_OPERATOR_AANC_MONO_16K
extern const CAPABILITY_DATA aanc_mono_16k_cap_data;
#endif

#ifdef INSTALL_OPERATOR_AANC_MONO_32K
extern const CAPABILITY_DATA aanc_mono_32k_cap_data;
#endif

#endif /* _AANC_H_ */