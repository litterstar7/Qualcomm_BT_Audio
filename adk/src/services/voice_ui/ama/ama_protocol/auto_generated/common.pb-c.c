/*****************************************************************************
Copyright (c) 2018 - 2020 Qualcomm Technologies International, Ltd.
******************************************************************************/

/* Generated by the protocol buffer compiler.  DO NOT EDIT! */
/* Generated from: common.proto */

/* Do not generate deprecated warnings for self */
#ifndef PROTOBUF_C__NO_DEPRECATED
#define PROTOBUF_C__NO_DEPRECATED
#endif

#include "common.pb-c.h"
static const ProtobufCEnumValue transport__enum_values_by_number[3] =
{
  { "BLUETOOTH_LOW_ENERGY", "TRANSPORT__BLUETOOTH_LOW_ENERGY", 0 },
  { "BLUETOOTH_RFCOMM", "TRANSPORT__BLUETOOTH_RFCOMM", 1 },
  { "BLUETOOTH_IAP", "TRANSPORT__BLUETOOTH_IAP", 2 },
};
static const ProtobufCIntRange transport__value_ranges[] = {
{0, 0},{0, 3}
};
static const ProtobufCEnumValueIndex transport__enum_values_by_name[3] =
{
  { "BLUETOOTH_IAP", 2 },
  { "BLUETOOTH_LOW_ENERGY", 0 },
  { "BLUETOOTH_RFCOMM", 1 },
};
const ProtobufCEnumDescriptor transport__descriptor =
{
  PROTOBUF_C__ENUM_DESCRIPTOR_MAGIC,
  "Transport",
  "Transport",
  "Transport",
  "",
  3,
  transport__enum_values_by_number,
  3,
  transport__enum_values_by_name,
  1,
  transport__value_ranges,
  NULL,NULL,NULL,NULL   /* reserved[1234] */
};
static const ProtobufCEnumValue error_code__enum_values_by_number[8] =
{
  { "SUCCESS", "ERROR_CODE__SUCCESS", 0 },
  { "UNKNOWN", "ERROR_CODE__UNKNOWN", 1 },
  { "INTERNAL", "ERROR_CODE__INTERNAL", 2 },
  { "UNSUPPORTED", "ERROR_CODE__UNSUPPORTED", 3 },
  { "USER_CANCELLED", "ERROR_CODE__USER_CANCELLED", 4 },
  { "NOT_FOUND", "ERROR_CODE__NOT_FOUND", 5 },
  { "INVALID", "ERROR_CODE__INVALID", 6 },
  { "BUSY", "ERROR_CODE__BUSY", 7 },
};
static const ProtobufCIntRange error_code__value_ranges[] = {
{0, 0},{0, 8}
};
static const ProtobufCEnumValueIndex error_code__enum_values_by_name[8] =
{
  { "BUSY", 7 },
  { "INTERNAL", 2 },
  { "INVALID", 6 },
  { "NOT_FOUND", 5 },
  { "SUCCESS", 0 },
  { "UNKNOWN", 1 },
  { "UNSUPPORTED", 3 },
  { "USER_CANCELLED", 4 },
};
const ProtobufCEnumDescriptor error_code__descriptor =
{
  PROTOBUF_C__ENUM_DESCRIPTOR_MAGIC,
  "ErrorCode",
  "ErrorCode",
  "ErrorCode",
  "",
  8,
  error_code__enum_values_by_number,
  8,
  error_code__enum_values_by_name,
  1,
  error_code__value_ranges,
  NULL,NULL,NULL,NULL   /* reserved[1234] */
};
static const ProtobufCEnumValue speech_initiation_type__enum_values_by_number[3] =
{
  { "PRESS_AND_HOLD", "SPEECH_INITIATION_TYPE__PRESS_AND_HOLD", 0 },
  { "TAP", "SPEECH_INITIATION_TYPE__TAP", 1 },
  { "WAKEWORD", "SPEECH_INITIATION_TYPE__WAKEWORD", 2 },
};
static const ProtobufCIntRange speech_initiation_type__value_ranges[] = {
{0, 0},{0, 3}
};
static const ProtobufCEnumValueIndex speech_initiation_type__enum_values_by_name[3] =
{
  { "PRESS_AND_HOLD", 0 },
  { "TAP", 1 },
  { "WAKEWORD", 2 },
};
const ProtobufCEnumDescriptor speech_initiation_type__descriptor =
{
  PROTOBUF_C__ENUM_DESCRIPTOR_MAGIC,
  "SpeechInitiationType",
  "SpeechInitiationType",
  "SpeechInitiationType",
  "",
  3,
  speech_initiation_type__enum_values_by_number,
  3,
  speech_initiation_type__enum_values_by_name,
  1,
  speech_initiation_type__value_ranges,
  NULL,NULL,NULL,NULL   /* reserved[1234] */
};
