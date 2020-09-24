/****************************************************************************
 * Copyright (c) 2014 - 2019 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \defgroup AANC
 * \ingroup capabilities
 * \file  aanc_cap.c
 * \ingroup AANC
 *
 * Adaptive ANC (AANC) operator capability.
 *
 */

/****************************************************************************
Include Files
*/

#include "aanc_cap.h"

/*****************************************************************************
Private Constant Definitions
*/
#ifdef CAPABILITY_DOWNLOAD_BUILD
#define AANC_MONO_16K_CAP_ID   CAP_ID_DOWNLOAD_AANC_MONO_16K
#define AANC_MONO_32K_CAP_ID   CAP_ID_DOWNLOAD_AANC_MONO_32K
#else
#define AANC_MONO_16K_CAP_ID   CAP_ID_AANC_MONO_16K
#define AANC_MONO_32K_CAP_ID   CAP_ID_AANC_MONO_32K
#endif

/* Message handlers */
const handler_lookup_struct aanc_handler_table =
{
    aanc_create,             /* OPCMD_CREATE */
    aanc_destroy,            /* OPCMD_DESTROY */
    aanc_start,              /* OPCMD_START */
    base_op_stop,            /* OPCMD_STOP */
    aanc_reset,              /* OPCMD_RESET */
    aanc_connect,            /* OPCMD_CONNECT */
    aanc_disconnect,         /* OPCMD_DISCONNECT */
    aanc_buffer_details,     /* OPCMD_BUFFER_DETAILS */
    base_op_get_data_format, /* OPCMD_DATA_FORMAT */
    aanc_get_sched_info      /* OPCMD_GET_SCHED_INFO */
};

/* Null-terminated operator message handler table */
const opmsg_handler_lookup_table_entry aanc_opmsg_handler_table[] =
    {{OPMSG_COMMON_ID_GET_CAPABILITY_VERSION, base_op_opmsg_get_capability_version},
    {OPMSG_COMMON_ID_SET_CONTROL,             aanc_opmsg_set_control},
    {OPMSG_COMMON_ID_GET_PARAMS,              aanc_opmsg_get_params},
    {OPMSG_COMMON_ID_GET_DEFAULTS,            aanc_opmsg_get_defaults},
    {OPMSG_COMMON_ID_SET_PARAMS,              aanc_opmsg_set_params},
    {OPMSG_COMMON_ID_GET_STATUS,              aanc_opmsg_get_status},
    {OPMSG_COMMON_ID_SET_UCID,                aanc_opmsg_set_ucid},
    {OPMSG_COMMON_ID_GET_LOGICAL_PS_ID,       aanc_opmsg_get_ps_id},

    {OPMSG_AANC_ID_SET_AANC_STATIC_GAIN,      aanc_opmsg_set_static_gain},
    {OPMSG_AANC_ID_SET_AANC_PLANT_COEFFS,     aanc_opmsg_set_plant_model},
    {OPMSG_AANC_ID_SET_AANC_CONTROL_COEFFS,   aanc_opmsg_set_control_model},
    {0, NULL}};

#ifdef INSTALL_OPERATOR_AANC_MONO_16K
const CAPABILITY_DATA aanc_mono_16k_cap_data =
    {
        /* Capability ID */
        AANC_MONO_16K_CAP_ID,
        /* Version information - hi and lo */
        AANC_AANC_MONO_16K_VERSION_MAJOR, AANC_CAP_VERSION_MINOR,
        /* Max number of sinks/inputs and sources/outputs */
        8, 4,
        /* Pointer to message handler function table */
        &aanc_handler_table,
        /* Pointer to operator message handler function table */
        aanc_opmsg_handler_table,
        /* Pointer to data processing function */
        aanc_process_data,
        /* Reserved */
        0,
        /* Size of capability-specific per-instance data */
        sizeof(AANC_OP_DATA)
    };

MAP_INSTANCE_DATA(AANC_MONO_16K_CAP_ID, AANC_OP_DATA)
#endif /* INSTALL_OPERATOR_AANC_MONO_16K */

#ifdef INSTALL_OPERATOR_AANC_MONO_32K
const CAPABILITY_DATA aanc_mono_32k_cap_data =
    {
        /* Capability ID */
        AANC_MONO_32K_CAP_ID,
        /* Version information - hi and lo */
        AANC_AANC_MONO_32K_VERSION_MAJOR, AANC_CAP_VERSION_MINOR,
        /* Max number of sinks/inputs and sources/outputs */
        8, 4,
        /* Pointer to message handler function table */
        &aanc_handler_table,
        /* Pointer to operator message handler function table */
        aanc_opmsg_handler_table,
        /* Pointer to data processing function */
        aanc_process_data,
        /* Reserved */
        0,
        /* Size of capability-specific per-instance data */
        sizeof(AANC_OP_DATA)
    };
MAP_INSTANCE_DATA(AANC_MONO_32K_CAP_ID, AANC_OP_DATA)
#endif /* INSTALL_OPERATOR_AANC_MONO_32K */

/****************************************************************************
Inline Functions
*/

/**
 * \brief  Get AANC instance data.
 *
 * \param  op_data  Pointer to the operator data.
 *
 * \return  Pointer to extra operator data AANC_OP_DATA.
 */
static inline AANC_OP_DATA *get_instance_data(OPERATOR_DATA *op_data)
{
    return (AANC_OP_DATA *) base_op_get_instance_data(op_data);
}

/**
 * \brief  Calculate the number of samples to process
 *
 * \param  p_ext_data  Pointer to capability data
 *
 * \return  Number of samples to process
 *
 * If there is less data or space than the default frame size then only that
 * number of samples will be returned.
 *
 */
static inline int aanc_calc_samples_to_process(AANC_OP_DATA *p_ext_data)
{
    int i, amt, min_data_space = AANC_DEFAULT_FRAME_SIZE;

    /* Return if int and ext mic input terminals are not connected */
    if ((p_ext_data->touched_sinks & AANC_MIN_VALID_SINKS) != AANC_MIN_VALID_SINKS)
    {
        return INT_MAX;
    }

    /* Calculate the amount of data available */
    for (i = AANC_PLAYBACK_TERMINAL_ID; i <= AANC_MIC_EXT_TERMINAL_ID; i++)
    {
        if (p_ext_data->inputs[i] != NULL)
        {
            amt = cbuffer_calc_amount_data_in_words(p_ext_data->inputs[i]);
            if (amt < min_data_space)
            {
                min_data_space = amt;
            }
        }
    }

    /*  Calculate the available space */
    if (p_ext_data->touched_sources != 0)
    {
        for (i = AANC_PLAYBACK_TERMINAL_ID; i <= AANC_MIC_EXT_TERMINAL_ID; i++)
        {
            if (p_ext_data->outputs[i] != NULL)
            {
                amt = cbuffer_calc_amount_space_in_words(p_ext_data->outputs[i]);
                if (amt < min_data_space)
                {
                    min_data_space = amt;
                }
            }
        }
    }
    /* Samples to process determined as minimum of data and space available */
    return min_data_space;
}

#ifdef RUNNING_ON_KALSIM
/**
 * \brief  Simulate a gain update to the HW.
 *
 * \param  op_data  Address of the operator data.
 * \param  p_ext_data  Address of the AANC extra_op_data.
 *
 * \return  boolean indicating success or failure.
 *
 * Because simulator tests need to change the gain and also analyze the
 * behavior of the capability an unsolicited message is sent only in
 * simulation.
 */
static bool aanc_update_gain(OPERATOR_DATA *op_data, AANC_OP_DATA *p_ext_data)
{
    unsigned msg_size = OPMSG_UNSOLICITED_AANC_INFO_WORD_SIZE;
    unsigned *trigger_message = NULL;
    OPMSG_REPLY_ID message_id = OPMSG_REPLY_ID_AANC_TRIGGER;

    trigger_message = xzpnewn(msg_size, unsigned);
    if (trigger_message == NULL)
    {
        return FALSE;
    }

    OPMSG_CREATION_FIELD_SET32(trigger_message, OPMSG_UNSOLICITED_AANC_INFO,
                                FLAGS, p_ext_data->flags);

    p_ext_data->ff_gain_prev.coarse = p_ext_data->ff_gain.coarse;
    OPMSG_CREATION_FIELD_SET(trigger_message,
                                OPMSG_UNSOLICITED_AANC_INFO,
                                FF_COARSE_GAIN,
                                p_ext_data->ff_gain.coarse);
    p_ext_data->ff_gain_prev.fine = p_ext_data->ff_gain.fine;
    OPMSG_CREATION_FIELD_SET(trigger_message, OPMSG_UNSOLICITED_AANC_INFO,
                                FF_FINE_GAIN, p_ext_data->ff_gain.fine);

    /* Only update EC and FB gains if in hybrid mode */
    if (p_ext_data->anc_fb_path > 0)
    {
        p_ext_data->fb_gain_prev.coarse = p_ext_data->fb_gain.coarse;
        OPMSG_CREATION_FIELD_SET(trigger_message, OPMSG_UNSOLICITED_AANC_INFO,
                                    FB_COARSE_GAIN, p_ext_data->fb_gain.coarse);
        p_ext_data->fb_gain_prev.fine = p_ext_data->fb_gain.fine;
        OPMSG_CREATION_FIELD_SET(trigger_message, OPMSG_UNSOLICITED_AANC_INFO,
                                    FB_FINE_GAIN, p_ext_data->fb_gain.fine);

        p_ext_data->ec_gain_prev.coarse = p_ext_data->ec_gain.coarse;
        OPMSG_CREATION_FIELD_SET(trigger_message, OPMSG_UNSOLICITED_AANC_INFO,
                                    EC_COARSE_GAIN, p_ext_data->ec_gain.coarse);
        p_ext_data->ec_gain_prev.fine = p_ext_data->ec_gain.fine;
        OPMSG_CREATION_FIELD_SET(trigger_message, OPMSG_UNSOLICITED_AANC_INFO,
                                    EC_FINE_GAIN, p_ext_data->ec_gain.fine);
    }

    common_send_unsolicited_message(op_data, (unsigned)message_id, msg_size,
                                    trigger_message);

    pdelete(trigger_message);

    return TRUE;
}
/**
 * \brief  Update the gain in the ANC HW.
 *
 * \param  op_data  Address of the operator data.
 * \param  p_ext_data  Address of the AANC extra_op_data.
 *
 * \return  boolean indicating success or failure.
 *
 * Any changes in the gain value since the previous value was set is written
 * to the HW.
 *
 */
#else
static bool aanc_update_gain(OPERATOR_DATA *op_data, AANC_OP_DATA *p_ext_data)
{
    /* Only update EC and FB gains if in hybrid mode */
    if (p_ext_data->anc_fb_path > 0)
    {
        /* Update EC gain */
        if (p_ext_data->ec_gain.fine != p_ext_data->ec_gain_prev.fine)
        {
            stream_anc_set_anc_fine_gain(p_ext_data->anc_channel,
                                        AANC_ANC_PATH_FB_ID,
                                        p_ext_data->ec_gain.fine);
            p_ext_data->ec_gain_prev.fine = p_ext_data->ec_gain.fine;
        }
        if (p_ext_data->ec_gain.coarse != p_ext_data->ec_gain_prev.coarse)
        {
            stream_anc_set_anc_coarse_gain(p_ext_data->anc_channel,
                                        AANC_ANC_PATH_FB_ID,
                                        p_ext_data->ec_gain.coarse);
            p_ext_data->ec_gain_prev.coarse = p_ext_data->ec_gain.coarse;
        }

        /* Update FB gain */
        if (p_ext_data->fb_gain.fine != p_ext_data->fb_gain_prev.fine)
        {
            stream_anc_set_anc_fine_gain(p_ext_data->anc_channel,
                                        p_ext_data->anc_fb_path,
                                        p_ext_data->fb_gain.fine);
            p_ext_data->fb_gain_prev.fine = p_ext_data->fb_gain.fine;
        }
        if (p_ext_data->fb_gain.coarse != p_ext_data->fb_gain_prev.coarse)
        {
            stream_anc_set_anc_coarse_gain(p_ext_data->anc_channel,
                                        p_ext_data->anc_fb_path,
                                        p_ext_data->fb_gain.coarse);
            p_ext_data->fb_gain_prev.coarse = p_ext_data->fb_gain.coarse;
        }
    }

    /* Update FF gain */
    if (p_ext_data->ff_gain.fine != p_ext_data->ff_gain_prev.fine)
    {
        stream_anc_set_anc_fine_gain(p_ext_data->anc_channel,
                                     p_ext_data->anc_ff_path,
                                     p_ext_data->ff_gain.fine);
        p_ext_data->ff_gain_prev.fine = p_ext_data->ff_gain.fine;
    }
    if (p_ext_data->ff_gain.coarse != p_ext_data->ff_gain_prev.coarse)
    {
        stream_anc_set_anc_coarse_gain(p_ext_data->anc_channel,
                                       p_ext_data->anc_ff_path,
                                       p_ext_data->ff_gain.coarse);
        p_ext_data->ff_gain_prev.coarse = p_ext_data->ff_gain.coarse;
    }

    return TRUE;
}
#endif /* RUNNING_ON_KALSIM */

/**
 * \brief  Update touched terminals for the capability
 *
 * \param  p_ext_data  Address of the AANC extra_op_data.
 *
 * \return  boolean indicating success or failure.
 *
 * Because this is solely dependent on the terminal connections it can be
 * calculated in connect/disconnect rather than in every process_data loop.
 */
static bool update_touched_sink_sources(AANC_OP_DATA *p_ext_data)
{
    uint16 touched_sinks = 0;
    uint16 touched_sources = 0;

    /* Update touched sinks & sources */
    if (p_ext_data->inputs[AANC_PLAYBACK_TERMINAL_ID] != NULL)
    {
        touched_sinks |= (1 << AANC_PLAYBACK_TERMINAL_ID);
    }
    if (p_ext_data->outputs[AANC_PLAYBACK_TERMINAL_ID] != NULL)
    {
        touched_sources |= (1 << AANC_PLAYBACK_TERMINAL_ID);
    }

    if (p_ext_data->inputs[AANC_FB_MON_TERMINAL_ID] != NULL &&
        p_ext_data->outputs[AANC_FB_MON_TERMINAL_ID] != NULL)
    {
        touched_sinks |= (1 << AANC_FB_MON_TERMINAL_ID);
        touched_sources |= (1 << AANC_FB_MON_TERMINAL_ID);
    }

    if (p_ext_data->outputs[AANC_MIC_INT_TERMINAL_ID] != NULL)
    {
        touched_sources |= (1 << AANC_MIC_INT_TERMINAL_ID);
    }
    /* Always read internal mic input buffer when processing data */
    touched_sinks |= (1 << AANC_MIC_INT_TERMINAL_ID);

    if (p_ext_data->outputs[AANC_MIC_EXT_TERMINAL_ID] != NULL)
    {
        touched_sources |= (1 << AANC_MIC_EXT_TERMINAL_ID);
    }
    /* Always read external mic input buffer when processing data */
    touched_sinks |= (1 << AANC_MIC_EXT_TERMINAL_ID);

    p_ext_data->touched_sinks = touched_sinks;
    p_ext_data->touched_sources = touched_sources;

    return TRUE;
}

/**
 * \brief  Override the gain value from a SET_CONTROL message.
 *
 * \param  p_ext_data  Address of the AANC extra_op_data.
 * \param  ctrl_value  Value to override with
 * \param  ctrl_src  Source of the control message
 * \param  p_gain_value  Pointer to the gain value to override
 *
 * \return  boolean indicating success or failure.
 */
static bool override_gain(AANC_OP_DATA *p_ext_data, unsigned ctrl_value,
                          unsigned ctrl_src, uint16 *p_gain_value)
{
    if (!((p_ext_data->cur_mode == AANC_SYSMODE_FREEZE) ||
          (p_ext_data->cur_mode == AANC_SYSMODE_STATIC)))
    {
        return FALSE;
    }

    /* Set the gain */
    ctrl_value &= 0xFF;
    *p_gain_value = (uint16) ctrl_value;
    L4_DBG_MSG1("AANC gain override: %d", *p_gain_value);

    return TRUE;
}

static inline void aanc_clear_event(AANC_EVENT *p_event)
{
       p_event->frame_counter =p_event->set_frames;
       p_event->running = AANC_EVENT_CLEAR;
}

/**
 * \brief  Sent an event trigger message.
 *
 * \param op_data  Address of the AANC operator data.
 * \param  detect Boolean indicating whether it is a positive (TRUE) or negative
 *                (FALSE) trigger.
 * \param  id  ID for the event message
 * \param  payload Payload for the event message
 *
 * \return  bool indicating success
 */
static bool aanc_send_event_trigger(OPERATOR_DATA *op_data, bool detect,
                                    uint16 id, uint16 payload)
{
    unsigned msg_size = OPMSG_UNSOLICITED_AANC_EVENT_TRIGGER_WORD_SIZE;
    unsigned *trigger_message = NULL;
    OPMSG_REPLY_ID message_id = OPMSG_REPLY_ID_AANC_EVENT_TRIGGER;
    if (!detect)
    {
        message_id = OPMSG_REPLY_ID_AANC_EVENT_NEGATIVE_TRIGGER;
    }

    trigger_message = xpnewn(msg_size, unsigned);
    if (trigger_message == NULL)
    {
        L2_DBG_MSG("Failed to send AANC event message");
        return FALSE;
    }

    OPMSG_CREATION_FIELD_SET(trigger_message,
                             OPMSG_UNSOLICITED_AANC_EVENT_TRIGGER,
                             ID,
                             id);
    OPMSG_CREATION_FIELD_SET(trigger_message,
                             OPMSG_UNSOLICITED_AANC_EVENT_TRIGGER,
                             PAYLOAD,
                             payload);

    L4_DBG_MSG2("AANC Event Sent: [%d, %d]", trigger_message[0],
                trigger_message[1]);
    common_send_unsolicited_message(op_data, (unsigned)message_id, msg_size,
                                    trigger_message);

    pdelete(trigger_message);

    return TRUE;
}

/**
 * \brief  Process an event clear condition.
 *
 * \param op_data  Address of the AANC operator data.
 * \param  p_event  Address of the event to process
 * \param  id  ID for the negative event message
 * \param  payload Payload for the negative event message
 *
 * \return  void.
 */
static void aanc_process_event_clear_condition(OPERATOR_DATA *op_data,
                                               AANC_EVENT *p_event,
                                               uint16 id, uint16 payload)
{
    switch (p_event->running)
    {
            case AANC_EVENT_CLEAR:
                break;
            case AANC_EVENT_DETECTED:
                /* Have detected but not sent message so clear */
                aanc_clear_event(p_event);
                break;
            case AANC_EVENT_SENT:
                aanc_send_event_trigger(op_data, FALSE, id, payload);
                aanc_clear_event(p_event);
                break;
    }
}

/**
 * \brief  Initialize events for messaging.
 *
 * \param  op_data  Address of the operator data
 * \param  p_ext_data  Address of the AANC extra_op_data.
 *
 * \return  void.
 */
static void aanc_initialize_events(OPERATOR_DATA *op_data, AANC_OP_DATA *p_ext_data)
{
    unsigned set_frames = 0;
    AANC_PARAMETERS *p_params = &p_ext_data->aanc_cap_params;

    set_frames = (p_params->OFFSET_EVENT_GAIN_STUCK * AANC_FRAME_RATE)
                                                         >> TIMER_PARAM_SHIFT;
    L4_DBG_MSG1("AANC Gain Event Initialized at %d frames", set_frames);
    p_ext_data->gain_event.set_frames = set_frames;
    aanc_process_event_clear_condition(op_data, &p_ext_data->gain_event,
                                       AANC_EVENT_ID_GAIN, 0);

    set_frames = (p_params->OFFSET_EVENT_ED_STUCK * AANC_FRAME_RATE)
                                                         >> TIMER_PARAM_SHIFT;
    L4_DBG_MSG1("AANC ED Event Initialized at %d frames", set_frames);
    p_ext_data->ed_event.set_frames = set_frames;
    aanc_process_event_clear_condition(op_data, &p_ext_data->ed_event,
                                       AANC_EVENT_ID_ED, 0);

    set_frames = (p_params->OFFSET_EVENT_QUIET_DETECT * AANC_FRAME_RATE)
                                                         >> TIMER_PARAM_SHIFT;
    L4_DBG_MSG1("AANC Quiet Mode Detect Initialized at %d frames", set_frames);
    p_ext_data->quiet_event_detect.set_frames = set_frames;
    aanc_process_event_clear_condition(op_data, &p_ext_data->quiet_event_detect,
                                       AANC_EVENT_ID_QUIET, 0);

    set_frames = (p_params->OFFSET_EVENT_QUIET_CLEAR * AANC_FRAME_RATE)
                                                         >> TIMER_PARAM_SHIFT;
    L4_DBG_MSG1("AANC Quiet Mode Cleared Initialized at %d frames", set_frames);
    p_ext_data->quiet_event_clear.set_frames = set_frames;
    aanc_process_event_clear_condition(op_data, &p_ext_data->quiet_event_clear,
                                       AANC_EVENT_ID_QUIET, 0);

    set_frames = (p_params->OFFSET_EVENT_CLIP_STUCK * AANC_FRAME_RATE)
                                                         >> TIMER_PARAM_SHIFT;
    L4_DBG_MSG1("AANC Clip Event Initialized at %d frames", set_frames);
    p_ext_data->clip_event.set_frames = set_frames;
    aanc_process_event_clear_condition(op_data, &p_ext_data->clip_event,
                                       AANC_EVENT_ID_CLIP, 0);

    set_frames = (p_params->OFFSET_EVENT_SAT_STUCK * AANC_FRAME_RATE)
                                                         >> TIMER_PARAM_SHIFT;
    L4_DBG_MSG1("AANC Saturation Event Initialized at %d frames", set_frames);
    p_ext_data->sat_event.set_frames = set_frames;
    aanc_process_event_clear_condition(op_data, &p_ext_data->sat_event,
                                       AANC_EVENT_ID_SAT, 0);

    set_frames = (p_params->OFFSET_EVENT_SELF_TALK * AANC_FRAME_RATE)
                                                         >> TIMER_PARAM_SHIFT;
    L4_DBG_MSG1("AANC Self-Talk Event Initialized at %d frames", set_frames);
    p_ext_data->self_talk_event.set_frames = set_frames;
    aanc_process_event_clear_condition(op_data, &p_ext_data->self_talk_event,
                                       AANC_EVENT_ID_SELF_TALK, 0);
}

/**
 * \brief  Process an event detection condition.
 *
 * \param op_data  Address of the AANC operator data.
 * \param  p_event  Address of the event to process
 * \param  id  ID for the event message
 * \param  payload Payload for the event message
 *
 * \return  void.
 */
static void aanc_process_event_detect_condition(OPERATOR_DATA *op_data,
                                                AANC_EVENT *p_event,
                                                uint16 id, uint16 payload)
{
    switch (p_event->running)
    {
        case AANC_EVENT_CLEAR:
            p_event->frame_counter -= 1;
            p_event->running = AANC_EVENT_DETECTED;
            break;
        case AANC_EVENT_DETECTED:
            p_event->frame_counter -= 1;
            if (p_event->frame_counter <= 0)
            {
                aanc_send_event_trigger(op_data, TRUE, id, payload);
                p_event->running = AANC_EVENT_SENT;
            }
            break;
        case AANC_EVENT_SENT:
            break;
    }
}

/**
 * \brief  Calculate events for messaging.
 *
 * \param op_data  Address of the AANC operator data.
 * \param  p_ext_data  Address of the AANC extra_op_data.
 *
 * \return  boolean indicating success or failure.
 */
static bool aanc_process_events(OPERATOR_DATA *op_data,
                                AANC_OP_DATA *p_ext_data)
{
    /* Adaptive gain event: reset if ED detected */
    if (p_ext_data->flags & AANC_ED_FLAG_MASK)
    {
        /* If we had previously sent a message then send the negative trigger */
        if (p_ext_data->gain_event.running == AANC_EVENT_SENT)
        {
            aanc_send_event_trigger(op_data, FALSE, AANC_EVENT_ID_GAIN, 0);
        }
        aanc_clear_event(&p_ext_data->gain_event);
    }
    /* Condition holds */
    else if (p_ext_data->ff_gain.fine == p_ext_data->ff_gain_prev.fine)
    {
        aanc_process_event_detect_condition(op_data, &p_ext_data->gain_event,
                                            AANC_EVENT_ID_GAIN,
                                            p_ext_data->ff_gain.fine);
    }
    /* Condition cleared */
    else
    {
        aanc_process_event_clear_condition(op_data, &p_ext_data->gain_event,
                                           AANC_EVENT_ID_GAIN,
                                           p_ext_data->ff_gain.fine);
    }

    /* ED event */
    bool cur_ed = p_ext_data->flags & AANC_ED_FLAG_MASK;
    bool prev_ed = p_ext_data->prev_flags & AANC_ED_FLAG_MASK;
    if (cur_ed)
    {
        /* Non-zero flags and no change starts/continues event */
        if (cur_ed == prev_ed)
        {
            aanc_process_event_detect_condition(op_data, &p_ext_data->ed_event,
                                                AANC_EVENT_ID_ED,
                                                (uint16)cur_ed);
        }
    }
    else
    {
        /* Flags reset causes event to be reset */
        if (cur_ed != prev_ed)
        {
            aanc_process_event_clear_condition(op_data, &p_ext_data->ed_event,
                                               AANC_EVENT_ID_ED,
                                               (uint16)cur_ed);
        }
    }

    /* Quiet mode has positive and negative triggers */
    bool cur_qm = p_ext_data->flags & AANC_FLAGS_QUIET_MODE;
    bool prev_qm = p_ext_data->flags & AANC_FLAGS_QUIET_MODE;

    if (cur_qm)
    {
        if (cur_qm == prev_qm)
        {
            switch (p_ext_data->quiet_event_detect.running)
            {
                case AANC_EVENT_CLEAR:
                    p_ext_data->quiet_event_detect.frame_counter -= 1;
                    p_ext_data->quiet_event_detect.running = AANC_EVENT_DETECTED;
                    aanc_clear_event(&p_ext_data->quiet_event_clear);
                    break;
                case AANC_EVENT_DETECTED:
                    p_ext_data->quiet_event_detect.frame_counter -= 1;
                    if (p_ext_data->quiet_event_detect.frame_counter <= 0)
                    {
                        aanc_send_event_trigger(op_data, TRUE,
                                                AANC_EVENT_ID_QUIET, 0);
                        p_ext_data->quiet_event_detect.running = AANC_EVENT_SENT;
                    }
                    break;
                case AANC_EVENT_SENT:
                    break;
            }
        }
    }
    else
    {
        if (cur_qm == prev_qm)
        {
            switch (p_ext_data->quiet_event_clear.running)
            {
                case AANC_EVENT_CLEAR:
                    p_ext_data->quiet_event_clear.frame_counter -= 1;
                    p_ext_data->quiet_event_clear.running = AANC_EVENT_DETECTED;
                    aanc_clear_event(&p_ext_data->quiet_event_detect);
                    break;
                case AANC_EVENT_DETECTED:
                    p_ext_data->quiet_event_clear.frame_counter -= 1;
                    if (p_ext_data->quiet_event_clear.frame_counter <= 0)
                    {
                        aanc_send_event_trigger(op_data, FALSE,
                                                AANC_EVENT_ID_QUIET, 0);
                        p_ext_data->quiet_event_clear.running = AANC_EVENT_SENT;
                    }
                    break;
                case AANC_EVENT_SENT:
                    break;
            }
        }
    }

    /* Clipping event */
    bool cur_clip = p_ext_data->flags & AANC_CLIPPING_FLAG_MASK;
    bool prev_clip = p_ext_data->prev_flags & AANC_CLIPPING_FLAG_MASK;
    if (cur_clip)
    {
        /* Non-zero flags and no change starts/continues event */
        if (cur_clip == prev_clip)
        {
            aanc_process_event_detect_condition(op_data,
                                                &p_ext_data->clip_event,
                                                AANC_EVENT_ID_CLIP,
                                                (uint16)cur_clip);
        }
    }
    else
    {
        /* Flags reset causes event to be reset */
        if (cur_clip != prev_clip)
        {
            aanc_process_event_clear_condition(op_data, &p_ext_data->clip_event,
                                               AANC_EVENT_ID_CLIP,
                                               (uint16)cur_clip);
        }
    }

    /* Saturation event */
    bool cur_sat = p_ext_data->flags & AANC_SATURATION_FLAG_MASK;
    bool prev_sat = p_ext_data->prev_flags & AANC_SATURATION_FLAG_MASK;
    if (cur_sat)
    {
        /* Non-zero flags and no change starts/continues event */
        if (cur_sat == prev_sat)
        {
            aanc_process_event_detect_condition(op_data, &p_ext_data->sat_event,
                                                AANC_EVENT_ID_SAT,
                                                (uint16)cur_sat);
        }
    }
    else
    {
        /* Flags reset causes event to be reset */
        if (cur_sat != prev_sat)
        {
            aanc_process_event_clear_condition(op_data, &p_ext_data->sat_event,
                                               AANC_EVENT_ID_SAT,
                                               (uint16)cur_sat);
        }
    }

    /* Self-talk event */
    int cur_ext = p_ext_data->ag->p_ed_ext_stats->spl;
    int cur_int = p_ext_data->ag->p_ed_int_stats->spl;
    int delta_ext = cur_int - cur_ext;
    if (delta_ext > 0)
    {
        aanc_process_event_detect_condition(op_data,
                                            &p_ext_data->self_talk_event,
                                            AANC_EVENT_ID_SELF_TALK,
                                            (uint16)(delta_ext >> 16));
    }
    else
    {
        aanc_process_event_clear_condition(op_data,
                                           &p_ext_data->self_talk_event,
                                           AANC_EVENT_ID_SELF_TALK,
                                           (uint16)(delta_ext >> 16));
    }

    return TRUE;
}

/****************************************************************************
Capability API Handlers
*/

bool aanc_create(OPERATOR_DATA *op_data, void *message_data,
                 unsigned *response_id, void **resp_data)
{
    AANC_OP_DATA *p_ext_data = get_instance_data(op_data);

    /* NB: create is passed a zero-initialized structure so any fields not
     * explicitly initialized are 0.
     */

    L5_DBG_MSG1("AANC Create: p_ext_data at 0x%08X", p_ext_data);

    int i;
    unsigned *p_default_params = NULL; /* Pointer to default params */
    unsigned *p_cap_params = NULL;     /* Pointer to capability params */

    if (!base_op_create(op_data, message_data, response_id, resp_data))
    {
        return FALSE;
    }

    /* patch_fn_shared(aanc_capability);  TODO: patch functions */

    /* Assume the response to be command FAILED. If we reach the correct
     * termination point in create then change it to STATUS_OK.
     */
    base_op_change_response_status(resp_data, STATUS_CMD_FAILED);

    /* Initialize buffers */
    for (i = 0; i < AANC_MAX_SINKS; i++)
    {
        p_ext_data->inputs[i] = NULL;
    }
    for (i = 0; i < AANC_MAX_SOURCES; i++)
    {
        p_ext_data->outputs[i] = NULL;
    }

    for (i = 0; i < AANC_NUM_METADATA_CHANNELS; i++)
    {
        p_ext_data->metadata_ip[i] = NULL;
        p_ext_data->metadata_op[i] = NULL;
    }

    /* Initialize capid and sample rate fields */
    p_ext_data->cap_id = base_op_get_cap_id(op_data);
    if (p_ext_data->cap_id == AANC_MONO_32K_CAP_ID)
    {
        /* 32kHz is currently unsupported. */
        L1_DBG_MSG("AANC 32kHz not unsupported");
        return TRUE;
    }
    else
    {
        p_ext_data->sample_rate = 16000;
    }

    /* Initialize parameters */
    p_default_params = (unsigned*) AANC_GetDefaults(p_ext_data->cap_id);
    p_cap_params = (unsigned*) &p_ext_data->aanc_cap_params;
    if(!cpsInitParameters(&p_ext_data->params_def, p_default_params,
                          p_cap_params, sizeof(AANC_PARAMETERS)))
    {
       return TRUE;
    }

    /* Initialize system mode */
    p_ext_data->cur_mode = AANC_SYSMODE_FULL;
    p_ext_data->host_mode = AANC_SYSMODE_FULL;
    p_ext_data->qact_mode = AANC_SYSMODE_FULL;

    /* Trigger re-initialization at start */
    p_ext_data->re_init_flag = TRUE;
    p_ext_data->re_init_hard = TRUE;

    if (!aanc_proc_create(&p_ext_data->ag, p_ext_data->sample_rate))
    {
        L4_DBG_MSG("Failed to create AG data");
        return TRUE;
    }

    p_ext_data->anc_channel = AANC_ANC_INSTANCE_ANC0_ID;
    /* Default to hybrid: ff path is FFB, fb path is FFA */
    p_ext_data->anc_ff_path = AANC_ANC_PATH_FFB_ID;
    p_ext_data->anc_fb_path = AANC_ANC_PATH_FFA_ID;
    p_ext_data->anc_clock_check_value = AANC_HYBRID_ENABLE;

#ifdef USE_AANC_LICENSING
    p_ext_data->license_status = AANC_LICENSE_STATUS_LICENSING_BUILD_STATUS;
#endif

    /* Operator creation was succesful, change respone to STATUS_OK*/
    base_op_change_response_status(resp_data, STATUS_OK);

    L4_DBG_MSG("AANC: Created");
    return TRUE;
}

bool aanc_destroy(OPERATOR_DATA *op_data, void *message_data,
                  unsigned *response_id, void **resp_data)
{
    AANC_OP_DATA *p_ext_data = get_instance_data(op_data);

    /* call base_op destroy that creates and fills response message, too */
    if (!base_op_destroy(op_data, message_data, response_id, resp_data))
    {
        return FALSE;
    }

    /* patch_fn_shared(aanc_capability); TODO: patch functions */

    if (p_ext_data != NULL)
    {
        aanc_proc_destroy(&p_ext_data->ag);

        L4_DBG_MSG("AANC: Cleanup complete.");
    }

    L4_DBG_MSG("AANC: Destroyed");
    return TRUE;
}

bool aanc_start(OPERATOR_DATA *op_data, void *message_data,
                unsigned *response_id, void **resp_data)
{
    AANC_OP_DATA *p_ext_data = get_instance_data(op_data);
    /* patch_fn_shared(aanc_capability); TODO: patch functions */

    /* Start with the assumption that we fail and change later if we succeed */
    if (!base_op_build_std_response_ex(op_data, STATUS_CMD_FAILED, resp_data))
    {
        return FALSE;
    }

    /* Check that the model has been loaded */
    unsigned debug = p_ext_data->aanc_cap_params.OFFSET_AANC_DEBUG;
    unsigned model_loaded = p_ext_data->flags & AANC_MODEL_LOADED;
    if ((model_loaded != AANC_MODEL_LOADED) &&
        !(debug & AANC_CONFIG_AANC_DEBUG_DISABLE_AG_MODEL_CHECK))
    {
        L4_DBG_MSG("AANC start failure: model not loaded");
        return TRUE;
    }

    /* Check that we have a minimum number of terminals connected */
    if (p_ext_data->inputs[AANC_MIC_INT_TERMINAL_ID] == NULL ||
        p_ext_data->inputs[AANC_MIC_EXT_TERMINAL_ID] == NULL)
    {
        L4_DBG_MSG("AANC start failure: mic inputs not connected");
        return TRUE;
    }

    /* TODO: check ANC HW enable status */

    /* Initialize EC paths to static values */
    p_ext_data->ec_gain.fine = p_ext_data->ec_static_gain.fine;
    p_ext_data->ec_gain.coarse = p_ext_data->ec_static_gain.coarse;

    /* Initialize coarse to static and fine to 0 for FF and FB paths. */
    if (p_ext_data->cur_mode == AANC_SYSMODE_FULL)
    {
        p_ext_data->fb_gain.coarse = p_ext_data->fb_static_gain.coarse;
        p_ext_data->ff_gain.coarse = p_ext_data->ff_static_gain.coarse;
        p_ext_data->fb_gain.fine = 0;
        p_ext_data->ff_gain.fine = 0;

        aanc_update_gain(op_data, p_ext_data);
    }

    /* Set reinitialization flags to ensure first run behavior */
    p_ext_data->re_init_flag = TRUE;
    p_ext_data->re_init_hard = TRUE;

    /* All good */
    base_op_change_response_status(resp_data, STATUS_OK);

    L4_DBG_MSG("AANC Started");
    return TRUE;
}

bool aanc_reset(OPERATOR_DATA *op_data, void *message_data,
                unsigned *response_id, void **resp_data)
{
    AANC_OP_DATA *p_ext_data = get_instance_data(op_data);

    if (!base_op_reset(op_data, message_data, response_id, resp_data))
    {
        return FALSE;
    }

    p_ext_data->re_init_flag = TRUE;
    p_ext_data->re_init_hard = TRUE;

    L4_DBG_MSG("AANC: Reset");
    return TRUE;
}

bool aanc_connect(OPERATOR_DATA *op_data, void *message_data,
                  unsigned *response_id, void **resp_data)
{
    AANC_OP_DATA *p_ext_data = get_instance_data(op_data);
    unsigned terminal_id = OPMGR_GET_OP_CONNECT_TERMINAL_ID(message_data);
    tCbuffer* pterminal_buf = OPMGR_GET_OP_CONNECT_BUFFER(message_data);
    unsigned terminal_num = terminal_id & TERMINAL_NUM_MASK;
    unsigned max_value;
    tCbuffer** selected_buffer;
    tCbuffer** selected_metadata;

    /* Create the response. If there aren't sufficient resources for this fail
     * early. */
    if (!base_op_build_std_response_ex(op_data, STATUS_OK, resp_data))
    {
        return FALSE;
    }

    /* can't connect while running if adaptive gain is not disabled */
    if (opmgr_op_is_running(op_data))
    {
        if (p_ext_data->aanc_cap_params.OFFSET_DISABLE_AG_CALC == 0)
        {
            base_op_change_response_status(resp_data, STATUS_CMD_FAILED);
            return TRUE;
        }
    }

    /* Determine whether sink or source terminal being connected */
    if (terminal_id & TERMINAL_SINK_MASK)
    {
        L4_DBG_MSG1("AANC connect: sink terminal %d", terminal_num);
        max_value = AANC_MAX_SINKS;
        selected_buffer = p_ext_data->inputs;
        selected_metadata = p_ext_data->metadata_ip;
    }
    else
    {
        L4_DBG_MSG1("AANC connect: source terminal %d", terminal_num);
        max_value = AANC_MAX_SOURCES;
        selected_buffer = p_ext_data->outputs;
        selected_metadata = p_ext_data->metadata_op;
    }

    /* Can't use invalid ID */
    if (terminal_num >= max_value)
    {
        /* invalid terminal id */
        L4_DBG_MSG1("AANC connect failed: invalid terminal %d", terminal_num);
        base_op_change_response_status(resp_data, STATUS_INVALID_CMD_PARAMS);
        return TRUE;
    }

    /* Can't connect if already connected */
    if (selected_buffer[terminal_num] != NULL)
    {
        L4_DBG_MSG1("AANC connect failed: terminal %d already connected",
                    terminal_num);
        base_op_change_response_status(resp_data, STATUS_CMD_FAILED);
        return TRUE;
    }

    selected_buffer[terminal_num] = pterminal_buf;

    if (terminal_num == AANC_PLAYBACK_TERMINAL_ID)
    {
        /* playback metadata has its own metadata channel */
        if (selected_metadata[AANC_METADATA_PLAYBACK_ID] == NULL &&
            buff_has_metadata(pterminal_buf))
        {
            selected_metadata[AANC_METADATA_PLAYBACK_ID] = pterminal_buf;
        }
    }
    else
    {
        /* mic int/ext and fb mon metadata all muxed onto the same metadata
         * channel
         */
        if (selected_metadata[AANC_METADATA_MIC_ID] == NULL &&
            buff_has_metadata(pterminal_buf))
        {
            selected_metadata[AANC_METADATA_MIC_ID] = pterminal_buf;
        }
    }

    update_touched_sink_sources(p_ext_data);

    return TRUE;
}

bool aanc_disconnect(OPERATOR_DATA *op_data, void *message_data,
                     unsigned *response_id, void **resp_data)
{
    AANC_OP_DATA *p_ext_data = get_instance_data(op_data);
    unsigned terminal_id = OPMGR_GET_OP_CONNECT_TERMINAL_ID(message_data);
    unsigned terminal_num = terminal_id & TERMINAL_NUM_MASK;

    /* Variables used for distinguishing source/sink */
    unsigned max_value;
    tCbuffer** selected_buffer;
    tCbuffer** selected_metadata;

    /* Variables used for finding an alternative metadata channel at
     * disconnect.
     */
    unsigned i;
    bool found_alternative = FALSE;

    /* Create the response. If there aren't sufficient resources for this fail
     * early. */
    if (!base_op_build_std_response_ex(op_data, STATUS_OK, resp_data))
    {
        return FALSE;
    }

    /* can't disconnect while running if adaptive gain is not disabled */
    if (opmgr_op_is_running(op_data))
    {
        if (p_ext_data->aanc_cap_params.OFFSET_DISABLE_AG_CALC == 0)
        {
            base_op_change_response_status(resp_data, STATUS_CMD_FAILED);
            return TRUE;
        }
    }

    /* Determine whether sink or source terminal being disconnected */
    if (terminal_id & TERMINAL_SINK_MASK)
    {
        L4_DBG_MSG1("AANC disconnect: sink terminal %d", terminal_num);
        max_value = AANC_MAX_SINKS;
        selected_buffer = p_ext_data->inputs;
        selected_metadata = p_ext_data->metadata_ip;
    }
    else
    {
        L4_DBG_MSG1("AANC disconnect: source terminal %d", terminal_num);
        max_value = AANC_MAX_SOURCES;
        selected_buffer = p_ext_data->outputs;
        selected_metadata = p_ext_data->metadata_op;
    }

    /* Can't use invalid ID */
    if (terminal_num >= max_value)
    {
        /* invalid terminal id */
        L4_DBG_MSG1("AANC disconnect failed: invalid terminal %d",
                    terminal_num);
        base_op_change_response_status(resp_data, STATUS_INVALID_CMD_PARAMS);
        return TRUE;
    }

    /* Can't disconnect if not connected */
    if (selected_buffer[terminal_num] == NULL)
    {
        L4_DBG_MSG1("AANC disconnect failed: terminal %d not connected",
                    terminal_num);
        base_op_change_response_status(resp_data, STATUS_CMD_FAILED);
        return TRUE;
    }

    if (terminal_num == AANC_PLAYBACK_TERMINAL_ID)
    {
        /* playback metadata has its own metadata channel */
        if (selected_metadata[AANC_METADATA_PLAYBACK_ID] != NULL)
        {
            selected_metadata[AANC_METADATA_PLAYBACK_ID] = NULL;
        }
    }
    else
    {
        /* Mic int/ext and fb mon metadata all muxed onto the same metadata
         * channel. Try to find an alternative channel to set the metadata to if
         * we're disconnecting the existing metadata channel. */
        if (selected_metadata[AANC_METADATA_MIC_ID] ==
            selected_buffer[terminal_num])
        {
            for (i = 1; i < max_value; i++)
            {
                if (i == terminal_num)
                {
                    continue;
                }
                if (selected_buffer[i] != NULL &&
                    buff_has_metadata(selected_buffer[i]))
                {
                    selected_metadata[AANC_METADATA_MIC_ID] = selected_buffer[i];
                    found_alternative = TRUE;
                    break;
                }
            }
            if (!found_alternative)
            {
                selected_metadata[AANC_METADATA_MIC_ID] = NULL;
            }
        }
    }

    selected_buffer[terminal_num] = NULL;

    update_touched_sink_sources(p_ext_data);

    return TRUE;
}

bool aanc_buffer_details(OPERATOR_DATA *op_data, void *message_data,
                         unsigned *response_id, void **resp_data)
{
    AANC_OP_DATA *p_ext_data = get_instance_data(op_data);
    unsigned terminal_id = OPMGR_GET_OP_CONNECT_TERMINAL_ID(message_data);
#ifndef DISABLE_IN_PLACE
    unsigned terminal_num = terminal_id & TERMINAL_NUM_MASK;
#endif

    /* Variables used for distinguishing source/sink */
    unsigned max_value;
    tCbuffer** opposite_buffer;
    tCbuffer** selected_metadata;

    /* Response pointer */
    OP_BUF_DETAILS_RSP *p_resp = (OP_BUF_DETAILS_RSP*) *resp_data;

    if (!base_op_buffer_details(op_data, message_data, response_id, resp_data))
    {
        return FALSE;
    }

#ifdef DISABLE_IN_PLACE
    p_resp->runs_in_place = FALSE;
    p_resp->b.buffer_size = AANC_DEFAULT_BUFFER_SIZE;
#else

    /* Determine whether sink or source terminal being disconnected */
    if (terminal_id & TERMINAL_SINK_MASK)
    {
        L4_DBG_MSG1("AANC buffer details: sink buffer %d", terminal_num);
        max_value = AANC_MAX_SINKS;
        opposite_buffer = p_ext_data->outputs;
        selected_metadata = p_ext_data->metadata_ip;
    }
    else
    {
        L4_DBG_MSG1("AANC buffer details: source buffer %d", terminal_num);
        max_value = AANC_MAX_SOURCES;
        opposite_buffer = p_ext_data->inputs;
        selected_metadata = p_ext_data->metadata_op;
    }

    /* Can't use invalid ID */
    if (terminal_num >= max_value)
    {
        /* invalid terminal id */
        L4_DBG_MSG1("AANC buffer details failed: invalid terminal %d",
                    terminal_num);
        base_op_change_response_status(resp_data, STATUS_INVALID_CMD_PARAMS);
        return TRUE;
    }

    if (terminal_num == AANC_PLAYBACK_TERMINAL_ID)
    {
        if (opposite_buffer[AANC_PLAYBACK_TERMINAL_ID] != NULL)
        {
            p_resp->runs_in_place = TRUE;
            p_resp->b.in_place_buff_params.in_place_terminal = \
                terminal_id ^ TERMINAL_SINK_MASK;
            p_resp->b.in_place_buff_params.size = AANC_DEFAULT_BUFFER_SIZE;
            p_resp->b.in_place_buff_params.buffer = \
                opposite_buffer[AANC_PLAYBACK_TERMINAL_ID];
            L4_DBG_MSG1("aanc_playback_buffer_details: %d",
                        p_resp->b.buffer_size);
        }
        else
        {
            p_resp->runs_in_place = FALSE;
            p_resp->b.buffer_size = AANC_DEFAULT_BUFFER_SIZE;
        }
        p_resp->supports_metadata = TRUE;
        p_resp->metadata_buffer = selected_metadata[AANC_METADATA_PLAYBACK_ID];
    }
    else
    {
        if (opposite_buffer[terminal_num] != NULL)
        {
            p_resp->runs_in_place = TRUE;
            p_resp->b.in_place_buff_params.in_place_terminal = \
                terminal_id ^ TERMINAL_SINK_MASK;
            p_resp->b.in_place_buff_params.size = AANC_DEFAULT_BUFFER_SIZE;
            p_resp->b.in_place_buff_params.buffer = \
                opposite_buffer[terminal_num];
            L4_DBG_MSG1("aanc_buffer_details: %d", p_resp->b.buffer_size);
        }
        else
        {
            p_resp->runs_in_place = FALSE;
            p_resp->b.buffer_size = AANC_DEFAULT_BUFFER_SIZE;
        }
        p_resp->supports_metadata = TRUE;
        p_resp->metadata_buffer = selected_metadata[AANC_METADATA_MIC_ID];
    }

#endif /* DISABLE_IN_PLACE */
    return TRUE;
}

bool aanc_get_sched_info(OPERATOR_DATA *op_data, void *message_data,
                         unsigned *response_id, void **resp_data)
{
    OP_SCHED_INFO_RSP* resp;

    resp = base_op_get_sched_info_ex(op_data, message_data, response_id);
    if (resp == NULL)
    {
        return base_op_build_std_response_ex(op_data, STATUS_CMD_FAILED,
                                             resp_data);
    }

    *resp_data = resp;
    resp->block_size = AANC_DEFAULT_BLOCK_SIZE;

    return TRUE;
}

/****************************************************************************
Opmsg handlers
*/
bool aanc_opmsg_set_control(OPERATOR_DATA *op_data, void *message_data,
                            unsigned *resp_length,
                            OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AANC_OP_DATA *p_ext_data = get_instance_data(op_data);

    unsigned i;
    unsigned num_controls;
    unsigned ctrl_value;
    unsigned ctrl_id;

    CPS_CONTROL_SOURCE  ctrl_src;
    OPMSG_RESULT_STATES result = OPMSG_RESULT_STATES_NORMAL_STATE;

    if(!cps_control_setup(message_data, resp_length, resp_data, &num_controls))
    {
       return FALSE;
    }

    /* Iterate through the control messages looking for mode and gain override
     * messages */
    for (i=0; i<num_controls; i++)
    {
        ctrl_id = cps_control_get(message_data, i, &ctrl_value, &ctrl_src);

        /* Mode override */
        if (ctrl_id == OPMSG_CONTROL_MODE_ID)
        {

            /* Check for valid mode */
            ctrl_value &= AANC_SYSMODE_MASK;
            if (ctrl_value >= AANC_SYSMODE_MAX_MODES)
            {
                result = OPMSG_RESULT_STATES_INVALID_CONTROL_VALUE;
                break;
            }

            /* Re-initialize event states if not in quiet mode */
            if (ctrl_value != AANC_SYSMODE_QUIET)
            {
                aanc_initialize_events(op_data, p_ext_data);
            }

            /* Gain update logic */
            switch (ctrl_value)
            {
                case AANC_SYSMODE_STANDBY:
                    /* Standby doesn't change gains */
                case AANC_SYSMODE_GENTLE_MUTE:
                    /* Gentle mute doesn't change gains */
                case AANC_SYSMODE_FREEZE:
                    /* Freeze doesn't change gains */
                    break;
                case AANC_SYSMODE_MUTE_ANC:
                    /* Mute FF and FB gains */
                    p_ext_data->ff_gain.fine = 0;
                    p_ext_data->fb_gain.fine = 0;
                    break;
                case AANC_SYSMODE_STATIC:
                    /* Set all gains to static values */
                    p_ext_data->ff_gain.fine = p_ext_data->ff_static_gain.fine;
                    p_ext_data->ff_gain.coarse = p_ext_data->ff_static_gain.coarse;
                    p_ext_data->fb_gain.fine = p_ext_data->fb_static_gain.fine;
                    p_ext_data->fb_gain.coarse = p_ext_data->fb_static_gain.coarse;
                    p_ext_data->ec_gain.fine = p_ext_data->ec_static_gain.fine;
                    p_ext_data->ec_gain.coarse = p_ext_data->ec_static_gain.coarse;
                    break;
                case AANC_SYSMODE_FULL:
                    /* Set gains to static & FF fine to algorithm value */
                    p_ext_data->ff_gain.fine = (uint16)p_ext_data->ag->p_fxlms_stats->adaptive_gain;
                    p_ext_data->ff_gain.coarse = p_ext_data->ff_static_gain.coarse;
                    p_ext_data->fb_gain.fine = p_ext_data->fb_static_gain.fine;
                    p_ext_data->fb_gain.coarse = p_ext_data->fb_static_gain.coarse;
                    p_ext_data->ec_gain.fine = p_ext_data->ec_static_gain.fine;
                    p_ext_data->ec_gain.coarse = p_ext_data->ec_static_gain.coarse;
                    break;
                case AANC_SYSMODE_QUIET:
                    /* Quiet mode changes FB fine gain */
                    p_ext_data->ff_gain.fine = (uint16)p_ext_data->ag->p_fxlms_stats->adaptive_gain;
                    p_ext_data->ff_gain.coarse = p_ext_data->ff_static_gain.coarse;
                    p_ext_data->fb_gain.fine = p_ext_data->fb_static_gain.fine >> 1;
                    p_ext_data->fb_gain.coarse = p_ext_data->fb_static_gain.coarse;
                    p_ext_data->ec_gain.fine = p_ext_data->ec_static_gain.fine;
                    p_ext_data->ec_gain.coarse = p_ext_data->ec_static_gain.coarse;
                    break;
                default:
                    /* Handled by early exit above */
                    break;
            }

            /* Gentle mute reinitializes the flag */
            if (ctrl_value == AANC_SYSMODE_GENTLE_MUTE)
            {
                p_ext_data->gentle_mute_init_flag = FALSE;
            }

            /* Set target noise reduction to 1 when in Quiet Mode */
            if (ctrl_value == AANC_SYSMODE_QUIET)
            {
                p_ext_data->ag->p_fxlms_params->target_nr = AANC_QUIET_MODE_TARGET_NR;

            }
            else
            {
                p_ext_data->ag->p_fxlms_params->target_nr = AANC_NORMAL_TARGET_NR;
            }

            /* Determine control mode source and set override flags for mode */
            if (ctrl_src == CPS_SOURCE_HOST)
            {
                p_ext_data->host_mode = ctrl_value;
            }
            else
            {
                p_ext_data->qact_mode = ctrl_value;
                /* Set or clear the QACT override flag.
                * &= is used to preserve the state of the gain bits in the
                * override word.
                */
                if (ctrl_src == CPS_SOURCE_OBPM_ENABLE)
                {
                    p_ext_data->ovr_control |= AANC_CONTROL_MODE_OVERRIDE;
                }
                else
                {
                    p_ext_data->ovr_control &= AANC_OVERRIDE_MODE_MASK;
                }
            }

            continue;
        }

        /* In/Out of Ear control */
        else if (ctrl_id == AANC_CONSTANT_IN_OUT_EAR_CTRL)
        {
            ctrl_value &= 0x01;
            p_ext_data->in_out_status = ctrl_value;

            /* No override flags indicated for in/out of ear */
            continue;
        }

        /* Channel control */
        else if (ctrl_id == AANC_CONSTANT_CHANNEL_CTRL)
        {
            /* Channel can only be updated from the host */
            if (ctrl_src == CPS_SOURCE_HOST)
            {
                ctrl_value &= 0x1;
                if (ctrl_value == 0)
                {
                    p_ext_data->anc_channel = AANC_ANC_INSTANCE_ANC0_ID;
                }
                else
                {
                    p_ext_data->anc_channel = AANC_ANC_INSTANCE_ANC1_ID;
                }
                L4_DBG_MSG1("AANC channel override: %d",
                            p_ext_data->anc_channel);
            }

            /* No override flags indicated for channel */
            continue;
        }

        /* Feedforward control */
        else if (ctrl_id == AANC_CONSTANT_FEEDFORWARD_CTRL)
        {
            /* Feedforward can only be updated from the host */
            if (ctrl_src == CPS_SOURCE_HOST)
            {
                ctrl_value &= 0x1;
                if (ctrl_value == 0)
                {
                    /* hybrid */
                    p_ext_data->anc_ff_path = AANC_ANC_PATH_FFB_ID;
                    p_ext_data->anc_fb_path = AANC_ANC_PATH_FFA_ID;
                    p_ext_data->anc_clock_check_value = AANC_HYBRID_ENABLE;
                }
                else
                {
                    /* feedforward only */
                    p_ext_data->anc_ff_path = AANC_ANC_PATH_FFA_ID;
                    p_ext_data->anc_fb_path = 0;
                    p_ext_data->anc_clock_check_value = AANC_FEEDFORWARD_ENABLE;
                }
                L4_DBG_MSG2("AANC feedforward override: %d - %d",
                            p_ext_data->anc_ff_path, p_ext_data->anc_fb_path);
            }

            /* No override flags indicated for feedforward */
            continue;
        }

        /* Gain override */
        else if (ctrl_id == AANC_CONSTANT_FF_FINE_GAIN_CTRL)
        {
            if (override_gain(p_ext_data, ctrl_value, ctrl_src,
                              &p_ext_data->ff_gain.fine))
            {
                aanc_update_gain(op_data, p_ext_data);
            }
            else
            {
                result = OPMSG_RESULT_STATES_PARAMETER_STATE_NOT_READY;
            }
            continue;
        }
        else if (ctrl_id == AANC_CONSTANT_FF_COARSE_GAIN_CTRL)
        {
            if (override_gain(p_ext_data, ctrl_value, ctrl_src,
                              &p_ext_data->ff_gain.coarse))
            {
                aanc_update_gain(op_data, p_ext_data);
            }
            else
            {
                result = OPMSG_RESULT_STATES_PARAMETER_STATE_NOT_READY;
            }
            continue;
        }
        else if (ctrl_id == AANC_CONSTANT_FB_FINE_GAIN_CTRL)
        {
            if (override_gain(p_ext_data, ctrl_value, ctrl_src,
                              &p_ext_data->fb_gain.fine))
            {
                aanc_update_gain(op_data, p_ext_data);
            }
            else
            {
                result = OPMSG_RESULT_STATES_PARAMETER_STATE_NOT_READY;
            }
            continue;
        }
        else if (ctrl_id == AANC_CONSTANT_FB_COARSE_GAIN_CTRL)
        {
            if (override_gain(p_ext_data, ctrl_value, ctrl_src,
                              &p_ext_data->fb_gain.coarse))
            {
                aanc_update_gain(op_data, p_ext_data);
            }
            else
            {
                result = OPMSG_RESULT_STATES_PARAMETER_STATE_NOT_READY;
            }
            continue;
        }
        else if (ctrl_id == AANC_CONSTANT_EC_FINE_GAIN_CTRL)
        {
            if (override_gain(p_ext_data, ctrl_value, ctrl_src,
                              &p_ext_data->ec_gain.fine))
            {
                aanc_update_gain(op_data, p_ext_data);
            }
            else
            {
                result = OPMSG_RESULT_STATES_PARAMETER_STATE_NOT_READY;
            }
            continue;
        }
        else if (ctrl_id == AANC_CONSTANT_EC_COARSE_GAIN_CTRL)
        {
            if (override_gain(p_ext_data, ctrl_value, ctrl_src,
                              &p_ext_data->ec_gain.coarse))
            {
                aanc_update_gain(op_data, p_ext_data);
            }
            else
            {
                result = OPMSG_RESULT_STATES_PARAMETER_STATE_NOT_READY;
            }
            continue;
        }

        result = OPMSG_RESULT_STATES_UNSUPPORTED_CONTROL;
    }

    /* Set current operating mode based on override */
    /* NB: double AND removes gain override bits from comparison */
    if ((p_ext_data->ovr_control & AANC_CONTROL_MODE_OVERRIDE)
        & AANC_CONTROL_MODE_OVERRIDE)
    {
        p_ext_data->cur_mode = p_ext_data->qact_mode;
    }
    else
    {
        p_ext_data->cur_mode = p_ext_data->host_mode;
    }

    cps_response_set_result(resp_data, result);

    return TRUE;
}

bool aanc_opmsg_get_params(OPERATOR_DATA *op_data, void *message_data,
                           unsigned *resp_length,
                           OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AANC_OP_DATA *p_ext_data = get_instance_data(op_data);
    return cpsGetParameterMsgHandler(&p_ext_data->params_def, message_data,
                                     resp_length, resp_data);
}

bool aanc_opmsg_get_defaults(OPERATOR_DATA *op_data, void *message_data,
                             unsigned *resp_length,
                             OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AANC_OP_DATA *p_ext_data = get_instance_data(op_data);
    return cpsGetDefaultsMsgHandler(&p_ext_data->params_def, message_data,
                                    resp_length, resp_data);
}

bool aanc_opmsg_set_params(OPERATOR_DATA *op_data, void *message_data,
                           unsigned *resp_length,
                           OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AANC_OP_DATA *p_ext_data = get_instance_data(op_data);
    bool retval;
    /* patch_fn(aanc_opmsg_set_params); */

    retval = cpsSetParameterMsgHandler(&p_ext_data->params_def, message_data,
                                       resp_length, resp_data);

    if (retval)
    {
        /* Set re-initialization flag for capability */
        p_ext_data->re_init_flag = TRUE;
    }
    else
    {
        L2_DBG_MSG("AANC Set Parameters Failed");
    }

    return retval;
}

bool aanc_opmsg_get_status(OPERATOR_DATA *op_data, void *message_data,
                           unsigned *resp_length,
                           OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AANC_OP_DATA *p_ext_data = get_instance_data(op_data);
    /* patch_fn_shared(aanc_capability);  TODO: patch functions */
    int i;

    /* Build the response */
    unsigned *resp = NULL;
    if(!common_obpm_status_helper(message_data, resp_length, resp_data,
                                  sizeof(AANC_STATISTICS), &resp))
    {
         return FALSE;
    }

    if (resp)
    {
        AANC_STATISTICS stats;
        AANC_STATISTICS *pstats = &stats;
        FXLMS100_STATISTICS *p_fxlms_stats = p_ext_data->ag->p_fxlms_stats;
        ED100_STATISTICS *p_ed_int_stats = p_ext_data->ag->p_ed_int_stats;
        ED100_STATISTICS *p_ed_ext_stats = p_ext_data->ag->p_ed_ext_stats;
        ED100_STATISTICS *p_ed_pb_stats = p_ext_data->ag->p_ed_pb_stats;
        ParamType *pparam = (ParamType*)pstats;

#ifdef USE_AANC_LICENSING
        p_ext_data->license_status = AANC_LICENSE_STATUS_LICENSING_BUILD_STATUS;
        if (p_fxlms_stats->licensed)
        {
            p_ext_data->license_status |= AANC_LICENSE_STATUS_FxLMS;
        }
        /* NB: License status won't be set if the block is disabled.
         * Given that all EDs use the same license check, OR a comparison
         * between them.
         */
        if (p_ed_ext_stats->licensed || p_ed_int_stats->licensed ||
            p_ed_pb_stats->licensed)
        {
            p_ext_data->license_status |= AANC_LICENSE_STATUS_ED;
        }
#endif /* USE_AANC_LICENSING */

        pstats->OFFSET_CUR_MODE            = p_ext_data->cur_mode;
        pstats->OFFSET_OVR_CONTROL         = p_ext_data->ovr_control;
        pstats->OFFSET_IN_OUT_EAR_CTRL     = p_ext_data->in_out_status;
        pstats->OFFSET_CHANNEL             = p_ext_data->anc_channel;
        pstats->OFFSET_FEEDFORWARD_PATH    = p_ext_data->anc_ff_path;
        pstats->OFFSET_LICENSE_STATUS      = p_ext_data->license_status;
        pstats->OFFSET_FLAGS               = p_ext_data->flags;
        pstats->OFFSET_AG_CALC             = p_fxlms_stats->adaptive_gain;
        /* Send previous gain values as stats because these are only updated
         * when the value is actually written to HW.
         */
        pstats->OFFSET_FF_FINE_GAIN_CTRL   = p_ext_data->ff_gain_prev.fine;
        pstats->OFFSET_FF_COARSE_GAIN_CTRL = p_ext_data->ff_gain_prev.coarse;
        pstats->OFFSET_FF_GAIN_DB = aanc_proc_calc_gain_db(
            p_ext_data->ff_gain_prev.fine, p_ext_data->ff_gain_prev.coarse);
        pstats->OFFSET_FB_FINE_GAIN_CTRL   = p_ext_data->fb_gain_prev.fine;
        pstats->OFFSET_FB_COARSE_GAIN_CTRL = p_ext_data->fb_gain_prev.coarse;
        pstats->OFFSET_FB_GAIN_DB = aanc_proc_calc_gain_db(
            p_ext_data->fb_gain_prev.fine, p_ext_data->fb_gain_prev.coarse);
        pstats->OFFSET_EC_FINE_GAIN_CTRL   = p_ext_data->ec_gain_prev.fine;
        pstats->OFFSET_EC_COARSE_GAIN_CTRL = p_ext_data->ec_gain_prev.coarse;
        pstats->OFFSET_EC_GAIN_DB = aanc_proc_calc_gain_db(
            p_ext_data->ec_gain_prev.fine, p_ext_data->ec_gain_prev.coarse);
        pstats->OFFSET_SPL_EXT             = p_ed_ext_stats->spl;
        pstats->OFFSET_SPL_INT             = p_ed_int_stats->spl;
        pstats->OFFSET_SPL_PB              = p_ed_pb_stats->spl;
        pstats->OFFSET_PEAK_EXT            = p_ext_data->ag->ext_peak_value;
        pstats->OFFSET_PEAK_INT            = p_ext_data->ag->int_peak_value;
        pstats->OFFSET_PEAK_PB             = p_ext_data->ag->pb_peak_value;

        /* Reset peak meters */
        p_ext_data->ag->ext_peak_value = 0;
        p_ext_data->ag->int_peak_value = 0;
        p_ext_data->ag->pb_peak_value = 0;

        for (i=0; i<AANC_N_STAT/2; i++)
        {
            resp = cpsPack2Words(pparam[2*i], pparam[2*i+1], resp);
        }
        if (AANC_N_STAT % 2) // last one
        {
            cpsPack1Word(pparam[AANC_N_STAT-1], resp);
        }
    }

    return TRUE;
}

bool ups_params_aanc(void* instance_data, PS_KEY_TYPE key,
                     PERSISTENCE_RANK rank, uint16 length,
                     unsigned* data, STATUS_KYMERA status,
                     uint16 extra_status_info)
{
    OPERATOR_DATA *op_data = (OPERATOR_DATA*) instance_data;
    AANC_OP_DATA *p_ext_data = get_instance_data(op_data);

    cpsSetParameterFromPsStore(&p_ext_data->params_def, length, data, status);

    /* Set the reinitialization flag after setting the parameters */
    p_ext_data->re_init_flag = TRUE;

    return TRUE;
}

bool aanc_opmsg_set_ucid(OPERATOR_DATA *op_data, void *message_data,
                         unsigned *resp_length,
                         OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AANC_OP_DATA *p_ext_data = get_instance_data(op_data);
    PS_KEY_TYPE key;
    bool retval;

    retval = cpsSetUcidMsgHandler(&p_ext_data->params_def, message_data,
                                  resp_length, resp_data);
    L5_DBG_MSG1("AANC cpsSetUcidMsgHandler Return Value %d", retval);
    key = MAP_CAPID_UCID_SBID_TO_PSKEYID(p_ext_data->cap_id,
                                         p_ext_data->params_def.ucid,
                                         OPMSG_P_STORE_PARAMETER_SUB_ID);

    ps_entry_read((void*)op_data, key, PERSIST_ANY, ups_params_aanc);

    L5_DBG_MSG1("AANC UCID Set to %d", p_ext_data->params_def.ucid);

    p_ext_data->re_init_flag = TRUE;

    return retval;
}

bool aanc_opmsg_get_ps_id(OPERATOR_DATA *op_data, void *message_data,
                          unsigned *resp_length,
                          OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AANC_OP_DATA *p_ext_data = get_instance_data(op_data);
    return cpsGetUcidMsgHandler(&p_ext_data->params_def, p_ext_data->cap_id,
                                message_data, resp_length, resp_data);
}

/****************************************************************************
Custom opmsg handlers
*/
bool aanc_opmsg_set_static_gain(OPERATOR_DATA *op_data, void *message_data,
                                unsigned *resp_length,
                                OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AANC_OP_DATA *p_ext_data = get_instance_data(op_data);

    p_ext_data->ff_static_gain.coarse = OPMSG_FIELD_GET(
        message_data, OPMSG_SET_AANC_STATIC_GAIN, FF_COARSE_STATIC_GAIN);
    p_ext_data->ff_static_gain.fine = OPMSG_FIELD_GET(
        message_data, OPMSG_SET_AANC_STATIC_GAIN, FF_FINE_STATIC_GAIN);
    L4_DBG_MSG2("AANC Set FF Static Gain: Coarse = %d, Fine = %d",
        p_ext_data->ff_static_gain.coarse, p_ext_data->ff_static_gain.fine);

    p_ext_data->fb_static_gain.coarse = OPMSG_FIELD_GET(
        message_data, OPMSG_SET_AANC_STATIC_GAIN, FB_COARSE_STATIC_GAIN);
    p_ext_data->fb_static_gain.fine = OPMSG_FIELD_GET(
        message_data, OPMSG_SET_AANC_STATIC_GAIN, FB_FINE_STATIC_GAIN);
    L4_DBG_MSG2("AANC Set FB Static Gain: Coarse = %d, Fine = %d",
        p_ext_data->fb_static_gain.coarse, p_ext_data->fb_static_gain.fine);

    p_ext_data->ec_static_gain.coarse = OPMSG_FIELD_GET(
        message_data, OPMSG_SET_AANC_STATIC_GAIN, EC_COARSE_STATIC_GAIN);
    p_ext_data->ec_static_gain.fine = OPMSG_FIELD_GET(
        message_data, OPMSG_SET_AANC_STATIC_GAIN, EC_FINE_STATIC_GAIN);
    L4_DBG_MSG2("AANC Set EC Static Gain: Coarse = %d, Fine = %d",
        p_ext_data->ec_static_gain.coarse, p_ext_data->ec_static_gain.fine);
    p_ext_data->flags |= AANC_FLAGS_STATIC_GAIN_LOADED;

    return TRUE;
}

bool aanc_opmsg_set_plant_model(OPERATOR_DATA *op_data, void *message_data,
                                unsigned *resp_length,
                                OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AANC_OP_DATA *p_ext_data = get_instance_data(op_data);

    if (!aanc_fxlms100_set_plant_model(p_ext_data->ag->p_fxlms, message_data))
    {
        L4_DBG_MSG("AANC set plant coefficients failed");
        return FALSE;
    }

    p_ext_data->flags |= AANC_FLAGS_PLANT_MODEL_LOADED;

    return TRUE;
}

bool aanc_opmsg_set_control_model(OPERATOR_DATA *op_data,
                                  void *message_data,
                                  unsigned *resp_length,
                                  OP_OPMSG_RSP_PAYLOAD **resp_data)
{
    AANC_OP_DATA *p_ext_data = get_instance_data(op_data);

    if (!aanc_fxlms100_set_control_model(p_ext_data->ag->p_fxlms, message_data))
    {
        L4_DBG_MSG("AANC set control coefficients failed");
        return FALSE;
    }

    p_ext_data->flags |= AANC_FLAGS_CONTROL_MODEL_LOADED;

    p_ext_data->re_init_flag = TRUE;

    return TRUE;
}

/****************************************************************************
Data processing function
*/
void aanc_process_data(OPERATOR_DATA *op_data, TOUCHED_TERMINALS *touched)
{
    AANC_OP_DATA *p_ext_data = get_instance_data(op_data);

    int i = 0;

    /* Certain conditions require an "early exit" that will just discard any
     * data in the input buffers and not do any other processing
     */
    bool exit_early = FALSE;

    /* track the number of samples process */
    int sample_count = 0;

    /* After data is processed flags are tested to determine the equivalent
     * operating state. This is an input to the gain update decision state
     * machine.
     */
    unsigned mode_after_flags = p_ext_data->cur_mode;

    /* Reference the calculated gain */
    unsigned *p_gain_calc = &p_ext_data->ag->p_fxlms_stats->adaptive_gain;

    /* Reference the capability parameters */
    AANC_PARAMETERS *p_params = &p_ext_data->aanc_cap_params;

    bool calculate_gain = TRUE;

#ifdef RUNNING_ON_KALSIM
    unsigned pre_process_flags = p_ext_data->flags;
#endif /* RUNNING_ON_KALSIM */

    int samples_to_process = INT_MAX;

    /*********************
     * Early exit testing
     *********************/

    /* Without adequate data or space we can just return */

    /* Determine whether to copy any input data to output terminals */
    samples_to_process = aanc_calc_samples_to_process(p_ext_data);

    /* Return early if int and ext mic input terminals are not connected */
    if (samples_to_process == INT_MAX)
    {
        L5_DBG_MSG("Minimum number of ports (int and ext mic) not connected");
        return;
    }

     /* Return early if no data or not enough space to process */
    if (samples_to_process < AANC_DEFAULT_FRAME_SIZE)
    {
        L5_DBG_MSG1("Not enough data/space to process (%d)", samples_to_process);
        return;
    }

    /* Other conditions that are invalid for running AANC need to discard
     * input data if it exists.
     */

    /* Don't do any processing in standby */
    if (p_ext_data->cur_mode == AANC_SYSMODE_STANDBY)
    {
        exit_early = TRUE;
    }

    /* Don't do any processing if out of ear */
    bool disable_ear_check = (p_params->OFFSET_AANC_DEBUG &
                              AANC_CONFIG_AANC_DEBUG_DISABLE_EAR_STATUS_CHECK);
    if ((p_ext_data->in_out_status != AANC_IN_EAR) && !disable_ear_check)
    {
        exit_early = TRUE;
    }

    /* Don't do any processing if ANC HW clocks are invalid */
#ifndef RUNNING_ON_KALSIM
    uint16 anc0_enable;
    uint16 anc1_enable;
    uint16 *anc_selected = &anc0_enable;

    stream_get_anc_enable(&anc0_enable, &anc1_enable);

    if (p_ext_data->anc_channel == AANC_ANC_INSTANCE_ANC1_ID)
    {
        anc_selected = &anc1_enable;
    }

    bool anc_is_running = *anc_selected == p_ext_data->anc_clock_check_value;
    bool disable_clock_check = (p_params->OFFSET_AANC_DEBUG &
                                AANC_CONFIG_AANC_DEBUG_DISABLE_ANC_CLOCK_CHECK);
    /* Don't do any processing if HW clocks aren't running */
    if (!anc_is_running && !disable_clock_check)
    {
        L2_DBG_MSG1("AANC invalid clocks detected: %d", *anc_selected);
        exit_early = TRUE;
    }
#endif

    if (exit_early)
    {
        bool discard_data = TRUE;

        /* There is at least 1 frame to process */
        do {
            /* Iterate through all sinks */
            for (i = 0; i < AANC_MAX_SINKS; i++)
            {
                if (p_ext_data->inputs[i] != NULL)
                {
                    /* Discard a frame of data */
                    cbuffer_discard_data(p_ext_data->inputs[i],
                                         AANC_DEFAULT_FRAME_SIZE);

                    /* If there isn't a frame worth of data left then don't
                     * iterate through the input terminals again.
                     */
                    samples_to_process = cbuffer_calc_amount_data_in_words(
                        p_ext_data->inputs[i]);

                    if (samples_to_process < AANC_DEFAULT_FRAME_SIZE)
                    {
                        discard_data = FALSE;
                    }
                }
            }
        } while (discard_data);
    }

    /***************************
     * Adaptive gain processing
     ***************************/

    if (p_ext_data->re_init_flag == TRUE)
    {
        ADAPTIVE_GAIN *p_ag = p_ext_data->ag;
        p_ext_data->re_init_flag = FALSE;

        /* Copy terminal buffer pointers */
        p_ag->p_playback_ip = p_ext_data->inputs[AANC_PLAYBACK_TERMINAL_ID];
        p_ag->p_fbmon_ip = p_ext_data->inputs[AANC_FB_MON_TERMINAL_ID];
        p_ag->p_mic_int_ip = p_ext_data->inputs[AANC_MIC_INT_TERMINAL_ID];
        p_ag->p_mic_ext_ip = p_ext_data->inputs[AANC_MIC_EXT_TERMINAL_ID];

        p_ag->p_playback_op = p_ext_data->outputs[AANC_PLAYBACK_TERMINAL_ID];
        p_ag->p_fbmon_op = p_ext_data->outputs[AANC_FB_MON_TERMINAL_ID];
        p_ag->p_mic_int_op = p_ext_data->outputs[AANC_MIC_INT_TERMINAL_ID];
        p_ag->p_mic_ext_op = p_ext_data->outputs[AANC_MIC_EXT_TERMINAL_ID];

        /* Initialize gentle mute duration frame count */
        p_ext_data->gentle_mute_duration = (uint16) (
            (p_params->OFFSET_GENTLE_MUTE_TIMER * AANC_FRAME_RATE) >> 20);

        /* Check if gentle mute duration is 0,
        if it is 0, set to 1 to allow 1 frame for gain to ramp down to 0 */

        if (p_ext_data->gentle_mute_duration == 0)
        {
            p_ext_data->gentle_mute_duration = 1;
        }

        p_ext_data->gentle_mute_init_flag = FALSE;
        p_ext_data->gain_dec_rate = 0;
        p_ext_data->gentle_mute_gain = 0;

        aanc_initialize_events(op_data, p_ext_data);

        aanc_proc_initialize(p_params, p_ag, p_params->OFFSET_FXLMS_INITIAL_VALUE,
                             &p_ext_data->flags, p_ext_data->re_init_hard);
    }

    /* Identify whether to do the gain calculation step */
    if ((p_params->OFFSET_DISABLE_AG_CALC & 0x1) ||
        (p_ext_data->cur_mode != AANC_SYSMODE_FULL &&
         p_ext_data->cur_mode != AANC_SYSMODE_QUIET))
    {
        calculate_gain = FALSE;
    }

    /* Consume all the data in the input buffer, or until there isn't space
     * available.
     */
    while (samples_to_process >= AANC_DEFAULT_FRAME_SIZE)
    {
        aanc_proc_process_data(p_ext_data->ag, calculate_gain);

        samples_to_process = aanc_calc_samples_to_process(p_ext_data);

        sample_count += AANC_DEFAULT_FRAME_SIZE;
    }
    /*********************************************
     * Send unsolicited message (simulation only)
     ********************************************/
#ifdef RUNNING_ON_KALSIM
    if (pre_process_flags != p_ext_data->flags)
    {
        aanc_update_gain(op_data, p_ext_data);
    }
#endif /* RUNNING_ON_KALSIM */

    /*************************
     * Check processing flags
     *************************/
    if (p_ext_data->flags & AANC_ED_FLAG_MASK)
    {
        L5_DBG_MSG1("AANC ED detected: %d",
                    p_ext_data->flags & AANC_ED_FLAG_MASK);
        mode_after_flags = AANC_SYSMODE_FREEZE;
    }

    if (p_ext_data->flags & AANC_CLIPPING_FLAG_MASK)
    {
        L5_DBG_MSG1("AANC Clipping detected: %d",
                    p_ext_data->flags & AANC_CLIPPING_FLAG_MASK);
        mode_after_flags = AANC_SYSMODE_FREEZE;
    }

    if (p_ext_data->flags & AANC_SATURATION_FLAG_MASK)
    {
        L5_DBG_MSG1("AANC Saturation detected: %d",
                    p_ext_data->flags & AANC_SATURATION_FLAG_MASK);
        mode_after_flags = AANC_SYSMODE_FREEZE;
    }

    /**************
     * Update gain
     **************/
    /* Check SYSMODE state as this is the primary control */
    switch (p_ext_data->cur_mode)
    {
        case AANC_SYSMODE_STANDBY:
            break; /* Shouldn't ever get here */
        case AANC_SYSMODE_MUTE_ANC:
            /* Mute action is taken in SET_CONTROL */
            break;
        case AANC_SYSMODE_FULL:
            /* Don't update the gain if any flags were set */
            if (mode_after_flags == AANC_SYSMODE_FREEZE)
            {
                L4_DBG_MSG1("AANC FULL Mode, FREEZE: gain frozen at %d",
                            p_ext_data->ff_gain.fine);
                break;
            }
            else if (mode_after_flags == AANC_SYSMODE_MUTE_ANC)
            {
                L4_DBG_MSG("AANC FULL Mode, MUTE: updating gain to 0");
                p_ext_data->ff_gain.fine = 0;
                break;
            }
            L4_DBG_MSG1("AANC FULL mode, FULL: updating gain to %d",
                        *p_gain_calc);
            p_ext_data->ff_gain.fine = (uint16) *p_gain_calc;
            break;
        case AANC_SYSMODE_STATIC:
            /* Static action is taken in SET_CONTROL */
            break;
        case AANC_SYSMODE_FREEZE:
            /* Freeze does nothing to change the gains */
            break;
        case AANC_SYSMODE_GENTLE_MUTE:
            /* Gentle mute ramps gain down to 0 */
            if (p_ext_data->ff_gain.fine > 0)
            {
                if (!(p_ext_data->gentle_mute_init_flag))
                {
                    /* Initialize gain value,
                    calculate gain decrease rate during gentle mute period */
                    p_ext_data->gentle_mute_init_flag = TRUE;
                    p_ext_data->gentle_mute_gain = p_ext_data->ff_gain.fine << 16;
                    p_ext_data->gain_dec_rate = p_ext_data->gentle_mute_gain/p_ext_data->gentle_mute_duration;
                }
                /* decrease gain at the determined rate */
                if (p_ext_data->gentle_mute_gain >= p_ext_data->gain_dec_rate)
                {
                    p_ext_data->gentle_mute_gain = p_ext_data->gentle_mute_gain - p_ext_data->gain_dec_rate;
                }
                else
                {
                    p_ext_data->gentle_mute_gain = 0;
                }
                p_ext_data->ff_gain.fine = (uint16) ((p_ext_data->gentle_mute_gain + (1 << 15)) >> 16);
                aanc_fxlms100_update_gain(p_ext_data->ag->p_fxlms,
                                          p_ext_data->ag->p_fxlms_stats,
                                          p_ext_data->ff_gain.fine);
                L4_DBG_MSG1("AANC GENTLE_MUTE mode: updating gain to %d",
                            p_ext_data->ff_gain.fine);
            }
            break;
        case AANC_SYSMODE_QUIET:
            /* FF fine gain determined by FxLMS gain adaptation */
            p_ext_data->ff_gain.fine = (uint16) *p_gain_calc;
            L4_DBG_MSG1("AANC QUIET Mode, gain = %d", p_ext_data->ff_gain.fine);
            break;
        default:
            L2_DBG_MSG1("AANC SYSMODE invalid: %d", p_ext_data->cur_mode);
            break;
    }

    /* "Hard initialization" is associated with first-time process, so
    * set the FB fine gain to its static value.
    *
    * Clear "hard" reinitializion so that FB gain is not touched in
    * subsequent iterations.
    */
    if (p_ext_data->re_init_hard)
    {
        p_ext_data->fb_gain.fine = p_ext_data->fb_static_gain.fine;
        p_ext_data->re_init_hard = FALSE;
    }

    /* Evaluate event messaging criteria */
    if (!(p_params->OFFSET_AANC_DEBUG &
          AANC_CONFIG_AANC_DEBUG_DISABLE_EVENT_MESSAGING))
    {
        aanc_process_events(op_data, p_ext_data);
        p_ext_data->prev_flags = p_ext_data->flags;
    }

    aanc_update_gain(op_data, p_ext_data);

    /****************
     * Pass Metadata
     ****************/
    for (i = 0; i < AANC_NUM_METADATA_CHANNELS; i++)
    {
        metadata_strict_transport(p_ext_data->metadata_ip[i],
                                  p_ext_data->metadata_op[i],
                                  sample_count * OCTETS_PER_SAMPLE);
    }

    /***************************
     * Update touched terminals
     ***************************/
    touched->sinks = (unsigned) p_ext_data->touched_sinks;
    touched->sources = (unsigned) p_ext_data->touched_sources;

    L5_DBG_MSG("AANC process channel data completed");

    return;
}
