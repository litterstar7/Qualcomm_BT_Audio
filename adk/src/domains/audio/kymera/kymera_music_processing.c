/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    kymera
\brief      Handles music processing chain

*/

#include "kymera_music_processing.h"
#include "kymera.h"
#include "kymera_ucid.h"

#include <chain.h>
#include <panic.h>

bool Kymera_IsMusicProcessingPresent(void)
{
    return Kymera_GetChainConfigs()->chain_music_processing_config ? TRUE : FALSE;
}

void Kymera_CreateMusicProcessingChain(void)
{
    if(Kymera_IsMusicProcessingPresent())
    {
        kymeraTaskData *theKymera = KymeraGetTaskData();

        theKymera->chain_music_processing_handle = PanicNull(ChainCreate(Kymera_GetChainConfigs()->chain_music_processing_config));
    }
}

void Kymera_ConfigureMusicProcessing(uint32 sample_rate)
{
    if(Kymera_IsMusicProcessingPresent())
    {
        kymera_chain_handle_t chain = KymeraGetTaskData()->chain_music_processing_handle;
        Operator eq;

        PanicNull(chain);

        PanicFalse(Kymera_SetOperatorUcid(chain, OPR_ADD_HEADROOM, UCID_PASS_ADD_HEADROOM));
        PanicFalse(Kymera_SetOperatorUcid(chain, OPR_SPEAKER_EQ, UCID_SPEAKER_EQ));
        PanicFalse(Kymera_SetOperatorUcid(chain, OPR_REMOVE_HEADROOM, UCID_PASS_REMOVE_HEADROOM));


        eq = PanicZero(ChainGetOperatorByRole(chain, OPR_SPEAKER_EQ));
        OperatorsStandardSetSampleRate(eq, sample_rate);

        ChainConnect(chain);
    }
}

void Kymera_StartMusicProcessingChain(void)
{
    if(Kymera_IsMusicProcessingPresent())
    {
        kymeraTaskData *theKymera = KymeraGetTaskData();

        PanicNull(theKymera->chain_music_processing_handle);

        ChainStart(theKymera->chain_music_processing_handle);
    }
}

void Kymera_StopMusicProcessingChain(void)
{
    if(Kymera_IsMusicProcessingPresent())
    {
        kymeraTaskData *theKymera = KymeraGetTaskData();

        PanicNull(theKymera->chain_music_processing_handle);

        ChainStop(theKymera->chain_music_processing_handle);
    }
}

void Kymera_DestroyMusicProcessingChain(void)
{
    if(Kymera_IsMusicProcessingPresent())
    {
        kymeraTaskData *theKymera = KymeraGetTaskData();

        PanicNull(theKymera->chain_music_processing_handle);

        ChainDestroy(theKymera->chain_music_processing_handle);
        theKymera->chain_music_processing_handle = NULL;
    }
}
