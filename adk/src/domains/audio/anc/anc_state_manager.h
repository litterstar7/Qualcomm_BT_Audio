/*!
\copyright  Copyright (c) 2019 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       anc_state_manager.h
\defgroup   anc_state_manager anc 
\ingroup    audio_domain
\brief      State manager for Active Noise Cancellation (ANC).

Responsibilities:
  Handles state transitions between init, enable and disable states.
  The ANC audio domain is used by \ref audio_curation.
*/

#ifndef ANC_STATE_MANAGER_H_
#define ANC_STATE_MANAGER_H_

/*\{*/
#include <anc.h>
#include <operators.h>
#include <rtime.h>
#include <marshal_common.h>
#include "domain_message.h"

/*! \brief ANC state manager defines the various states handled in ANC. */
typedef enum
{
    anc_state_manager_uninitialised,
    anc_state_manager_power_off,
    anc_state_manager_enabled,
    anc_state_manager_disabled,
    anc_state_manager_tuning_mode_active,
    anc_state_manager_adaptive_anc_tuning_mode_active
} anc_state_manager_t;

typedef struct
{
    uint8 mode;
} ANC_UPDATE_MODE_CHANGED_IND_T;

typedef struct
{
    uint8 anc_leakthrough_gain; /* Leakthrough gain */
} ANC_UPDATE_LEAKTHROUGH_GAIN_IND_T;

typedef struct
{
    uint8 aanc_ff_gain; /* FF gain */
} AANC_FF_GAIN_UPDATE_IND_T; /* Used to Update ANC Clients when local device AANC FF Gain is read from capablity*/

/*Currently FF Gain is the only logging information. If any data is added, this double typecasting can be removed */
typedef AANC_FF_GAIN_UPDATE_IND_T AANC_LOGGING_T;

typedef struct
{
    uint8 left_aanc_ff_gain;
    uint8 right_aanc_ff_gain;
} AANC_FF_GAIN_NOTIFY_T; /* Used to notify ANC Clients with both(local & remote device) FF Gains*/

/*! \brief Events sent by Anc_Statemanager to other modules. */
typedef enum
{
    ANC_UPDATE_STATE_DISABLE_IND = ANC_MESSAGE_BASE,
    ANC_UPDATE_STATE_ENABLE_IND,
    ANC_UPDATE_MODE_CHANGED_IND,
    ANC_UPDATE_LEAKTHROUGH_GAIN_IND,
    ANC_UPDATE_AANC_ENABLE_IND,
    ANC_UPDATE_AANC_DISABLE_IND,
    AANC_FF_GAIN_UPDATE_IND,
    AANC_FF_GAIN_NOTIFY,
    ANC_UPDATE_QUIETMODE_ON_IND,
    ANC_UPDATE_QUIETMODE_OFF_IND,
    AANC_UPDATE_QUIETMODE_IND,
} anc_msg_t;

/* Register with state proxy after initialization */
#ifdef ENABLE_ANC
void AncStateManager_PostInitSetup(void);
#else
#define AncStateManager_PostInitSetup() ((void)(0))
#endif

/*!
    \brief Initialisation of ANC feature, reads microphone configuration  
           and default mode.
    \param init_task Unused
    \return TRUE always.
*/
#ifdef ENABLE_ANC
bool AncStateManager_Init(Task init_task);
#else
#define AncStateManager_Init(x) (FALSE)
#endif

#ifdef ENABLE_ANC
TaskData *AncStateManager_GetTask(void);
#else
#define AncStateManager_GetTask() (NULL)
#endif


#ifdef ENABLE_ANC
bool AncStateManager_CheckIfDspClockBoostUpRequired(void);
#else
#define AncStateManager_CheckIfDspClockBoostUpRequired() (FALSE)
#endif
/*!
    \brief ANC specific handling due to the device Powering On.
*/
#ifdef ENABLE_ANC
void AncStateManager_PowerOn(void);
#else
#define AncStateManager_PowerOn() ((void)(0))
#endif

/*!
    \brief ANC specific handling due to the device Powering Off.
*/  
#ifdef ENABLE_ANC
void AncStateManager_PowerOff(void);
#else
#define AncStateManager_PowerOff() ((void)(0))
#endif

/*!
    \brief Enable ANC functionality.  
*/   
#ifdef ENABLE_ANC
void AncStateManager_Enable(void);
#else
#define AncStateManager_Enable() ((void)(0))
#endif

/*! 
    \brief Disable ANC functionality.
 */   
#ifdef ENABLE_ANC
void AncStateManager_Disable(void);
#else
#define AncStateManager_Disable() ((void)(0))
#endif

/*!
    \brief Is ANC supported in this build ?

    This just checks if ANC may be supported in the build.
    Separate checks are needed to determine if ANC is permitted
    (licenced) or enabled.

    \return TRUE if ANC is enabled in the build, FALSE otherwise.
 */
#ifdef ENABLE_ANC
#define AncStateManager_IsSupported() TRUE
#else
#define AncStateManager_IsSupported() FALSE
#endif


/*!
    \brief Set the operating mode of ANC to configured mode_n. 
    \param mode To be set from existing avalaible modes 0 to 9.
*/
#ifdef ENABLE_ANC
void AncStateManager_SetMode(anc_mode_t mode);
#else
#define AncStateManager_SetMode(x) ((void)(0 * (x)))
#endif

/*!
    \brief Get the AANC params to implicitly enable ANC on a SCO call
    \param KYMERA_INTERNAL_AANC_ENABLE_T parameters to configure AANC capability
*/
#ifdef ENABLE_ANC
void AncStateManager_GetAdaptiveAncEnableParams(bool *in_ear, audio_anc_path_id *control_path, adaptive_anc_hw_channel_t *hw_channel, anc_mode_t *current_mode);
#else
#define AncStateManager_GetAdaptiveAncEnableParams(in_ear, control_path, hw_channel, current_mode) ((void)in_ear, (void)control_path, (void)hw_channel, (void)current_mode)
#endif

/*! 
    \brief Get the Anc mode configured.
    \return mode which is set (from available mode 0 to 9).
 */
#ifdef ENABLE_ANC
anc_mode_t AncStateManager_GetMode(void);
#else
#define AncStateManager_GetMode() (0)
#endif

/*! 
    \brief Checks if ANC is due to be enabled.
    \return TRUE if it is enabled else FALSE.
 */
#ifdef ENABLE_ANC
bool AncStateManager_IsEnabled (void);
#else
#define AncStateManager_IsEnabled() (FALSE)
#endif

/*! 
    \brief Get the Anc mode configured.
    \return mode which is set (from available mode 0 to 9).
 */
#ifdef ENABLE_ANC
anc_mode_t AncStateManager_GetCurrentMode(void);
#else
#define AncStateManager_GetCurrentMode() (0)
#endif

/*! 
    \brief The function returns the number of modes configured.
    \return total modes in anc_modes_t.
 */
#ifdef ENABLE_ANC
uint8 AncStateManager_GetNumberOfModes(void);
#else
#define AncStateManager_GetNumberOfModes() (0)
#endif

/*!
    \brief Checks whether tuning mode is currently active.
    \return TRUE if it is active, else FALSE.
 */
#ifdef ENABLE_ANC
bool AncStateManager_IsTuningModeActive(void);
#else
#define AncStateManager_IsTuningModeActive() (FALSE)
#endif

/*! 
    \brief Cycles through next mode and sets it.
 */
#ifdef ENABLE_ANC
void AncStateManager_SetNextMode(void);
#else
#define AncStateManager_SetNextMode() ((void)(0))
#endif

/*! 
    \brief Enters ANC tuning mode.
 */
#ifdef ENABLE_ANC
void AncStateManager_EnterAncTuningMode(void);
#else
#define AncStateManager_EnterAncTuningMode() ((void)(0))
#endif

/*! 
    \brief Exits the ANC tuning mode.
 */
#ifdef ENABLE_ANC
void AncStateManager_ExitAncTuningMode(void);
#else
#define AncStateManager_ExitAncTuningMode() ((void)(0))
#endif

/*! 
    \brief Enters Adaptive ANC tuning mode.
 */
#if defined(ENABLE_ANC) && defined(ENABLE_ADAPTIVE_ANC)
void AncStateManager_EnterAdaptiveAncTuningMode(void);
#else
#define AncStateManager_EnterAdaptiveAncTuningMode() ((void)(0))
#endif

/*! 
    \brief Exits Adaptive ANC tuning mode.
 */
#if defined(ENABLE_ANC) && defined(ENABLE_ADAPTIVE_ANC)
void AncStateManager_ExitAdaptiveAncTuningMode(void);
#else
#define AncStateManager_ExitAdaptiveAncTuningMode() ((void)(0))
#endif

/*!
    \brief Checks whether Adaptive ANC tuning mode is currently active.
    \return TRUE if it is active, else FALSE.
 */
#if defined(ENABLE_ANC) && defined(ENABLE_ADAPTIVE_ANC)
bool AncStateManager_IsAdaptiveAncTuningModeActive(void);
#else
#define AncStateManager_IsAdaptiveAncTuningModeActive() (FALSE)
#endif

/*! 
    \brief Updates ANC feedforward fine gain from ANC Data structure to ANC H/W. This is not applicable when in 'Mode 1'.
		   AncStateManager_StoreAncLeakthroughGain(uint8 leakthrough_gain) has to be called BEFORE calling AncStateManager_UpdateAncLeakthroughGain()
		   
		   This function shall be called for "World Volume Leakthrough".
		   
 */
#ifdef ENABLE_ANC
void AncStateManager_UpdateAncLeakthroughGain(void);
#else
#define AncStateManager_UpdateAncLeakthroughGain() ((void)(0))
#endif

/*! \brief Register a Task to receive notifications from Anc_StateManager.

    Once registered, #client_task will receive #shadow_profile_msg_t messages.

    \param client_task Task to register to receive shadow_profile notifications.
*/
#ifdef ENABLE_ANC
void AncStateManager_ClientRegister(Task client_task);
#else
#define AncStateManager_ClientRegister(x) ((void)(0))
#endif

/*! \brief Un-register a Task that is receiving notifications from Anc_StateManager.

    If the task is not currently registered then nothing will be changed.

    \param client_task Task to un-register from shadow_profile notifications.
*/
#ifdef ENABLE_ANC
void AncStateManager_ClientUnregister(Task client_task);
#else
#define AncStateManager_ClientUnregister(x) ((void)(0))
#endif

/*! \brief To obtain Leakthrough gain for current mode stored in ANC data structure

    \returns leakthrough gain of ANC H/w 
*/
#ifdef ENABLE_ANC
uint8 AncStateManager_GetAncLeakthroughGain(void);
#else
#define AncStateManager_GetAncLeakthroughGain() (0)
#endif

/*! \brief To store Leakthrough gain in ANC data structure

    \param leakthrough_gain Leakthrough gain to be stored
*/
#ifdef ENABLE_ANC
void AncStateManager_StoreAncLeakthroughGain(uint8 leakthrough_gain);
#else
#define AncStateManager_StoreAncLeakthroughGain(x) ((void)(0))
#endif

/*! 
    \brief Interface to implicitly enable ANC
 */
#ifdef ENABLE_ANC
void AncStateManager_ImplicitEnableReq(void);
#else
#define AncStateManager_ImplicitEnableReq() ((void)(0))
#endif

/*! 
    \brief Interface to implicitly disable ANC
 */
#ifdef ENABLE_ANC
void AncStateManager_ImplicitDisableReq(void);
#else
#define AncStateManager_ImplicitDisableReq() ((void)(0))
#endif

/*! 
    \brief Test hook for unit tests to reset the ANC state.
    \param state  Reset the particular state
 */
#ifdef ANC_TEST_BUILD

#ifdef ENABLE_ANC
void AncStateManager_ResetStateMachine(anc_state_manager_t state);
#else
#define AncStateManager_ResetStateMachine(x) ((void)(0))
#endif

#endif /* ANC_TEST_BUILD*/

/*\}*/
#endif /* ANC_STATE_MANAGER_H_ */
