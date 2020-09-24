/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       anc_config.c
\brief      Configuration for microphones and stubs for Active Noise Cancellation (ANC).
            Static conifguration for ANC FF, FB, HY modes.
*/


#ifdef ENABLE_ANC
#include <logging.h>
#include "anc_config.h"
#include "kymera_config.h"
#include "anc_state_manager.h"

/*
 * There is no config manager setup yet, so hard-code the default value as Feed Forward Mode on Analog Mic from
 * Kymera_config for reference.
 */
#ifdef INCLUDE_STEREO
#define getFeedForwardLeftMicConfig()   (appConfigAncFeedForwardLeftMic())
#define getFeedForwardRightMicConfig()  (appConfigAncFeedForwardRightMic())
#define getFeedBackLeftMicConfig()      (appConfigAncFeedBackLeftMic())
#define getFeedBackRightMicConfig()     (appConfigAncFeedBackRightMic())
#else
#define getFeedForwardLeftMicConfig()   (appConfigAncFeedForwardMic())
#define getFeedForwardRightMicConfig()  (microphone_none)
#define getFeedBackLeftMicConfig()      (appConfigAncFeedBackMic())
#define getFeedBackRightMicConfig()     (microphone_none)
#endif

anc_readonly_config_def_t anc_readonly_config =
{
    .anc_mic_params_r_config = {
        .feed_forward_left_mic = getFeedForwardLeftMicConfig(),
        .feed_forward_right_mic = getFeedForwardRightMicConfig(),
        .feed_back_left_mic = getFeedBackLeftMicConfig(),
        .feed_back_right_mic = getFeedBackRightMicConfig(),
     },
     .num_anc_modes = appConfigNumOfAncModes(),
};

/* Write to persistance is not enabled for now and set to defaults */
static anc_writeable_config_def_t anc_writeable_config = {

    .persist_initial_mode = appConfigAncMode(),
    .persist_initial_state = anc_state_manager_uninitialised,
    .initial_anc_state = anc_state_manager_uninitialised,
    .initial_anc_mode = appConfigAncMode(),
};


uint16 ancConfigManagerGetReadOnlyConfig(uint16 config_id, const void **data)
{
    UNUSED(config_id);
    *data = &anc_readonly_config;
    DEBUG_LOG("ancConfigManagerGetReadOnlyConfig\n");
    return (uint16) sizeof(anc_readonly_config);
}

void ancConfigManagerReleaseConfig(uint16 config_id)
{
    UNUSED(config_id);
    DEBUG_LOG("ancConfigManagerReleaseConfig\n");
}

uint16 ancConfigManagerGetWriteableConfig(uint16 config_id, void **data, uint16 size)
{
    UNUSED(config_id);
    UNUSED(size);
    *data = &anc_writeable_config;
    DEBUG_LOG("ancConfigManagerGetWriteableConfig\n");
    return (uint16) sizeof(anc_writeable_config);
}

void ancConfigManagerUpdateWriteableConfig(uint16 config_id)
{
    UNUSED(config_id);
    DEBUG_LOG("ancConfigManagerUpdateWriteableConfig\n");
}
#endif /* ENABLE_ANC */
