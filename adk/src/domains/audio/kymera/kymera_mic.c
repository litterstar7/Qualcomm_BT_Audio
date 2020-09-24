/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Kymera module to manage MIC connections
*/

#include "kymera_mic_if.h"
#include "kymera_aec.h"
#include "kymera_mic_resampler.h"
#include "kymera_private.h"
#include "kymera_splitter.h"

#define CVC_FRAME_IN_US (7500)
#define MIN_SAMPLE_RATE_IN_KHZ (16)
#define MAX_SAMPLE_RATE_IN_KHZ (32)
#define MAX_CVC_FRAME_SIZE ((CVC_FRAME_IN_US * MAX_SAMPLE_RATE_IN_KHZ) / 1000)

#define MIC_PATH_TRANSFORM_SIZE (1024)
#define AEC_PATH_TRANSFORM_SIZE ((MAX_CVC_FRAME_SIZE / 2) + MIC_PATH_TRANSFORM_SIZE)

/*! Registration array for all available users */
typedef struct
{
    unsigned nr_entries;
    const mic_registry_per_user_t* *entry;
} mic_registry_t;

typedef struct
{
    aec_usecase_t aec_usecase;
    uint32 ttp_delay;
} aec_ref_user_config_t;

typedef void (* SourceFunction) (Source *array, unsigned length_of_array);

static const splitter_config_t aec_splitter_config =
{
    .transform_size_in_words = AEC_PATH_TRANSFORM_SIZE,
    .data_format = operator_data_format_pcm,
};

static const splitter_config_t mic_splitter_config =
{
    .transform_size_in_words = MIC_PATH_TRANSFORM_SIZE,
    .data_format = operator_data_format_pcm,
};

static const struct
{
    mic_users_t mic_users;
    unsigned leakthrough_enabled:1;
    aec_ref_user_config_t config;
} aec_usecase_map[] =
{
    {mic_user_va, TRUE, {aec_usecase_va_leakthrough, VA_MIC_TTP_LATENCY}},
    {mic_user_va, FALSE, {aec_usecase_voice_assistant, VA_MIC_TTP_LATENCY}},
};

static struct
{
    mic_registry_t registry;
    splitter_handle_t aec_splitter;
    splitter_handle_t mic_splitter;
    uint32 mic_sample_rate;
    uint8 sleep_count;
    mic_users_t current_users;
    mic_users_t stream_map[MAX_NUM_OF_CONCURRENT_MIC_USERS];
    unsigned leakthrough_enabled:1;
} state =
{
    {0, NULL},
    NULL,
    NULL,
    0,
    0,
    mic_user_none,
    {mic_user_none},
    FALSE,
};

static bool kymera_IsMicConcurrencyEnabled(void)
{
    return Kymera_GetChainConfigs()->chain_mic_resampler_config != NULL;
}

static const aec_ref_user_config_t * kymera_GetAecRefUserConfig(mic_users_t users)
{
    unsigned i;

    for(i = 0; i < ARRAY_DIM(aec_usecase_map); i++)
    {
        if ((aec_usecase_map[i].mic_users == users) && (aec_usecase_map[i].leakthrough_enabled == state.leakthrough_enabled))
        {
            return &aec_usecase_map[i].config;
        }
    }

    return NULL;
}

static void kymera_SetAecRefUseCase(mic_users_t users)
{
    aec_usecase_t aec_usecase = aec_usecase_default;
    const aec_ref_user_config_t *config = kymera_GetAecRefUserConfig(users);

    if (config)
    {
        aec_usecase = config->aec_usecase;
    }
    Kymera_SetAecUseCase(aec_usecase);
}

static const mic_registry_per_user_t * kymera_GetRegistryEntry(mic_users_t user)
{
    unsigned i;
    for(i = 0; i < state.registry.nr_entries; i++)
    {
        if (state.registry.entry[i]->user == user)
        {
            return state.registry.entry[i];
        }
    }
    DEBUG_LOG_ERROR("kymera_GetRegistryEntry: User not found");
    Panic();
    return NULL;
}

static bool kymera_IsCurrentUser(mic_users_t user)
{
    return (state.current_users & user) != 0;
}

static uint8 kymera_GetStreamIndex(mic_users_t user)
{
    uint8 stream_index;

    for(stream_index = 0; stream_index < ARRAY_DIM(state.stream_map); stream_index++)
    {
        if (state.stream_map[stream_index] == user)
        {
            return stream_index;
        }
    }

    DEBUG_LOG_ERROR("kymera_GetStreamIndex: No stream entry for user enum:mic_users_t:0x%X", user);
    Panic();
    return 0;
}

static void kymera_ReplaceEntryInStreamMap(mic_users_t old_entry, mic_users_t new_entry)
{
    uint8 stream_index;

    for(stream_index = 0; stream_index < ARRAY_DIM(state.stream_map); stream_index++)
    {
        if (state.stream_map[stream_index] == old_entry)
        {
            state.stream_map[stream_index] = new_entry;
            return;
        }
    }

    DEBUG_LOG_ERROR("kymera_ReplaceEntryInStreamMap: Couldn't find entry enum:mic_users_t:0x%X", old_entry);
    Panic();
}

static void kymera_AddMicUser(mic_users_t user)
{
    PanicFalse(kymera_IsCurrentUser(user) == FALSE);
    state.current_users |= user;
    kymera_SetAecRefUseCase(state.current_users);
    kymera_ReplaceEntryInStreamMap(mic_user_none, user);
}

static void kymera_RemoveUser(mic_users_t user)
{
    PanicFalse(kymera_IsCurrentUser(user));
    state.current_users &= ~user;
    kymera_SetAecRefUseCase(state.current_users);
    kymera_ReplaceEntryInStreamMap(user, mic_user_none);
}

static void kymera_SynchroniseMics(uint8 num_of_mics, const Source *mic_sources)
{
    unsigned i;
    for(i = 1; i < num_of_mics; i++)
    {
        SourceSynchronise(mic_sources[i-1], mic_sources[i]);
    }
}

static bool kymera_AreMicsInUse(uint8 num_of_mics, const microphone_number_t *mic_ids)
{
    unsigned i;

    for(i = 0; i < num_of_mics; i++)
    {
        if (Microphones_GetMicrophoneSource(mic_ids[i]) == NULL)
            return FALSE;
    }

    return TRUE;
}

static void kymera_TurnOnMics(uint32 sample_rate, uint8 num_of_mics, const microphone_number_t *mic_ids, Source *mic_sources)
{
    unsigned i;
    bool create_mics = (kymera_AreMicsInUse(num_of_mics, mic_ids) == FALSE);

    if (create_mics)
    {
        state.mic_sample_rate = sample_rate;
    }

    for(i = 0; i < num_of_mics; i++)
    {
        // Simply increases number of users if mic already created
        mic_sources[i] = Microphones_TurnOnMicrophone(mic_ids[i], state.mic_sample_rate, non_exclusive_user);
    }

    if (create_mics)
    {
        kymera_SynchroniseMics(num_of_mics, mic_sources);
    }
}

static void kymera_TurnOffMics(uint8 num_of_mics, const microphone_number_t *mic_types)
{
    unsigned i;

    for(i = 0; i < num_of_mics; i++)
    {
        Microphones_TurnOffMicrophone(mic_types[i], non_exclusive_user);
    }
}

static void kymera_PopulateAecConnectParams(uint8 num_of_mics, const Sink *mic_sinks, const Source *mic_sources,
                                            Sink aec_ref_sink, aec_connect_audio_input_t *aec_params)
{
    aec_params->reference_output = aec_ref_sink;

    switch (num_of_mics)
    {
        case 3:
            aec_params->mic_input_3 = mic_sources[2];
            aec_params->mic_output_3 = mic_sinks[2];
        // Fallthrough
        case 2:
            aec_params->mic_input_2 = mic_sources[1];
            aec_params->mic_output_2 = mic_sinks[1];
        // Fallthrough
        case 1:
            aec_params->mic_input_1 = mic_sources[0];
            aec_params->mic_output_1 = mic_sinks[0];
            break;
        default:
            DEBUG_LOG_ERROR("kymera_PopulateAecConnectParams: Unsupported number of mics = %d", num_of_mics);
            Panic();
    }
}

static void kymera_PopulateAecConfig(uint32 sample_rate, aec_audio_config_t *aec_config)
{
    const aec_ref_user_config_t *config = kymera_GetAecRefUserConfig(state.current_users);

    if (config)
    {
        aec_config->ttp_delay = config->ttp_delay;
    }
    aec_config->mic_sample_rate = sample_rate;
}

static void kymera_ConnectUserDirectlyToAec(uint8 num_of_mics, const Sink *mic_sinks, const Source *mic_sources,
                                            Sink aec_ref_sink)
{
    aec_connect_audio_input_t connect_params = {0};
    aec_audio_config_t config = {0};

    kymera_PopulateAecConnectParams(num_of_mics, mic_sinks, mic_sources, aec_ref_sink, &connect_params);
    kymera_PopulateAecConfig(state.mic_sample_rate, &config);
    Kymera_ConnectAudioInputToAec(&connect_params, &config);
}

static void kymera_ConnectUserToConcurrencyChain(uint8 stream_index, uint8 num_of_mics, const Sink *mic_sinks,
                                                 uint32 sample_rate, Sink aec_ref_sink)
{
    unsigned i;
    bool use_resampler = (sample_rate != state.mic_sample_rate);
    Sink mics[MAX_NUM_OF_CONCURRENT_MICS] = {0};

    if (use_resampler)
    {
        Kymera_MicResamplerCreate(stream_index, state.mic_sample_rate, sample_rate);
        for(i = 0; i < num_of_mics; i++)
        {
            PanicNull(StreamConnect(Kymera_MicResamplerGetMicOutput(stream_index, i), mic_sinks[i]));
            mics[i] = Kymera_MicResamplerGetMicInput(stream_index, i);
        }
        if (aec_ref_sink)
        {
            PanicNull(StreamConnect(Kymera_MicResamplerGetAecOutput(stream_index), aec_ref_sink));
            aec_ref_sink = Kymera_MicResamplerGetAecInput(stream_index);
        }
    }
    else
    {
        memcpy(mics, mic_sinks, num_of_mics * sizeof(mics[0]));
    }

    if (aec_ref_sink)
    {
        Kymera_SplitterConnectToOutputStream(&state.aec_splitter, stream_index, &aec_ref_sink);
    }

    Kymera_SplitterConnectToOutputStream(&state.mic_splitter, stream_index, mics);

    if (use_resampler)
    {
        Kymera_MicResamplerStart(stream_index);
    }
}

static void kymera_ConnectSplitterChainToAec(uint8 num_of_mics, const Source *mic_sources)
{
    unsigned i;
    Sink mic_sinks[MAX_NUM_OF_CONCURRENT_MICS] = {NULL};
    Sink aec_sink = Kymera_SplitterGetInput(&state.aec_splitter, 0);
    aec_connect_audio_input_t connect_params = {0};
    aec_audio_config_t config = {0};

    for(i = 0; i < num_of_mics; i++)
    {
        mic_sinks[i] = Kymera_SplitterGetInput(&state.mic_splitter, i);
    }

    kymera_PopulateAecConnectParams(num_of_mics, mic_sinks, mic_sources, aec_sink, &connect_params);
    kymera_PopulateAecConfig(state.mic_sample_rate, &config);
    Kymera_ConnectAudioInputToAec(&connect_params, &config);
}

static void kymera_ConnectUserViaConcurrencyChain(mic_users_t user, uint8 num_of_mics, const Sink *mic_sinks,
                                                  const Source *mic_sources, uint32 sample_rate, Sink aec_ref_sink)
{
    uint8 stream_index = kymera_GetStreamIndex(user);

    if (state.mic_splitter == NULL)
    {
        state.mic_splitter = Kymera_SplitterCreate(MAX_NUM_OF_CONCURRENT_MIC_USERS, num_of_mics, &mic_splitter_config);
        state.aec_splitter = Kymera_SplitterCreate(MAX_NUM_OF_CONCURRENT_MIC_USERS, 1, &aec_splitter_config);
        kymera_ConnectSplitterChainToAec(num_of_mics, mic_sources);
    }

    kymera_ConnectUserToConcurrencyChain(stream_index, num_of_mics, mic_sinks, sample_rate, aec_ref_sink);

    Kymera_SplitterStartOutputStream(&state.mic_splitter, stream_index);
    Kymera_SplitterStartOutputStream(&state.aec_splitter, stream_index);
}

static void kymera_DisconnectUserFromConcurrencyChain(mic_users_t user)
{
    uint8 stream_index = kymera_GetStreamIndex(user);

    Kymera_SplitterDisconnectFromOutputStream(&state.aec_splitter, stream_index);
    Kymera_SplitterDisconnectFromOutputStream(&state.mic_splitter, stream_index);

    if (Kymera_MicResamplerIsCreated(stream_index))
    {
        Kymera_MicResamplerDestroy(stream_index);
    }

    if (state.current_users == user)
    {
        // Destroy splitter and disconnect from AEC since there are no other users
        Kymera_SplitterDestroy(&state.aec_splitter);
        Kymera_SplitterDestroy(&state.mic_splitter);
    }
}

static void kymera_UserGetConnectionParameters(mic_users_t user, mic_connect_params_t *mic_params, microphone_number_t *mic_ids, Sink *mic_sinks, Sink *aec_ref_sink)
{
    const mic_registry_per_user_t * reg_entry;
    reg_entry = kymera_GetRegistryEntry(user);
    reg_entry->callbacks->MicGetConnectionParameters(mic_params, mic_ids, mic_sinks, aec_ref_sink);
}

static void kymera_AddToListOfMics(microphone_number_t *mic_ids, Sink *mic_sinks, uint8 *idx, microphone_number_t new_mic_id, Sink new_mic_sink)
{
    PanicFalse(*idx < MAX_NUM_OF_CONCURRENT_MICS);
    mic_ids[*idx] = new_mic_id;
    mic_sinks[*idx] = new_mic_sink;
    (*idx)++;
}

static void kymera_AddReconnectMicsToOrderedList(microphone_number_t *current_mic_ids, Sink *current_mic_sinks, uint8 current_num_of_mics,
                                                microphone_number_t *combined_mic_ids, Sink *combined_mic_sinks, uint8 *combined_num_of_mics)
{
    /* Todo: Special handling for LeakThrough mic in this function */
    unsigned i, j;
    bool already_available;

    for(i = 0; i < current_num_of_mics; i++)
    {
        already_available = FALSE;
        for(j = 0; j < *combined_num_of_mics; j++)
        {
            if (current_mic_ids[i] == combined_mic_ids[j])
            {
                already_available = TRUE;
                break;
            }
        }
        if((!already_available) && (j < current_num_of_mics))
        {
            kymera_AddToListOfMics(combined_mic_ids, combined_mic_sinks, combined_num_of_mics,
                                   current_mic_ids[i], current_mic_sinks[i]);
        }
    }
}

static void kymera_AddMandatoryMicsFromCurrentUser(mic_users_t user, microphone_number_t *mic_ids, Sink *mic_sinks, uint8 *num_of_mics)
{
    unsigned i, j;
    bool already_available;
    const mic_registry_per_user_t * reg_entry;
    reg_entry = kymera_GetRegistryEntry(user);

    for(i = 0; i < reg_entry->num_of_mandatory_mics; i++)
    {
        PanicFalse(reg_entry->mandatory_mic_ids[i] != microphone_none);
        already_available = FALSE;
        for(j = 0; j < *num_of_mics; j++)
        {
            if(mic_ids[j] == (microphone_number_t)reg_entry->mandatory_mic_ids[i])
            {
                already_available = TRUE;
                break;
            }
        }
        if(!already_available)
        {
            kymera_AddToListOfMics(mic_ids, mic_sinks, num_of_mics, reg_entry->mandatory_mic_ids[i], NULL);
        }
    }
}

static void kymera_AddMandatoryMicsFromAllUsers(microphone_number_t *mic_ids, Sink *mic_sinks, uint8 *num_of_mics)
{
    uint8 user_index;
    mic_users_t current_user;

    for(user_index = 0; user_index < state.registry.nr_entries; user_index++)
    {
        current_user = state.registry.entry[user_index]->user;
        kymera_AddMandatoryMicsFromCurrentUser(current_user, mic_ids, mic_sinks, num_of_mics);
    }
}

static void kymera_PopulateMicSources(mic_users_t user, const mic_connect_params_t *mic_params, mic_users_t reconnect_users,
                                      microphone_number_t *combined_mic_ids, Sink *combined_mic_sinks, uint8 *combined_num_of_mics)
{
    uint8 user_index;
    mic_users_t current_user;
    mic_connect_params_t current_mic_params;
    microphone_number_t current_mic_ids[MAX_NUM_OF_CONCURRENT_MICS] = {0};
    Sink current_mic_sinks[MAX_NUM_OF_CONCURRENT_MICS] = {NULL};
    current_mic_params.connections.mic_ids = current_mic_ids;
    current_mic_params.connections.mic_sinks = current_mic_sinks;
    current_mic_params.connections.num_of_mics = 0;
    Sink aec_ref_sink;

    if (user != mic_user_none)
    {
        /* Populate with mic params from user if available */
        memcpy(combined_mic_ids, mic_params->connections.mic_ids, mic_params->connections.num_of_mics*sizeof(*combined_mic_ids));
        memcpy(combined_mic_sinks, mic_params->connections.mic_sinks, mic_params->connections.num_of_mics*sizeof(*combined_mic_sinks));
        *combined_num_of_mics = mic_params->connections.num_of_mics;
    }

    if (reconnect_users != mic_user_none)
    {
        /* Collect mic params from all users that want to be reconnected */
        for(user_index = 0; user_index < state.registry.nr_entries; user_index++)
        {
            current_user = state.registry.entry[user_index]->user;
            if (((reconnect_users & current_user) != 0) && (current_user != user))
            {
                kymera_UserGetConnectionParameters(current_user, &current_mic_params, current_mic_ids, current_mic_sinks, &aec_ref_sink);
                kymera_AddReconnectMicsToOrderedList(current_mic_ids, current_mic_sinks, current_mic_params.connections.num_of_mics,
                                                    combined_mic_ids, combined_mic_sinks, combined_num_of_mics);
            }
        }
    }
    kymera_AddMandatoryMicsFromAllUsers(combined_mic_ids, combined_mic_sinks, combined_num_of_mics);
}

static bool kymera_UserDisconnectIndication(mic_users_t user, mic_disconnect_info_t *info)
{
    bool want_to_reconnect;
    const mic_registry_per_user_t * reg_entry;
    reg_entry = kymera_GetRegistryEntry(user);
    want_to_reconnect = reg_entry->callbacks->MicDisconnectIndication(info);
    return want_to_reconnect;
}

static void kymera_UserReconnectedIndication(mic_users_t user)
{
    const mic_registry_per_user_t * reg_entry;
    reg_entry = kymera_GetRegistryEntry(user);
    reg_entry->callbacks->MicReconnectedIndication();
}

static void kymera_SendReconnectedIndicationToAllUsers(mic_users_t reconnected_users)
{
    uint8 user_index;
    mic_users_t current_user;

    if (reconnected_users != mic_user_none)
    {
        for(user_index = 0; user_index < state.registry.nr_entries; user_index++)
        {
            current_user = state.registry.entry[user_index]->user;
            if ((reconnected_users & current_user) != 0)
            {
                kymera_UserReconnectedIndication(current_user);
                DEBUG_LOG("kymera_SendReconnectedIndicationToAllUsers: user enum:mic_users_t:0x%X reconnected", current_user);
            }
        }
    }
}

static void kymera_ReconnectAllUsers(mic_users_t reconnect_users, Source *mic_sources)
{
    /* Reconnect all others */
    unsigned user_index;
    mic_users_t current_user;
    mic_connect_params_t local_mic_params;
    microphone_number_t mic_ids[MAX_NUM_OF_CONCURRENT_MICS] = {0};
    Sink mic_sinks[MAX_NUM_OF_CONCURRENT_MICS] = {NULL};
    local_mic_params.connections.mic_ids = mic_ids;
    local_mic_params.connections.mic_sinks = mic_sinks;
    Sink aec_ref_sink;

    if (reconnect_users != mic_user_none)
    {
        for(user_index = 0; user_index < state.registry.nr_entries; user_index++)
        {
            current_user = state.registry.entry[user_index]->user;
            if ((reconnect_users & current_user) != 0)
            {
                kymera_UserGetConnectionParameters(current_user, &local_mic_params, mic_ids, mic_sinks, &aec_ref_sink);
                kymera_AddMicUser(current_user);
                kymera_ConnectUserViaConcurrencyChain(current_user, local_mic_params.connections.num_of_mics,
                                                      local_mic_params.connections.mic_sinks, mic_sources,
                                                      local_mic_params.sample_rate, aec_ref_sink);
            }
        }
    }
}

static void kymera_ConnectUserToMics(mic_users_t user, const mic_connect_params_t *mic_params, Sink aec_ref_sink, mic_users_t reconnect_users)
{
    uint8 num_of_mics =  mic_params->connections.num_of_mics;
    Source mic_sources[MAX_NUM_OF_CONCURRENT_MICS] = {NULL};
    uint32 mic_sample_rate;
    mic_connect_params_t local_mic_params;
    microphone_number_t local_mic_ids[MAX_NUM_OF_CONCURRENT_MICS] = {0};
    Sink local_mic_sinks[MAX_NUM_OF_CONCURRENT_MICS] = {NULL};
    local_mic_params.connections.mic_ids = local_mic_ids;
    local_mic_params.connections.mic_sinks = local_mic_sinks;
    local_mic_params.connections.num_of_mics = 0;

    if (kymera_IsMicConcurrencyEnabled())
    {
        mic_sample_rate = MAX(mic_params->sample_rate, (MIN_SAMPLE_RATE_IN_KHZ * 1000));
        kymera_PopulateMicSources(user, mic_params, reconnect_users, local_mic_ids, local_mic_sinks, &local_mic_params.connections.num_of_mics);
        kymera_TurnOnMics(mic_sample_rate, local_mic_params.connections.num_of_mics,
                          local_mic_params.connections.mic_ids, mic_sources);

        /* Connect new user first */
        kymera_AddMicUser(user);
        kymera_ConnectUserViaConcurrencyChain(user, num_of_mics, mic_params->connections.mic_sinks, mic_sources,
                                              mic_params->sample_rate, aec_ref_sink);

        kymera_ReconnectAllUsers(reconnect_users, mic_sources);
        kymera_SendReconnectedIndicationToAllUsers(reconnect_users);
    }
    else
    {
        kymera_TurnOnMics(mic_params->sample_rate, num_of_mics, mic_params->connections.mic_ids, mic_sources);
        kymera_ConnectUserDirectlyToAec(num_of_mics, mic_params->connections.mic_sinks, mic_sources, aec_ref_sink);
    }
}

static void kymera_DisconnectUserFromMics(mic_users_t user, const mic_connect_params_t *mic_params,
                                          bool disconnect_mics, mic_users_t reconnect_users)
{
    mic_connect_params_t local_mic_params;
    microphone_number_t local_mic_ids[MAX_NUM_OF_CONCURRENT_MICS] = {0};
    Sink local_mic_sinks[MAX_NUM_OF_CONCURRENT_MICS] = {NULL};
    local_mic_params.connections.mic_ids = local_mic_ids;
    local_mic_params.connections.mic_sinks = local_mic_sinks;
    local_mic_params.connections.num_of_mics = 0;

    if (kymera_IsMicConcurrencyEnabled())
    {
        kymera_DisconnectUserFromConcurrencyChain(user);

        if (disconnect_mics)
        {
            Kymera_DisconnectAudioInputFromAec();
            kymera_PopulateMicSources(user, mic_params, reconnect_users, local_mic_ids, local_mic_sinks, &local_mic_params.connections.num_of_mics);
            kymera_TurnOffMics(local_mic_params.connections.num_of_mics, local_mic_params.connections.mic_ids);
        }
    }
    else
    {
        Kymera_DisconnectAudioInputFromAec();
        kymera_TurnOffMics(mic_params->connections.num_of_mics, mic_params->connections.mic_ids);
    }
}

static mic_users_t kymera_InformUsersAboutDisconnection(mic_users_t user, mic_disconnect_info_t *info)
{
    uint8 user_index;
    mic_users_t current_user;
    bool reconnect_request;
    mic_users_t reconnect_users = 0;

    /* Inform active users about disconnection */
    for(user_index = 0; user_index < state.registry.nr_entries; user_index++)
    {
        current_user = state.registry.entry[user_index]->user;
        if((kymera_IsCurrentUser(current_user)) && (current_user != user))
        {
            /* Active user found -> send disconnect indication */
            reconnect_request = kymera_UserDisconnectIndication(current_user, info);
            if (reconnect_request)
            {
                /* Collect which user wants to be reconnected */
                reconnect_users |= current_user;
            }
            DEBUG_LOG("kymera_InformUsersAboutDisconnection: enum:mic_users_t:0x%X enum:mic_event_t:%d reconnect_request %d",
                      current_user, info->event, reconnect_request);
        }
    }
    return reconnect_users;
}

static disconnect_strategy_t kymera_GetDisconnectStrategyForRemainingUsers(mic_users_t user)
{
    uint8 user_index;
    mic_users_t current_user;
    disconnect_strategy_t strategy = disconnect_strategy_interruptible;

    for(user_index = 0; user_index < state.registry.nr_entries; user_index++)
    {
        current_user = state.registry.entry[user_index]->user;
        if((kymera_IsCurrentUser(current_user)) && (current_user != user))
        {
            strategy = state.registry.entry[user_index]->disconnect_strategy;
            DEBUG_LOG("kymera_GetDisconnectStrategyForRemainingUsers: enum:disconnect_strategy_t:%d", strategy);
            if (strategy == disconnect_strategy_non_interruptible)
            {
                return strategy;
            }
        }
    }
    return strategy;
}

static void kymera_DisconnectAllUsers(void)
{
    uint8 user_index;
    mic_users_t current_user;
    mic_connect_params_t current_mic_params;
    microphone_number_t current_mic_ids[MAX_NUM_OF_CONCURRENT_MICS] = {0};
    Sink current_mic_sinks[MAX_NUM_OF_CONCURRENT_MICS] = {NULL};
    current_mic_params.connections.mic_ids = current_mic_ids;
    current_mic_params.connections.mic_sinks = current_mic_sinks;
    Sink aec_ref_sink;

    for(user_index = 0; user_index < state.registry.nr_entries; user_index++)
    {
        current_user = state.registry.entry[user_index]->user;
        if(kymera_IsCurrentUser(current_user))
        {
            kymera_UserGetConnectionParameters(current_user, &current_mic_params, current_mic_ids, current_mic_sinks, &aec_ref_sink);
            kymera_DisconnectUserFromMics(current_user, &current_mic_params, TRUE, microphone_none);
            kymera_RemoveUser(current_user);
            DEBUG_LOG("kymera_DisconnectAllUsers: enum:mic_users_t:0x%X", current_user);
        }
    }
}

static void kymera_PrepareForConnection(mic_users_t new_user, const mic_connect_params_t *mic_params, mic_users_t *reconnect_users)
{
    mic_disconnect_info_t info = {0};

    if (state.current_users == mic_user_none)
        return;

    /* If concurrency is enabled:
           1) Mics in use must include all mics requested (you can only sync mics and connect to AEC REF once)
           2) Sample rate requested cannot be higher than sample rate of mics in use
    */
    if (kymera_IsMicConcurrencyEnabled())
    {
        info.user = new_user;
        info.event = 0;
        if (state.mic_sample_rate < mic_params->sample_rate)
        {
            info.event |= mic_event_higher_sample_rate;
        }
        if (!kymera_AreMicsInUse(mic_params->connections.num_of_mics, mic_params->connections.mic_ids))
        {
            info.event |= mic_event_extra_mic;
        }
        if (info.event == 0)
        {
            return;
        }

        /* Conflicting mic params detected */
        *reconnect_users = kymera_InformUsersAboutDisconnection(new_user, &info);
        kymera_DisconnectAllUsers();
    }
}

static bool kymera_PrepareForDisconnection(mic_users_t user, mic_users_t *reconnect_users)
{
    mic_disconnect_info_t info = {0};
    disconnect_strategy_t strategy;
    PanicFalse(kymera_IsCurrentUser(user));

    /* User to be disconnected is the only user -> disconnect mics */
    if (state.current_users == user)
        return TRUE;

    strategy = kymera_GetDisconnectStrategyForRemainingUsers(user);
    if (strategy == disconnect_strategy_interruptible)
    {
        info.user = user;
        info.event = mic_event_disconnecting;
        *reconnect_users = kymera_InformUsersAboutDisconnection(user, &info);
        return TRUE;
    }
    /* disconnect_strategy_non_interruptible: Do not disconnect the mics */
    return FALSE;
}

static void kymera_PreserveSources(Source *array, unsigned length_of_array)
{
    PanicFalse(OperatorFrameworkPreserve(0, NULL, length_of_array, array, 0, NULL));
}

static void kymera_ReleaseSources(Source *array, unsigned length_of_array)
{
    PanicFalse(OperatorFrameworkRelease(0, NULL, length_of_array, array, 0, NULL));
}

static void kymera_RunOnAllMics(SourceFunction function)
{
    microphone_number_t mic_id, number_of_mics = 0;
    Source source, mics[MAX_NUM_OF_CONCURRENT_MICS] = {NULL};

    for(mic_id = microphone_1; mic_id < Microphones_MaxSupported(); mic_id++)
    {
        source = Microphones_GetMicrophoneSource(mic_id);
        if (source)
        {
            PanicFalse(number_of_mics < MAX_NUM_OF_CONCURRENT_MICS);
            mics[number_of_mics] = source;
            number_of_mics++;
        }
    }

    function(mics, number_of_mics);
}

static void kymera_Sleep(void)
{
    kymera_RunOnAllMics(kymera_PreserveSources);
    Kymera_MicResamplerSleep();
    Kymera_SplitterSleep(&state.aec_splitter);
    Kymera_SplitterSleep(&state.mic_splitter);
    Kymera_AecSleep();
}

static void kymera_Wake(void)
{
    Kymera_AecWake();
    Kymera_SplitterWake(&state.mic_splitter);
    Kymera_SplitterWake(&state.aec_splitter);
    Kymera_MicResamplerWake();
    kymera_RunOnAllMics(kymera_ReleaseSources);
}

void Kymera_MicRegisterUser(const mic_registry_per_user_t * const info)
{
    DEBUG_LOG("Kymera_MicRegisterUser: enum:mic_users_t:0x%X", info->user);

    state.registry.entry = PanicNull(realloc(state.registry.entry, (state.registry.nr_entries + 1) * sizeof(*state.registry.entry)));

    PanicNull((void*)info->callbacks);
    PanicNull((void*)info->callbacks->MicGetConnectionParameters);
    PanicNull((void*)info->callbacks->MicDisconnectIndication);
    PanicNull((void*)info->callbacks->MicReconnectedIndication);

    state.registry.entry[state.registry.nr_entries] = info;
    state.registry.nr_entries++;
}

void Kymera_MicConnect(mic_users_t user, const mic_connect_params_t *mic_params, Sink aec_ref_sink)
{
    bool is_asleep = (state.sleep_count > 0);
    mic_users_t reconnect_users = mic_user_none;

    PanicFalse((mic_params->connections.num_of_mics > 0) && (mic_params->connections.num_of_mics <= MAX_NUM_OF_CONCURRENT_MICS));
    if (is_asleep)
    {
        /* We wake the operators/sources previously preserved some of which may be removed,
         * then put the chain back to sleep with any new operators/sources added
         */
        kymera_Wake();
    }
    kymera_PrepareForConnection(user, mic_params, &reconnect_users);
    kymera_ConnectUserToMics(user, mic_params, aec_ref_sink, reconnect_users);
    if (is_asleep)
    {
        kymera_Sleep();
    }
}

void Kymera_MicDisconnect(mic_users_t user)
{
    bool is_asleep = (state.sleep_count > 0);
    Source mic_sources[MAX_NUM_OF_CONCURRENT_MICS] = {NULL};
    Sink aec_ref_sink;
    mic_connect_params_t local_mic_params;
    microphone_number_t local_mic_ids[MAX_NUM_OF_CONCURRENT_MICS] = {0};
    Sink local_mic_sinks[MAX_NUM_OF_CONCURRENT_MICS] = {NULL};
    local_mic_params.connections.mic_ids = local_mic_ids;
    local_mic_params.connections.mic_sinks = local_mic_sinks;
    local_mic_params.connections.num_of_mics = 0;

    mic_users_t reconnect_users = mic_user_none;
    bool disconnect_mics;

    if (is_asleep)
    {
        kymera_Wake();
    }
    disconnect_mics = kymera_PrepareForDisconnection(user, &reconnect_users);
    kymera_UserGetConnectionParameters(user, &local_mic_params, local_mic_ids, local_mic_sinks, &aec_ref_sink);
    kymera_DisconnectUserFromMics(user, &local_mic_params, disconnect_mics, reconnect_users);
    kymera_RemoveUser(user);

    if (reconnect_users)
    {
        kymera_PopulateMicSources(user, &local_mic_params, reconnect_users, local_mic_ids, local_mic_sinks, &local_mic_params.connections.num_of_mics);
        kymera_TurnOnMics(local_mic_params.sample_rate, local_mic_params.connections.num_of_mics,
                          local_mic_params.connections.mic_ids, mic_sources);
        kymera_ReconnectAllUsers(reconnect_users, mic_sources);
        kymera_SendReconnectedIndicationToAllUsers(reconnect_users);
    }
    if (is_asleep)
    {
        kymera_Sleep();
    }
}

void Kymera_MicSleep(mic_users_t user)
{
    UNUSED(user);
    if (state.sleep_count == 0)
    {
        kymera_Sleep();
    }
    state.sleep_count++;
}

void Kymera_MicWake(mic_users_t user)
{
    UNUSED(user);
    PanicFalse(state.sleep_count > 0);
    state.sleep_count--;
    if (state.sleep_count == 0)
    {
        kymera_Wake();
    }
}
