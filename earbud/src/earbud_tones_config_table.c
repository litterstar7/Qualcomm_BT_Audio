/*!
\copyright  Copyright (c) 2019 - 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief	    Earbud Tones UI Indicator configuration table
*/
#include "earbud_tones_config_table.h"
#include "earbud_tones.h"

#if defined(HAVE_9_BUTTONS)
#include "9_buttons.h"
#elif defined(HAVE_7_BUTTONS)
#include "7_buttons.h"
#elif defined(HAVE_6_BUTTONS)
#include "6_buttons.h"
#elif defined(HAVE_5_BUTTONS)
#include "5_buttons.h"
#elif defined(HAVE_4_BUTTONS)
#include "4_buttons.h"
#elif defined(HAVE_2_BUTTONS)
#include "2_button.h"
#elif defined(HAVE_1_BUTTON)
#include "1_button.h"
#endif

#include <domain_message.h>
#include <ui_inputs.h>
#include <ui_tones.h>

#include <av.h>
#include <earbud_gaming_mode.h>
#include <pairing.h>
#include <peer_signalling.h>
#include <telephony_messages.h>
#include <scofwd_profile.h>
#include <power_manager.h>
#include <volume_service.h>

#if INCLUDE_TONES
const ui_event_indicator_table_t earbud_ui_tones_table[] =
{
    {.sys_event=TELEPHONY_INCOMING_CALL_OUT_OF_BAND_RINGTONE,  { .tone.tone = app_tone_hfp_ring,
                                                                 .tone.queueable = FALSE,
                                                                 .tone.interruptible = FALSE }},
    {.sys_event=TELEPHONY_TRANSFERED,                          { .tone.tone = app_tone_hfp_talk_long_press,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE }},
    {.sys_event=TELEPHONY_LINK_LOSS_OCCURRED,                  { .tone.tone = app_tone_hfp_link_loss,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE }},
#ifndef QCC3020_FF_ENTRY_LEVEL_AA
    {.sys_event=TELEPHONY_ERROR,                               { .tone.tone = app_tone_error,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE }},
    {.sys_event=AV_CONNECTED_PEER,                             { .tone.tone = app_tone_av_connected,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE }},
#endif
    {.sys_event=AV_LINK_LOSS,                                  { .tone.tone = app_tone_av_link_loss,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE }},
    {.sys_event=AV_ERROR,                                      { .tone.tone = app_tone_error,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE }},
#ifdef INCLUDE_GAMING_MODE
    {.sys_event=GAMING_MODE_ON,                                { .tone.tone = app_tone_gaming_mode_on,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE }},
    {.sys_event=GAMING_MODE_OFF,                               { .tone.tone = app_tone_gaming_mode_off,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE }},
#endif
    {.sys_event=VOLUME_SERVICE_MIN_VOLUME,                     { .tone.tone = app_tone_volume_limit,
                                                                 .tone.queueable = FALSE,
                                                                 .tone.interruptible = FALSE }},
    {.sys_event=VOLUME_SERVICE_MAX_VOLUME,                     { .tone.tone = app_tone_volume_limit,
                                                                 .tone.queueable = FALSE,
                                                                 .tone.interruptible = FALSE }},
    {.sys_event=APP_MFB_BUTTON_SINGLE_CLICK,                   { .tone.tone = app_tone_button,
                                                                 .tone.queueable = FALSE,
                                                                 .tone.interruptible = FALSE,
                                                                 .tone.button_feedback = TRUE }},
    {.sys_event=APP_MFB_BUTTON_HELD_1SEC,                      { .tone.tone = app_tone_button,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE,
                                                                 .tone.button_feedback = TRUE }},
#if !(defined (QCC5141_FF_HYBRID_ANC_AA) || defined (QCC5141_FF_HYBRID_ANC_BA)) 
    {.sys_event=APP_MFB_BUTTON_HELD_3SEC,                      { .tone.tone = app_tone_button_2,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE,
                                                                 .tone.button_feedback = TRUE }},
#endif
    {.sys_event=APP_MFB_BUTTON_HELD_6SEC,                      { .tone.tone = app_tone_button_3,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE,
                                                                 .tone.button_feedback = TRUE }},
    {.sys_event=APP_MFB_BUTTON_HELD_8SEC,                      { .tone.tone = app_tone_button_4,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE,
                                                                 .tone.button_feedback = TRUE }},

#ifdef INCLUDE_CAPSENSE
#ifdef TOUCH_SENSOR_V2
    {.sys_event=APP_ANC_SET_NEXT_MODE,                        { .tone.tone = app_tone_double_tap,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE,
                                                                 .tone.button_feedback = TRUE }},

#else
    {.sys_event=CAP_SENSE_FIVE_PRESS,                          { .tone.tone = app_tone_double_tap,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE,
                                                                 .tone.button_feedback = TRUE }},
    {.sys_event=CAP_SENSE_VERY_VERY_VERY_LONG_PRESS,           { .tone.tone = app_tone_button_factory_reset,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE,
                                                                 .tone.button_feedback = TRUE }},
#endif
#endif
    {.sys_event=APP_BUTTON_DFU,                                { .tone.tone = app_tone_button_dfu,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE,
                                                                 .tone.button_feedback = TRUE }},
    {.sys_event=APP_BUTTON_HELD_DFU,                           { .tone.tone = app_tone_button_dfu,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE,
                                                                 .tone.button_feedback = TRUE }},
    {.sys_event=APP_BUTTON_HELD_FACTORY_RESET,                 { .tone.tone = app_tone_button_factory_reset,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE,
                                                                 .tone.button_feedback = TRUE }},
#ifdef EXCLUDE_CONN_PROMPTS

    {.sys_event=TELEPHONY_CONNECTED,                             { .tone.tone = app_tone_hfp_connected,
                                                                  .tone.queueable = TRUE,
                                                                  .tone.interruptible = FALSE }},
    {.sys_event=TELEPHONY_DISCONNECTED,                          { .tone.tone = app_tone_hfp_disconnected,
                                                                    .tone.queueable = TRUE,
                                                                    .tone.interruptible = FALSE }},
#endif
#ifdef EXCLUDE_POWER_PROMPTS
    {.sys_event=POWER_ON,                                     { .tone.tone = app_tone_power_on,
                                                                  .tone.queueable = TRUE,
                                                                  .tone.interruptible = FALSE,
                                                                  .tone.button_feedback = TRUE }},
    {.sys_event=POWER_OFF,                                     { .tone.tone = app_tone_battery_empty,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE,
                                                                 .tone.button_feedback = TRUE },
                                                                 .await_indication_completion = TRUE },
#endif
};

const ui_repeating_indication_table_t earbud_ui_repeating_tones_table[] =
{
    {.triggering_sys_event = TELEPHONY_MUTE_ACTIVE,              .reminder_period = 15,
                                                                 .cancelling_sys_event = TELEPHONY_MUTE_INACTIVE,
                                                               { .tone.tone = app_tone_hfp_mute_reminder,
                                                                 .tone.queueable = TRUE,
                                                                 .tone.interruptible = FALSE }}
};

#endif

uint8 EarbudTonesConfigTable_SingleGetSize(void)
{
#if INCLUDE_TONES
    return ARRAY_DIM(earbud_ui_tones_table);
#else
    return 0;
#endif
}

uint8 EarbudTonesConfigTable_RepeatingGetSize(void)
{
#if INCLUDE_TONES
    return ARRAY_DIM(earbud_ui_repeating_tones_table);
#else
    return 0;
#endif
}
