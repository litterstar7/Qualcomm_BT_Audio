/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   audio_router Audio Router
\ingroup    audio_domain
\brief      The audio router provides a standard API to enable or disable audio paths

Specific implementations of the audio router behaviour can be configured by the registration
of handlers with the AudioRouter_ConfigureCallbacks() function.

This implementation can then call into the audio router to connect and disconnect
sources as required.
*/

#ifndef AUDIO_ROUTER_H_
#define AUDIO_ROUTER_H_

#include "source_param_types.h"
#include "audio_sources_list.h"
#include "voice_sources_list.h"

typedef struct
{
    audio_source_type_t type;
    union
    {
        audio_source_t audio;
        voice_source_t voice;
    } u;
} audio_router_source_t;

/* structure containing the implementation specific handlers for the audio_router APIs */
typedef struct
{
    void (*add_source)(audio_router_source_t source);
    void (*remove_source)(audio_router_source_t source);
    void (*configure_priorities)(audio_source_type_t* priorities, unsigned num_priorities);
    void (*refresh_routing)(void);
} audio_router_t;

/*! \brief Configures the handlers for AddSource and RemoveSource functions.

    \param callbacks Pointers to the handler functions.
 */
void AudioRouter_ConfigureHandlers(const audio_router_t* handlers);
 
/*! \brief Configures the relative priorities of the source types.

    \param priorities Pointer to array of audio_source_type_t in order of priority.

    \param num_priorities size of array pointed to by priorities.

    This function sets which audio_source_type_t should be routed in the event that
    more than one has focus. The array passed to priorities should be in order of
    desired priority. i.e. priorities[0] would be routed in preference to priorities[1].
 */
void AudioRouter_ConfigurePriorities(audio_source_type_t* priorities, unsigned num_priorities);
 
/*! \brief Calls handler for adding source configured with AudioRouter_ConfigureHandlers

    \param source The source to be added
 */
void AudioRouter_AddSource(audio_router_source_t source);
 
/*! \brief Calls handler for removing source configured with AudioRouter_ConfigureHandlers

    \param source The source to be removed
 */
void AudioRouter_RemoveSource(audio_router_source_t source);

/*! \brief Connects the passed source

    \param source The source to be connected

    \return TRUE if successfully connected.
 */
bool AudioRouter_CommonConnectSource(audio_router_source_t source);

/*! \brief Disconnects the passed source

    \param source The source to be disconnected

    \return TRUE if successfully disconnected.
 */
bool AudioRouter_CommonDisconnectSource(audio_router_source_t source);

/*! \brief Kick the audio router to update the routing if the focused device
           or priorities have changed
 */
void AudioRouter_RefreshRouting(void);

#endif /* #define AUDIO_ROUTER_H_ */
