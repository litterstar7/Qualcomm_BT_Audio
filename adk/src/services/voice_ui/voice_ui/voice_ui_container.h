/*!
\copyright  Copyright (c) 2019 - 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Voice UI container private header
*/

#ifndef VOICE_UI_CONTAINER_H_
#define VOICE_UI_CONTAINER_H_

#include "voice_ui_va_client_if.h"

/*! \brief Get the active voice assistant
 */
voice_ui_provider_t VoiceUi_GetSelectedAssistant(void);

/*! \brief Get the available voice assistant
    \param assistants Array of uint8 to be populated with identifiers of the
           available assistants.
    \return Number of supported assistants.
 */
uint16 VoiceUi_GetSupportedAssistants(uint8 *assistants);

/*! \brief Stores the active voice assistant into the Device database
 */
void VoiceUi_SetSelectedAssistant(uint8 voice_ui_provider);

/*! \brief Get the active Voice Assistant
*/
voice_ui_handle_t* VoiceUi_GetActiveVa(void);

/*! \brief Function called by voice assistant to handle ui events.
    \param
 */
void VoiceUi_EventHandler(voice_ui_handle_t* va_handle, ui_input_t event_id);

#ifdef HOSTED_TEST_ENVIRONMENT
void VoiceUi_UnRegister(voice_ui_handle_t *va_handle);
#endif

#endif

