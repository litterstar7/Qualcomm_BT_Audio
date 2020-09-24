/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      MIC interface for kymera component
*/

#ifndef KYMERA_MIC_IF_H_
#define KYMERA_MIC_IF_H_

#include <microphones.h>

#define MAX_NUM_OF_CONCURRENT_MICS (3)
#define MAX_NUM_OF_CONCURRENT_MIC_USERS (2)

/*!
\brief  List of each possible user.
        Since multiple users can coexist, each user has a separate bit.
*/
typedef enum
{
    mic_user_none = 0,
    mic_user_sco = (1 << 0),
    mic_user_scofwd = (1 << 1),
    mic_user_aanc = (1 << 2),
    mic_user_va  = (1 << 3),
} mic_users_t;

/*!
\brief  Each user specifies if it is allowed or not to be interrupted when an other user disconnects.
        If all remaining users are interruptible, all mics are disconnected and reconnected again.
*/
typedef enum
{
    disconnect_strategy_interruptible = 0,
    disconnect_strategy_non_interruptible,
} disconnect_strategy_t;

typedef struct
{
    uint8 num_of_mics;
    /* List in the order you want the mics in (first will be primary etc) */
    const microphone_number_t *mic_ids;
    /* List in the same order as types */
    const Sink *mic_sinks;
} mic_connections_t;

typedef struct
{
    uint32 sample_rate;
    mic_connections_t connections;
} mic_connect_params_t;

/*!
\brief  List of possible events that are sent to the users to inform about
        the reason for their disconnection.
*/
typedef enum
{
    mic_event_extra_mic = (1 << 0),
    mic_event_higher_sample_rate = (1 << 1),
    mic_event_disconnecting = (1 << 2)
} mic_event_t;

/*!
\brief  Complete disconnection info sent to all users
*/
typedef struct
{
    mic_users_t user;
    mic_event_t event;
} mic_disconnect_info_t;

/*!
\brief  Callbacks to inform each active user about microphone related events.
*/
typedef struct
{
    /*! Mics will be disconnected in case of a concurrency. The reason is delivered in *info.
     *  The client returns TRUE if it wants to reconnect and FALSE if it wants to stop. */
    bool (*MicDisconnectIndication)(const mic_disconnect_info_t *info);
    /*! Triggered from mic interface, the client provides mic_params */
    bool (*MicGetConnectionParameters)(mic_connect_params_t *mic_params, microphone_number_t *mic_ids, Sink *mic_sinks, Sink *aec_ref_sink);
    /*! Reconnected microphones after a DisconnectIndication. */
    void (*MicReconnectedIndication)(void);
} mic_callbacks_t;

/*!
\brief  User registration structure
*/
typedef struct
{
    /*! registering user */
    mic_users_t user;
    /*! Users will be informed about events via callbacks */
    const mic_callbacks_t *callbacks;
    /*! Mic interface will connect to all mandatory microphones of all registered users,
    *   independent from the current use case. With that a transition between different
    *   use case can be achieved without discontinuities. */
    uint8 num_of_mandatory_mics;
    const microphone_number_t *mandatory_mic_ids;
    const disconnect_strategy_t disconnect_strategy;
} mic_registry_per_user_t;

/*! \brief Connects requested microphones to a user chain.
           The connection from and to AEC Reference is handled by this function.
           The function will take care about concurrency chains. The user is informed via callbacks functions.
    \param user name of requesting user
    \param mic_params connection infos like sample rate, microphone(s) and sink endpoints of the chain
    \param aec_ref_sink Sink to which the aec_ref signal needs to be connected
 */
void Kymera_MicConnect(mic_users_t user, const mic_connect_params_t *mic_params, Sink aec_ref_sink);

/*! \brief Disconnects requested microphones from a user chain.
           The disconnection of AEC Reference is handled by this function.
           The microphone(s) might be in use by a different user. The function takes care about disconnecting the
           microphones, AEC Reference or only the requesting user chain.
 *  \param user name of disconnecting user
 */
void Kymera_MicDisconnect(mic_users_t user);

/*! \brief Register possible microphone users at initialzation time.
    \param info containing user name, callbacks, mandatory mics and disconnect strategy
 */
void Kymera_MicRegisterUser(const mic_registry_per_user_t * const info);

/*! \brief Facilitate transition to low power mode for MIC chain
    \param user name of requesting user
*/
void Kymera_MicSleep(mic_users_t user);

/*! \brief Facilitate transition to exit low power mode for MIC chain
    \param user name of requesting user
*/
void Kymera_MicWake(mic_users_t user);

#endif // KYMERA_MIC_IF_H_
