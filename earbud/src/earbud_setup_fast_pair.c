/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Scrambled APSK.

*/

#include "earbud_setup_fast_pair.h"

#include "fast_pair.h"

#define SCRAMBLED_ASPK {0xD66D, 0x3D4B, 0x57E0, 0x9312, 0x4AAE, 0xBBBD, 0xE947, 0xAC73, 0x8945, 0x30CB, 0xFC08, 0xCD86, 0xC698,\
0xACE3, 0xB7DE, 0xA79B}

#ifdef INCLUDE_FAST_PAIR
static const uint16 scrambled_apsk[] = SCRAMBLED_ASPK;
#endif

void Earbud_SetupFastPair(void)
{
#ifdef INCLUDE_FAST_PAIR
    FastPair_SetPrivateKey(scrambled_apsk, sizeof(scrambled_apsk));
#endif
}
