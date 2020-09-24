/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    kymera
\brief      Handles music processing chain
 
*/

#ifndef KYMERA_MUSIC_PROCESSING_H_
#define KYMERA_MUSIC_PROCESSING_H_


/*@{*/

/*! \brief Check if music processing chain is present and can be used.

    \return TRUE if chain is present
*/
bool Kymera_IsMusicProcessingPresent(void);

/*! \brief Create music processing operators.

    Currently only Speaker EQ is implemented, which doesn't require any parameters.
*/
void Kymera_CreateMusicProcessingChain(void);

/*! \brief Configure music processing operators.

    Currently only Speaker EQ is implemented, which doesn't require any parameters.
*/
void Kymera_ConfigureMusicProcessing(uint32 sample_rate);

void Kymera_StartMusicProcessingChain(void);

void Kymera_StopMusicProcessingChain(void);

void Kymera_DestroyMusicProcessingChain(void);

/*@}*/

#endif /* KYMERA_MUSIC_PROCESSING_H_ */
