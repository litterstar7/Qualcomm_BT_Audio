/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    domains
\brief      Interface to Fast Pair battery notification handling.
*/

#ifndef FAST_PAIR_BATTERY_NOTFICATIONS_H
#define FAST_PAIR_BATTERY_NOTFICATIONS_H

#ifdef INCLUDE_CASE
#include <case.h>

/*! Size of battery notification data used in adverts and bloom filter generation. */
#define FP_BATTERY_NOTFICATION_SIZE   (4)

/*! \brief Get pointer to start of the battery notification data for adverts.
    \return uint8* Pointer to the start of the data.
*/
uint8* fastPair_BatteryGetData(void);

/*! \brief Handle updated battery states from the case.
*/
void fastPair_BatteryHandleCasePowerState(const CASE_POWER_STATE_T* cps);

/*! \brief Handle updated lid status from the case.
*/
void fastPair_BatteryHandleCaseLidState(const CASE_LID_STATE_T* cls);

#else

/* No battery notification support and therefore no data size. */
#define FP_BATTERY_NOTFICATION_SIZE   (0)
#define fastPair_BatteryGetData()     (0)

#endif

#endif /* FAST_PAIR_BATTERY_NOTFICATIONS_H */
