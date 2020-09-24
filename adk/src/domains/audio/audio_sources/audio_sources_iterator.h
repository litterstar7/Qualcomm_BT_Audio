/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   select_focus_domains Focus Select
\ingroup    focus_domains
\brief      API for iterating through the registered audio sources in the audio source registry.
*/

#ifndef AUDIO_SOURCES_ITERATOR_H__
#define AUDIO_SOURCES_ITERATOR_H__

#include "audio_sources_interface_registry.h"
#include "audio_sources_list.h"

/* Opaque handle for an audio source iterator. */
typedef struct audio_sources_iterator_tag * audio_sources_iterator_t;

/*! \brief Create an iterator handle to use for iterating through all the
           registered audio sources of an interface type.

    \param interface_type - the type of interface that the caller wants to iterate
           through in the registry
    \return the iterator handle
*/
audio_sources_iterator_t AudioSourcesIterator_Create(audio_interface_types_t interface_type);

/*! \brief Get the next registered audio source

    \param iterator - an iterator handle
    \return The next audio source in the iterator object, or audio_source_none
            if there are no more registered audio sources (the iterator is empty)
*/
audio_source_t AudioSourcesIterator_NextSource(audio_sources_iterator_t iterator);

/*! \brief Destroy the iterator handle

    \param iterator - the iterator handle to destroy
*/
void AudioSourcesIterator_Destroy(audio_sources_iterator_t iterator);

#endif /* AUDIO_SOURCES_ITERATOR_H__ */