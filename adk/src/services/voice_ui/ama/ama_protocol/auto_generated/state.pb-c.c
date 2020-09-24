/*****************************************************************************
Copyright (c) 2018 - 2020 Qualcomm Technologies International, Ltd.
******************************************************************************/

/* Generated by the protocol buffer compiler.  DO NOT EDIT! */
/* Generated from: state.proto */

/* Do not generate deprecated warnings for self */
#ifndef PROTOBUF_C__NO_DEPRECATED
#define PROTOBUF_C__NO_DEPRECATED
#endif

#include "state.pb-c.h"
void   state__init
                     (State         *message)
{
  static const State init_value = STATE__INIT;
  *message = init_value;
}
size_t state__get_packed_size
                     (const State *message)
{
  assert(message->base.descriptor == &state__descriptor);
  return protobuf_c_message_get_packed_size ((const ProtobufCMessage*)(message));
}
size_t state__pack
                     (const State *message,
                      uint8_t       *out)
{
  assert(message->base.descriptor == &state__descriptor);
  return protobuf_c_message_pack ((const ProtobufCMessage*)message, out);
}
size_t state__pack_to_buffer
                     (const State *message,
                      ProtobufCBuffer *buffer)
{
  assert(message->base.descriptor == &state__descriptor);
  return protobuf_c_message_pack_to_buffer ((const ProtobufCMessage*)message, buffer);
}
State *
       state__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data)
{
  return (State *)
     protobuf_c_message_unpack (&state__descriptor,
                                allocator, len, data);
}
void   state__free_unpacked
                     (State *message,
                      ProtobufCAllocator *allocator)
{
  if(!message)
    return;
  assert(message->base.descriptor == &state__descriptor);
  protobuf_c_message_free_unpacked ((ProtobufCMessage*)message, allocator);
}
void   get_state__init
                     (GetState         *message)
{
  static const GetState init_value = GET_STATE__INIT;
  *message = init_value;
}
size_t get_state__get_packed_size
                     (const GetState *message)
{
  assert(message->base.descriptor == &get_state__descriptor);
  return protobuf_c_message_get_packed_size ((const ProtobufCMessage*)(message));
}
size_t get_state__pack
                     (const GetState *message,
                      uint8_t       *out)
{
  assert(message->base.descriptor == &get_state__descriptor);
  return protobuf_c_message_pack ((const ProtobufCMessage*)message, out);
}
size_t get_state__pack_to_buffer
                     (const GetState *message,
                      ProtobufCBuffer *buffer)
{
  assert(message->base.descriptor == &get_state__descriptor);
  return protobuf_c_message_pack_to_buffer ((const ProtobufCMessage*)message, buffer);
}
GetState *
       get_state__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data)
{
  return (GetState *)
     protobuf_c_message_unpack (&get_state__descriptor,
                                allocator, len, data);
}
void   get_state__free_unpacked
                     (GetState *message,
                      ProtobufCAllocator *allocator)
{
  if(!message)
    return;
  assert(message->base.descriptor == &get_state__descriptor);
  protobuf_c_message_free_unpacked ((ProtobufCMessage*)message, allocator);
}
void   set_state__init
                     (SetState         *message)
{
  static const SetState init_value = SET_STATE__INIT;
  *message = init_value;
}
size_t set_state__get_packed_size
                     (const SetState *message)
{
  assert(message->base.descriptor == &set_state__descriptor);
  return protobuf_c_message_get_packed_size ((const ProtobufCMessage*)(message));
}
size_t set_state__pack
                     (const SetState *message,
                      uint8_t       *out)
{
  assert(message->base.descriptor == &set_state__descriptor);
  return protobuf_c_message_pack ((const ProtobufCMessage*)message, out);
}
size_t set_state__pack_to_buffer
                     (const SetState *message,
                      ProtobufCBuffer *buffer)
{
  assert(message->base.descriptor == &set_state__descriptor);
  return protobuf_c_message_pack_to_buffer ((const ProtobufCMessage*)message, buffer);
}
SetState *
       set_state__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data)
{
  return (SetState *)
     protobuf_c_message_unpack (&set_state__descriptor,
                                allocator, len, data);
}
void   set_state__free_unpacked
                     (SetState *message,
                      ProtobufCAllocator *allocator)
{
  if(!message)
    return;
  assert(message->base.descriptor == &set_state__descriptor);
  protobuf_c_message_free_unpacked ((ProtobufCMessage*)message, allocator);
}
void   synchronize_state__init
                     (SynchronizeState         *message)
{
  static const SynchronizeState init_value = SYNCHRONIZE_STATE__INIT;
  *message = init_value;
}
size_t synchronize_state__get_packed_size
                     (const SynchronizeState *message)
{
  assert(message->base.descriptor == &synchronize_state__descriptor);
  return protobuf_c_message_get_packed_size ((const ProtobufCMessage*)(message));
}
size_t synchronize_state__pack
                     (const SynchronizeState *message,
                      uint8_t       *out)
{
  assert(message->base.descriptor == &synchronize_state__descriptor);
  return protobuf_c_message_pack ((const ProtobufCMessage*)message, out);
}
size_t synchronize_state__pack_to_buffer
                     (const SynchronizeState *message,
                      ProtobufCBuffer *buffer)
{
  assert(message->base.descriptor == &synchronize_state__descriptor);
  return protobuf_c_message_pack_to_buffer ((const ProtobufCMessage*)message, buffer);
}
SynchronizeState *
       synchronize_state__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data)
{
  return (SynchronizeState *)
     protobuf_c_message_unpack (&synchronize_state__descriptor,
                                allocator, len, data);
}
void   synchronize_state__free_unpacked
                     (SynchronizeState *message,
                      ProtobufCAllocator *allocator)
{
  if(!message)
    return;
  assert(message->base.descriptor == &synchronize_state__descriptor);
  protobuf_c_message_free_unpacked ((ProtobufCMessage*)message, allocator);
}
static const ProtobufCFieldDescriptor state__field_descriptors[3] =
{
  {
    "feature",
    1,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_UINT32,
    0,   /* quantifier_offset */
    offsetof(State, feature),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "boolean",
    2,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_BOOL,
    offsetof(State, value_case),
    offsetof(State, u.boolean),
    NULL,
    NULL,
    0 | PROTOBUF_C_FIELD_FLAG_ONEOF,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "integer",
    3,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_UINT32,
    offsetof(State, value_case),
    offsetof(State, u.integer),
    NULL,
    NULL,
    0 | PROTOBUF_C_FIELD_FLAG_ONEOF,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
};
static const unsigned state__field_indices_by_name[] = {
  1,   /* field[1] = boolean */
  0,   /* field[0] = feature */
  2,   /* field[2] = integer */
};
static const ProtobufCIntRange state__number_ranges[1 + 1] =
{
  { 1, 0 },
  { 0, 3 }
};
const ProtobufCMessageDescriptor state__descriptor =
{
  PROTOBUF_C__MESSAGE_DESCRIPTOR_MAGIC,
  "State",
  "State",
  "State",
  "",
  sizeof(State),
  3,
  state__field_descriptors,
  state__field_indices_by_name,
  1,  state__number_ranges,
  (ProtobufCMessageInit) state__init,
  NULL,NULL,NULL    /* reserved[123] */
};
static const ProtobufCFieldDescriptor get_state__field_descriptors[1] =
{
  {
    "feature",
    1,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_UINT32,
    0,   /* quantifier_offset */
    offsetof(GetState, feature),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
};
static const unsigned get_state__field_indices_by_name[] = {
  0,   /* field[0] = feature */
};
static const ProtobufCIntRange get_state__number_ranges[1 + 1] =
{
  { 1, 0 },
  { 0, 1 }
};
const ProtobufCMessageDescriptor get_state__descriptor =
{
  PROTOBUF_C__MESSAGE_DESCRIPTOR_MAGIC,
  "GetState",
  "GetState",
  "GetState",
  "",
  sizeof(GetState),
  1,
  get_state__field_descriptors,
  get_state__field_indices_by_name,
  1,  get_state__number_ranges,
  (ProtobufCMessageInit) get_state__init,
  NULL,NULL,NULL    /* reserved[123] */
};
static const ProtobufCFieldDescriptor set_state__field_descriptors[1] =
{
  {
    "state",
    1,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_MESSAGE,
    0,   /* quantifier_offset */
    offsetof(SetState, state),
    &state__descriptor,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
};
static const unsigned set_state__field_indices_by_name[] = {
  0,   /* field[0] = state */
};
static const ProtobufCIntRange set_state__number_ranges[1 + 1] =
{
  { 1, 0 },
  { 0, 1 }
};
const ProtobufCMessageDescriptor set_state__descriptor =
{
  PROTOBUF_C__MESSAGE_DESCRIPTOR_MAGIC,
  "SetState",
  "SetState",
  "SetState",
  "",
  sizeof(SetState),
  1,
  set_state__field_descriptors,
  set_state__field_indices_by_name,
  1,  set_state__number_ranges,
  (ProtobufCMessageInit) set_state__init,
  NULL,NULL,NULL    /* reserved[123] */
};
static const ProtobufCFieldDescriptor synchronize_state__field_descriptors[1] =
{
  {
    "state",
    1,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_MESSAGE,
    0,   /* quantifier_offset */
    offsetof(SynchronizeState, state),
    &state__descriptor,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
};
static const unsigned synchronize_state__field_indices_by_name[] = {
  0,   /* field[0] = state */
};
static const ProtobufCIntRange synchronize_state__number_ranges[1 + 1] =
{
  { 1, 0 },
  { 0, 1 }
};
const ProtobufCMessageDescriptor synchronize_state__descriptor =
{
  PROTOBUF_C__MESSAGE_DESCRIPTOR_MAGIC,
  "SynchronizeState",
  "SynchronizeState",
  "SynchronizeState",
  "",
  sizeof(SynchronizeState),
  1,
  synchronize_state__field_descriptors,
  synchronize_state__field_indices_by_name,
  1,  synchronize_state__number_ranges,
  (ProtobufCMessageInit) synchronize_state__init,
  NULL,NULL,NULL    /* reserved[123] */
};
