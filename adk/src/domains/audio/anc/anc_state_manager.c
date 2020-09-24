/*!
\copyright  Copyright (c) 2019 - 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       anc_state_manager.c
\brief      State manager implementation for Active Noise Cancellation (ANC) which handles transitions
            between init, powerOff, powerOn, enable, disable and tuning states.
*/


#ifdef ENABLE_ANC
#include "anc_state_manager.h"
#include "anc_state_manager_private.h"
#include "anc_config.h"
#include "kymera.h"
#include "microphones.h"
#include "state_proxy.h"
#include "phy_state.h"
#include "usb_common.h"
#include "system_clock.h"
#include "kymera_adaptive_anc.h"
#include "aanc_quiet_mode.h"
#include "microphones.h"

#ifndef INCLUDE_STEREO
#include "earbud_config.h"
#endif

#include <panic.h>
#include <task_list.h>
#include <logging.h>

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_TYPE(anc_msg_t)

#define DEBUG_ASSERT(x, y) {if (!(x)) {DEBUG_LOG(y);Panic();}}

#define ADAPTIVE_ANC_MODE    (anc_mode_1) /*Currently Adaptive ANC is configured for mode_1 alone*/

#define NO_USB_CONFIG                   0
#define STATIC_ANC_TUNING_USB_CONFIG    1
#define ADAPTIVE_ANC_TUNING_USB_CONFIG  2

static void ancstateManager_HandleMessage(Task task, MessageId id, Message message);

#define ANC_SM_IS_ADAPTIVE_ANC_ENABLED() (Kymera_IsAdaptiveAncEnabled())

#define ANC_SM_IS_ADAPTIVE_ANC_DISABLED() (!Kymera_IsAdaptiveAncEnabled())

#define ANC_SM_READ_AANC_FF_GAIN_TIMER                (250) /*ms*/
#define ANC_SM_DEFAULT_SECONDARY_FF_GAIN            (0) /*used when peer is not connected*/
/*! \brief Config timer to allow ANC Hardware to configure for QCC512x chip variants 
This timer is not applicable to QCC514x chip varaints and value can be set to zero*/
#define KYMERA_CONFIG_ANC_DELAY_TIMER     (0) /*ms*/

#define QUIET_MODE_DETECTED TRUE
#define QUIET_MODE_NOT_DETECTED FALSE

#define QUIET_MODE_TIME_DELAY_MS  (200U)
#define QUIET_MODE_TIME_DELAY_US  (US_PER_MS * QUIET_MODE_TIME_DELAY_MS)
#define US_TO_MS(us) ((us) / US_PER_MS)

#define STATIC_ANC_CONFIG_SETTLING_TIME (500U)

/* ANC state manager data */
typedef struct
{
    /*! Anc StateManager task */
    TaskData task_data;
    /*! List of tasks registered for notifications */
    task_list_t *client_tasks;
    unsigned requested_enabled:1;
    unsigned actual_enabled:1;
    unsigned power_on:1;
    unsigned persist_anc_mode:1;
    unsigned persist_anc_enabled:1;
    unsigned enable_dsp_clock_boostup:1;
    unsigned unused:6;
    anc_state_manager_t state;
    anc_mode_t current_mode;
    anc_mode_t requested_mode;
    uint8 num_modes;
    uint16 usb_sample_rate;
    uint8 anc_leakthrough_gain;
    uint8 aanc_ff_gain;

/*used only in case of implicit enable of ANC during SCO call*/
    anc_mode_t previous_mode;
    bool user_initiated_enable;
    bool user_initiated_mode_switch;
    bool implicit_anc_enable_during_sco;
/*used only in case of implicit enable of ANC during SCO call*/

    marshal_rtime_t timestamp;
    Sink sink;/*L2CAP SINK*/

    /* added to test SCO disconnect issue in RDP */
    Source mic_src_ff_left;
    Source mic_src_fb_left;
    Source mic_src_ff_right;
    Source mic_src_fb_right;

} anc_state_manager_data_t;

static anc_state_manager_data_t anc_data;

/*! Get pointer to Anc state manager structure */
#define GetAncData() (&anc_data)
#define GetAncClients() TaskList_GetBaseTaskList(&GetAncData()->client_tasks)

/*Implicit ANC enable*/
#define ancSmIsImplicitAncEnabled() (GetAncData()->implicit_anc_enable_during_sco == TRUE)
#define ancSmSetImplicitAncEnable() (GetAncData()->implicit_anc_enable_during_sco = TRUE)
#define ancSmResetImplicitAncEnable() (GetAncData()->implicit_anc_enable_during_sco = FALSE)

#ifndef ENABLE_ADAPTIVE_ANC /* Static ANC build */
#define ancStateManager_StopPathGainsUpdateTimer() (MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_event_set_filter_path_gains))
#define ancStateManager_StartPathGainsUpdateTimer(time) (MessageSendLater(AncStateManager_GetTask(), anc_state_manager_event_set_filter_path_gains, NULL, time))
#else
#define ancStateManager_StopPathGainsUpdateTimer() ((void)(0))
#define ancStateManager_StartPathGainsUpdateTimer(x) ((void)(0 * (x)))
#endif

static bool ancStateManager_HandleEvent(anc_state_manager_event_id_t event);
static void ancStateManager_DisableAnc(anc_state_manager_t next_state);
static void ancStateManager_UpdateAncMode(void);

static void ancStateManager_EnableAncMics(void);
static void ancStateManager_DisableAncMics(void);
static bool ancStateManager_EnableAncHw(void);
static bool ancStateManager_DisableAncHw(void);
static bool ancStateManager_EnableAncHwWithMutePathGains(void);

TaskData *AncStateManager_GetTask(void)
{
   return (&anc_data.task_data);
}

task_list_t *AncStateManager_GetClientTask(void)
{

    return (anc_data.client_tasks);
}

void AncStateManager_PostInitSetup(void)
{
   StateProxy_EventRegisterClient(AncStateManager_GetTask(), state_proxy_event_type_anc);

#ifdef ENABLE_ADAPTIVE_ANC
    /* To receive FF Gain from remote device*/
   StateProxy_EventRegisterClient(AncStateManager_GetTask(), state_proxy_event_type_aanc_logging);

    /* To identify if remote device has gone incase*/
   StateProxy_EventRegisterClient(AncStateManager_GetTask(), state_proxy_event_type_phystate);
#endif
   StateProxy_EventRegisterClient(AncStateManager_GetTask(), state_proxy_event_type_aanc);
}

bool AncStateManager_CheckIfDspClockBoostUpRequired(void)
{
   return (anc_data.enable_dsp_clock_boostup);
}

/***************************************************************************
DESCRIPTION
    Get path configured for ANC

RETURNS
    None
*/
static audio_anc_path_id ancStateManager_GetAncPath(void)
{
    audio_anc_path_id audio_anc_path = AUDIO_ANC_PATH_ID_NONE;
    anc_path_enable anc_path = appConfigAncPathEnable();

    switch(anc_path)
    {
        case feed_forward_mode:
        case feed_forward_mode_left_only: /* fallthrough */
        case feed_back_mode:
        case feed_back_mode_left_only:
            audio_anc_path = AUDIO_ANC_PATH_ID_FFA;
            break;

        case hybrid_mode:
        case hybrid_mode_left_only:
            audio_anc_path = AUDIO_ANC_PATH_ID_FFB;
            break;

        default:
            break;
    }

    return audio_anc_path;
}

static anc_mode_t getModeFromSetModeEvent(anc_state_manager_event_id_t event)
{
    anc_mode_t mode = anc_mode_1;
    
    switch(event)
    {
        case anc_state_manager_event_set_mode_2:
            mode = anc_mode_2;
            break;
        case anc_state_manager_event_set_mode_3:
            mode = anc_mode_3;
            break;
        case anc_state_manager_event_set_mode_4:
            mode = anc_mode_4;
            break;
        case anc_state_manager_event_set_mode_5:
            mode = anc_mode_5;
            break;
        case anc_state_manager_event_set_mode_6:
            mode = anc_mode_6;
            break;
        case anc_state_manager_event_set_mode_7:
            mode = anc_mode_7;
            break;
        case anc_state_manager_event_set_mode_8:
            mode = anc_mode_8;
            break;
        case anc_state_manager_event_set_mode_9:
            mode = anc_mode_9;
            break;
        case anc_state_manager_event_set_mode_10:
            mode = anc_mode_10;
            break;
        case anc_state_manager_event_set_mode_1:
        default:
            break;
    }
    return mode;
}

static anc_state_manager_event_id_t getSetModeEventFromMode(anc_mode_t mode)
{
    anc_state_manager_event_id_t state_event = anc_state_manager_event_set_mode_1;
    
    switch(mode)
    {
        case anc_mode_2:
            state_event = anc_state_manager_event_set_mode_2;
            break;
        case anc_mode_3:
            state_event = anc_state_manager_event_set_mode_3;
            break;
        case anc_mode_4:
            state_event = anc_state_manager_event_set_mode_4;
            break;
        case anc_mode_5:
            state_event = anc_state_manager_event_set_mode_5;
            break;
        case anc_mode_6:
            state_event = anc_state_manager_event_set_mode_6;
            break;
        case anc_mode_7:
            state_event = anc_state_manager_event_set_mode_7;
            break;
        case anc_mode_8:
            state_event = anc_state_manager_event_set_mode_8;
            break;
        case anc_mode_9:
            state_event = anc_state_manager_event_set_mode_9;
            break;
        case anc_mode_10:
            state_event = anc_state_manager_event_set_mode_10;
            break;
        case anc_mode_1:
        default:
            break;
    }
    return state_event;
}

static void ancStateManager_UpdateState(bool new_anc_state)
{
    bool current_anc_state = AncStateManager_IsEnabled();
    DEBUG_LOG("ancStateManager_UpdateState: current state = %u, new state = %u", current_anc_state, new_anc_state);

    if(current_anc_state != new_anc_state)
    {
        if(new_anc_state)
        {
            AncStateManager_Enable();
        }
        else
        {
            AncStateManager_Disable();
        }
    }
}

static void ancStateManager_UpdateMode(uint8 new_anc_mode)
{
    uint8 current_anc_mode = AncStateManager_GetMode();
    DEBUG_LOG("ancStateManager_UpdateMode: current mode = %u, new mode = %u", current_anc_mode, new_anc_mode);

    if(current_anc_mode != new_anc_mode)
    {
        AncStateManager_SetMode(new_anc_mode);
    }
}

/**Functions used for implicit enable of ANC during SCO call**/

/*! \brief Interface to get previous ANC mode. 
      This is used during implicit enable of ANC during SCO call, to decide to fallback to previous mode on SCO Stop if user did not change the mode.

    \returns ANC previous mode
*/
static anc_mode_t ancStateManager_GetPreviousMode(void)
{
    anc_state_manager_data_t *anc_sm = GetAncData();
    DEBUG_LOG("ancStateManager_GetPreviousMode %d", anc_sm->previous_mode);
    return anc_sm->previous_mode;
}


/*! \brief Interface to set previous mode
      This is used during implicit enable of ANC during SCO call, to decide to fallback to previous mode on SCO Stop if user did not change the mode.

    \param ANC previous mode
*/
static void ancStateManager_SetPreviousMode(anc_mode_t mode)
{
    anc_data.previous_mode = mode;
}

/*! \brief Interface to set the user initiated mode switch
      This is used during implicit enable of ANC during SCO call, to decide to fallback to previous state of ANC (ON or OFF)

    \param void
*/
static void ancStateManager_SetUserInitiatedEnable(void)
{
    DEBUG_LOG("ancStateManager_SetUserInitiatedEnable");
    anc_data.user_initiated_enable = TRUE;
}

/*! \brief Interface to reset the user initiated enable
      This is used during implicit enable of ANC during SCO call, to decide to fallback to previous state of ANC (ON or OFF)

    \returns void
*/
static void ancStateManager_ResetUserInitiatedEnable(void)
{
    DEBUG_LOG("ancStateManager_ResetUserInitiatedEnable");
    anc_data.user_initiated_enable = FALSE;
}

/*! \brief Interface to check if user has initiated enable
      This is used during implicit enable of ANC during SCO call, to decide to fallback to previous state of ANC (ON or OFF)

    \returns if User has initiated Enable or not
*/
static bool ancStateManager_IsUserInitiatedEnable(void)
{
    DEBUG_LOG("ancStateManager_IsUserInitiatedEnable %d", anc_data.user_initiated_enable);
    return anc_data.user_initiated_enable;
}

/*! \brief Interface to set when user initiates mode switch
      This is used during implicit enable of ANC during SCO call, to decide to fallback to previous mode on SCO Stop if user did not change the mode.

    \param void
*/
static void ancStateManager_SetUserInitiatedModeSwitch(void)
{
   DEBUG_LOG("ancStateManager_SetUserInitiatedModeSwitch");
    anc_data.user_initiated_mode_switch = TRUE;
}

/*! \brief Interface to reset the user initiated mode switch
      This is used during implicit enable of ANC during SCO call, to decide to fallback to previous mode on SCO Stop if user did not change the mode.

    \returns void
*/
static void ancStateManager_ResetUserInitiatedModeSwitch(void)
{
    DEBUG_LOG("ancStateManager_ResetUserInitiatedModeSwitch");
    anc_data.user_initiated_mode_switch = FALSE;
}

/*! \brief Interface to check if user has initiated mode switch
      This is used during implicit enable of ANC during SCO call, to decide to fallback to previous mode on SCO Stop if user did not change the mode.

    \returns if User has initiated Enable or not
*/static bool ancStateManager_IsUserInitiatedModeSwitch(void)
{
    DEBUG_LOG("ancStateManager_ResetUserInitiatedModeSwitch %d", anc_data.user_initiated_mode_switch);
    return anc_data.user_initiated_mode_switch;
}

static bool ancStateManager_InternalSetMode(anc_mode_t mode)
{
    anc_state_manager_event_id_t state_event = getSetModeEventFromMode (mode);

    if (ancStateManager_HandleEvent(state_event))
    {
        AancQuietMode_ResetQuietModeData();
        return TRUE;
    }
    return FALSE;
}

/*! \brief Action of ANC during SCO call start 
    On start of a SCO Call implicitly enable ANC in Leakthrough mode if ANC is not already ON.
    If ANC is already ON, set mode to Leakthrough during SCO Call.
    For action on SCO stop refer KymeraAdaptiveAnc_ImplicitEnableActionOnScoStop()
*/
static void ancStateManager_ActionOnImplicitEnable(void)
{
    if (appConfigImplicitEnableAANCDuringSCO())
    {
        /*Start by reseting user triggers and monitor during SCO call*/
        ancStateManager_ResetUserInitiatedEnable();
        ancStateManager_ResetUserInitiatedModeSwitch();
        ancSmResetImplicitAncEnable();

        if (AncStateManager_IsEnabled())
        {   
            DEBUG_LOG("AANC already enabled, set mode to LKT during Concurrency");
            if (AncStateManager_GetCurrentMode()  != appConfigAAncModeDuringSCO())
            {
                ancStateManager_SetPreviousMode(AncStateManager_GetCurrentMode());
                ancStateManager_InternalSetMode(appConfigAAncModeDuringSCO());
            }
        }
        else
        {                
            DEBUG_LOG("Implicitly enable AANC in LKT mode");
            ancStateManager_InternalSetMode(appConfigAAncModeDuringSCO());
            ancSmSetImplicitAncEnable();
            ancStateManager_HandleEvent(anc_state_manager_event_enable);
        }
    }
}

/*! \brief Action of ANC during SCO call stop
       If ANC was implicitly enabled on SCO start, on SCO stop, disable ANC
       If ANC was already enabled on SCO start, on SCO stop, go back to previous mode if user did not change the mode 
       else remain in current mode
*/
static void ancStateManager_ActionOnImplicitDisable(void)
{
    if (appConfigImplicitEnableAANCDuringSCO())
    {
        DEBUG_LOG("Implicit ANC enable action on SCO Stop");

        if (ancSmIsImplicitAncEnabled())
        {   
            DEBUG_LOG("ANC was implicitly enabled during SCO call");
            if (!ancStateManager_IsUserInitiatedEnable() && AncStateManager_IsEnabled())
            {
                /*Disable ANC since user did not enable it*/
                DEBUG_LOG("Disable ANC");
                AncStateManager_Disable();
            }
            /*If user initiated mode switch only during SCO call, we will keep the same mode for next ANC*/
        }
        else
        {
            DEBUG_LOG("ANC was enabled by user");

            if (!ancStateManager_IsUserInitiatedModeSwitch())
            {
                /*If no user intervention during SCO call, return to previous mode*/
                DEBUG_LOG("Mode changed to previous mode");
                ancStateManager_InternalSetMode(ancStateManager_GetPreviousMode());  
            }
            /*Rest of the cases, do nothing, ANC state manager will handle*/
        }

        /*End with reseting user triggers*/
        ancStateManager_ResetUserInitiatedEnable();
        ancStateManager_ResetUserInitiatedModeSwitch();
        ancSmResetImplicitAncEnable();
    }
}
/**Functions used for implicit enable of ANC during SCO call**/


/******************************************************************************
DESCRIPTION
    Set ANC Leakthrough gain for FeedForward path
    FFA path is used in FeedForward mode and FFB path in Hybrid mode
	ANC Leakthrough gain is not applicable for 'Mode 1'
*/
static void setAncLeakthroughGain(void)
{
    anc_state_manager_data_t *anc_sm = GetAncData();
    uint8 gain=  anc_sm -> anc_leakthrough_gain;
    anc_path_enable anc_path = appConfigAncPathEnable();

    DEBUG_LOG_FN_ENTRY("setAncLeakthroughGain: %d \n", gain);

    if(AncStateManager_GetCurrentMode() != anc_mode_1)
    {
        switch(anc_path)
        {
            case hybrid_mode:
                if(!(AncConfigureFFBPathGain(AUDIO_ANC_INSTANCE_0, gain) && AncConfigureFFBPathGain(AUDIO_ANC_INSTANCE_1, gain)))
                {
                    DEBUG_LOG_INFO("setAncLeakthroughGain failed for hybrid mode configuration!");
                }
                break;

            case hybrid_mode_left_only:
                if(!(AncConfigureFFBPathGain(AUDIO_ANC_INSTANCE_0, gain)))
                {
                    DEBUG_LOG_INFO("setAncLeakthroughGain failed for hybrid mode left only configuration!");
                }
                break;

            case feed_forward_mode:
                if(!(AncConfigureFFAPathGain(AUDIO_ANC_INSTANCE_0, gain) && AncConfigureFFAPathGain(AUDIO_ANC_INSTANCE_1, gain)))
                {
                    DEBUG_LOG_INFO("setAncLeakthroughGain failed for feed forward mode configuration!");
                }
                break;

            case feed_forward_mode_left_only:
                if(!(AncConfigureFFAPathGain(AUDIO_ANC_INSTANCE_0, gain)))
                {
                    DEBUG_LOG_INFO("setAncLeakthroughGain failed for feed forward mode left only configuration!");
                }
                break;

            default:
                DEBUG_LOG_INFO("setAncLeakthroughGain, cannot set Anc Leakthrough gain for anc_path:  %u", anc_path);
                break;
        }
    }
    else
    {
    DEBUG_LOG_INFO("Anc Leakthrough gain cannot be set in mode 0!");
    }
}

static void ancStateManager_StoreAndUpdateAncLeakthroughGain(uint8 new_anc_leakthrough_gain)
{
    uint8 current_anc_leakthrough_gain = AncStateManager_GetAncLeakthroughGain();
    DEBUG_LOG("ancStateManager_StoreAndUpdateAncLeakthroughGain: current anc leakthrough gain  = %u, new anc leakthrough gain  = %u", current_anc_leakthrough_gain, new_anc_leakthrough_gain);

    if(current_anc_leakthrough_gain != new_anc_leakthrough_gain)
    {
        AncStateManager_StoreAncLeakthroughGain(new_anc_leakthrough_gain);
        setAncLeakthroughGain();
    }
}
#ifdef ENABLE_ADAPTIVE_ANC

/****************************************************************************
DESCRIPTION
    Stop aanc ff gain timer

RETURNS
    None
*/
static void ancStateManager_StopAancFFGainTimer(void)
{
    MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_event_read_aanc_ff_gain_timer_expiry);
}

/****************************************************************************
DESCRIPTION
    Start aanc ff gain timer
    To read the AANC FF gain capability at regular intervals when AANC is enabled

RETURNS
    None
*/
static void ancStateManager_StartAancFFGainTimer(void)
{
    ancStateManager_StopAancFFGainTimer();

    if(AncStateManager_GetCurrentMode() == ADAPTIVE_ANC_MODE)
    {
        MessageSendLater(AncStateManager_GetTask(), anc_state_manager_event_read_aanc_ff_gain_timer_expiry,
                         NULL, ANC_SM_READ_AANC_FF_GAIN_TIMER);
    }
}

static uint8 ancStateManager_GetAancFFGain(void)
{
    anc_state_manager_data_t *anc_sm = GetAncData();
    return anc_sm -> aanc_ff_gain;
}

static void ancStateManager_SetAancFFGain(uint8 aanc_ff_gain)
{

    if(AncStateManager_GetCurrentMode() == ADAPTIVE_ANC_MODE)
    {
        anc_state_manager_data_t *anc_sm = GetAncData();
        anc_sm -> aanc_ff_gain = aanc_ff_gain;
    }
}

/*! \brief To identify if local device is left, incase of earbud application. */
static bool ancstateManager_IsLocalDeviceLeft(void)
{
    bool isLeft = TRUE;

#ifndef INCLUDE_STEREO
    isLeft = appConfigIsLeft();
#endif

    return isLeft;
}

/*! \brief Notify Aanc FF gain update to registered clients. */
static void ancstateManager_MsgRegisteredClientsOnFFGainUpdate(void)
{
    anc_state_manager_data_t *anc_sm = GetAncData();

    /* Check if current mode is AANC mode and check if any of client registered */
    if((AncStateManager_GetCurrentMode() == ADAPTIVE_ANC_MODE) && (anc_sm->client_tasks))
    {
        MESSAGE_MAKE(ind, AANC_FF_GAIN_UPDATE_IND_T);
        ind->aanc_ff_gain = ancStateManager_GetAancFFGain();

        TaskList_MessageSend(anc_sm->client_tasks, AANC_FF_GAIN_UPDATE_IND, ind);
    }
}

/*! \brief Notify Anc FF gains of both devices to registered clients. */
static void ancstateManager_MsgRegisteredClientsWithBothFFGains(uint8 secondary_ff_gain)
{
    anc_state_manager_data_t *anc_sm = GetAncData();

    /* Check if current mode is AANC mode and check if any of client registered */
    if((AncStateManager_GetCurrentMode() == ADAPTIVE_ANC_MODE) && (anc_sm->client_tasks))
    {
        MESSAGE_MAKE(ind, AANC_FF_GAIN_NOTIFY_T);

        if(ancstateManager_IsLocalDeviceLeft())
        {
            ind->left_aanc_ff_gain = ancStateManager_GetAancFFGain();
            ind->right_aanc_ff_gain = secondary_ff_gain;
        }
        else
        {
            ind->left_aanc_ff_gain = secondary_ff_gain;
            ind->right_aanc_ff_gain = ancStateManager_GetAancFFGain();
        }

        TaskList_MessageSend(anc_sm->client_tasks, AANC_FF_GAIN_NOTIFY, ind);
    }
}

/*! \brief Notify Anc state update to registered clients. */
static void ancStateManager_MsgRegisteredClientsOnAdaptiveAncStateUpdate(bool aanc_enable)
{
    anc_state_manager_data_t *anc_sm = GetAncData();
    MessageId message_id;

    DEBUG_LOG("ancStateManager_MsgRegisteredClientsOnAdaptiveAncStateUpdate, Enable: %d \n", aanc_enable);

    if(anc_sm->client_tasks) /* Check if any of client registered */
    {
        message_id = aanc_enable ? ANC_UPDATE_AANC_ENABLE_IND : ANC_UPDATE_AANC_DISABLE_IND;

        TaskList_MessageSendId(anc_sm->client_tasks, message_id);
    }
}

/*! \brief Reads AANC FF gain from capability and stores it in anc data. Notifies ANC Clients and restarts timer.
                Timer will not be restarted if current mode is not Adaptive ANC mode. */
static void ancStateManager_HandleFFGainTimerExpiryEvent(void)
{
    if(AncStateManager_GetCurrentMode() == ADAPTIVE_ANC_MODE)
    {
        uint8 aanc_ff_gain;

        /* Read FF gain from AANC Capability and strore it in anc_data*/
        Kymera_GetApdativeAncFFGain(&aanc_ff_gain);
        ancStateManager_SetAancFFGain(aanc_ff_gain);

        /* restart the timer to read FF gain after specified time interval*/
        ancStateManager_StartAancFFGainTimer();

        /* Notifies ANC clients on FF gain update of local device*/
        ancstateManager_MsgRegisteredClientsOnFFGainUpdate();

        /* If secondary is in case, immediately notify ANC Clients with default Secondary gain */
        if (StateProxy_IsPeerInCase())
        {
            ancstateManager_MsgRegisteredClientsWithBothFFGains(ANC_SM_DEFAULT_SECONDARY_FF_GAIN);
        }
    }
}

/*! \brief Notifies ANC clients With Adaptive ANC state based on ANC state and mode updates
                Also starts/stops FF Gain timer accordingly. */
static void ancStateManager_NotifyAancStateUponAncStateAndModeChange(bool prev_anc_state, anc_mode_t prev_anc_mode)
{
    bool aanc_enable; /* Adaptive ANC state*/
    bool cur_anc_state = anc_data.actual_enabled; /* Current ANC state*/
    anc_mode_t cur_anc_mode = anc_data.current_mode; /* Current ANC Mode */
    bool notify = FALSE;

    /* AANC mode is configured and ANC state has been changed*/
    if((cur_anc_state != prev_anc_state) && (cur_anc_mode == ADAPTIVE_ANC_MODE))
    {
        notify = TRUE;
    }
    /* Mode has been changed from AANC mode to non AANC mode or vice-versa*/
    else if((cur_anc_mode != prev_anc_mode) && ((cur_anc_mode == ADAPTIVE_ANC_MODE) || (prev_anc_mode == ADAPTIVE_ANC_MODE)))
    {
        notify = TRUE;
    }

    if(notify)
    {
        /* Identify Adaptive ANC state based on current ANC state and current ANC mode*/
        aanc_enable = ((cur_anc_state) && (cur_anc_mode == ADAPTIVE_ANC_MODE));

        /* Start/stop AANC FF gain timer based on AANC is enabled/disbaled*/
        aanc_enable ? ancStateManager_StartAancFFGainTimer() : ancStateManager_StopAancFFGainTimer();
        /* Update ANC Clients with AANC state change*/
        ancStateManager_MsgRegisteredClientsOnAdaptiveAncStateUpdate(aanc_enable);
    }
}
#endif


/*! \brief Notify Anc state update to registered clients. */
static void ancStateManager_MsgRegisteredClientsOnStateUpdate(bool enable)
{
    anc_state_manager_data_t *anc_sm = GetAncData();
    MessageId message_id;

    if(anc_sm->client_tasks) /* Check if any of client registered */
    {
        message_id = enable ? ANC_UPDATE_STATE_ENABLE_IND : ANC_UPDATE_STATE_DISABLE_IND;

        TaskList_MessageSendId(anc_sm->client_tasks, message_id);
    }
}

/*! \brief Notify Anc mode update to registered clients. */
static void ancStateManager_MsgRegisteredClientsOnModeUpdate(void)
{
    anc_state_manager_data_t *anc_sm = GetAncData();

    if(anc_sm->client_tasks) /* Check if any of client registered */
    {
        MESSAGE_MAKE(ind, ANC_UPDATE_MODE_CHANGED_IND_T);
        ind->mode = AncStateManager_GetCurrentMode();

        TaskList_MessageSend(anc_sm->client_tasks, ANC_UPDATE_MODE_CHANGED_IND, ind);
    }
}

/*! \brief Notify Anc leakthrough gain update to registered clients. */
static void ancStateManager_MsgRegisteredClientsOnLeakthroughGainUpdate(void)
{
    anc_state_manager_data_t *anc_sm = GetAncData();

    /* Leakthrough Gain is not supported in Mode1. Check for current mode and Check if any of client registered */
    if(AncStateManager_GetCurrentMode() != anc_mode_1 && anc_sm->client_tasks) 
    {
        MESSAGE_MAKE(ind, ANC_UPDATE_LEAKTHROUGH_GAIN_IND_T);
        ind->anc_leakthrough_gain = AncStateManager_GetAncLeakthroughGain();
        TaskList_MessageSend(anc_sm->client_tasks, ANC_UPDATE_LEAKTHROUGH_GAIN_IND, ind);
    }
}

static void ancStateManager_HandleStateProxyEvent(const STATE_PROXY_EVENT_T* event)
{
    switch(event->type)
    {
    //Message sent by state proxy - on remote device for update.
        case state_proxy_event_type_anc:
            DEBUG_LOG_INFO("ancStateManager_HandleStateProxyEvent: state proxy anc sync");
            if (!StateProxy_IsPeerInCase())
            {
                ancStateManager_UpdateState(event->event.anc_data.state);
                ancStateManager_UpdateMode(event->event.anc_data.mode);
                ancStateManager_StoreAndUpdateAncLeakthroughGain(event->event.anc_data.anc_leakthrough_gain);
           }
        break;

#ifdef ENABLE_ADAPTIVE_ANC
        case state_proxy_event_type_aanc_logging:
            /* received FF Gain from remote device, Update ANC Clients with local and remote FF Gains */
            ancstateManager_MsgRegisteredClientsWithBothFFGains(event->event.aanc_logging.aanc_ff_gain);
        break;

        case state_proxy_event_type_phystate:
            DEBUG_LOG_INFO("ancStateManager_HandleStateProxyEvent: state_proxy_event_type_phystate");
            /* Checking if peer has gone incase. If yes, update ANC clients with default FF gain irrespective of timer expiry */
            if ((event->source == state_proxy_source_remote) && (event->event.phystate.new_state == PHY_STATE_IN_CASE))
            {
                ancstateManager_MsgRegisteredClientsWithBothFFGains(ANC_SM_DEFAULT_SECONDARY_FF_GAIN);
                /*Restart the timer */
                ancStateManager_StartAancFFGainTimer();
            }
        break;
#endif

        case state_proxy_event_type_aanc:
             DEBUG_LOG_INFO("ancStateManager_HandleStateProxyEvent: state proxy aanc sync");
             if (!StateProxy_IsPeerInCase())
             {
                 AancQuietMode_HandleQuietModeRx(&event->event.aanc_data);
             }
        break;

        default:
            break;
    }
}


static void ancStateManager_HandlePhyStateChangedInd(PHY_STATE_CHANGED_IND_T* ind)
{
    DEBUG_LOG_FN_ENTRY("ancStateManager_HandlePhyStateChangedInd  new state %d, event %d ", ind->new_state, ind->event);

    if ((anc_data.actual_enabled) && (anc_data.state==anc_state_manager_enabled))
    {
        switch(ind->new_state)
        {
            case PHY_STATE_IN_EAR:
            {
                if(ANC_SM_IS_ADAPTIVE_ANC_ENABLED())
                    Kymera_UpdateAdaptiveAncInEarStatus();
            }
            break;

            case PHY_STATE_OUT_OF_EAR:
            case PHY_STATE_OUT_OF_EAR_AT_REST:
            {
                if(ANC_SM_IS_ADAPTIVE_ANC_ENABLED())
                    Kymera_UpdateAdaptiveAncOutOfEarStatus();
            }
            break;
                
            case PHY_STATE_IN_CASE:
                AncStateManager_Disable();
                break;

            default:
                break;
        }
    }
}

static uint16 ancStateManager_GetUsbSampleRate(void)
{
    anc_state_manager_data_t *anc_sm = GetAncData();
    return anc_sm->usb_sample_rate;
}

static void ancStateManager_SetUsbSampleRate(uint16 usb_sample_rate)
{
    anc_state_manager_data_t *anc_sm = GetAncData();
    anc_sm->usb_sample_rate = usb_sample_rate;
}

static void ancstateManager_HandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch(id)
    {
        case STATE_PROXY_EVENT:
            ancStateManager_HandleStateProxyEvent((const STATE_PROXY_EVENT_T*)message);
            break;

        case anc_state_manager_event_config_timer_expiry:
            ancStateManager_HandleEvent((anc_state_manager_event_id_t)id);
            break;

        case PHY_STATE_CHANGED_IND:
            ancStateManager_HandlePhyStateChangedInd((PHY_STATE_CHANGED_IND_T*)message);
            break;

        case KYMERA_AANC_QUIET_MODE_TRIGGER_IND:
            ancStateManager_HandleEvent(anc_state_manager_event_aanc_quiet_mode_detected);
            break;

        case KYMERA_AANC_QUIET_MODE_CLEAR_IND:
            ancStateManager_HandleEvent(anc_state_manager_event_aanc_quiet_mode_not_detected);
            break;

        case anc_state_manager_event_disable_anc_post_gentle_mute_timer_expiry:
        case anc_state_manager_event_update_mode_post_gentle_mute_timer_expiry:
            ancStateManager_HandleEvent((anc_state_manager_event_id_t)id);
            break;
            
        case anc_state_manager_event_aanc_quiet_mode_enable:
            ancStateManager_HandleEvent((anc_state_manager_event_id_t)id);
        break;

        case MESSAGE_USB_ENUMERATED:
        {
            const MESSAGE_USB_ENUMERATED_T *m = (const MESSAGE_USB_ENUMERATED_T *)message;

            ancStateManager_SetUsbSampleRate(m -> sample_rate);

            anc_state_manager_event_id_t state_event = anc_state_manager_event_usb_enumerated_start_tuning;
            ancStateManager_HandleEvent(state_event);
        }
        break;

        case MESSAGE_USB_DETACHED:
        {
            anc_state_manager_event_id_t state_event = anc_state_manager_event_usb_detached_stop_tuning;
            ancStateManager_HandleEvent(state_event);
        }
        break;

#ifdef ENABLE_ADAPTIVE_ANC
        case anc_state_manager_event_read_aanc_ff_gain_timer_expiry:
            ancStateManager_HandleFFGainTimerExpiryEvent();
            break;
#endif
        case anc_state_manager_event_aanc_quiet_mode_disable:
            ancStateManager_HandleEvent((anc_state_manager_event_id_t)id);
        break;

        case anc_state_manager_event_implicit_enable:
        case anc_state_manager_event_implicit_disable:
            ancStateManager_HandleEvent((anc_state_manager_event_id_t)id);
        break;

        case anc_state_manager_event_set_filter_path_gains:
            ancStateManager_HandleEvent((anc_state_manager_event_id_t)id);
        break;

        default:
            DEBUG_LOG("ancstateManager_HandleMessage: Event not handled");
            break;
    }
}

/******************************************************************************
DESCRIPTION
    This function is responsible for persisting any of the ANC session data
    that is required.
*/
static void setSessionData(void)
{
    anc_writeable_config_def_t *write_data = NULL;

    if(ancConfigManagerGetWriteableConfig(ANC_WRITEABLE_CONFIG_BLK_ID, (void **)&write_data, 0))
    {
        if (anc_data.persist_anc_enabled)
        {
            DEBUG_LOG("setSessionData: Persisting ANC enabled state %d\n", anc_data.requested_enabled);
            write_data->initial_anc_state =  anc_data.requested_enabled;
        }
    
        if (anc_data.persist_anc_mode)
        {
            DEBUG_LOG("setSessionData: Persisting ANC mode %d\n", anc_data.requested_mode);
            write_data->initial_anc_mode = anc_data.requested_mode;
        }
    
        ancConfigManagerUpdateWriteableConfig(ANC_WRITEABLE_CONFIG_BLK_ID);
    }
}




/****************************************************************************
DESCRIPTION
    Stop config timer

RETURNS
    None
*/
static void ancStateManager_StopConfigTimer(void)
{
    MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_event_config_timer_expiry);
}

/****************************************************************************
DESCRIPTION
    Start config timer
    To cater to certain chip variants (QCC512x) where ANC hardware takes around 300ms to configure, 
    it is essential to wait for the configuration to complete before starting Adaptive ANC chain

RETURNS
    None
*/
#ifdef ENABLE_ADAPTIVE_ANC
static void ancStateManager_StartConfigTimer(void)
{
    DEBUG_LOG("Timer value: %d\n", KYMERA_CONFIG_ANC_DELAY_TIMER);

    ancStateManager_StopConfigTimer();
    MessageSendLater(AncStateManager_GetTask(), anc_state_manager_event_config_timer_expiry,
                     NULL, KYMERA_CONFIG_ANC_DELAY_TIMER);
}
#endif

static void ancStateManager_StopGentleMuteTimer(void)
{
    MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_event_update_mode_post_gentle_mute_timer_expiry);
    MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_event_disable_anc_post_gentle_mute_timer_expiry);
}

static void ancStateManager_DisableAncPostGentleMute(void)
{
    DEBUG_LOG("ancStateManager_DisableAncPostGentleMute");
    /* Cancel if any outstanding message in the queue */
    MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_event_disable_anc_post_gentle_mute_timer_expiry);

    MessageSendLater(AncStateManager_GetTask(), anc_state_manager_event_disable_anc_post_gentle_mute_timer_expiry,
                     NULL, KYMERA_CONFIG_ANC_GENTLE_MUTE_TIMER);
}

static void ancStateManager_UpdateAncModePostGentleMute(void)
{
    DEBUG_LOG("ancStateManager_UpdateAncModePostGentleMute");
    /* Cancel if any outstanding message in the queue */
    MessageCancelAll(AncStateManager_GetTask(), anc_state_manager_event_update_mode_post_gentle_mute_timer_expiry);

    MessageSendLater(AncStateManager_GetTask(), anc_state_manager_event_update_mode_post_gentle_mute_timer_expiry,
                     NULL, KYMERA_CONFIG_ANC_GENTLE_MUTE_TIMER);
}

/****************************************************************************
DESCRIPTION
    Get In Ear Status from Phy state

RETURNS
    TRUE if Earbud is in Ear, FALSE otherwise
*/
static bool ancStateManager_GetInEarStatus(void)
{
    return (appPhyStateGetState()==PHY_STATE_IN_EAR) ? (TRUE):(FALSE);
}


/****************************************************************************
DESCRIPTION
    This ensures on Config timer expiry ANC hardware is now setup
    It is safe to enable Adaptive ANC capability
    On ANC Enable request, enable Adatpive ANC independent of the mode

RETURNS
    None
*/
static void ancStateManager_EnableAdaptiveAnc(void)
{
    if ((anc_data.actual_enabled) && 
        (anc_data.state==anc_state_manager_enabled) && 
        ANC_SM_IS_ADAPTIVE_ANC_DISABLED())
    {
        DEBUG_LOG("EnableAdaptiveAnc \n");
        Kymera_EnableAdaptiveAnc(ancStateManager_GetInEarStatus(), /*Use the current Phy state*/
                                 ancStateManager_GetAncPath(), 
                                 adaptive_anc_hw_channel_0, anc_data.current_mode);
    }
}

/******************************************************************************
DESCRIPTION
    Handle the transition into a new state. This function is responsible for
    generating the state related system events.
*/
static void changeState(anc_state_manager_t new_state)
{
    DEBUG_LOG("changeState: ANC State %d -> %d\n", anc_data.state, new_state);

    if ((new_state == anc_state_manager_power_off) && (anc_data.state != anc_state_manager_uninitialised))
    {
        /*Stop Internal Timers, if running*/
        ancStateManager_StopConfigTimer();
        
        /* When we power off from an on state persist any state required */
        setSessionData();
    }
    /* Update state */
    anc_data.state = new_state;
}

/******************************************************************************
DESCRIPTION
    Enumerate as USB device to enable ANC tuning
    Common for both Static ANC and Adaptive ANC tuning
*/
static void ancStateManager_UsbEnumerateTuningDevice(uint8 current_config)
{
    static uint8 config_done = NO_USB_CONFIG;

    if(config_done != current_config)
    {
        Usb_TimeCriticalInit();
        config_done = current_config;
    }

    Usb_ClientRegister(AncStateManager_GetTask());
    Usb_AttachtoHub();
}

/******************************************************************************
DESCRIPTION
    Exits tuning by suspending USB enumeration
    Common for both Static ANC and Adaptive ANC tuning
*/
static void ancStateManager_UsbDetachTuningDevice(void)
{
    Usb_DetachFromHub();
}

/******************************************************************************
DESCRIPTION
    Sets up static ANC tuning mode by disabling ANC and changes state to tuning mode active.
*/
static void ancStateManager_SetupAncTuningMode(void)
{
    DEBUG_LOG_FN_ENTRY("ancStateManager_SetupAncTuningMode\n");

    if(AncStateManager_IsEnabled())
    {
        /*Stop Internal Timers, if running*/
        ancStateManager_StopConfigTimer();
        ancStateManager_StopGentleMuteTimer();

        /* Disables ANC and sets the state to Tuning mode active */
        ancStateManager_DisableAnc(anc_state_manager_tuning_mode_active);
    }
    else
    {
        /*Sets the state to tuning mode active */
        changeState(anc_state_manager_tuning_mode_active);
    }

    ancStateManager_UsbEnumerateTuningDevice(STATIC_ANC_TUNING_USB_CONFIG);
}

/******************************************************************************
DESCRIPTION
    Exits from static Anc tuning mode and unregisters ANC task from USB
*/
static void ancStateManager_ExitTuning(void)
{
    DEBUG_LOG_FN_ENTRY("ancStateManager_ExitTuning");

    KymeraAnc_ExitTuning();
    Usb_ClientUnRegister(AncStateManager_GetTask());
}

/******************************************************************************
DESCRIPTION
    Sets up Adaptive ANC tuning mode and changes state to adaptive anc tuning mode active.
    Enables ANC, as Adaptive ANC needs ANC HW to be running
*/
static void ancStateManager_setupAdaptiveAncTuningMode(void)
{
    DEBUG_LOG_FN_ENTRY("ancStateManager_setupAdaptiveAncTuningMode\n");

    changeState(anc_state_manager_adaptive_anc_tuning_mode_active);

    /* Enable ANC if disabled */
    if(!AncIsEnabled())
    {
        ancStateManager_EnableAncHw();
    }

    ancStateManager_UsbEnumerateTuningDevice(ADAPTIVE_ANC_TUNING_USB_CONFIG);
}

/******************************************************************************
DESCRIPTION
    Exits from tuning mode and unregisters ANC task from USB clients
*/
static void ancStateManager_ExitAdaptiveAncTuning(void)
{
    DEBUG_LOG_FN_ENTRY("ancStateManager_ExitAdaptiveAncTuning");

    /* Disable Anc*/
    if(AncIsEnabled())
    {
       ancStateManager_DisableAncHw();
    }

    kymeraAdaptiveAnc_ExitAdaptiveAncTuning();

    /* Unregister ANC task from USB Clients */
    Usb_ClientUnRegister(AncStateManager_GetTask());
}

static uint8 AncStateManager_ReadFineGainFromInstance(void)
{
    uint8 gain;
    audio_anc_path_id gain_path = ancStateManager_GetAncPath();

    AncReadFineGainFromInstance(AUDIO_ANC_INSTANCE_0, gain_path, &gain);

    return gain;
}

static void ancStateManager_UpdatePathGainsAfterSettlingTime(void)
{
#ifndef ENABLE_ADAPTIVE_ANC /* Static ANC build */
    DEBUG_LOG_FN_ENTRY("ancStateManager_UpdatePathGainsAfterSettlingTime");

    ancStateManager_StopPathGainsUpdateTimer();
    ancStateManager_StartPathGainsUpdateTimer(STATIC_ANC_CONFIG_SETTLING_TIME);
#endif
}

#ifndef ENABLE_ADAPTIVE_ANC /* Static ANC build */
static bool ancStateManager_IsLeftChannelPathEnabled(void)
{
    bool status = FALSE;
    anc_path_enable anc_path = appConfigAncPathEnable();

    switch(anc_path)
    {
        case feed_forward_mode:
        case feed_forward_mode_left_only: /* fallthrough */
        case feed_back_mode: /* fallthrough */
        case feed_back_mode_left_only: /* fallthrough */
        case hybrid_mode: /* fallthrough */
        case hybrid_mode_left_only: /* fallthrough */
            status = TRUE;
            break;

        default:
            status = FALSE;
            break;
    }

    return status;
}

static bool ancStateManager_IsRightChannelPathEnabled(void)
{
    bool status = FALSE;
    anc_path_enable anc_path = appConfigAncPathEnable();

    switch(anc_path)
    {
        case feed_forward_mode:
        case feed_forward_mode_right_only: /* fallthrough */
        case feed_back_mode: /* fallthrough */
        case feed_back_mode_right_only: /* fallthrough */
        case hybrid_mode: /* fallthrough */
        case hybrid_mode_right_only: /* fallthrough */
            status = TRUE;
            break;

        default:
            status = FALSE;
            break;
    }

    return status;
}


/*! \brief Interface to ramp-down filter path fine gain.
 * To avoid sudden dip in dB level the gain value is reduced by 2steps on higher dB level
 * and reduced by 1 step on lower dB level .

*/
static void rampDownFilterPathFineGain(audio_anc_instance inst)
{
    uint8 fine_gain = 0;
    uint8 cnt = 0;
    uint8 fine_gain_lower_db_offset = 0;
#define GAIN_LOWER_DB_LEVEL_OFFSET (12U)

    /* Get a FFA filter path fine gain value from audio PS key for current mode */
    /* Ramp down internal mic path filter fine gain incase of hybrid/feedback mode, and
     * external mic path filter gain incase of feedforward mode */
    AncReadFineGainFromInstance(inst, AUDIO_ANC_PATH_ID_FFA, &fine_gain);

    if(fine_gain > GAIN_LOWER_DB_LEVEL_OFFSET)
    {
        /* Ramp down by 2 steps until the raw value of 12 */
        for(cnt = fine_gain; cnt > GAIN_LOWER_DB_LEVEL_OFFSET; cnt = (cnt - 2))
        {
            DEBUG_LOG("Fine Gain: %d", cnt);
            AncConfigureFFAPathGain(inst, cnt);
        }
        fine_gain_lower_db_offset = GAIN_LOWER_DB_LEVEL_OFFSET;
    }
    else
    {
        fine_gain_lower_db_offset = fine_gain;
    }

    /* and afterwards in setp of 1 */
    for(cnt = fine_gain_lower_db_offset; cnt > 0; cnt--)
    {
        PanicFalse(cnt > 0);
        DEBUG_LOG("Fine Gain: %d", cnt);
        AncConfigureFFAPathGain(inst, cnt);
    }

    AncConfigureFFAPathGain(inst, 0U);
}
#endif

static void ancStateManager_RampDownFilterPathFineGain(void)
{
#ifndef ENABLE_ADAPTIVE_ANC /* Static ANC build */
    DEBUG_LOG("ancStateManager_RampDownFilterPathFineGain, ramp-down start");

    if(ancStateManager_IsLeftChannelPathEnabled())
    {
        /* Note: DAC left channel is hardwired with AUDIO_ANC_INSTANCE_0 in hardware */
        rampDownFilterPathFineGain(AUDIO_ANC_INSTANCE_0);
    }

    if(ancStateManager_IsRightChannelPathEnabled())
    {
        /* Note: DAC right channel is hardwired with AUDIO_ANC_INSTANCE_1 in hardware */
        rampDownFilterPathFineGain(AUDIO_ANC_INSTANCE_1);
    }

    DEBUG_LOG("ancStateManager_RampDownFilterPathFineGain, ramp-down end");
#endif
}

/****************************************************************************
DESCRIPTION
    Call appropriate ANC Enable API based on Adaptive ANC support

RETURNS
    None
*/
static bool ancStateManager_EnableAnc(bool enable)
{
    bool status=FALSE;

    if(!enable)
    {
        /* Static ANC build */
        ancStateManager_StopPathGainsUpdateTimer();
        ancStateManager_RampDownFilterPathFineGain();

        status = ancStateManager_DisableAncHw();
        appKymeraExternalAmpControl(FALSE);
    }
    else
    {
        status = ancStateManager_EnableAncHwWithMutePathGains();
        if(status) /* ANC HW if enabled in Static ANC build */
        {
            ancStateManager_UpdatePathGainsAfterSettlingTime();
        }
        appKymeraExternalAmpControl(TRUE);
    }
    return status;
}

static void ancStateManager_DisableAdaptiveAnc(void)
{
    /*Stop config timer if running, if ANC is getting disabled*/
    ancStateManager_StopConfigTimer();

    if (ANC_SM_IS_ADAPTIVE_ANC_ENABLED())
    {
        /*Disable Adaptive ANC*/
        Kymera_DisableAdaptiveAnc();
    }
}

static void ancStateManager_StartAdaptiveAncTimer(void)
{
#ifdef ENABLE_ADAPTIVE_ANC
    if (ANC_SM_IS_ADAPTIVE_ANC_DISABLED())
    {
        /*To accomodate the ANC hardware delay to configure and to start Adaptive ANC capability*/
        ancStateManager_StartConfigTimer();
    }
#endif
}

/*Maintain AANC chain even on mode change, so do not disable AANC
On Mode change, Set UCID for the new mode, Enable Gentle mute, 
Tell ANC hardware to change filters, LPFs using static ANC APIs (through Set Mode)
And Un mute FF and FB through operator message to AANC operator with static gain values */
static void ancStateManager_UpdateAdaptiveAncOnModeChange(anc_mode_t new_mode)
{
    /* check if ANC is enabled */
    if (anc_data.actual_enabled)
    {
        DEBUG_LOG("ancStateManager_UpdateAdaptiveAncOnModeChange");
        Kymera_AdaptiveAncApplyModeChange(new_mode, ancStateManager_GetAncPath(), adaptive_anc_hw_channel_0);
    }
}

static bool ancStateManager_SetAncMode(anc_mode_t new_mode)
{
    if (ANC_SM_IS_ADAPTIVE_ANC_ENABLED())
    {
        DEBUG_LOG("ancStateManager_SetAncMode: Adaptive ANC mode change request");
        /* Set ANC filter coefficients alone if requested mode is Adaptive ANC. Path gain would be handled by Adaptive ANC operator */
        return AncSetModeFilterCoefficients(new_mode);
    }
    else
    {
        DEBUG_LOG("ancStateManager_SetAncMode: Static ANC or passthrough mode change request");
        /* Static ANC build */
        ancStateManager_StopPathGainsUpdateTimer();

        return AncSetMode(new_mode);
    }
}

/******************************************************************************
DESCRIPTION
    Update the state of the ANC VM library. This is the 'actual' state, as opposed
    to the 'requested' state and therefore the 'actual' state variables should
    only ever be updated in this function.
    
    RETURNS
    Bool indicating if updating lib state was successful or not.

*/  
static bool updateLibState(bool enable, anc_mode_t new_mode)
{
    bool retry_later = TRUE;
    anc_data.enable_dsp_clock_boostup = TRUE;

#ifdef ENABLE_ADAPTIVE_ANC
    anc_mode_t prev_mode = anc_data.current_mode;
    bool prev_anc_state = anc_data.actual_enabled;
#endif

    /* Enable operator framwork before updating DSP clock */
    OperatorFrameworkEnable(1);

    /*Change the DSP clock before enabling ANC and changing up the mode*/
    KymeraAnc_UpdateDspClock();

    DEBUG_LOG("updateLibState: ANC Current Mode %d, Requested Mode %d \n", anc_data.current_mode+1, new_mode+1);
    /* Check to see if we are changing mode */
    if (anc_data.current_mode != new_mode)
    {
        if (ANC_SM_IS_ADAPTIVE_ANC_ENABLED())
        {
             Kymera_AdaptiveAncSetUcid(anc_data.requested_mode);
        }       
        
        /* Set ANC Mode */
        if (!ancStateManager_SetAncMode(new_mode) || (anc_data.requested_mode >=  AncStateManager_GetNumberOfModes()))
        {
            DEBUG_LOG("updateLibState: ANC Set Mode failed %d \n", new_mode + 1);
            retry_later = FALSE;
            /* fallback to previous success mode set */
            anc_data.requested_mode = anc_data.current_mode;
            /*Revert UCID*/
            if (ANC_SM_IS_ADAPTIVE_ANC_ENABLED())
            {
                Kymera_AdaptiveAncSetUcid(anc_data.current_mode);
            }
        }
        else
        {           
            /* Update mode state */
            DEBUG_LOG("updateLibState: ANC Set Mode %d\n", new_mode + 1);
            anc_data.current_mode = new_mode;
            ancStateManager_UpdateAdaptiveAncOnModeChange(new_mode);

            /* Notify ANC mode update to registered clients */
            ancStateManager_MsgRegisteredClientsOnModeUpdate();

            /* Update Leakthrough gain in ANC data structure */
            AncStateManager_StoreAncLeakthroughGain(AncStateManager_ReadFineGainFromInstance());

            /* Notify ANC gain update to registered clients, gain gets updated everytime mode is modified */
            ancStateManager_MsgRegisteredClientsOnLeakthroughGainUpdate();
        }
     }

     /* Determine state to update in VM lib */
     if (anc_data.actual_enabled != enable)
     {
        if (!enable)
        {
            ancStateManager_DisableAdaptiveAnc();
        }
        
         if (ancStateManager_EnableAnc(enable))
        {
            if (enable)
            {
                ancStateManager_StartAdaptiveAncTimer();
            }
            /* Notify ANC state update to registered clients */
            ancStateManager_MsgRegisteredClientsOnStateUpdate(enable);
        }
        else
        {
            /* If this does fail in a release build then we will continue
             and updating the ANC state will be tried again next time
             an event causes a state change. */
            DEBUG_LOG("updateLibState: ANC Enable failed %d\n", enable);
            retry_later = FALSE;
        }

         /* Update enabled state */
         DEBUG_LOG("updateLibState: ANC Enable %d\n", enable);
         anc_data.actual_enabled = enable;
     }

#ifdef ENABLE_ADAPTIVE_ANC
     ancStateManager_NotifyAancStateUponAncStateAndModeChange(prev_anc_state, prev_mode);
#endif

     anc_data.enable_dsp_clock_boostup = FALSE;

     /*Revert DSP clock to its previous speed*/
     KymeraAnc_UpdateDspClock();

     /*Disable operator framwork after reverting DSP clock*/
     OperatorFrameworkEnable(0);
     return retry_later;
}

/******************************************************************************
DESCRIPTION
    Update session data retrieved from config

RETURNS
    Bool indicating if updating config was successful or not.
*/
static bool getSessionData(void)
{
    anc_writeable_config_def_t *write_data = NULL;

    ancConfigManagerGetWriteableConfig(ANC_WRITEABLE_CONFIG_BLK_ID, (void **)&write_data, (uint16)sizeof(anc_writeable_config_def_t));

    /* Extract session data */
    anc_data.requested_enabled = write_data->initial_anc_state;
    anc_data.persist_anc_enabled = write_data->persist_initial_state;
    anc_data.requested_mode = write_data->initial_anc_mode;
    anc_data.persist_anc_mode = write_data->persist_initial_mode;
    
    ancConfigManagerReleaseConfig(ANC_WRITEABLE_CONFIG_BLK_ID);

    return TRUE;
}

static void ancStateManager_EnableAncMics(void)
{
    anc_readonly_config_def_t *read_data = NULL;

    DEBUG_LOG_FN_ENTRY("ancStateManager_EnableAncMics");

    if (ancConfigManagerGetReadOnlyConfig(ANC_READONLY_CONFIG_BLK_ID, (const void **)&read_data))
    {
/* Since ANC HW is running in PDM domain and sample rate config is ideally ignored;
 * On concurrency case probably keeping sample rate at 16kHz is an optimal value */
#define ANC_SAMPLE_RATE        (16000U)
        microphone_number_t feedForwardLeftMic = read_data->anc_mic_params_r_config.feed_forward_left_mic;
        microphone_number_t feedForwardRightMic = read_data->anc_mic_params_r_config.feed_forward_right_mic;
        microphone_number_t feedBackLeftMic = read_data->anc_mic_params_r_config.feed_back_left_mic;
        microphone_number_t feedBackRightMic = read_data->anc_mic_params_r_config.feed_back_right_mic;

        if(feedForwardLeftMic != microphone_none)
            anc_data.mic_src_ff_left = Microphones_TurnOnMicrophone(feedForwardLeftMic, ANC_SAMPLE_RATE, non_exclusive_user);

        if(feedForwardRightMic != microphone_none)
            anc_data.mic_src_ff_right = Microphones_TurnOnMicrophone(feedForwardRightMic, ANC_SAMPLE_RATE, non_exclusive_user);

        if(feedBackLeftMic != microphone_none)
            anc_data.mic_src_fb_left = Microphones_TurnOnMicrophone(feedBackLeftMic, ANC_SAMPLE_RATE, non_exclusive_user);

        if(feedBackRightMic != microphone_none)
            anc_data.mic_src_fb_right = Microphones_TurnOnMicrophone(feedBackRightMic, ANC_SAMPLE_RATE, non_exclusive_user);
    }
}

static void ancStateManager_DisableAncMics(void)
{
    anc_readonly_config_def_t *read_data = NULL;

    DEBUG_LOG_FN_ENTRY("ancStateManager_DisableAncMics");

    if (ancConfigManagerGetReadOnlyConfig(ANC_READONLY_CONFIG_BLK_ID, (const void **)&read_data))
    {
        microphone_number_t feedForwardLeftMic = read_data->anc_mic_params_r_config.feed_forward_left_mic;
        microphone_number_t feedForwardRightMic = read_data->anc_mic_params_r_config.feed_forward_right_mic;
        microphone_number_t feedBackLeftMic = read_data->anc_mic_params_r_config.feed_back_left_mic;
        microphone_number_t feedBackRightMic = read_data->anc_mic_params_r_config.feed_back_right_mic;

        if(feedForwardLeftMic != microphone_none)
        {
            Microphones_TurnOffMicrophone(feedForwardLeftMic, non_exclusive_user);
            anc_data.mic_src_ff_left = NULL;
        }

        if(feedForwardRightMic != microphone_none)
        {
            Microphones_TurnOffMicrophone(feedForwardRightMic, non_exclusive_user);
            anc_data.mic_src_ff_right = NULL;
        }

        if(feedBackLeftMic != microphone_none)
        {
            Microphones_TurnOffMicrophone(feedBackLeftMic, non_exclusive_user);
            anc_data.mic_src_fb_left = NULL;
        }

        if(feedBackRightMic != microphone_none)
        {
            Microphones_TurnOffMicrophone(feedBackRightMic, non_exclusive_user);
            anc_data.mic_src_fb_right = NULL;
        }
    }
}

static bool ancStateManager_EnableAncHw(void)
{
    DEBUG_LOG_FN_ENTRY("ancStateManager_EnableAncHw");
    ancStateManager_EnableAncMics();

    return AncEnable(TRUE);
}

static bool ancStateManager_DisableAncHw(void)
{
    bool ret_val = FALSE;

    DEBUG_LOG_FN_ENTRY("ancStateManager_DisableAncHw");
    ret_val = AncEnable(FALSE);
    ancStateManager_DisableAncMics();

    return ret_val;
}

static bool ancStateManager_EnableAncHwWithMutePathGains(void)
{
    DEBUG_LOG_FN_ENTRY("ancStateManager_EnableAncHwWithMutePathGains");
    ancStateManager_EnableAncMics();
    return AncEnableWithMutePathGains();
}

/******************************************************************************
DESCRIPTION
    Read the configuration from the ANC Mic params.
*/
static bool readMicConfigParams(anc_mic_params_t *anc_mic_params)
{
    anc_readonly_config_def_t *read_data = NULL;

    if (ancConfigManagerGetReadOnlyConfig(ANC_READONLY_CONFIG_BLK_ID, (const void **)&read_data))
    {
        microphone_number_t feedForwardLeftMic = read_data->anc_mic_params_r_config.feed_forward_left_mic;
        microphone_number_t feedForwardRightMic = read_data->anc_mic_params_r_config.feed_forward_right_mic;
        microphone_number_t feedBackLeftMic = read_data->anc_mic_params_r_config.feed_back_left_mic;
        microphone_number_t feedBackRightMic = read_data->anc_mic_params_r_config.feed_back_right_mic;

        memset(anc_mic_params, 0, sizeof(anc_mic_params_t));

        if (feedForwardLeftMic)
        {
            anc_mic_params->enabled_mics |= feed_forward_left;
            anc_mic_params->feed_forward_left = *Microphones_GetMicrophoneConfig(feedForwardLeftMic);
        }

        if (feedForwardRightMic)
        {
            anc_mic_params->enabled_mics |= feed_forward_right;
            anc_mic_params->feed_forward_right = *Microphones_GetMicrophoneConfig(feedForwardRightMic);
        }

        if (feedBackLeftMic)
        {
            anc_mic_params->enabled_mics |= feed_back_left;
            anc_mic_params->feed_back_left = *Microphones_GetMicrophoneConfig(feedBackLeftMic);
        }

        if (feedBackRightMic)
        {
            anc_mic_params->enabled_mics |= feed_back_right;
            anc_mic_params->feed_back_right = *Microphones_GetMicrophoneConfig(feedBackRightMic);
        }

        ancConfigManagerReleaseConfig(ANC_READONLY_CONFIG_BLK_ID);

        return TRUE;
    }
    DEBUG_LOG("readMicConfigParams: Failed to read ANC Config Block\n");
    return FALSE;
}

/****************************************************************************    
DESCRIPTION
    Read the number of configured Anc modes.
*/
static uint8 readNumModes(void)
{
    anc_readonly_config_def_t *read_data = NULL;
    uint8 num_modes = 0;

    /* Read the existing Config data */
    if (ancConfigManagerGetReadOnlyConfig(ANC_READONLY_CONFIG_BLK_ID, (const void **)&read_data))
    {
        num_modes = read_data->num_anc_modes;
        ancConfigManagerReleaseConfig(ANC_READONLY_CONFIG_BLK_ID);
    }
    return num_modes;
}

anc_mode_t AncStateManager_GetMode(void)
{
    return (anc_data.requested_mode);
}

/******************************************************************************
DESCRIPTION
    This function reads the ANC configuration and initialises the ANC library
    returns TRUE on success FALSE otherwise 
*/ 
static bool configureAndInit(void)
{
    anc_mic_params_t anc_mic_params;
    bool init_success = FALSE;

    if(readMicConfigParams(&anc_mic_params) && getSessionData())
    {
        if(AncInit(&anc_mic_params, AncStateManager_GetMode()))
        {
            /* Update local state to indicate successful initialisation of ANC */
            anc_data.current_mode = anc_data.requested_mode;
            anc_data.previous_mode = anc_data.current_mode;
            anc_data.actual_enabled = FALSE;
            anc_data.num_modes = readNumModes();
            anc_data.task_data.handler = ancstateManager_HandleMessage;
            /* ANC state manger task creation */
            anc_data.client_tasks = TaskList_Create();
            init_success = TRUE;
        }
    }

    return init_success;
}

/******************************************************************************
DESCRIPTION
    Event handler for the Uninitialised State

RETURNS
    Bool indicating if processing event was successful or not.
*/ 
static bool ancStateManager_HandleEventsInUninitialisedState(anc_state_manager_event_id_t event)
{
    bool init_success = FALSE;

    switch (event)
    {
        case anc_state_manager_event_initialise:
        {
            if(configureAndInit())
            {
                init_success = TRUE;
                changeState(anc_state_manager_power_off);
            }
            else
            {
                DEBUG_LOG("handleUninitialisedEvent: ANC Failed to initialise due to incorrect mic configuration/ licencing issue \n");
                /* indicate error by Led */
            }
        }
        break;
        
        default:
        {
            DEBUG_LOG("ancStateManager_HandleEventsInUninitialisedState: Unhandled event [%d]\n", event);
        }
        break;
    }
    return init_success;
}

/******************************************************************************
DESCRIPTION
    Event handler for the Power Off State

RETURNS
    Bool indicating if processing event was successful or not.
*/ 
static bool ancStateManager_HandleEventsInPowerOffState(anc_state_manager_event_id_t event)
{
    bool event_handled = FALSE;

    DEBUG_ASSERT(!anc_data.actual_enabled, "ancStateManager_HandleEventsInPowerOffState: ANC actual enabled in power off state\n");

    switch (event)
    {
        case anc_state_manager_event_power_on:
        {
            anc_state_manager_t next_state = anc_state_manager_disabled;
            anc_data.power_on = TRUE;

            /* If we were previously enabled then enable on power on */
            if (anc_data.requested_enabled)
            {
                if(updateLibState(anc_data.requested_enabled, anc_data.requested_mode))
                {
                    /* Lib is enabled */
                    next_state = anc_state_manager_enabled;
                }
            }
            /* Update state */
            changeState(next_state);
            
            event_handled = TRUE;
        }
        break;

        default:
        {
            DEBUG_LOG("ancStateManager_HandleEventsInPowerOffState: Unhandled event [%d]\n", event);
        }
        break;
    }
    return event_handled;
}

/******************************************************************************
DESCRIPTION
    Event handler for the Enabled State

RETURNS
    Bool indicating if processing event was successful or not.
*/
static bool ancStateManager_HandleEventsInEnabledState(anc_state_manager_event_id_t event)
{
    /* Assume failure until proven otherwise */
    bool event_handled = FALSE;
    anc_state_manager_t next_state = anc_state_manager_disabled;

    switch (event)
    {
        case anc_state_manager_event_power_off:
        {
            /* When powering off we need to disable ANC in the VM Lib first */
            next_state = anc_state_manager_power_off;
            anc_data.power_on = FALSE;
        }
        /* fallthrough */
        case anc_state_manager_event_disable:
        {
            /* Only update requested enabled if not due to a power off event */
            anc_data.requested_enabled = (next_state == anc_state_manager_power_off);

#ifdef INCLUDE_ANC_PASSTHROUGH_SUPPORT_CHAIN
            KymeraAnc_DisconnectPassthroughSupportChainFromDac();
            KymeraAnc_DestroyPassthroughSupportChain();
#endif

            /*Stop Internal Timers, if running*/
            ancStateManager_StopConfigTimer();
            ancStateManager_StopGentleMuteTimer();

            if (next_state == anc_state_manager_power_off)
            {
                ancStateManager_DisableAnc(anc_state_manager_power_off);
            }
            else
            {
                if(ANC_SM_IS_ADAPTIVE_ANC_ENABLED())
                {
                    Kymera_EnableAdaptiveAncGentleMute();
                    ancStateManager_DisableAncPostGentleMute();
                }
                else
                {
                    ancStateManager_DisableAnc(anc_state_manager_disabled);
                }
            }
            
            event_handled = TRUE;
        }
        break;

        case anc_state_manager_event_set_mode_1: /* fallthrough */
        case anc_state_manager_event_set_mode_2:
        case anc_state_manager_event_set_mode_3:
        case anc_state_manager_event_set_mode_4:
        case anc_state_manager_event_set_mode_5:
        case anc_state_manager_event_set_mode_6:
        case anc_state_manager_event_set_mode_7:
        case anc_state_manager_event_set_mode_8:
        case anc_state_manager_event_set_mode_9:
        case anc_state_manager_event_set_mode_10:            
        {
            anc_data.requested_mode = getModeFromSetModeEvent(event);

            if (anc_data.requested_mode != anc_data.current_mode)
            {
                if (ANC_SM_IS_ADAPTIVE_ANC_ENABLED())
                {
                    Kymera_EnableAdaptiveAncGentleMute();
                    ancStateManager_UpdateAncModePostGentleMute();
                }
                else
                {
                    ancStateManager_UpdateAncMode();
                }
            }
            event_handled = TRUE;
        }
        break;

        case anc_state_manager_event_activate_anc_tuning_mode:
        {            
            ancStateManager_SetupAncTuningMode ();
            event_handled = TRUE;
        }
        break;

        case anc_state_manager_event_activate_adaptive_anc_tuning_mode:
        {
            ancStateManager_setupAdaptiveAncTuningMode();
            event_handled = TRUE;
        }
        break;

        case anc_state_manager_event_set_anc_leakthrough_gain:
        {
            setAncLeakthroughGain();

            /* Notify ANC gain update to registered clients */
            ancStateManager_MsgRegisteredClientsOnLeakthroughGainUpdate();

            event_handled = TRUE;
        }
        break;
        
        case anc_state_manager_event_config_timer_expiry:
        {            
            ancStateManager_EnableAdaptiveAnc();
            event_handled = TRUE;
        }
        break;
        
        case anc_state_manager_event_disable_anc_post_gentle_mute_timer_expiry:
        {
            ancStateManager_DisableAnc(anc_state_manager_disabled);
            event_handled = TRUE;
        }
        break;

        case anc_state_manager_event_update_mode_post_gentle_mute_timer_expiry:
        {
            ancStateManager_UpdateAncMode();
            event_handled = TRUE;
        }
        break;

        case anc_state_manager_event_aanc_quiet_mode_detected:
        {
            if (ANC_SM_IS_ADAPTIVE_ANC_ENABLED())
            {
                AancQuietMode_HandleQuietModeDetected();
            }
        }
        break;

        case anc_state_manager_event_aanc_quiet_mode_not_detected:
        {
            if (ANC_SM_IS_ADAPTIVE_ANC_ENABLED())
            {
                AancQuietMode_HandleQuietModeCleared();
            }
        }
        break;

        case anc_state_manager_event_aanc_quiet_mode_enable:
        {
            if (ANC_SM_IS_ADAPTIVE_ANC_ENABLED())
            {
                AancQuietMode_HandleQuietModeEnable();
            }
        }
        break;

        case anc_state_manager_event_aanc_quiet_mode_disable:
        {
            if (ANC_SM_IS_ADAPTIVE_ANC_ENABLED())
            {
                AancQuietMode_HandleQuietModeDisable();
            }
        }
        break;
        
        case anc_state_manager_event_implicit_enable:
            ancStateManager_ActionOnImplicitEnable();
            break;

        case anc_state_manager_event_implicit_disable:
            ancStateManager_ActionOnImplicitDisable();
            break;

        case anc_state_manager_event_set_filter_path_gains:
            AncSetCurrentFilterPathGains();
        break;

        default:
        {
            DEBUG_LOG_INFO("ancStateManager_HandleEventsInEnabledState: Unhandled event [%d]\n", event);
        }
        break;
    }
    return event_handled;
}

/******************************************************************************
DESCRIPTION
    Event handler for the Disabled State

RETURNS
    Bool indicating if processing event was successful or not.
*/
static bool ancStateManager_HandleEventsInDisabledState(anc_state_manager_event_id_t event)
{
    /* Assume failure until proven otherwise */
    bool event_handled = FALSE;

    switch (event)
    {
        case anc_state_manager_event_power_off:
        {
            /* Nothing to do, just update state */
            changeState(anc_state_manager_power_off);
            anc_data.power_on = FALSE;
            event_handled = TRUE;
        }
        break;

        case anc_state_manager_event_enable:
        {
            /* Try to enable */
            anc_state_manager_t next_state = anc_state_manager_enabled;
            anc_data.requested_enabled = TRUE;

            KymeraAnc_CreatePassthroughSupportChain();
            KymeraAnc_ConnectPassthroughSupportChainToDac();

            /* Enable ANC */
            updateLibState(anc_data.requested_enabled, anc_data.requested_mode);
            
            /* Update state */
            changeState(next_state);
           
            event_handled = TRUE;
        }
        break;

        case anc_state_manager_event_set_mode_1: /* fallthrough */
        case anc_state_manager_event_set_mode_2:
        case anc_state_manager_event_set_mode_3:
        case anc_state_manager_event_set_mode_4:
        case anc_state_manager_event_set_mode_5:
        case anc_state_manager_event_set_mode_6:
        case anc_state_manager_event_set_mode_7:
        case anc_state_manager_event_set_mode_8:
        case anc_state_manager_event_set_mode_9:
        case anc_state_manager_event_set_mode_10:     
        {
            /* Update the requested ANC Mode, will get applied next time we enable */
            anc_data.requested_mode = getModeFromSetModeEvent(event);

            /* Update Leakthrough gain in ANC data structure */
            AncStateManager_StoreAncLeakthroughGain(AncStateManager_ReadFineGainFromInstance());

            event_handled = TRUE;
        }
        break;
        
        case anc_state_manager_event_activate_anc_tuning_mode:
        {
            ancStateManager_SetupAncTuningMode ();
            event_handled = TRUE;
        }
        break;

        case anc_state_manager_event_activate_adaptive_anc_tuning_mode:
        {
            ancStateManager_setupAdaptiveAncTuningMode();
            event_handled = TRUE;
        }
        break;

        case anc_state_manager_event_implicit_enable:
            ancStateManager_ActionOnImplicitEnable();
            break;

        case anc_state_manager_event_implicit_disable:
            ancStateManager_ActionOnImplicitDisable();
            break;
                   
        default:
        {
            DEBUG_LOG("ancStateManager_HandleEventsInDisabledState: Unhandled event [%d]\n", event);
        }
        break;
    }
    return event_handled;
}

static bool ancStateManager_HandleEventsInTuningState(anc_state_manager_event_id_t event)
{
    bool event_handled = FALSE;
    
    switch(event)
    {
        case anc_state_manager_event_usb_enumerated_start_tuning:
        {
            DEBUG_LOG("ancStateManager_HandleEventsInTuningState: anc_state_manager_event_usb_enumerated_start_tuning\n");

            KymeraAnc_EnterTuning(ancStateManager_GetUsbSampleRate());

            event_handled = TRUE;
        }
        break;

        case anc_state_manager_event_power_off:
        {
            DEBUG_LOG("ancStateManager_HandleEventsInTuningState: anc_state_manager_event_power_off\n");

            ancStateManager_UsbDetachTuningDevice();
            ancStateManager_ExitTuning();

            changeState(anc_state_manager_power_off);
            event_handled = TRUE;
        }
        break;

        case anc_state_manager_event_deactivate_tuning_mode:
        {
            DEBUG_LOG("ancStateManager_HandleEventsInTuningState: anc_state_manager_event_deactivate_tuning_mode\n");

            ancStateManager_UsbDetachTuningDevice();
            ancStateManager_ExitTuning();

            changeState(anc_state_manager_disabled);
            event_handled = TRUE;
        }
        break;

        case anc_state_manager_event_usb_detached_stop_tuning:
        {
            DEBUG_LOG("ancStateManager_HandleEventsInTuningState: anc_state_manager_event_usb_detached_stop_tuning\n");

            ancStateManager_UsbDetachTuningDevice();
            ancStateManager_ExitTuning();

            changeState(anc_state_manager_disabled);
            event_handled = TRUE;
        }
        break;
        
        default:
        break;
    }
    return event_handled;
}

static bool ancStateManager_HandleEventsInAdaptiveAncTuningState(anc_state_manager_event_id_t event)
{
    bool event_handled = FALSE;
    
    switch(event)
    {
        case anc_state_manager_event_usb_enumerated_start_tuning:
        {
            DEBUG_LOG("ancStateManager_HandleEventsInAdaptiveAncTuningState: anc_state_manager_event_usb_enumerated_start_tuning\n");

            kymeraAdaptiveAnc_EnterAdaptiveAncTuning(ancStateManager_GetUsbSampleRate());

            event_handled = TRUE;
        }
        break;

        case anc_state_manager_event_power_off:
        {
            DEBUG_LOG("ancStateManager_HandleEventsInAdaptiveAncTuningState: anc_state_manager_event_power_off\n");

            ancStateManager_UsbDetachTuningDevice();
            ancStateManager_ExitAdaptiveAncTuning();

            changeState(anc_state_manager_power_off);
            event_handled = TRUE;
        }
        break;

        case anc_state_manager_event_deactivate_adaptive_anc_tuning_mode:
        {
            DEBUG_LOG("ancStateManager_HandleEventsInAdaptiveAncTuningState: anc_state_manager_event_deactivate_adaptive_anc_tuning_mode\n");

            ancStateManager_UsbDetachTuningDevice();
            ancStateManager_ExitAdaptiveAncTuning();

            changeState(anc_state_manager_disabled);
            event_handled = TRUE;
        }
        break;

        case anc_state_manager_event_usb_detached_stop_tuning:
        {
            DEBUG_LOG("ancStateManager_HandleEventsInAdaptiveAncTuningState: anc_state_manager_event_usb_detached_stop_tuning\n");

            ancStateManager_UsbDetachTuningDevice();
            ancStateManager_ExitAdaptiveAncTuning();

            changeState(anc_state_manager_disabled);
            event_handled = TRUE;
        }
        break;
        
        default:
        break;
    }
    return event_handled;
}

/******************************************************************************
DESCRIPTION
    Entry point to the ANC State Machine.

RETURNS
    Bool indicating if processing event was successful or not.
*/
static bool ancStateManager_HandleEvent(anc_state_manager_event_id_t event)
{
    bool ret_val = FALSE;

    DEBUG_LOG("ancStateManager_HandleEvent: ANC Handle Event %d in State %d\n", event, anc_data.state);

    switch(anc_data.state)
    {
        case anc_state_manager_uninitialised:
            ret_val = ancStateManager_HandleEventsInUninitialisedState(event);
        break;
        
        case anc_state_manager_power_off:
            ret_val = ancStateManager_HandleEventsInPowerOffState(event);
        break;

        case anc_state_manager_enabled:
            ret_val = ancStateManager_HandleEventsInEnabledState(event);
        break;

        case anc_state_manager_disabled:
            ret_val = ancStateManager_HandleEventsInDisabledState(event);
        break;

        case anc_state_manager_tuning_mode_active:
            ret_val = ancStateManager_HandleEventsInTuningState(event);
        break;

        case anc_state_manager_adaptive_anc_tuning_mode_active:
            ret_val = ancStateManager_HandleEventsInAdaptiveAncTuningState(event);
        break;

        default:
            DEBUG_LOG("ancStateManager_HandleEvent: Unhandled state [%d]\n", anc_data.state);
        break;
    }
    return ret_val;
}

static void ancStateManager_DisableAnc(anc_state_manager_t next_state)
{
    DEBUG_LOG("ancStateManager_DisableAnc");
    /* Disable ANC */
    updateLibState(FALSE, anc_data.requested_mode);

    /* Update state */
    changeState(next_state);

    ancStateManager_ResetUserInitiatedEnable();
    ancStateManager_ResetUserInitiatedModeSwitch();
}

static void ancStateManager_UpdateAncMode(void)
{
    DEBUG_LOG("ancStateManager_UpdateAncMode");
    /* Update the ANC Mode */
    updateLibState(anc_data.requested_enabled, anc_data.requested_mode);
}



/*******************************************************************************
 * All the functions from this point onwards are the ANC module API functions
 * The functions are simply responsible for injecting
 * the correct event into the ANC State Machine, which is then responsible
 * for taking the appropriate action.
 ******************************************************************************/

bool AncStateManager_Init(Task init_task)
{
    UNUSED(init_task);

    /* Initialise the ANC VM Lib */
    if(ancStateManager_HandleEvent(anc_state_manager_event_initialise))
    {
        /* Register with Physical state as observer to know if there are any physical state changes */
        appPhyStateRegisterClient(AncStateManager_GetTask());

        /*Register with Kymera for unsolicited  messaging */
        Kymera_ClientRegister(AncStateManager_GetTask());

        /* Initialisation successful, go ahead with ANC power ON*/
        AncStateManager_PowerOn();
#ifdef ANC_DEFAULT_ON
        /* Set ANC mode to On by default for ANC RDP platform */
        AncStateManager_Enable();
#endif
    }
    return TRUE;
}

void AncStateManager_PowerOn(void)
{
    /* Power On ANC */
    if(!ancStateManager_HandleEvent(anc_state_manager_event_power_on))
    {
        DEBUG_LOG("AncStateManager_PowerOn: Power On ANC failed\n");
    }
}

void AncStateManager_PowerOff(void)
{
    /* Power Off ANC */
    if (!ancStateManager_HandleEvent(anc_state_manager_event_power_off))
    {
        DEBUG_LOG("AncStateManager_PowerOff: Power Off ANC failed\n");
    }
}

void AncStateManager_Enable(void)
{
    /* Enable ANC */
    if (ancStateManager_HandleEvent(anc_state_manager_event_enable))
    {
        ancStateManager_SetUserInitiatedEnable();
    }
    else
    {        
        DEBUG_LOG("AncStateManager_Enable: Enable ANC failed\n");
    }        
}

void AncStateManager_Disable(void)
{
    /* Disable ANC */
    if (!ancStateManager_HandleEvent(anc_state_manager_event_disable))
    {
        DEBUG_LOG("AncStateManager_Disable: Disable ANC failed\n");
    }
}

void AncStateManager_SetMode(anc_mode_t mode)
{
    if (ancStateManager_InternalSetMode(mode))
    {        
        ancStateManager_SetUserInitiatedModeSwitch();
        AancQuietMode_ResetQuietModeData();
    }
    else
    {
        DEBUG_LOG("AncStateManager_SetMode: Set ANC Mode %d failed\n", mode);
    }
}

void AncStateManager_EnterAncTuningMode(void)
{
    if(!ancStateManager_HandleEvent(anc_state_manager_event_activate_anc_tuning_mode))
    {
       DEBUG_LOG("AncStateManager_EnterAncTuningMode: Tuning mode event failed\n");
    }
}

void AncStateManager_ExitAncTuningMode(void)
{
    if(!ancStateManager_HandleEvent(anc_state_manager_event_deactivate_tuning_mode))
    {
       DEBUG_LOG("AncStateManager_ExitAncTuningMode: Tuning mode event failed\n");
    }
}

#ifdef ENABLE_ADAPTIVE_ANC
void AncStateManager_EnterAdaptiveAncTuningMode(void)
{
    if(!ancStateManager_HandleEvent(anc_state_manager_event_activate_adaptive_anc_tuning_mode))
    {
       DEBUG_LOG("AncStateManager_EnterAdaptiveAncTuningMode: Adaptive ANC Tuning mode event failed\n");
    }
}

void AncStateManager_ExitAdaptiveAncTuningMode(void)
{
    if(!ancStateManager_HandleEvent(anc_state_manager_event_deactivate_adaptive_anc_tuning_mode))
    {
       DEBUG_LOG("AncStateManager_ExitAdaptiveAncTuningMode: Adaptive ANC Tuning mode event failed\n");
    }
}

bool AncStateManager_IsAdaptiveAncTuningModeActive(void)
{
    return (anc_data.state == anc_state_manager_adaptive_anc_tuning_mode_active);
}
#endif

void AncStateManager_UpdateAncLeakthroughGain(void)
{
    if(!ancStateManager_HandleEvent(anc_state_manager_event_set_anc_leakthrough_gain))
    {
       DEBUG_LOG("AncStateManager_UpdateAncLeakthroughGain: Set Anc Leakthrough gain event failed\n");
    }
}

bool AncStateManager_IsEnabled(void)
{
    return (anc_data.state == anc_state_manager_enabled);
}

anc_mode_t AncStateManager_GetCurrentMode(void)
{
    anc_state_manager_data_t *anc_sm = GetAncData();
    return anc_sm->current_mode;
}

uint8 AncStateManager_GetNumberOfModes(void)
{
    return anc_data.num_modes;
}

static anc_mode_t ancStateManager_GetNextMode(anc_mode_t anc_mode)
{
    anc_mode++;
    if(anc_mode >= AncStateManager_GetNumberOfModes())
    {
       anc_mode = anc_mode_1;
    }
    return anc_mode;
}

void AncStateManager_SetNextMode(void)
{
    DEBUG_LOG("AncStateManager_SetNextMode cur:%d req:%d", anc_data.current_mode, anc_data.requested_mode);
    anc_data.requested_mode = ancStateManager_GetNextMode(anc_data.current_mode);
    AncStateManager_SetMode(anc_data.requested_mode);
 }

bool AncStateManager_IsTuningModeActive(void)
{
    return (anc_data.state == anc_state_manager_tuning_mode_active);
}

void AncStateManager_ClientRegister(Task client_task)
{
    anc_state_manager_data_t *anc_sm = GetAncData();
    TaskList_AddTask(anc_sm->client_tasks, client_task);
}

void AncStateManager_ClientUnregister(Task client_task)
{
    anc_state_manager_data_t *anc_sm = GetAncData();
    TaskList_RemoveTask(anc_sm->client_tasks, client_task);
}

uint8 AncStateManager_GetAncLeakthroughGain(void)
{
    anc_state_manager_data_t *anc_sm = GetAncData();
    return anc_sm -> anc_leakthrough_gain;
}

void AncStateManager_StoreAncLeakthroughGain(uint8 anc_leakthrough_gain)
{

    if(AncStateManager_GetCurrentMode() != anc_mode_1)
    {
        anc_state_manager_data_t *anc_sm = GetAncData();
        anc_sm -> anc_leakthrough_gain = anc_leakthrough_gain;
    }
}

void AncStateManager_GetAdaptiveAncEnableParams(bool *in_ear, audio_anc_path_id *control_path, adaptive_anc_hw_channel_t *hw_channel, anc_mode_t *current_mode)
{
    *in_ear=ancStateManager_GetInEarStatus();
    *control_path = ancStateManager_GetAncPath();
    *hw_channel = adaptive_anc_hw_channel_0;
    *current_mode = anc_data.current_mode;
}

/*! \brief Interface to enable ANC implicitly */
void AncStateManager_ImplicitEnableReq(void)
{
    DEBUG_LOG_FN_ENTRY("AncStateManager_ImplicitEnableReq");

    MessageSend(AncStateManager_GetTask(), anc_state_manager_event_implicit_enable, NULL);
}

/*! \brief Interface to disable ANC implicitly */
void AncStateManager_ImplicitDisableReq(void)
{
    DEBUG_LOG_FN_ENTRY("AncStateManager_ImplicitDisableReq");

    MessageSend(AncStateManager_GetTask(), anc_state_manager_event_implicit_disable, NULL);
}


#ifdef ANC_TEST_BUILD
void AncStateManager_ResetStateMachine(anc_state_manager_t state)
{
    anc_data.state = state;
}
#endif /* ANC_TEST_BUILD */

#endif /* ENABLE_ANC */
