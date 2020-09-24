/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Audio source specific parameters used in the kymera adaptation layer
*/

#ifndef KYMERA_AUDIO_PROTECTED_H_
#define KYMERA_AUDIO_PROTECTED_H_

#include "volume_types.h"
#include <stream.h>
#include <a2dp.h>

typedef enum
{
    channel_mode_mono,
    channel_mode_dual_channel,
    channel_mode_stereo,
    channel_mode_joint_stereo
} channel_mode_t;

typedef struct
{
    uint16 * client_lock;
    uint16 client_lock_mask;
    volume_t volume;
    uint8 master_pre_start_delay;
    uint32 rate;
    channel_mode_t channel_mode;
    uint8 seid;
    Sink sink;
    bool content_protection;
    uint8 bitpool;
    uint8 format;
    uint16 packet_size;
    /*! In bps. Ignored if zero. */
    uint32 max_bitrate;
    bool q2q_mode;
    aptx_adaptive_ttp_latencies_t nq2q_ttp;
} a2dp_connect_parameters_t;

typedef struct
{
    Source source;
    uint8 seid;
} a2dp_disconnect_parameters_t;

/* connection parameter for wired analog audio */
typedef struct
{
    volume_t volume;
    uint32 rate;
    uint32 min_latency; /* in micro-seconds */
    uint32 max_latency; /* in micro-seconds */
    uint32 target_latency; /* in micro-seconds */
}wired_analog_connect_parameters_t;

typedef struct
{
    uint8 channels;
    uint8 frame_size;
    Source spkr_src;
    volume_t volume;
    uint32 sample_freq;
    uint32 min_latency_ms;
    uint32 max_latency_ms;
    uint32 target_latency_ms;
} usb_audio_connect_parameters_t;

typedef struct
{
    Source source;
} usb_audio_disconnect_parameters_t;

#endif /* KYMERA_AUDIO_PROTECTED_H_ */


