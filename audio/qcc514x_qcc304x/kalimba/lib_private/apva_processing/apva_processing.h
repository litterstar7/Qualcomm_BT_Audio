// *****************************************************************************
// Copyright (c) 2020 Qualcomm Technologies International, Ltd.
// %%version
//
// *****************************************************************************
// *****************************************************************************
// NAME:
//    WWE secure processing functions
//
//
// *****************************************************************************

#ifndef WWE_PROC_LIB_H
#define WWE_PROC_LIB_H


#include "pryon_lite_ww.h"
#include "pryon_lite.h"

/**
@brief Initialize the feature handle for APVA
*/
bool load_apva_handle(void** f_handle);

/**
@brief Secure process data function for APVA
*/
PryonLiteError PryonLiteDecoder_ProcessData(void *f_handle, PryonLiteDecoderHandle handle,
                                            const short *p_buffer, int framesize);

/**
@brief Unload the feature handle for VAD
*/
void unload_wwe_handle(void* f_handle);


#endif /* WWE_PROC_LIB_H */
