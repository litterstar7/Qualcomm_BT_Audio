/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   select_focus_domains Focus Select
\ingroup    focus_domains
\brief      Focus Select module API
*/
#ifndef FOCUS_SELECT_H
#define FOCUS_SELECT_H

#include <message.h>

/*! \brief This enumeration is used to specify the priority order by which the audio
           sources shall assume focus. Use in conjunction with the
           FocusSelect_ConfigureAudioSourcePriorities API. */
typedef enum
{
    FOCUS_SELECT_AUDIO_LINE_IN,
    FOCUS_SELECT_AUDIO_USB,
    FOCUS_SELECT_AUDIO_A2DP,
    FOCUS_SELECT_AUDIO_MAX_SOURCES
} focus_select_audio_priority_t;

/*! \brief Initialise the Focus Select module.

    This function registers the Focus Select module with the Focus Device interface
    so the Application framework can resolve which audio source or voice source
    should be routed to the Audio subsystem or interact with the UI module.

    \param  init_task Init task - not used, should be passed NULL
    \return True if successful
*/
bool FocusSelect_Init(Task init_task);

/*! \brief Configure the Audio Source prioritisation to use when establishing focus.

    This function configures the prioritisation of Audio Sources which shall be used
    by the Focus Select module for determining which source has the foreground focus.

    \param  prioritisation - an array, of length FOCUS_SELECT_AUDIO_MAX_SOURCES, specifying
            the order of prioritisation. The highest priority audio source shall be index 0
            in the array. The lowest priority shall be (FOCUS_SELECT_AUDIO_MAX_SOURCES-1).
*/
void FocusSelect_ConfigureAudioSourcePriorities(const focus_select_audio_priority_t *prioritisation);

#endif /* FOCUS_SELECT_H */
