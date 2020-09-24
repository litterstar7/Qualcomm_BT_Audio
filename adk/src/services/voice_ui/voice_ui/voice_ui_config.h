/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       voice_ui_config.h
\brief      Voice UI configuration header
*/

#ifndef VOICE_UI_CONFIG_H_
#define VOICE_UI_CONFIG_H_

#if defined ENABLE_AUDIO_TUNING_MODE
#define VOICE_UI_PROVIDER_DEFAULT   voice_ui_provider_audio_tuning
#elif defined INCLUDE_GAA
#define VOICE_UI_PROVIDER_DEFAULT   voice_ui_provider_gaa
#elif defined INCLUDE_AMA
#define VOICE_UI_PROVIDER_DEFAULT   voice_ui_provider_ama
#else
#define VOICE_UI_PROVIDER_DEFAULT   voice_ui_provider_none
#endif

/*! \brief Macro that defines the maximum no of voice assistants supported in the system */
#ifdef ENABLE_AUDIO_TUNING_MODE
    #ifdef INCLUDE_VA_COEXIST
        #define MAX_NO_VA_SUPPORTED     (3)
    #else
        #define MAX_NO_VA_SUPPORTED     (2)
    #endif
#else
    #ifdef INCLUDE_VA_COEXIST
        #define MAX_NO_VA_SUPPORTED     (2)
    #else
        #define MAX_NO_VA_SUPPORTED     (1)
    #endif
#endif

#endif /* VOICE_UI_CONFIG_H_ */
