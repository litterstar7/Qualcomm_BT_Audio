/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief     The Kymera USB Voice API

*/

#ifndef KYMERA_USB_VOICE_H
#define KYMERA_USB_VOICE_H

#include "kymera_private.h"

/*! \brief Create and start USB Voice chain.
    \param voice_params Parameters for USB voice chain connect.
*/
void KymeraUsbVoice_Start(KYMERA_INTERNAL_USB_VOICE_START_T *voice_params);

/*! \brief Stop and destroy USB Voice chain.
    \param voice_params Parameters for USB voice chain disconnect.
*/
void KymeraUsbVoice_Stop(KYMERA_INTERNAL_USB_VOICE_STOP_T *voice_params);

/*! \brief Set USB Voice volume.

    \param[in] volume_in_db.
 */
void KymeraUsbVoice_SetVolume(int16 volume_in_db);

/*! \brief Enable or disable MIC muting.

    \param[in] mute TRUE to mute MIC, FALSE to unmute MIC.
*/
void KymeraUsbVoice_MicMute(bool mute);

#endif /* KYMERA_USB_VOICE_H */
