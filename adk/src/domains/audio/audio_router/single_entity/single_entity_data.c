/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   audio_router single_entity
\ingroup    audio_domain
\brief

*/

#include "single_entity_typedef.h"
#include "single_entity_data.h"
#include "focus_audio_source.h"
#include "focus_voice_source.h"
#include "logging.h"
#include "panic.h"

#define IS_ROUTED(source_data) (((source_data).state & SOURCE_ROUTED) != 0)
#define IS_PRESENT(source_data) (((source_data).state & SOURCE_PRESENT) != 0)

single_entity_data_t single_entity_data = {0};

static unsigned source_type_priority[source_type_max] = {0};


bool SingleEntityData_AreSourcesSame(audio_router_source_t source1, audio_router_source_t source2)
{
    bool same = FALSE;

    DEBUG_LOG_FN_ENTRY("SingleEntityData_AreSourcesSame");

    if(source1.type == source2.type)
    {
        switch(source1.type)
        {
            case source_type_voice:
                same = (source1.u.voice == source2.u.voice);
                break;

            case source_type_audio:
                same = (source1.u.audio == source2.u.audio);
                break;

            default:
                break;
        }
    }
    return same;
}

static audio_router_data_t* singleEntityData_FindSourceInData(audio_router_source_t source)
{
    unsigned index;

    DEBUG_LOG_FN_ENTRY("singleEntityData_FindSourceInData");

    for(index = 0 ; index < MAX_NUM_SOURCES ; index++)
    {
        if(IS_PRESENT(single_entity_data.sources[index]) | IS_ROUTED(single_entity_data.sources[index]))
        {
            if(SingleEntityData_AreSourcesSame(
                    single_entity_data.sources[index].source, source))
            {
                return &single_entity_data.sources[index];
            }
        }
    }
    return NULL;
}

static bool singleEntityData_AddSourceToList(audio_router_source_t source)
{
    unsigned index;

    DEBUG_LOG_FN_ENTRY("singleEntityData_AddSourceToList");

    for(index = 0 ; index < MAX_NUM_SOURCES ; index++)
    {
        if(single_entity_data.sources[index].state == 0)
        {
            single_entity_data.sources[index].source = source;
            single_entity_data.sources[index].state = SOURCE_PRESENT;
            return TRUE;
        }
    }
    return FALSE;
}

static bool singleEntityData_SetSourceNotPresent(audio_router_source_t source)
{
    audio_router_data_t* source_entry = singleEntityData_FindSourceInData(source);

    DEBUG_LOG_FN_ENTRY("singleEntityData_SetSourceNotPresent");

    if((source_entry) && IS_PRESENT(*source_entry))
    {
        source_entry->state &= ~SOURCE_PRESENT;
        return TRUE;
    }
    return FALSE;
}

bool SingleEntityData_AddSource(audio_router_source_t source)
{
    DEBUG_LOG_FN_ENTRY("SingleEntityData_AddSource");

    if(!singleEntityData_FindSourceInData(source))
    {
        return singleEntityData_AddSourceToList(source);
    }
    return FALSE;
}

bool SingleEntityData_RemoveSource(audio_router_source_t source)
{
    DEBUG_LOG_FN_ENTRY("SingleEntityData_RemoveSource");

    return singleEntityData_SetSourceNotPresent(source);
}

bool SingleEntityData_GetRoutedSource(audio_router_source_t* source)
{
    unsigned index;

    DEBUG_LOG_FN_ENTRY("SingleEntityData_GetRoutedSource");

    for(index = 0 ; index < MAX_NUM_SOURCES ; index++)
    {
        if(IS_ROUTED(single_entity_data.sources[index]))
        {
            *source = single_entity_data.sources[index].source;
            return TRUE;
        }
    }
    return FALSE;
}


void SingleEntityData_SetRoutedSource(audio_router_source_t source)
{
    audio_router_source_t dummy_source;

    DEBUG_LOG_FN_ENTRY("SingleEntityData_SetRoutedSource");

    if(!SingleEntityData_GetRoutedSource(&dummy_source))
    {
        audio_router_data_t* source_entry = singleEntityData_FindSourceInData(source);

        if((source_entry) && IS_PRESENT(*source_entry))
        {
            source_entry->state |= SOURCE_ROUTED;
            return;
        }
    }
    Panic();
}

bool SingleEntityData_ClearRoutedSource(audio_router_source_t source)
{
    audio_router_data_t* source_entry = singleEntityData_FindSourceInData(source);
    bool success = FALSE;

    DEBUG_LOG_FN_ENTRY("SingleEntityData_ClearRoutedSource");

    if(source_entry)
    {
        if(IS_ROUTED(*source_entry))
        {
            source_entry->state &= ~SOURCE_ROUTED;
            success = TRUE;
        }
    }
    return success;
}

unsigned SingleEntityData_GetSourceTypePriority(audio_source_type_t source_type)
{
    unsigned priority = 0;

    DEBUG_LOG_FN_ENTRY("SingleEntityData_GetSourceTypePriority");

    if(source_type < source_type_max)
    {
        priority = source_type_priority[source_type];
    }
    return priority;
}

bool SingleEntityData_SetSourceTypePriority(audio_source_type_t source_type, unsigned priority)
{
    bool success = FALSE;

    DEBUG_LOG_FN_ENTRY("SingleEntityData_SetSourceTypePriority");

    if(source_type < source_type_max)
    {
        source_type_priority[source_type] = priority;
        success = TRUE;
    }
    return success;
}


static bool singleEntityData_DoesSourceHaveFocus(audio_router_source_t source)
{
    focus_t focus = focus_none;

    DEBUG_LOG_FN_ENTRY("singleEntityData_DoesSourceHaveFocus");

    switch(source.type)
    {
        case source_type_audio:
            focus = Focus_GetFocusForAudioSource(source.u.audio);
            break;

        case source_type_voice:
            focus = Focus_GetFocusForVoiceSource(source.u.voice);
            break;

        default:
            break;
    }
    return (focus == focus_foreground);
}

static bool singleEntityData_FirstSourceIsHigherPriority(audio_router_source_t source1, audio_router_source_t source2)
{
    unsigned source1_priority, source2_priority;

    DEBUG_LOG_FN_ENTRY("singleEntityData_FirstSourceIsHigherPriority");

    source1_priority = SingleEntityData_GetSourceTypePriority(source1.type);
    source2_priority = SingleEntityData_GetSourceTypePriority(source2.type);

    return (source1_priority > source2_priority);
}


bool SingleEntityData_GetSourceToRoute(audio_router_source_t* source)
{
    bool have_entry = FALSE;
    unsigned data_index;

    DEBUG_LOG_FN_ENTRY("SingleEntityData_GetSourceToRoute");

    for(data_index = 0; data_index < MAX_NUM_SOURCES ; data_index++)
    {
        if(IS_PRESENT(single_entity_data.sources[data_index]))
        {
            if(singleEntityData_DoesSourceHaveFocus(single_entity_data.sources[data_index].source))
            {
                if(have_entry)
                {
                    if(singleEntityData_FirstSourceIsHigherPriority(single_entity_data.sources[data_index].source, *source))
                    {
                        *source = single_entity_data.sources[data_index].source;
                    }
                }
                else
                {
                    *source = single_entity_data.sources[data_index].source;
                    have_entry = TRUE;
                }
            }
        }
    }
    return have_entry;
}

void SingleEntityData_Init(void)
{
    memset(single_entity_data.sources, 0, sizeof(single_entity_data.sources));
    memset(source_type_priority, 0, sizeof(source_type_priority));
}

bool SingleEntityData_IsSourcePresent(audio_router_source_t source)
{
    audio_router_data_t* source_entry = singleEntityData_FindSourceInData(source);
    if(source_entry)
    {
        return IS_PRESENT(*source_entry);
    }
    return FALSE;
}
