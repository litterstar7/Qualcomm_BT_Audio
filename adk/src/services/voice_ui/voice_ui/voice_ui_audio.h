/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Voice UI audio private header
*/

#ifndef VOICE_UI_AUDIO_H_
#define VOICE_UI_AUDIO_H_

/*! \brief Enter suspended state (Unroute  VA related audio, don't allow VA related audio to be routed).
 */
void VoiceUi_SuspendAudio(void);

/*! \brief Exist suspended state (Resume VA related audio such as Wake-Up-Word detection).
 */
void VoiceUi_ResumeAudio(void);

/*! \brief Unroute all VA related audio (unlike suspend it doesn't stay in that state and nothing is resumed afterwards).
 */
void VoiceUi_UnrouteAudio(void);

/*! \brief Updates the HFP state
 */
void VoiceUi_UpdateHfpState(void);

#ifdef HOSTED_TEST_ENVIRONMENT
#include "voice_audio_manager.h"
unsigned VoiceUi_CaptureDataReceived(Source source);
va_audio_wuw_detected_response_t VoiceUi_WakeUpWordDetected(const va_audio_wuw_detection_info_t *wuw_info);
void VoiceUi_TestResetAudio(void);
#endif

#endif /* VOICE_UI_AUDIO_H_ */
