/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   focus_domains Focus
\ingroup    domains
\brief      Focus interface definition for instantiating a module which shall
            return the focussed Voice Source.
*/
#ifndef FOCUS_VOICE_SOURCE_H
#define FOCUS_VOICE_SOURCE_H

#include "focus_types.h"

#include <voice_sources.h>
#include <ui_inputs.h>

/*! \brief Focus interface callback used by Focus_GetVoiceSourceForContext API */
typedef bool (*focus_voice_source_for_context_t)(ui_providers_t provider, voice_source_t* voice_source);

/*! \brief Focus interface callback used by Focus_GetVoiceSourceForUiInput API */
typedef bool (*focus_voice_source_for_ui_input_t)(ui_input_t ui_input, voice_source_t* voice_source);

/*! \brief Focus interface callback used by Focus_GetFocusForVoiceSource API */
typedef focus_t (*focus_for_voice_source_t)(const voice_source_t voice_source);

/*! \brief Structure used to configure the focus interface callbacks to be used
           to access the focussed voice source. */
typedef struct
{
    focus_voice_source_for_context_t for_context;
    focus_voice_source_for_ui_input_t for_ui_input;
    focus_for_voice_source_t focus;
} focus_get_voice_source_t;

/*! \brief Configure a set of function pointers to use for retrieving the focussed voice source

    \param a structure containing the functions implementing the focus interface for retrieving
           the focussed voice source.
*/
void Focus_ConfigureVoiceSource(focus_get_voice_source_t focus_get_voice_source);

/*! \brief Get the focussed voice source to query the context of the specified UI Provider

    \param provider - a UI Provider
    \param audio_source - a pointer to the focussed voice_source_t handle
    \return a bool indicating whether or not a focussed voice source was returned in the
            voice_source parameter
*/
bool Focus_GetVoiceSourceForContext(ui_providers_t provider, voice_source_t* voice_source);

/*! \brief Get the focussed voice source that should consume the specified UI Input

    \param ui_input - the UI Input that shall be consumed
    \param voice_source - a pointer to the focussed voice_source_t handle
    \return a bool indicating whether or not a focussed voice source was returned in the
            voice_source parameter
*/
bool Focus_GetVoiceSourceForUiInput(ui_input_t ui_input, voice_source_t* voice_source);

/*! \brief Get the current focus status for the specified voice source

    \param voice_source - the voice_source_t handle
    \return the focus status of the specified voice source
*/
focus_t Focus_GetFocusForVoiceSource(const voice_source_t voice_source);

#endif /* FOCUS_VOICE_SOURCE_H */
