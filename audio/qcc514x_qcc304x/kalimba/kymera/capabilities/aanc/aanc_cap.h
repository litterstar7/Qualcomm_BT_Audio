/****************************************************************************
 * Copyright (c) 2014 - 2019 Qualcomm Technologies International, Ltd.
****************************************************************************/
/**
 * \defgroup AANC
 * \ingroup capabilities
 * \file  aanc_cap.h
 * \ingroup AANC
 *
 * Adaptive ANC (AANC) operator private header file.
 *
 */

#ifndef _AANC_CAP_H_
#define _AANC_CAP_H_

/******************************************************************************
Include Files
*/

#include "capabilities.h"

#include "aanc.h"
#include "aanc_gen_c.h"
#include "aanc_defs.h"
#include "aanc_proc.h"

/* ANC HAL features */
#include "stream/stream_anc.h"

/******************************************************************************
ANC HW type definitions
*/

/* Represent ANC channels */
typedef enum
{
    AANC_ANC_INSTANCE_NONE_ID = 0x0000,
    AANC_ANC_INSTANCE_ANC0_ID = 0x0001,
    AANC_ANC_INSTANCE_ANC1_ID = 0x0002
} AANC_ANC_INSTANCE;

/* Represent ANC paths */
typedef enum
{
    AANC_ANC_PATH_NONE_ID = 0x0000,
    AANC_ANC_PATH_FFA_ID = 0x0001,
    AANC_ANC_PATH_FFB_ID = 0x0002,
    AANC_ANC_PATH_FB_ID = 0x0003,
    AANC_ANC_PATH_SM_LPF_ID = 0x0004
} AANC_ANC_PATH;

/* Represent ANC clock enables */
typedef enum
{
    AANC_ANC_ENABLE_FFA_MASK = 0x0001,
    AANC_ANC_ENABLE_FFB_MASK = 0x0002,
    AANC_ANC_ENABLE_FB_MASK = 0x0004,
    AANC_ANC_ENABLE_OUT_MASK = 0x0008
} AANC_ANC_ENABLE;

/******************************************************************************
Private Constant Definitions
*/
/* Number of statistics reported by the capability */
#define AANC_N_STAT                     (sizeof(AANC_STATISTICS)/sizeof(ParamType))

/* Mask for the number of system modes */
#define AANC_SYSMODE_MASK               0x7

/* Mask for override control word */
#define AANC_OVERRIDE_MODE_MASK        (0xFFFF ^ AANC_CONTROL_MODE_OVERRIDE)
#define AANC_OVERRIDE_IN_OUT_EAR_MASK  (0xFFFF ^ AANC_CONTROL_IN_OUT_EAR)
#define AANC_OVERRIDE_GAIN_MASK        (0xFFFF ^ AANC_CONTROL_GAIN_OVERRIDE)
#define AANC_OVERRIDE_CHANNEL_MASK     (0xFFFF ^ AANC_CONTROL_CHANNEL)

/* Label terminals */
#define AANC_PLAYBACK_TERMINAL_ID       0
#define AANC_FB_MON_TERMINAL_ID         1
#define AANC_MIC_INT_TERMINAL_ID        2
#define AANC_MIC_EXT_TERMINAL_ID        3

#define AANC_MAX_SOURCES                4
#define AANC_MAX_SINKS                  4
#define AANC_MIN_VALID_SINKS            ((1 << AANC_MIC_INT_TERMINAL_ID) | \
                                         (1 << AANC_MIC_EXT_TERMINAL_ID))

/* Label metadata channels */
#define AANC_NUM_METADATA_CHANNELS      2
#define AANC_METADATA_PLAYBACK_ID       0
#define AANC_METADATA_MIC_ID            1

/* Label in/out of ear states */
#define AANC_IN_EAR                     TRUE
#define AANC_OUT_EAR                    FALSE

/* Capability minor version */
#define AANC_CAP_VERSION_MINOR 0

/* Feedforward and hybrid clock enable bitfields */
#define AANC_HYBRID_ENABLE              (AANC_ANC_ENABLE_FFA_MASK | \
                                         AANC_ANC_ENABLE_FFB_MASK | \
                                         AANC_ANC_ENABLE_FB_MASK |  \
                                         AANC_ANC_ENABLE_OUT_MASK)
#define AANC_FEEDFORWARD_ENABLE         (AANC_ANC_ENABLE_FFA_MASK | \
                                         AANC_ANC_ENABLE_OUT_MASK)

#define AANC_NORMAL_TARGET_NR          0
#define AANC_QUIET_MODE_TARGET_NR      0x7FFFFFFF

/* Event IDs */
#define AANC_EVENT_ID_GAIN      0
#define AANC_EVENT_ID_ED        1
#define AANC_EVENT_ID_QUIET     2
#define AANC_EVENT_ID_CLIP      3
#define AANC_EVENT_ID_SAT       4
#define AANC_EVENT_ID_SELF_TALK 5
#define AANC_EVENT_ID_SPL       6

/* Timer parameter is Q12.N */
#define TIMER_PARAM_SHIFT          20

/******************************************************************************
Public Type Declarations
*/

/* Represent ANC gain based on coarse and fine values */
typedef struct _AANC_GAIN
{
    uint16 coarse;
    uint16 fine;
} AANC_GAIN;

/* Represent the state of an AANC event */
typedef enum
{
    AANC_EVENT_CLEAR,
    AANC_EVENT_DETECTED,
    AANC_EVENT_SENT
} AANC_EVENT_STATE;

/* Represent ANC event messaging states */
typedef struct _AANC_EVENT
{
    unsigned frame_counter;
    unsigned set_frames;
    AANC_EVENT_STATE running;
} AANC_EVENT;

/* AANC operator data */
typedef struct aanc_exop
{
    /* Input buffers: playback, monitor, internal mic, external mic */
    tCbuffer *inputs[AANC_MAX_SINKS];

    /* Output buffers: playback, monitor, internal mic, external mic */
    tCbuffer *outputs[AANC_MAX_SOURCES];

    /* Metadata buffers:  */
    tCbuffer *metadata_ip[AANC_NUM_METADATA_CHANNELS];
    tCbuffer *metadata_op[AANC_NUM_METADATA_CHANNELS];

    /* Connection changed flag */
    bool connect_changed;

    /* Sample rate & cap id */
    unsigned sample_rate;
    unsigned cap_id;

    /* AANC parameters */
    AANC_PARAMETERS aanc_cap_params;

    /* Mode control */
    unsigned cur_mode;
    unsigned host_mode;
    unsigned qact_mode;
    unsigned ovr_control;

    /* Touched terminals */
    uint16 touched_sinks;
    uint16 touched_sources;

    /* Status */
    unsigned flags;
    unsigned prev_flags;
    unsigned anc_hw_status;

    /* Gain value used for updates during gentle mute */
    unsigned gentle_mute_gain;
    /* Rate of gain decrease during gentle mute */
    unsigned gain_dec_rate;
    /* Gentle mute duration in frames */
    uint16 gentle_mute_duration;

    /* Adaptive gain handle */
    ADAPTIVE_GAIN *ag;

    /* Current gains for ANC blocks */
    AANC_GAIN ff_gain;
    AANC_GAIN fb_gain;
    AANC_GAIN ec_gain;

    /* Previous gains for ANC blocks */
    AANC_GAIN ff_gain_prev;
    AANC_GAIN fb_gain_prev;
    AANC_GAIN ec_gain_prev;

    /* Static gains for ANC blocks */
    AANC_GAIN ff_static_gain;
    AANC_GAIN fb_static_gain;
    AANC_GAIN ec_static_gain;

    /* ANC channel controlled by the capability */
    AANC_ANC_INSTANCE anc_channel;
    AANC_ANC_PATH anc_ff_path;
    AANC_ANC_PATH anc_fb_path;
    uint16 anc_clock_check_value;

    /* Reinitialization */
    bool re_init_flag;
    bool re_init_hard;

    /* Gentle mute init flag */
    bool gentle_mute_init_flag;

    /* In/Our of ear status */
    bool in_out_status;

    /* Licensing statistic results */
    unsigned license_status;

    /* Standard CPS object */
    CPS_PARAM_DEF params_def;

    AANC_EVENT gain_event;
    AANC_EVENT ed_event;
    AANC_EVENT quiet_event_detect;
    AANC_EVENT quiet_event_clear;
    AANC_EVENT clip_event;
    AANC_EVENT sat_event;
    AANC_EVENT self_talk_event;
    AANC_EVENT spl_event;

} AANC_OP_DATA;

/******************************************************************************
Private Function Definitions
*/

/* Standard Capability API handlers */
extern bool aanc_create(OPERATOR_DATA *op_data, void *message_data,
                        unsigned *response_id, void **resp_data);
extern bool aanc_destroy(OPERATOR_DATA *op_data, void *message_data,
                         unsigned *response_id, void **resp_data);
extern bool aanc_start(OPERATOR_DATA *op_data, void *message_data,
                       unsigned *response_id, void **resp_data);
extern bool aanc_reset(OPERATOR_DATA *op_data, void *message_data,
                       unsigned *response_id, void **resp_data);
extern bool aanc_connect(OPERATOR_DATA *op_data, void *message_data,
                         unsigned *response_id, void **resp_data);
extern bool aanc_disconnect(OPERATOR_DATA *op_data, void *message_data,
                            unsigned *response_id, void **resp_data);
extern bool aanc_buffer_details(OPERATOR_DATA *op_data, void *message_data,
                                unsigned *response_id, void **resp_data);
extern bool aanc_get_sched_info(OPERATOR_DATA *op_data, void *message_data,
                                unsigned *response_id, void **resp_data);

/* Standard Opmsg handlers */
extern bool aanc_opmsg_set_control(OPERATOR_DATA *op_data,
                                   void *message_data,
                                   unsigned *resp_length,
                                   OP_OPMSG_RSP_PAYLOAD **resp_data);
extern bool aanc_opmsg_get_params(OPERATOR_DATA *op_data,
                                  void *message_data,
                                  unsigned *resp_length,
                                  OP_OPMSG_RSP_PAYLOAD **resp_data);
extern bool aanc_opmsg_get_defaults(OPERATOR_DATA *op_data,
                                    void *message_data,
                                    unsigned *resp_length,
                                    OP_OPMSG_RSP_PAYLOAD **resp_data);
extern bool aanc_opmsg_set_params(OPERATOR_DATA *op_data,
                                  void *message_data,
                                  unsigned *resp_length,
                                  OP_OPMSG_RSP_PAYLOAD **resp_data);
extern bool aanc_opmsg_get_status(OPERATOR_DATA *op_data,
                                  void *message_data,
                                  unsigned *resp_length,
                                  OP_OPMSG_RSP_PAYLOAD **resp_data);
extern bool aanc_opmsg_set_ucid(OPERATOR_DATA *op_data,
                                void *message_data,
                                unsigned *resp_length,
                                OP_OPMSG_RSP_PAYLOAD **resp_data);
extern bool aanc_opmsg_get_ps_id(OPERATOR_DATA *op_data,
                                 void *message_data,
                                 unsigned *resp_length,
                                 OP_OPMSG_RSP_PAYLOAD **resp_data);

/* Custom OPMSG handlers. Arguments follow standard OPMSG handlers. */

/**
 * \brief  Set the AANC Static Gain
 *
 * \param  op_data  Address of operator data
 * \param  message_data  Address of message data
 * \param  resp_length  Address of the response length
 * \param  resp_data  Pointer to the address of the response data
 *
 * \return  boolean indicating success or failure.
 *
 * Custom OPMSG handler for setting the AANC static gain values.
 */
extern bool aanc_opmsg_set_static_gain(OPERATOR_DATA *op_data,
                                       void *message_data,
                                       unsigned *resp_length,
                                       OP_OPMSG_RSP_PAYLOAD **resp_data);

/**
 * \brief  Set the AANC Plant Model
 *
 * \param  op_data  Address of operator data
 * \param  message_data  Address of message data
 * \param  resp_length  Address of the response length
 * \param  resp_data  Pointer to the address of the response data
 *
 * \return  boolean indicating success or failure.
 *
 * Custom OPMSG handler for setting the AANC plant model coefficients. The
 * capability assumes that this message preceeds the set_control_model message.
 */
extern bool aanc_opmsg_set_plant_model(OPERATOR_DATA *op_data,
                                       void *message_data,
                                       unsigned *resp_length,
                                       OP_OPMSG_RSP_PAYLOAD **resp_data);

/**
 * \brief  Set the AANC Control Model
 *
 * \param  op_data  Address of operator data
 * \param  message_data  Address of message data
 * \param  resp_length  Address of the response length
 * \param  resp_data  Pointer to the address of the response data
 *
 * \return  boolean indicating success or failure.
 *
 * Custom OPMSG handler for setting the AANC control model coefficients. The
 * capability assumes that this message follows the set_plant_model message,
 * and will recalculate the model following this message.
 */
extern bool aanc_opmsg_set_control_model(OPERATOR_DATA *op_data,
                                         void *message_data,
                                         unsigned *resp_length,
                                         OP_OPMSG_RSP_PAYLOAD **resp_data);

/* Standard data processing function */
extern void aanc_process_data(OPERATOR_DATA *op_data,
                              TOUCHED_TERMINALS *touched);

/* Standard parameter function to handle persistence */
extern bool ups_params_aanc(void* instance_data, PS_KEY_TYPE key,
                            PERSISTENCE_RANK rank, uint16 length,
                            unsigned* data, STATUS_KYMERA status,
                            uint16 extra_status_info);

#endif /* _AANC_CAP_H_ */
