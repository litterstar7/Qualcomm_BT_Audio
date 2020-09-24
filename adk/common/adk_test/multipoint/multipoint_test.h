/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   adk_test_common multipoint
\ingroup    common
\brief      Interface for common multipoint specifc testing functions.
*/

#ifndef MULTIPOINT_TEST_H
#define MULTIPOINT_TEST_H

/*! \brief Enable Multipoint so two BRERDR connections are allowed at a time. */
void MultipointTest_EnableMultipoint(void);

/*! \brief To check if Multipoint is enabled. */
bool MultipointTest_IsMultipointEnabled(void);

/*! \brief Returns number of handsets are connected at time. */
uint8 MultipointTest_NumberOfHandsetsConnected(void);

#endif /* MULTIPOINT_TEST_H */
