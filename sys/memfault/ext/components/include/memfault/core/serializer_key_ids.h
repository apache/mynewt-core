#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! Internal keys used for serializing different types of event ids. Users of the SDK should never
//! need to use any of these directly.

#ifdef __cplusplus
extern "C" {
#endif

#define MEMFAULT_CBOR_SCHEMA_VERSION_V1 (1) // NOTE: implies "sdk_version": "0.5.0"

typedef enum {
  kMemfaultEventKey_CapturedDateUnixTimestamp = 1,
  kMemfaultEventKey_Type = 2,
  kMemfaultEventKey_CborSchemaVersion = 3,
  kMemfaultEventKey_EventInfo = 4,
  kMemfaultEventKey_UserInfo = 5,
  kMemfaultEventKey_HardwareVersion = 6,
  kMemfaultEventKey_DeviceSerial = 7,
  kMemfaultEventKey_ReleaseVersionDeprecated = 8,
  kMemfaultEventKey_SoftwareVersion = 9,
  kMemfaultEventKey_SoftwareType = 10,
  kMemfaultEventKey_BuildId = 11,
} eMemfaultEventKey;

//! Possible values for the kMemfaultEventKey_Type field.
typedef enum {
  kMemfaultEventType_Heartbeat = 1,
  kMemfaultEventType_Trace = 2,
  kMemfaultEventType_LogError = 3,
  kMemfaultEventType_Logs = 4,
} eMemfaultEventType;

//! EventInfo dictionary keys for events with type kMemfaultEventType_Heartbeat.
typedef enum {
  kMemfaultHeartbeatInfoKey_Metrics = 1,
} eMemfaultHeartbeatInfoKey;

//! EventInfo dictionary keys for events with type kMemfaultEventType_Trace.
typedef enum {
  kMemfaultTraceInfoEventKey_Reason = 1,
  kMemfaultTraceInfoEventKey_ProgramCounter = 2,
  kMemfaultTraceInfoEventKey_LinkRegister = 3,
  kMemfaultTraceInfoEventKey_McuReasonRegister = 4,
  kMemfaultTraceInfoEventKey_CoredumpSaved = 5,
  kMemfaultTraceInfoEventKey_UserReason = 6,
  kMemfaultTraceInfoEventKey_StatusCode = 7,
  kMemfaultTraceInfoEventKey_Log = 8,
  kMemfaultTraceInfoEventKey_CompactLog = 9,
} eMemfaultTraceInfoEventKey;

//! EventInfo dictionary keys for events with type kMemfaultEventType_LogError.
typedef enum {
  kMemfaultLogErrorInfoKey_Log = 1,
} eMemfaultLogErrorInfoKey;

//! For events with type kMemfaultEventType_Logs, the EventInfo contains a single array containing
//! all logs: [lvl1, msg1, lvl2, msg2, ...]

#ifdef __cplusplus
}
#endif
