/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   select_focus_domains Focus Select
\ingroup    focus_domains
\brief      API for iterating through the active audio sources in the registry.
*/

#include "audio_sources_iterator.h"

#include "audio_sources_list.h"
#include "audio_sources.h"
#include "audio_sources_interface_registry.h"

#include <panic.h>
#include <stdlib.h>

typedef struct audio_sources_iterator_tag
{
    audio_source_t active_audio_sources[max_audio_sources];
    uint8 num_active_audio_sources;
    uint8 next_index;

} iterator_internal_t;

audio_sources_iterator_t AudioSourcesIterator_Create(audio_interface_types_t interface_type)
{
    iterator_internal_t * iter = NULL;
    interface_list_t list = {0};

    iter = (iterator_internal_t *)PanicUnlessMalloc(sizeof(iterator_internal_t));
    memset(iter, 0, sizeof(iterator_internal_t));

    audio_source_t source = audio_source_none;
    while(++source < max_audio_sources)
    {
        list = AudioInterface_Get(source, interface_type);
        if (list.number_of_interfaces >= 1)
        {
            iter->active_audio_sources[iter->num_active_audio_sources++] = source;
        }
    }
    return iter;
}

audio_source_t AudioSourcesIterator_NextSource(audio_sources_iterator_t iterator)
{
    audio_source_t next_source = audio_source_none;
    PanicNull(iterator);

    if (iterator->next_index < iterator->num_active_audio_sources)
    {
        next_source = iterator->active_audio_sources[iterator->next_index++];
    }
    return next_source;
}

void AudioSourcesIterator_Destroy(audio_sources_iterator_t iterator)
{
    PanicNull(iterator);

    memset(iterator, 0, sizeof(iterator_internal_t));
    free(iterator);
}