/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       gaia_profile.h
\brief  Interfaces defination of the profile interface for GAIA
*/

#ifndef GAIA_PROFILE_H
#define GAIA_PROFILE_H

/*! \brief This function prototype needs to be moved to an appropriate location in GAIA.
*/
bool Gaia_DisconnectIfRequired(void);

/*! \brief Initialise the GAIA profile handling
*/
void GaiaProfile_Init(void);

/*! \brief Send a connected indication for the profile
*/
void GaiaProfile_SendConnectedInd(void);

/*! \brief Send a disconnected indication for the profile
*/
void GaiaProfile_SendDisconnectedInd(void);

#endif /* GAIA_PROFILE_H */
