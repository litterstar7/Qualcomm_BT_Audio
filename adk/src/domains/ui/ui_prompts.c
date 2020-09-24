/*!
\copyright  Copyright (c) 2019 - 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Source file converts System Events to corresponding Audio Prompt UI Events
            by table look-up, using a configuration table passed in by the Application.
            It then plays these Prompts when required using the Kymera audio framework
            Aux path.
*/

#include "ui_log_level.h"
#include "ui_prompts.h"

#include "ui_inputs.h"
#include "pairing.h"
#include "ui.h"
#include "ui_private.h"
#include "av.h"
#include <power_manager.h>

#include <domain_message.h>
#include <logging.h>
#include <panic.h>

#include <stdlib.h>

#include "system_clock.h"

#define DEFAULT_NO_REPEAT_DELAY         D_SEC(5)

ui_prompts_task_data_t the_prompts;

#define PROMPT_NONE                     0xFFFF

#define UI_PROMPTS_WAIT_FOR_PROMPT_COMPLETION 0x1

/*! User interface internal messasges */
enum ui_internal_prompt_messages
{
    /*! Message sent later when a prompt is played. Until this message is delivered
        repeat prompts will not be played */
    UI_INTERNAL_CLEAR_LAST_PROMPT,
    UI_INTERNAL_PROMPT_PLAYBACK_COMPLETED
};
/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_ENUM(ui_internal_prompt_messages)

static bool uiPrompts_GetPromptIndexFromMappingTable(MessageId id, uint16 * prompt_index)
{
    return UiIndicator_GetIndexFromMappingTable(
                the_prompts.sys_event_to_prompt_data_mappings,
                the_prompts.mapping_table_size,
                id,
                prompt_index);
}

static const ui_prompt_data_t * uiPrompts_GetDataForPrompt(uint16 prompt_index)
{
    return &UiIndicator_GetDataForIndex(
                the_prompts.sys_event_to_prompt_data_mappings,
                the_prompts.mapping_table_size,
                prompt_index)->prompt;
}

inline static bool uiPrompt_isNotARepeatPlay(uint16 prompt_index)
{
    return prompt_index != the_prompts.last_prompt_played_index;
}

/*! \brief Play prompt.

    \param prompt_index The prompt to play from the mappings table.
    \param time_to_play The microsecond at which to begin mixing of this audio prompt.
    \param config The prompt configuration data for the prompt to play.
*/
static void uiPrompts_PlayPrompt(uint16 prompt_index, rtime_t time_to_play, const ui_prompt_data_t *config)
{
    DEBUG_LOG("uiPrompts_PlayPrompt index=%d ttp=%d enabled=%d",
              prompt_index, time_to_play, the_prompts.prompt_playback_enabled );

    if (the_prompts.prompt_playback_enabled)
    {
        FILE_INDEX *index = &the_prompts.prompt_file_indexes[prompt_index];

        if (*index == FILE_NONE)
        {
            const char* name = config->filename;
            *index = FileFind(FILE_ROOT, name, strlen(name));
            /* Prompt not found */
            PanicFalse(*index != FILE_NONE);
        }

        MessageSendConditionally(&the_prompts.task, UI_INTERNAL_PROMPT_PLAYBACK_COMPLETED, NULL, ui_GetKymeraResourceLockAddress());

        DEBUG_LOG("uiPrompts_PlayPrompt FILE_INDEX=%08x format=%d rate=%d", *index , config->format, config->rate );

        ui_SetKymeraResourceLock();
        appKymeraPromptPlay(*index, config->format, config->rate, time_to_play,
                            config->interruptible, ui_GetKymeraResourceLockAddress(), UI_KYMERA_RESOURCE_LOCKED);

        if(the_prompts.no_repeat_period_in_ms != 0)
        {
            MessageCancelFirst(&the_prompts.task, UI_INTERNAL_CLEAR_LAST_PROMPT);
            MessageSendLater(&the_prompts.task, UI_INTERNAL_CLEAR_LAST_PROMPT, NULL,
                             the_prompts.no_repeat_period_in_ms);
            the_prompts.last_prompt_played_index = prompt_index;
        }
    }
}

static void uiPrompts_SchedulePromptPlay(uint16 prompt_index)
{
    const ui_prompt_data_t *config = uiPrompts_GetDataForPrompt(prompt_index);

    if (uiPrompt_isNotARepeatPlay(prompt_index) &&
        (config->queueable || (!appKymeraIsTonePlaying() && !ui_IsKymeraResourceLocked())))
    {
        /* Factor in the propagation latency through the various buffers for the aux channel and the time to start the file source */
        rtime_t time_now = SystemClockGetTimerTime();
        rtime_t time_to_play = rtime_add(time_now, UI_SYNC_IND_AUDIO_SS_FIXED_DELAY);

        if(!Kymera_IsReadyForPrompt(config->format, config->rate))
        {
            time_to_play = rtime_add(time_to_play, UI_SYNC_IND_AUDIO_SS_CHAIN_CREATION_DELAY);
        }

        if (!config->local_feedback)
        {
            time_to_play = Ui_RaiseUiEvent(ui_indication_type_audio_prompt, prompt_index, time_to_play);
        }
        uiPrompts_PlayPrompt(prompt_index, time_to_play, config);
    }
}

static bool uiPrompts_IsPromptMandatory(uint16 prompt_index)
{
    ui_event_indicator_table_t prompt_to_play = the_prompts.sys_event_to_prompt_data_mappings[prompt_index];
    bool indicate_on_shutdown = prompt_to_play.await_indication_completion;
    return indicate_on_shutdown;
}

static void uiPrompts_HandleInternalPrompt(Task task, MessageId prompt_index, Message message)
{
    UNUSED(task);
    UNUSED(message);

    DEBUG_LOG("uiPrompts_HandleInternalPrompt index=%u", prompt_index);

    /* Mandatory prompts (e.g. indicating shutdown) should always be played,
       regardless of whether we are rendering indications based on the current
       device topology role and any other gating factors. */
    if (the_prompts.generate_ui_events || uiPrompts_IsPromptMandatory(prompt_index))
    {
        uiPrompts_SchedulePromptPlay(prompt_index);
    }
}

static void uiPrompts_HandleShutdownRequest(void)
{
    int32 time = 0;
    uint16 power_off_prompt_index = 0;

    bool prompt_is_currently_playing = MessagePendingFirst(&the_prompts.task, UI_INTERNAL_PROMPT_PLAYBACK_COMPLETED, &time);
    bool power_off_prompt_configured = uiPrompts_GetPromptIndexFromMappingTable(POWER_OFF, &power_off_prompt_index);

    bool mandatory_prompt_is_playing = prompt_is_currently_playing && uiPrompts_IsPromptMandatory(the_prompts.last_prompt_played_index);
    bool mandatory_power_off_prompt_reqd = power_off_prompt_configured && uiPrompts_IsPromptMandatory(power_off_prompt_index);

    if (the_prompts.prompt_playback_enabled &&
        (mandatory_prompt_is_playing || mandatory_power_off_prompt_reqd))
    {
        /* Await completion of prompts that have started playing or pending power off prompt (if mandatory). */
        the_prompts.indicate_when_power_shutdown_prepared = TRUE;
    }
    else
    {
        /* Otherwise shutdown immediately */
        appPowerShutdownPrepareResponse(&the_prompts.task);
    }
}

static void uiPrompts_HandleMessage(Task task, MessageId id, Message message)
{
    uint16 prompt_index = 0;

    UNUSED(task);
    UNUSED(message);

    DEBUG_LOG("uiPrompts_HandleMessage Id=%04x", id);
;
    if (uiPrompts_GetPromptIndexFromMappingTable(id, &prompt_index))
    {
        Task t = &the_prompts.prompt_task;
        if (MessagesPendingForTask(t, NULL) < UI_PROMPTS_MAX_QUEUE_SIZE || uiPrompts_IsPromptMandatory(prompt_index))
        {
            MessageSendConditionally(t, prompt_index, NULL, ui_GetKymeraResourceLockAddress());
        }
        else
        {
            DEBUG_LOG("uiPrompts_HandleMessage not queuing id=%04x", id);
        }
    }
    else if (id == UI_INTERNAL_CLEAR_LAST_PROMPT)
    {
        DEBUG_LOG("UI_INTERNAL_CLEAR_LAST_PROMPT");
        the_prompts.last_prompt_played_index = PROMPT_NONE;
    }
    else if (id == UI_INTERNAL_PROMPT_PLAYBACK_COMPLETED)
    {
        DEBUG_LOG("UI_INTERNAL_PROMPT_PLAYBACK_COMPLETED indicate=%d", the_prompts.indicate_when_power_shutdown_prepared);
        if (the_prompts.indicate_when_power_shutdown_prepared)
        {
            appPowerShutdownPrepareResponse(&the_prompts.task);
        }
    }
    else if (id == APP_POWER_SHUTDOWN_PREPARE_IND)
    {
        uiPrompts_HandleShutdownRequest();
    }
    else if (id == APP_POWER_SLEEP_PREPARE_IND)
    {
        appPowerSleepPrepareResponse(&the_prompts.task);
    }
    else
    {
        // Ignore message
    }
}

void UiPrompts_PrepareForPrompt(MessageId sys_event)
{
    uint16 prompt_index = 0;
    bool prompt_found = uiPrompts_GetPromptIndexFromMappingTable(sys_event, &prompt_index);
    if (prompt_found)
    {
        const ui_prompt_data_t *config = uiPrompts_GetDataForPrompt(prompt_index);
        Ui_RaiseUiEvent(ui_indication_type_prepare_for_prompt, prompt_index, 0);
        Kymera_PrepareForPrompt(config->format, config->rate);
    }
}

/*! \brief brief Set/reset play_prompt flag. This is flag is used to check if prompts
  can be played or not. Application will set and reset the flag. Scenarios like earbud
  is in ear or not and etc.

    \param play_prompt If TRUE, prompt can be played, if FALSE, the prompt can not be
    played.
*/
void UiPrompts_SetPromptPlaybackEnabled(bool play_prompt)
{
    the_prompts.prompt_playback_enabled = play_prompt;
}

Task UiPrompts_GetUiPromptsTask(void)
{
    return &the_prompts.task;
}

void UiPrompts_SetPromptConfiguration(const ui_event_indicator_table_t *table, uint8 size)
{
    the_prompts.sys_event_to_prompt_data_mappings = table;
    the_prompts.mapping_table_size = size;

    the_prompts.prompt_file_indexes = PanicUnlessMalloc(size*sizeof(FILE_INDEX));
    memset(the_prompts.prompt_file_indexes, FILE_NONE, size*sizeof(FILE_INDEX));

    UiIndicator_RegisterInterestInConfiguredSystemEvents(
                the_prompts.sys_event_to_prompt_data_mappings,
                the_prompts.mapping_table_size,
                &the_prompts.task);
}

void UiPrompts_SetNoRepeatPeriod(const Delay no_repeat_period_in_ms)
{
    the_prompts.no_repeat_period_in_ms = no_repeat_period_in_ms;
}

void UiPrompts_NotifyUiIndication(uint16 prompt_index, rtime_t time_to_play)
{
    const ui_prompt_data_t *config = uiPrompts_GetDataForPrompt(prompt_index);
    uiPrompts_PlayPrompt(prompt_index, time_to_play, config);
}

void UiPrompts_NotifyUiPrepareIndication(uint16 prompt_index)
{
     const ui_prompt_data_t *config = uiPrompts_GetDataForPrompt(prompt_index);
     Kymera_PrepareForPrompt(config->format, config->rate);
}

/*! brief Initialise Ui prompts module */
bool UiPrompts_Init(Task init_task)
{
    UNUSED(init_task);

    DEBUG_LOG("UiPrompts_Init");

    memset(&the_prompts, 0, sizeof(ui_prompts_task_data_t));

    the_prompts.last_prompt_played_index = PROMPT_NONE;
    the_prompts.task.handler = uiPrompts_HandleMessage;
    the_prompts.prompt_task.handler = uiPrompts_HandleInternalPrompt;
    the_prompts.no_repeat_period_in_ms = DEFAULT_NO_REPEAT_DELAY;
    the_prompts.generate_ui_events = TRUE;
    the_prompts.prompt_playback_enabled = FALSE;

    return TRUE;
}


/*! brief de-initialise Ui prompts module */
bool UiPrompts_DeInit(void)
{
    DEBUG_LOG("UiPrompts_DeInit");

    the_prompts.sys_event_to_prompt_data_mappings = NULL;
    the_prompts.mapping_table_size = 0;

    free(the_prompts.prompt_file_indexes);
    the_prompts.prompt_file_indexes = NULL;

    return TRUE;
}

void UiPrompts_GenerateUiEvents(bool generate)
{
    the_prompts.generate_ui_events = generate;
}
