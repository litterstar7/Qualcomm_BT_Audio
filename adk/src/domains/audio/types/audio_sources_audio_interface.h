/*!
\copyright  Copyright (c) 2019-2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Interface to audio sources.
*/

#ifndef AUDIO_SOURCES_AUDIO_INTERFACE_H_
#define AUDIO_SOURCES_AUDIO_INTERFACE_H_

#include "audio_sources_list.h"
#include "source_param_types.h"

#define MAX_AUDIO_INTERFACES (1)

typedef enum
{
    audio_source_disconnected,
    audio_source_connecting,
    audio_source_connected,
    audio_source_disconnecting
} audio_source_state_t;

typedef enum
{
    audio_source_status_ready,
    audio_source_status_preparing
}audio_source_status_t;

typedef struct
{
    bool (*GetConnectParameters)(audio_source_t source, source_defined_params_t * source_params);
    void (*ReleaseConnectParameters)(audio_source_t source, source_defined_params_t * source_params);
    bool (*GetDisconnectParameters)(audio_source_t source, source_defined_params_t * source_params);
    void (*ReleaseDisconnectParameters)(audio_source_t source, source_defined_params_t * source_params);
    bool (*IsAudioAvailable)(audio_source_t source);
    void (*SetState)(audio_source_t source, audio_source_state_t state);
} audio_source_audio_interface_t;

#endif /* AUDIO_SOURCES_AUDIO_INTERFACE_H_ */
