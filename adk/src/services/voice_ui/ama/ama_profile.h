/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_profile.h
\brief  Interfaces defination of the profile interface for Amazon AVS
*/

#ifndef AMA_PROFILE_H
#define AMA_PROFILE_H

/*! \brief Initialise the AMA profile handling
*/
void AmaProfile_Init(void);

/*! \brief Send a connected indication for the profile
*/
void AmaProfile_SendConnectedInd(void);

/*! \brief Send a disconnected indication for the profile
*/
void AmaProfile_SendDisconnectedInd(void);

#endif /* AMA_PROFILE_H */
