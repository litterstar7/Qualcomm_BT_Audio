/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Kymera module to manage creation of splitter chains with multiple streams
*/

#include "kymera_splitter.h"
#include <custom_operator.h>
#include <logging.h>
#include <panic.h>
#include <stream.h>
#include <stdlib.h>

/*!
    Changing the value of this define will break the code
    The implementation is heavily based on this value being true
*/
#define NUM_OF_STREAMS_PER_SPLITTER (2)

typedef struct
{
    Operator op;
    splitter_output_stream_set_t active_streams;
    unsigned started:1;
} splitter_operator_t;

typedef struct
{
    Operator op;
    unsigned started:1;
} buffer_operator_t;

struct splitter_tag
{
    const splitter_config_t *config;
    uint8 num_of_inputs;
    uint8 num_of_splitters;
    buffer_operator_t buffer;
    splitter_operator_t splitters[];
};

typedef void (*OperatorFunction)(Operator *ops, uint8 num_of_ops);

static void kymera_SetRunningStreams(splitter_operator_t *splitter, splitter_output_stream_set_t running_streams)
{
    if (running_streams != splitter->active_streams)
    {
        OperatorsSplitterSetRunningStreams(splitter->op, running_streams);
    }

    splitter->active_streams = running_streams;
}

static void kymera_ActivateStream(splitter_operator_t *splitter, splitter_output_stream_set_t stream)
{
    kymera_SetRunningStreams(splitter, splitter->active_streams | stream);
}

static void kymera_DeactivateStream(splitter_operator_t *splitter, splitter_output_stream_set_t stream)
{
    kymera_SetRunningStreams(splitter, splitter->active_streams & ~stream);
}

static void kymera_StartBuffer(buffer_operator_t *buffer)
{
    if (buffer->started == FALSE)
    {
        OperatorStart(buffer->op);
        buffer->started = TRUE;
    }
}

static void kymera_StopBuffer(buffer_operator_t *buffer)
{
    if (buffer->started)
    {
        OperatorStop(buffer->op);
        buffer->started = FALSE;
    }
}

static void kymera_CreateBuffer(splitter_handle_t handle)
{
    operator_data_format_t format = operator_data_format_pcm;
    PanicFalse(handle->buffer.op == INVALID_OPERATOR);
    handle->buffer.op = CustomOperatorCreate(capability_id_passthrough, OPERATOR_PROCESSOR_ID_0, operator_priority_high, NULL);

    if (handle->config)
    {
        format = handle->config->data_format;
        if (handle->config->transform_size_in_words)
        {
            OperatorsStandardSetBufferSize(handle->buffer.op, handle->config->transform_size_in_words);
        }
    }
    OperatorsSetPassthroughDataFormat(handle->buffer.op, format);
}

static void kymera_DestroyBuffer(buffer_operator_t *buffer)
{
    if (buffer->op != INVALID_OPERATOR)
    {
        kymera_StopBuffer(buffer);
        CustomOperatorDestroy(&buffer->op, 1);
        memset(buffer, 0, sizeof(*buffer));
    }
}

static void kymera_StartSplitter(splitter_operator_t *splitter)
{
    if (splitter->started == FALSE)
    {
        OperatorStart(splitter->op);
        splitter->started = TRUE;
    }
}

static void kymera_StopSplitter(splitter_operator_t *splitter)
{
    if (splitter->started)
    {
        OperatorStop(splitter->op);
        splitter->started = FALSE;
    }
}

static void kymera_CreateSplitter(splitter_handle_t handle, uint8 splitter_index)
{
    operator_data_format_t format = operator_data_format_pcm;
    PanicFalse(handle->splitters[splitter_index].op == INVALID_OPERATOR);
    handle->splitters[splitter_index].op = CustomOperatorCreate(capability_id_splitter, OPERATOR_PROCESSOR_ID_0, operator_priority_high, NULL);

    if (handle->config)
    {
        format = handle->config->data_format;
        if (handle->config->transform_size_in_words)
        {
            OperatorsStandardSetBufferSize(handle->splitters[splitter_index].op, handle->config->transform_size_in_words);
        }
    }
    OperatorsSplitterSetDataFormat(handle->splitters[splitter_index].op, format);
}

static void kymera_DestroySplitter(splitter_operator_t *splitter)
{
    if (splitter->op != INVALID_OPERATOR)
    {
        kymera_StopSplitter(splitter);
        CustomOperatorDestroy(&splitter->op, 1);
        memset(splitter, 0, sizeof(*splitter));
    }
}

static bool kymera_IsLastClientStream(splitter_handle_t handle, uint8 stream_index)
{
    PanicFalse(stream_index <= handle->num_of_splitters);
    return (stream_index == handle->num_of_splitters);
}

static bool kymera_IsLastSplitter(splitter_handle_t handle, uint8 splitter_index)
{
    PanicFalse(splitter_index < handle->num_of_splitters);
    return (splitter_index == (handle->num_of_splitters - 1));
}

static uint8 kymera_GetSplitterIndex(splitter_handle_t handle, uint8 stream_index)
{
    return (kymera_IsLastClientStream(handle, stream_index)) ? handle->num_of_splitters - 1 : stream_index;
}

static splitter_output_stream_set_t kymera_GetSplitterClientStream(splitter_handle_t handle, uint8 stream_index)
{
    return (kymera_IsLastClientStream(handle, stream_index)) ? splitter_output_stream_1 : splitter_output_stream_0;
}

static bool kymera_IsConnectedToClient(splitter_handle_t handle, uint8 splitter_index)
{
    return ((handle->splitters[splitter_index].active_streams &
            (kymera_IsLastSplitter(handle, splitter_index)) ? splitter_output_streams_all : splitter_output_stream_0)
            > 0);
}

static uint8 kymera_GetSplitterOutputTerminalForClient(splitter_handle_t handle, uint8 stream_index, uint8 input_index)
{
    return (kymera_IsLastClientStream(handle, stream_index)) ?
                (NUM_OF_STREAMS_PER_SPLITTER * input_index + 1) :
                (NUM_OF_STREAMS_PER_SPLITTER * input_index);
}

static uint8 kymera_GetSplitterInterconnectOutputTerminal(splitter_handle_t handle, uint8 splitter_index, uint8 input_index)
{
    PanicFalse(kymera_IsLastSplitter(handle, splitter_index) == FALSE);
    return NUM_OF_STREAMS_PER_SPLITTER * input_index + 1;
}

static uint8 kymera_GetNumOfSplittersRequired(uint8 num_of_streams)
{
    PanicZero(num_of_streams);
    return (num_of_streams <= NUM_OF_STREAMS_PER_SPLITTER) ? 1 : (num_of_streams - 1);
}

static void kymera_InterconnectSplitter(splitter_handle_t handle, uint8 splitter_index)
{
    uint8 i, j, terminal;
    Sink sink;
    Source source;

    for(i = 0; i < splitter_index; i++)
    {
        if (handle->splitters[i + 1].op == INVALID_OPERATOR)
        {
            kymera_CreateSplitter(handle, i + 1);

            for(j = 0; j < handle->num_of_inputs; j++)
            {
                terminal = kymera_GetSplitterInterconnectOutputTerminal(handle, i, j);
                source = StreamSourceFromOperatorTerminal(handle->splitters[i].op, terminal);
                sink = StreamSinkFromOperatorTerminal(handle->splitters[i + 1].op, j);
                PanicNull(StreamConnect(source, sink));
            }

            kymera_ActivateStream(&handle->splitters[i], splitter_output_stream_1);
        }
    }
}

static void kymera_DestroyUnconnectedSplitters(splitter_handle_t handle)
{
    uint8 i;

    // Never destroy the first splitter
    for(i = 1; i < handle->num_of_splitters; i++)
    {
        if (kymera_IsConnectedToClient(handle, handle->num_of_splitters - i) == FALSE)
        {
            kymera_DestroySplitter(&handle->splitters[handle->num_of_splitters - i]);
        }
    }
}

static void kymera_ConnectClientToStream(splitter_handle_t handle, uint8 stream_index, const Sink *input)
{
    uint8 i, terminal, splitter_index = kymera_GetSplitterIndex(handle, stream_index);
    Source source;

    for(i = 0; i < handle->num_of_inputs; i++)
    {
        terminal = kymera_GetSplitterOutputTerminalForClient(handle, stream_index, i);
        source = StreamSourceFromOperatorTerminal(handle->splitters[splitter_index].op, terminal);
        PanicNull(StreamConnect(source, input[i]));
    }

    kymera_ActivateStream(&handle->splitters[splitter_index], kymera_GetSplitterClientStream(handle, stream_index));
}

static void kymera_DisconnectClientFromStream(splitter_handle_t handle, uint8 stream_index)
{
    uint8 i, terminal, splitter_index = kymera_GetSplitterIndex(handle, stream_index);
    Source source;

    if (kymera_IsConnectedToClient(handle, splitter_index))
    {
        kymera_DeactivateStream(&handle->splitters[splitter_index], kymera_GetSplitterClientStream(handle, stream_index));
        for(i = 0; i < handle->num_of_inputs; i++)
        {
            terminal = kymera_GetSplitterOutputTerminalForClient(handle, stream_index, i);
            source = StreamSourceFromOperatorTerminal(handle->splitters[splitter_index].op, terminal);
            StreamDisconnect(source, NULL);
        }
    }
}

static void kymera_ConnectBufferToSplitter(splitter_handle_t handle)
{
    uint8 i;

    for(i = 0; i < handle->num_of_inputs; i++)
    {
        PanicNull(StreamConnect(StreamSourceFromOperatorTerminal(handle->buffer.op, i), StreamSinkFromOperatorTerminal(handle->splitters[0].op, i)));
    }
}

static void kymera_DisconnectChainInput(splitter_handle_t handle)
{
    uint8 i;

    for(i = 0; i < handle->num_of_inputs; i++)
    {
        StreamDisconnect(StreamSourceFromOperatorTerminal(handle->buffer.op, i), NULL);
    }
}

static void kymera_DestroyChain(splitter_handle_t handle)
{
   uint8 i;

   kymera_DisconnectChainInput(handle);
   kymera_StopBuffer(&handle->buffer);
   for(i = 1; i <= handle->num_of_splitters; i++)
   {
       kymera_DestroySplitter(&handle->splitters[handle->num_of_splitters - i]);
   }
   kymera_DestroyBuffer(&handle->buffer);
}

static splitter_handle_t kymera_CreateHandle(uint8 num_of_streams, uint8 num_of_inputs, const splitter_config_t *config)
{
    splitter_handle_t handle;
    uint8 num_of_splitters = kymera_GetNumOfSplittersRequired(num_of_streams);
    size_t handle_size = sizeof(*handle) + num_of_splitters * sizeof(handle->splitters[0]);

    handle = PanicUnlessMalloc(handle_size);
    memset(handle, 0, handle_size);
    handle->num_of_splitters = num_of_splitters;
    PanicZero(num_of_inputs);
    handle->num_of_inputs = num_of_inputs;
    handle->config = config;

    return handle;
}

static void kymera_RunOnChainOperators(splitter_handle_t handle, OperatorFunction function)
{
    uint8 num_of_ops = 0, num_of_splitters = handle->num_of_splitters;
    uint8 i, max_num_of_ops = (num_of_splitters + 1);
    splitter_operator_t *splitters = handle->splitters;
    buffer_operator_t buffer = handle->buffer;
    Operator *ops = PanicUnlessMalloc(max_num_of_ops * sizeof(*ops));

    for(i = 0; i < num_of_splitters; i++)
    {
        if (splitters[i].op != INVALID_OPERATOR)
        {
            ops[num_of_ops] = splitters[i].op;
            num_of_ops++;
        }
    }

    if (buffer.op != INVALID_OPERATOR)
    {
        ops[num_of_ops] = buffer.op;
        num_of_ops++;
    }

    if (num_of_ops)
    {
        function(ops, num_of_ops);
    }

    free(ops);
}

static void kymera_PreserveOperators(Operator *ops, uint8 num_of_ops)
{
    OperatorFrameworkPreserve(num_of_ops, ops, 0, NULL, 0, NULL);
}

static void kymera_ReleaseOperators(Operator *ops, uint8 num_of_ops)
{
    OperatorFrameworkRelease(num_of_ops, ops, 0, NULL, 0, NULL);
}

splitter_handle_t Kymera_SplitterCreate(uint8 num_of_streams, uint8 num_of_inputs, const splitter_config_t *config)
{
    splitter_handle_t handle = kymera_CreateHandle(num_of_streams, num_of_inputs, config);

    DEBUG_LOG("Kymera_SplitterCreate: %p", handle);
    OperatorsFrameworkEnable();
    kymera_CreateBuffer(handle);
    kymera_CreateSplitter(handle, 0);
    kymera_ConnectBufferToSplitter(handle);

    return handle;
}

void Kymera_SplitterDestroy(splitter_handle_t *handle)
{
    DEBUG_LOG("Kymera_SplitterDestroy: %p", *handle);
    kymera_DestroyChain(*handle);
    OperatorsFrameworkDisable();
    free(*handle);
    *handle = NULL;
}

Sink Kymera_SplitterGetInput(splitter_handle_t *handle, uint8 input_index)
{
    PanicFalse(input_index < (*handle)->num_of_inputs);
    return StreamSinkFromOperatorTerminal((*handle)->buffer.op, input_index);
}

void Kymera_SplitterConnectToOutputStream(splitter_handle_t *handle, uint8 stream_index, const Sink *input)
{
    uint8 splitter_index = kymera_GetSplitterIndex(*handle, stream_index);

    DEBUG_LOG("Kymera_SplitterConnectToOutputStream: handle %p, stream index %d", *handle, stream_index);
    kymera_InterconnectSplitter(*handle, splitter_index);
    kymera_ConnectClientToStream(*handle, stream_index, input);
}

void Kymera_SplitterDisconnectFromOutputStream(splitter_handle_t *handle, uint8 stream_index)
{
    DEBUG_LOG("Kymera_SplitterDisconnectFromOutputStream: handle %p, stream index %d", *handle, stream_index);
    kymera_DisconnectClientFromStream(*handle, stream_index);
    kymera_DestroyUnconnectedSplitters(*handle);
}

void Kymera_SplitterStartOutputStream(splitter_handle_t *handle, uint8 stream_index)
{
    uint8 i, splitter_index = kymera_GetSplitterIndex(*handle, stream_index);

    DEBUG_LOG("Kymera_SplitterStart: handle %p, stream index %d", *handle, stream_index);

    for(i = 0; i <= splitter_index; i++)
    {
        kymera_StartSplitter(&(*handle)->splitters[splitter_index - i]);
    }

    kymera_StartBuffer(&(*handle)->buffer);
}

void Kymera_SplitterSleep(splitter_handle_t *handle)
{
    kymera_RunOnChainOperators(*handle, kymera_PreserveOperators);
    OperatorsFrameworkDisable();
}

void Kymera_SplitterWake(splitter_handle_t *handle)
{
    OperatorsFrameworkEnable();
    kymera_RunOnChainOperators(*handle, kymera_ReleaseOperators);
}
