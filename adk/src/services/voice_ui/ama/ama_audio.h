/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_audio.h
\brief  File consists of function decalration for Amazon Voice Service's audio specific interface
*/
#ifndef AMA_AUDIO_H
#define AMA_AUDIO_H

#include "ama_protocol.h"
#include <voice_ui_va_client_if.h>
#include <va_audio_types.h>

#define MAX_AMA_LOCALES 12

typedef struct _ama_supported_locales
{
    uint8 num_locales;
    const char *name[MAX_AMA_LOCALES];
}ama_supported_locales_t;

typedef enum {
    ama_audio_trigger_tap,
    ama_audio_trigger_press,
    ama_audio_trigger_wake_word
}ama_audio_trigger_t;
/*!
    \brief This triggers Voice Session with AVS
*/
bool AmaAudio_Start(ama_audio_trigger_t type);

/*!
    \brief Stops the voice capture chain
*/
void AmaAudio_Stop(void);

/*!
    \brief Starts the voice capture chain
*/
bool AmaAudio_Provide(const AMA_SPEECH_PROVIDE_IND_T* ind);

/*!
    \brief Ends the AVS speech session
*/
void AmaAudio_End(void);

void AmaAudio_StartWakeWordDetection(void);

void AmaAudio_StopWakeWordDetection(void);

unsigned AmaAudio_HandleVoiceData(Source src);

bool AmaAudio_WakeWordDetected(va_audio_wuw_capture_params_t *capture_params, const va_audio_wuw_detection_info_t *wuw_info);

void AmaAudio_Suspended(void);

/*! \brief Loads a hotword model from the file system
    \param file_index Index of the file to load
    \return Identifer to loaded model (as provided by OperatorDataLoadXx function)
 */
DataFileID Ama_LoadHotwordModelFile(FILE_INDEX file_index);

/*!
    \brief Initialises AMA audio data
*/
void AmaAudio_Init(void);

/*!
    \brief Gets the current locale
*/
const char* AmaAudio_GetCurrentLocale(void);

/*!
    \brief Sets the current locale
*/
void AmaAudio_SetLocale(const char* locale);

/*!
    \brief Gets all supported locales
*/
void AmaAudio_GetSupportedLocales(ama_supported_locales_t* supported_locales);

#endif /* AMA_AUDIO_H*/

