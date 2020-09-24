/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   audio_router single_entity
\ingroup    audio_domain
\brief      Implementation of audio router functions for single signal path.
*/

#include "single_entity.h"
#include "audio_router.h"
#include "focus_audio_source.h"
#include "focus_voice_source.h"
#include "logging.h"
#include "single_entity_data.h"
#include "stdlib.h"

static void singleEntity_AddSource(audio_router_source_t source);
static void singleEntity_RemoveSource(audio_router_source_t source);
static void singleEntity_ConfigurePriorities(audio_source_type_t* priorities, unsigned num_priorities);
static void singleEntity_RefreshRoutedSource(void);

static const audio_router_t single_entity_router_functions = 
{
    .add_source = singleEntity_AddSource,
    .remove_source = singleEntity_RemoveSource,
    .configure_priorities = singleEntity_ConfigurePriorities,
    .refresh_routing = singleEntity_RefreshRoutedSource
};

bool SingleEntity_Init(Task init_task)
{
    UNUSED(init_task);
    AudioRouter_ConfigureHandlers(SingleEntity_GetHandlers());
    SingleEntityData_Init();
    return TRUE;
}

const audio_router_t* SingleEntity_GetHandlers(void)
{
    DEBUG_LOG_FN_ENTRY("SingleEntity_GetHandlers");

    return &single_entity_router_functions;
}

static void singleEntity_RefreshRoutedSource(void)
{
    audio_router_source_t routed_source;
    audio_router_source_t source_to_route;

    bool have_source_to_route = SingleEntityData_GetSourceToRoute(&source_to_route);
    bool have_routed_source = SingleEntityData_GetRoutedSource(&routed_source);

    bool disconnect = FALSE;
    bool connect = FALSE;

    DEBUG_LOG_FN_ENTRY("singleEntity_RefreshRoutedSource");

    /* Decide if connections or disconnections are required */
    if((!have_routed_source) && have_source_to_route)
    {
        connect = TRUE;
    }
    else if(have_routed_source && have_source_to_route)
    {
        if(!SingleEntityData_AreSourcesSame(source_to_route, routed_source))
        {
            connect = TRUE;
            disconnect = TRUE;
        }
    }
    else if(have_routed_source && (!have_source_to_route))
    {
        disconnect = TRUE;
    }

    /* Connect and disconnect as appropriate */
    if(disconnect)
    {
        AudioRouter_CommonDisconnectSource(routed_source);
        SingleEntityData_ClearRoutedSource(routed_source);
    }
    if(connect)
    {
        AudioRouter_CommonConnectSource(source_to_route);
        SingleEntityData_SetRoutedSource(source_to_route);
    }
}

static void singleEntity_AddSource(audio_router_source_t source)
{
    DEBUG_LOG_FN_ENTRY("singleEntity_AddSource type=%d, source=%d", source.type, (unsigned)source.u.audio);

    if(SingleEntityData_AddSource(source))
    {
        singleEntity_RefreshRoutedSource();
    }
}

static void singleEntity_RemoveSource(audio_router_source_t source)
{
    DEBUG_LOG_FN_ENTRY("singleEntity_RemoveSource type=%d, source=%d", source.type, (unsigned)source.u.audio);

    if(SingleEntityData_RemoveSource(source))
    {
        singleEntity_RefreshRoutedSource();
    }
}

static void singleEntity_ConfigurePriorities(audio_source_type_t* priorities, unsigned num_priorities)
{
    /* Start priorities at 1 so default of 'unconfigured priority' is 0 */
    unsigned priority = 1;

    DEBUG_LOG_FN_ENTRY("singleEntity_ConfigurePriorities");

    /* The source types in priorities are assigned an ascending priority from 
       the last entry in priorities. i.e If num_priorities is 3, priorities[2]
       would be assigned priority 1, priorities[1] priority 2 and priorities[0]
       priority 3. */
    while(num_priorities--)
    {
        SingleEntityData_SetSourceTypePriority(priorities[num_priorities], priority++);
    }
}