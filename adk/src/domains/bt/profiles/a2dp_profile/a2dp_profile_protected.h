/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   a2dp_profile A2DP profile
\ingroup    profiles
\brief      Protected A2DP profile functions.
*/

#ifndef A2DP_PROFILE_PROTECTED_H_
#define A2DP_PROFILE_PROTECTED_H_

#include "audio_sources_list.h"

/*! \brief Function type definition that sends A2DP audio connected/disconnected status message.
    \param source The audio source to connect/disconnect.
*/
typedef void(*A2dpProfile_SendAudioMessage)(audio_source_t source);

/*! \brief Override the function A2DP profile will call to send status message to
           start the A2DP audio chains. This allows the caller to customise the time
           at which the audio chains are started, for instance, if an action
           must be performed first.
    \param override The override function.
    \return The current function A2DP profile is calling to start audio chains.
    \note The caller shall store the return value from this function and call it
          once it is prepared for audio chains to be started.
    \note By overriding this function, A2DP profile defers responsibility for
          starting the A2DP audio chains.
*/
A2dpProfile_SendAudioMessage A2dpProfile_SetSendAudioConnectMessageFunction(A2dpProfile_SendAudioMessage override);

/*! \brief Override the function A2DP profile will call to send status message to
           stop the A2DP audio chains. This allows the caller to customise the time
           at which the audio chains are stopped, for instance, if an action
           must be performed first.
    \param override The override function.
    \return The current function A2DP profile is calling to stop audio chains.
    \note The caller shall store the return value from this function and call it
          once it is prepared for audio chains to be stopped.
    \note By overriding this function, A2DP profile defers responsibility for
          stopping the A2DP audio chains.
*/
A2dpProfile_SendAudioMessage A2dpProfile_SetSendAudioDisconnectMessageFunction(A2dpProfile_SendAudioMessage override);


#endif /* A2DP_PROFILE_PROTECTED_H_ */