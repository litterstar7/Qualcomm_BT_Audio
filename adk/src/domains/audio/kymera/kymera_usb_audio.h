#ifndef KYMERA_USB_AUDIO_H
#define KYMERA_USB_AUDIO_H

#include "kymera_private.h"

/*! \brief Start USB Audio.
    \param audio_params connect parameters defined by USB audio source.
*/
void KymeraUsbAudio_Start(KYMERA_INTERNAL_USB_AUDIO_START_T *audio_params);

/*! \brief Stop USB Audio.
    \param audio_params disconnect parameters defined by USB audio source.
*/
void KymeraUsbAudio_Stop(KYMERA_INTERNAL_USB_AUDIO_STOP_T *audio_params);

/*! \brief Set vloume for USB Audio.
    \param volume_in_db Volume value to be set for Audio chain.
*/
void KymeraUsbAudio_SetVolume(int16 volume_in_db);

#endif // KYMERA_USB_AUDIO_H
