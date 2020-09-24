/**
 * \file  va_pryon_lite_struct.h
 * \ingroup capabilities
 *
 *
 */

#ifndef VA_PRYON_LITE_STRUCT_H
#define VA_PRYON_LITE_STRUCT_H

#include "pryon_lite.h"
#include "wwe/wwe_struct.h"
#include "apva_gen_c.h"

/****************************************************************************
Public Type Definitions
*/

typedef struct VA_PRYON_LITE_OP_DATA{
    WWE_CLASS_DATA wwe_class;

    PryonLiteDecoderConfig config;
    PryonLiteDecoderHandle handle;

    APVA_PARAMETERS apva_cap_params;
    int16 *p_buffer;
    void *f_handle;

} VA_PRYON_LITE_OP_DATA;

#endif /* VA_PRYON_LITE_H */
