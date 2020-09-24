/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    domains

\brief      Fast Pair battery notification handling.
*/

#include "fast_pair_battery_notifications.h"
#include "fast_pair_bloom_filter.h"
#include "fast_pair_advertising.h"

#ifdef INCLUDE_CASE

#include <case.h>
#include <multidevice.h>

#include <logging.h>

/*! Number of battery values. */
#define FP_BATTERY_NUM_VALUES                   (0x3)
/*! Battery values bit offset. */
#define FP_BATTERY_NUM_VALUES_BIT_OFFSET        (4)
/*! Length component of lengthtype field. */
#define FP_BATTERY_LENGTH                       (FP_BATTERY_NUM_VALUES << FP_BATTERY_NUM_VALUES_BIT_OFFSET)
/*! Show battery notification on UI. */
#define FP_BATTERY_TYPE_UI_SHOW                 (0x3)
/*! Hide battery notification on UI. */
#define FP_BATTERY_TYPE_UI_HIDE                 (0x4)
/*! Combined length and type show */
#define FP_BATTERY_LENGTHTYPE_SHOW              (FP_BATTERY_LENGTH + FP_BATTERY_TYPE_UI_SHOW)
/*! Combined length and type hide */
#define FP_BATTERY_LENGTHTYPE_HIDE              (FP_BATTERY_LENGTH + FP_BATTERY_TYPE_UI_HIDE)

/*! Offsets to fields in the battery notification data. */
/*! @{ */
#define FP_BATTERY_NTF_DATA_LENGTHTYPE_OFFSET   (0)
#define FP_BATTERY_NTF_DATA_LEFT_STATE_OFFSET   (1)
#define FP_BATTERY_NTF_DATA_RIGHT_STATE_OFFSET  (2)
#define FP_BATTERY_NTF_DATA_CASE_STATE_OFFSET   (3)
/*! @} */

/*! Data used for battery notifications as optional extention to account key data in unidentifiable adverts.
    \note The ordering of fields matches the FastPair spec requirements and should not be changed.
*/
static uint8 fp_battery_ntf_data[FP_BATTERY_NOTFICATION_SIZE] = {FP_BATTERY_LENGTHTYPE_HIDE,
                                                                 BATTERY_STATUS_UNKNOWN,
                                                                 BATTERY_STATUS_UNKNOWN,
                                                                 BATTERY_STATUS_UNKNOWN};

uint8* fastPair_BatteryGetData(void)
{
    DEBUG_LOG("fastPair_BatteryGetData");
    return fp_battery_ntf_data;
}

void fastPair_BatteryHandleCasePowerState(const CASE_POWER_STATE_T* cps)
{
    DEBUG_LOG("fastPair_BatteryHandleCasePowerState");

    if (Multidevice_IsLeft())
    {
        fp_battery_ntf_data[FP_BATTERY_NTF_DATA_LEFT_STATE_OFFSET] = cps->local_battery_state;
        fp_battery_ntf_data[FP_BATTERY_NTF_DATA_RIGHT_STATE_OFFSET] = cps->peer_battery_state;
    }
    else
    {
        fp_battery_ntf_data[FP_BATTERY_NTF_DATA_LEFT_STATE_OFFSET] = cps->peer_battery_state;
        fp_battery_ntf_data[FP_BATTERY_NTF_DATA_RIGHT_STATE_OFFSET] = cps->local_battery_state;
    }
    fp_battery_ntf_data[FP_BATTERY_NTF_DATA_CASE_STATE_OFFSET] = cps->case_battery_state;

    /* bloom filter includes battery state in the hash generation phase, so needs to be updated */
    fastPair_GenerateBloomFilter();
    fastPair_AdvNotifyDataChange();
}

void fastPair_BatteryHandleCaseLidState(const CASE_LID_STATE_T* cls)
{
    DEBUG_LOG("fastPair_BatteryHandleCaseLidState");

    if (cls->lid_state == CASE_LID_STATE_OPEN)
    {
        fp_battery_ntf_data[FP_BATTERY_NTF_DATA_LENGTHTYPE_OFFSET] = FP_BATTERY_LENGTHTYPE_SHOW;
    }
    else
    {
        fp_battery_ntf_data[FP_BATTERY_NTF_DATA_LENGTHTYPE_OFFSET] = FP_BATTERY_LENGTHTYPE_HIDE;
    }
    fastPair_GenerateBloomFilter();
    fastPair_AdvNotifyDataChange();
}

#endif
