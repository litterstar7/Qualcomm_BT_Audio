/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Charger case application for CH273-1
*/

#ifndef MAIN_H_
#define MAIN_H_

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

/*#define CHARGER_COMMS_FAKE*/

/*
* FAST_TIMER_INTERRUPT: Enables interrupts on the fast timer, allowing
* the very convenient global_time_us to exist. Probably can't use it on the
* CH273 with the clock speed set to 8MHz as it causes the charger comms
* transmit to take too long.
*/
/*#define FAST_TIMER_INTERRUPT*/

#ifdef VARIANT_CH273
#define VARIANT_NAME "CH273"
#else
#define VARIANT_NAME "CB"
#define CHARGER_BQ24230
#define USB_ENABLED
#endif

#define BITMAP_GET(x, field, data) \
    (((data) & x##_MASK_##field) >> x##_BIT_##field)
#define BITMAP_SET(x, field, value) \
    (((value) << x##_BIT_##field) & x##_MASK_##field)

/*-----------------------------------------------------------------------------
------------------ TYPE DEFINITIONS -------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

#endif /* MAIN_H_ */
