/*!
\copyright  Copyright (c) 2019 - 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   voice_ui va_client_if
\ingroup    Service
\brief      Interface for Voice Assistant clients (VA protocol layers)
*/

#ifndef VOICE_UI_VA_CLIENT_IF_H_
#define VOICE_UI_VA_CLIENT_IF_H_

#include "ui.h"
#include "va_audio_types.h"
#include <operator.h>

typedef enum
{
    voice_ui_a2dp_state_suspended,
    voice_ui_a2dp_state_streaming
} voice_ui_a2dp_state_t;

/*! \brief Voice Assistant provider names
 *         Note: This is a list of voice assistants in priority order */
typedef enum
{
    voice_ui_provider_none = 0,
    voice_ui_provider_audio_tuning = 1,
    voice_ui_provider_gaa = 2,
    voice_ui_provider_ama = 3
} voice_ui_provider_t;

/*! \brief Voice Assistant reboot permission types
 *         Note: This is used with VoiceUi_SelectVoiceAssistant */
typedef enum
{
    voice_ui_reboot_denied,
    voice_ui_reboot_allowed
} voice_ui_reboot_permission_t;

typedef enum
{
    voice_ui_audio_success,
    voice_ui_audio_suspended,
    voice_ui_audio_not_active,
    voice_ui_audio_already_started,
    voice_ui_audio_failed
} voice_ui_audio_status_t;

typedef struct
{
    /*! \brief Called once data arrives at the source.
        \param source Source containing the audio data.
        \return If non-zero the function will be called again if the source is not empty after this timeout period in ms.
     */
    unsigned (*CaptureDataReceived)(Source source);
    /*! \brief Called once Wake-Up-Word is detected.
        \param capture_params Parameters to used for the audio capture (if an audio capture is needed).
        \param va_audio_wuw_detection_info_t Information related to the detection that occurred.
        \return TRUE to start an audio capture based on the parameters provided, FALSE to ignore the detection that occurred and resume.
     */
    bool (*WakeUpWordDetected)(va_audio_wuw_capture_params_t *capture_params, const va_audio_wuw_detection_info_t *wuw_info);
    /*! \brief Called when the active capture session was suspended (For example, due to HFP being routed).
     */
    void (*CaptureSuspended)(void);

    /*! \brief Loads a hotword model from the file system
        \param file_index Index of the file to load
        \return Identifer to loaded model (as provided by OperatorDataLoadXx function)
     */
    DataFileID (*LoadHotwordModelFile)(FILE_INDEX file_index);
} voice_ui_audio_if_t;

/*! \brief Voice Assistant Client Interface */
typedef struct
{
    voice_ui_provider_t va_provider;
    void (*EventHandler)(ui_input_t event_id);
    void (*DeselectVoiceAssistant)(void);
    void (*SelectVoiceAssistant)(void);
    voice_ui_audio_if_t audio_if;
}const voice_ui_if_t;

typedef struct
{
    voice_ui_provider_t (*GetActiveVaProvider)(void);
    void (*SetVoiceAssistantA2dpStreamState)(voice_ui_a2dp_state_t a2dp_stream_state);
}const voice_ui_protected_if_t;

/*! \brief Voice assistant handle descriptor */
typedef struct
{
    voice_ui_a2dp_state_t    va_a2dp_state;
    voice_ui_if_t*           voice_assistant;
    voice_ui_protected_if_t* va_protected_if;
}voice_ui_handle_t;

/*\{*/

/*! \brief Register Voice Assistant client.
    \param va_table Interface provided by voice assistant.
    \return Handle to be used by client in future calls to Voice UI interface.
 */
voice_ui_handle_t * VoiceUi_Register(voice_ui_if_t* va_table);

/*! \brief Check if you are the active Voice Assistant.
    \param va_handle Handle provided at registration.
 */
bool VoiceUi_IsActiveAssistant(voice_ui_handle_t* va_handle);

/*! \brief Start capturing mic data
    \param va_handle Handle provided at registration.
    \param audio_config Configuration related to capturing/encoding mic data.
    \return voice_ui_audio_success on success, else the type of failure.
*/
voice_ui_audio_status_t VoiceUi_StartAudioCapture(voice_ui_handle_t *va_handle, const va_audio_voice_capture_params_t *audio_config);

/*! \brief Stop capturing mic data.
    \param va_handle Handle provided at registration.
*/
void VoiceUi_StopAudioCapture(voice_ui_handle_t *va_handle);

/*! \brief Start Wake-Up-Word detection

    \details If this function is called when a detection chain is already started and
             the detection parameters are different, the current chain will be stopped and a new
             chain will be started with the new parameters.

    \param va_handle Handle provided at registration.
    \param audio_config Configuration related to Wake-Up-Word detection.
    \return voice_ui_audio_success if a new detection chain is started,
            voice_ui_audio_already_started if a detection chain with the same parameters is already started,
            else the type of failure.
*/
voice_ui_audio_status_t VoiceUi_StartWakeUpWordDetection(voice_ui_handle_t *va_handle, const va_audio_wuw_detection_params_t *audio_config);

/*! \brief Stop Wake-Up-Word detection
    \param va_handle Handle provided at registration.
*/
void VoiceUi_StopWakeUpWordDetection(voice_ui_handle_t *va_handle);

/*! \brief Set the active voice assistant
 *   \param va_provider Voice assistant provider to be selected
 *   \param reboot_permission Whether the device is permitted to reboot on changing Voice Assistant
 *   \return TRUE if the provider was successfully selected, FALSE otherwise
 */
bool VoiceUi_SelectVoiceAssistant(voice_ui_provider_t va_provider, voice_ui_reboot_permission_t reboot_permission);

/*! \brief Reboot the local device after a delay
 */
void VoiceUi_RebootLater(void);

/*! \brief Issued by VA provider to notify voiceui that a va connection has been established
 */
void VoiceUi_AssistantConnected(void);

/*! \brief Checks the status of HFP
 *   \return TRUE if HFP is active, FALSE otherwise
 */
bool VoiceUi_IsHfpIsActive(void);

#endif /* VOICE_UI_VA_CLIENT_IF_H_ */
