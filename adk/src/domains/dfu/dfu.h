/*!
\copyright  Copyright (c) 2017 - 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       dfu.h
\brief      Header file for the device firmware upgrade management.
*/

#ifndef DFU_DOMAIN_H_
#define DFU_DOMAIN_H_

#ifdef INCLUDE_DFU

#include <upgrade.h>
#include <task_list.h>

#include <domain_message.h>


#ifdef INCLUDE_DFU_PEER
#include "dfu_peer_sig_typedef.h"
#include "dfu_peer_sig_marshal_typedef.h"
#endif

/*! Defines the upgrade client task list initial capacity */
#define THE_DFU_CLIENT_LIST_INIT_CAPACITY 1

/*! Messages that are sent by the dfu module */
typedef enum {
    /*! Message sent after the device has restarted. This indicates that an
        upgrade has nearly completed and upgrade mode is needed to allow the
        upgrade to be confirmed */
    DFU_REQUESTED_TO_CONFIRM = DFU_MESSAGE_BASE,
    DFU_REQUESTED_IN_PROGRESS,      /*!< Message sent after the device has restarted. This
                                             indicates that an upgrade is in progress
                                             and has been interrupted */
    DFU_ACTIVITY,                   /*!< The DFU module has seen some DFU activity */
    DFU_STARTED,                    /*!< An DFU is now in progress. Started or continued. */
    DFU_COMPLETED,                  /*!< An DFU has been completed */
    DFU_CLEANUP_ON_ABORT,           /*!<An DFU is aborted, clean up DFU specific entities */
    DFU_ABORTED,                    /*!< An upgrade has been aborted.
                                             either owing to device
                                             initiated error OR device
                                             initiated error in Handover
                                             scenario OR Host inititated
                                             abort */
    DFU_PRE_START,                  /*!< A DFU is about to start, in the pre
                                             start phase, the alternate DFU bank
                                             may be erased. So early notify the
                                             app to cancel DFU timers to avoid
                                             false DFU timeouts. */

} dfu_messages_t;

/*! Types of DFU context used with Dfu_SetContext and Dfu_GetContext */
typedef enum {
    DFU_CONTEXT_UNUSED = 0,
    DFU_CONTEXT_GAIA,
    DFU_CONTEXT_GAA_OTA
} dfu_context_t;

typedef enum
{
    REBOOT_REASON_NONE,
    REBOOT_REASON_DFU_RESET,
    REBOOT_REASON_ABRUPT_RESET
} dfu_reboot_reason_t;

/*! Structure holding data for the DFU module */
typedef struct
{
        /*! Task for handling messaging from upgrade library */
    TaskData        dfu_task;

    bool            is_dfu_aborted_on_handover;

    /*! Flag to allow a specific DFU mode, entered when entering the case.
        This flag is set when using the UI to request DFU. The device will
        need to be placed into the case (attached to a charger) before in-case
        DFU will be allowed */
    bool enter_dfu_mode:1;

    /*!< Set to REBOOT_REASON_DFU_RESET for reboot phase of 
         upgrade i.e. when upgrade library sends APP_UPGRADE_REQUESTED_TO_CONFIRM
         and sets to REBOOT_REASON_ABRUPT_RESET for abrupt reset 
         i.e. when upgrade library sends APP_UPGRADE_REQUESTED_IN_PROGRESS. */
    dfu_reboot_reason_t dfu_reboot_reason;

#ifdef INCLUDE_DFU_PEER
    /*
     * Since this variable control conditional message triggers, reverse value
     * is used.
     * i.e. 1: Erase not done and 
     *      0: Erase done
     */
    uint16          peerEraseDone;

    uint32          peerProfilesToConnect;
#endif

        /*! List of tasks to notify of UPGRADE activity. */
    TASK_LIST_WITH_INITIAL_CAPACITY(THE_DFU_CLIENT_LIST_INIT_CAPACITY) client_list;
} dfu_task_data_t;

/*!< Task information for DFU support */
extern dfu_task_data_t app_dfu;

/*! Get the info for the applications dfu support */
#define Dfu_GetTaskData()     (&app_dfu)

/*! Get the Task info for the applications dfu task */
#define Dfu_GetTask()         (&app_dfu.dfu_task)

/*! Get the client list for the applications DFU task */
#define Dfu_GetClientList()         (task_list_flexible_t *)(&app_dfu.client_list)

/*
 * ToDo: These DFU states such as "DFU aborted on Handover"
 *       can be better managed as an enum rather than as exclusive boolean types
 */
#define Dfu_SetDfuAbortOnHandoverState(x) (app_dfu.is_dfu_aborted_on_handover = (x))
#define Dfu_IsDfuAbortOnHandoverDone()    (app_dfu.is_dfu_aborted_on_handover)

bool Dfu_EarlyInit(Task init_task);

bool Dfu_Init(Task init_task);


/*! \brief Allow upgrades to be started

    The library used for firmware upgrades will always allow connections.
    However, it is possible to stop upgrades from beginning or completing.

    \param allow    allow or disallow upgrades

    \return TRUE if the request has taken effect. This setting can not be
        changed if an upgrade is in progress in which case the
        function will return FALSE.
 */
extern bool Dfu_AllowUpgrades(bool allow);


/*! \brief Handler for system messages. All of which are sent to the application.

    This function is called to handle any system messages that this module
    is interested in. If a message is processed then the function returns TRUE.

    \param  id              Identifier of the system message
    \param  message         The message content (if any)
    \param  already_handled Indication whether this message has been processed by
                            another module. The handler may choose to ignore certain
                            messages if they have already been handled.

    \returns TRUE if the message has been processed, otherwise returns the
         value in already_handled
 */
bool Dfu_HandleSystemMessages(MessageId id, Message message, bool already_handled);


/*! \brief Add a client to the UPGRADE module

    Messages from #av_headet_upgrade_messages will be sent to any task
    registered through this API

    \param task Task to register as a client
 */
void Dfu_ClientRegister(Task task);

/*! \brief Set the context of the UPGRADE module

    The value is stored in the UPGRADE PsKey and hence is non-volatile

    \param dfu_context_t Upgrade context to set
 */
void Dfu_SetContext(dfu_context_t context);

/*! \brief Get the context of the UPGRADE module

    The value is stored in the UPGRADE PsKey and hence is non-volatile

    \returns The non-volatile context of the UPGRADE module from the UPGRADE PsKey

 */
dfu_context_t Dfu_GetContext(void);

/*! \brief Abort the ongoing Upgrade if the device is disconnected from GAIA app

    This function is called to abort the device upgrade in both initiator and
    peer device if the initiator device is disconnected from GAIA app

    \param None
 */
void Dfu_AbortDuringDeviceDisconnect(void);

/*! \brief Gets the reboot reason

    \param None

    \returns REBOOT_REASON_DFU_RESET for defined reboot phase of upgrade
            else REBOOT_REASON_ABRUPT_RESET for abrupt reset.

 */
dfu_reboot_reason_t Dfu_GetRebootReason(void);

/*! \brief Sets the reboot reason

    \param dfu_reboot_reason_t Reboot reason

 */
void Dfu_SetRebootReason(dfu_reboot_reason_t val);

/*! \brief Clears upgrade related PSKeys

    \param None

 \return TRUE if upgrade PSKEYs are cleared, FALSE otherwise */
bool Dfu_ClearPsStore(void);

/*!
    Set the current main role (Primary/Secondary).

    Update the current main role in DFU domain and upgrade library.

    \param role TRUE if Primary and FALSE if Secondary.
    \return none
*/
void Dfu_SetRole(bool role);

/*!
    Get the handset profile mask required for DFU.

    Returns the handset profile mask required for DFU operation.

    \param None
    \return Handset profile mask required for DFU operation
*/
uint32 Dfu_GetDFUHandsetProfiles(void);

#else
#define Dfu_EnteredDfuMode() ((void)(0))
#define Dfu_HandleSystemMessages(_id, _msg, _handled) (_handled)
#define Dfu_ClientRegister(tsk) ((void)0)
#define Dfu_SetContext(ctx) ((void)0)
#define Dfu_AbortDuringDeviceDisconnect() ((void)(0))
#define Dfu_GetContext() (APP_DFU_CONTEXT_UNUSED)
#define Dfu_GetRebootReason() (REBOOT_REASON_NONE)
#define Dfu_SetRebootReason(val) ((void)0)
#define Dfu_ClearPsStore() (FALSE)
#define Dfu_SetRole(role) ((void)0)
#define Dfu_GetDFUHandsetProfiles() (0);
#endif /* INCLUDE_DFU */

#endif /* DFU_DOMAIN_H_ */
