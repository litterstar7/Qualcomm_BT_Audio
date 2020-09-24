/*!
\copyright  Copyright (c) 2019-2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   telephony_service Telephony Service
\ingroup    services
\brief      The Telephony service provides a high level API for performing Telephony actions on a voice source,
            through the use of a sources telephony control interface.

The Telephony service uses \ref audio_domain Audio domain.
*/

#ifndef TELEPHONY_SERVICE_H_
#define TELEPHONY_SERVICE_H_

#include <message.h>
#include <voice_sources.h>

/*\{*/

/*! Interface of indications regarding telephony routing state.
*/
typedef struct
{
    /*! To be called immediately before being routed */
    void (*RoutingInd)(void);
    /*! To be called immediately after being unrouted */
    void (*UnroutedInd)(void);
} telephony_routing_ind_t;

/*! \brief Initialises the telephony service.

    \param init_task Not used

    \return TRUE
 */
bool TelephonyService_Init(Task init_task);

/*! \brief Register to receive indications regarding telephony routing
    \param interface Set of callbacks to serve as indications
 */
void TelephonyService_RegisterForRoutingInd(const telephony_routing_ind_t *interface);

/*\}*/

#endif /* TELEPHONY_SERVICE_H_ */
