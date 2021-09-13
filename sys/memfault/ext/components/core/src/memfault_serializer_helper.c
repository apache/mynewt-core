//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! See header for more details

#include "memfault/core/serializer_helper.h"

#include <inttypes.h>

#include "memfault/config.h"
#include "memfault/core/compiler.h"
#include "memfault/core/debug_log.h"
#include "memfault/core/event_storage_implementation.h"
#include "memfault/core/platform/device_info.h"
#include "memfault/core/platform/system_time.h"
#include "memfault/core/serializer_key_ids.h"
#include "memfault/util/cbor.h"

#if MEMFAULT_EVENT_INCLUDE_BUILD_ID
#include "memfault/core/build_info.h"
#include "memfault_build_id_private.h"
#endif

typedef struct MemfaultSerializerOptions {
  // By default, the device serial number is not encoded in each event to conserve space
  // and instead is derived from the identifier provided when posting to the chunks endpoint
  //  (api/v0/chunks/{{device_identifier}})
  //
  // To instead always encode the device serial number, compile the Memfault SDK with the following
  // CFLAG:
  //   MEMFAULT_EVENT_INCLUDE_DEVICE_SERIAL=0
  bool encode_device_serial;
} sMemfaultSerializerOptions;

//! The number of messages dropped since the last successful send
static uint32_t s_num_storage_drops = 0;
//! A running sum of total messages dropped since memfault_serializer_helper_read_drop_count() was
//! last called
static uint32_t s_last_drop_count = 0;

static const sMemfaultSerializerOptions s_memfault_serializer_options = {
  .encode_device_serial = (MEMFAULT_EVENT_INCLUDE_DEVICE_SERIAL != 0),
};

static bool prv_encode_event_key_string_pair(
    sMemfaultCborEncoder *encoder, eMemfaultEventKey key,  const char *value) {
  return memfault_cbor_encode_unsigned_integer(encoder, key) &&
      memfault_cbor_encode_string(encoder, value);
}

static bool prv_encode_device_version_info(sMemfaultCborEncoder *e) {
  // Encoding something like:
  //
  // (Optional) "device_serial": "ABCD1234",
  // "software_type": "main-fw",
  // "software_version": "1.0.0",
  // "hardware_version": "hwrev1",
  //
  // NOTE: int keys are used instead of strings to minimize the wire payload.

  sMemfaultDeviceInfo info = { 0 };
  memfault_platform_get_device_info(&info);

  if (s_memfault_serializer_options.encode_device_serial &&
      !prv_encode_event_key_string_pair(e, kMemfaultEventKey_DeviceSerial, info.device_serial)) {
    return false;
  }

  if (!prv_encode_event_key_string_pair(e, kMemfaultEventKey_SoftwareType, info.software_type)) {
    return false;
  }

  if (!prv_encode_event_key_string_pair(e, kMemfaultEventKey_SoftwareVersion, info.software_version)) {
    return false;
  }

  if (!prv_encode_event_key_string_pair(e, kMemfaultEventKey_HardwareVersion, info.hardware_version)) {
    return false;
  }

  return true;
}

bool memfault_serializer_helper_encode_uint32_kv_pair(
    sMemfaultCborEncoder *encoder, uint32_t key, uint32_t value) {
  return memfault_cbor_encode_unsigned_integer(encoder, key) &&
      memfault_cbor_encode_unsigned_integer(encoder, value);
}

bool memfault_serializer_helper_encode_int32_kv_pair(
    sMemfaultCborEncoder *encoder, uint32_t key, int32_t value) {
  return memfault_cbor_encode_unsigned_integer(encoder, key) &&
      memfault_cbor_encode_signed_integer(encoder, value);
}

bool memfault_serializer_helper_encode_byte_string_kv_pair(
  sMemfaultCborEncoder *encoder, uint32_t key, const void *buf, size_t buf_len) {
  return memfault_cbor_encode_unsigned_integer(encoder, key) &&
      memfault_cbor_encode_byte_string(encoder, buf, buf_len);
}

static bool prv_encode_event_key_uint32_pair(
    sMemfaultCborEncoder *encoder, eMemfaultEventKey key, uint32_t value) {
  return memfault_cbor_encode_unsigned_integer(encoder, key) &&
         memfault_cbor_encode_unsigned_integer(encoder, value);
}

bool memfault_serializer_helper_encode_metadata(sMemfaultCborEncoder *encoder,
                                                eMemfaultEventType type) {
  sMemfaultCurrentTime time;
  if (!memfault_platform_time_get_current(&time)) {
    time.type = kMemfaultCurrentTimeType_Unknown;
  }
  return memfault_serializer_helper_encode_metadata_with_time(encoder, type, &time);
}

bool memfault_serializer_helper_encode_metadata_with_time(sMemfaultCborEncoder *encoder,
                                                          eMemfaultEventType type,
                                                          const sMemfaultCurrentTime *time) {
  const bool unix_timestamp_available = (time != NULL) &&
      (time->type == kMemfaultCurrentTimeType_UnixEpochTimeSec);

#if MEMFAULT_EVENT_INCLUDE_BUILD_ID
  sMemfaultBuildInfo info;
  const bool has_build_id = memfault_build_info_read(&info);
#else
  const bool has_build_id = false;
#endif

  const size_t top_level_num_pairs =
      1 /* type */ +
      (unix_timestamp_available ? 1 : 0) +
      (s_memfault_serializer_options.encode_device_serial ? 1 : 0) +
      3 /* sw version, sw type, hw version */ +
      (has_build_id ? 1 : 0) +
      1 /* cbor schema version */ +
      1 /* event_info */;

  memfault_cbor_encode_dictionary_begin(encoder, top_level_num_pairs);


  if (!prv_encode_event_key_uint32_pair(encoder, kMemfaultEventKey_Type, type)) {
    return false;
  }

  if (!memfault_serializer_helper_encode_uint32_kv_pair(
          encoder, kMemfaultEventKey_CborSchemaVersion, MEMFAULT_CBOR_SCHEMA_VERSION_V1)) {
    return false;
  }

  if (!prv_encode_device_version_info(encoder)) {
    return false;
  }

#if MEMFAULT_EVENT_INCLUDE_BUILD_ID
  MEMFAULT_STATIC_ASSERT(MEMFAULT_EVENT_INCLUDED_BUILD_ID_SIZE_BYTES >= 5 &&
                         MEMFAULT_EVENT_INCLUDED_BUILD_ID_SIZE_BYTES <= sizeof(info.build_id),
                         "MEMFAULT_EVENT_INCLUDED_BUILD_ID_SIZE_BYTES must be between 5 and 20 (inclusive)");
  if (has_build_id &&
      !memfault_serializer_helper_encode_byte_string_kv_pair(encoder, kMemfaultEventKey_BuildId, info.build_id,
                                                             MEMFAULT_EVENT_INCLUDED_BUILD_ID_SIZE_BYTES)) {
    return false;
  }
#endif

  return !unix_timestamp_available || prv_encode_event_key_uint32_pair(
          encoder, kMemfaultEventKey_CapturedDateUnixTimestamp,
          (uint32_t)time->info.unix_timestamp_secs);
}

bool memfault_serializer_helper_encode_trace_event(sMemfaultCborEncoder *e,
                                                   const sMemfaultTraceEventHelperInfo *info) {
  if (!memfault_serializer_helper_encode_metadata(e, kMemfaultEventType_Trace)) {
    return false;
  }

  const size_t num_entries = 1 /* reason */ +
      ((info->pc != 0) ? 1 : 0) +
      ((info->lr != 0) ? 1 : 0) +
      info->extra_event_info_pairs;

  if (!memfault_cbor_encode_unsigned_integer(e, kMemfaultEventKey_EventInfo) ||
      !memfault_cbor_encode_dictionary_begin(e, num_entries)) {
    return false;
  }

  if (!memfault_serializer_helper_encode_uint32_kv_pair(
      e, info->reason_key, info->reason_value)) {
    return false;
  }

  bool success = true;
  if (info->pc) {
    success = memfault_serializer_helper_encode_uint32_kv_pair(e,
        kMemfaultTraceInfoEventKey_ProgramCounter, info->pc);
  }

  if (success && info->lr) {
    success = memfault_serializer_helper_encode_uint32_kv_pair(e,
        kMemfaultTraceInfoEventKey_LinkRegister, info->lr);
  }

  return success;
}

typedef struct {
  const sMemfaultEventStorageImpl *storage_impl;
} sMemfaultSerializerHelperEncoderCtx;

static void prv_encoder_write_cb(void *ctx, MEMFAULT_UNUSED uint32_t offset, const void *buf, size_t buf_len) {
  const sMemfaultEventStorageImpl *storage_impl = ((sMemfaultSerializerHelperEncoderCtx *) ctx)->storage_impl;
  storage_impl->append_data_cb(buf, buf_len);
}

bool memfault_serializer_helper_encode_to_storage(sMemfaultCborEncoder *encoder,
    const sMemfaultEventStorageImpl *storage_impl,
    MemfaultSerializerHelperEncodeCallback encode_callback, void *ctx) {
  const size_t space_available = storage_impl->begin_write_cb();
  bool success;
  {
    sMemfaultSerializerHelperEncoderCtx encoder_ctx = {
        .storage_impl = storage_impl,
    };
    memfault_cbor_encoder_init(encoder, prv_encoder_write_cb, &encoder_ctx, space_available);
    success = encode_callback(encoder, ctx);
    memfault_cbor_encoder_deinit(encoder);
  }
  const bool rollback = !success;
  storage_impl->finish_write_cb(rollback);

  if (!success) {
    if (s_num_storage_drops == 0) {
      MEMFAULT_LOG_ERROR("Event storage full");
    }
    s_num_storage_drops++;
  } else if (s_num_storage_drops != 0) {
    MEMFAULT_LOG_INFO("Event saved successfully after %d drops",
                      (int)s_num_storage_drops);
    s_last_drop_count += s_num_storage_drops;
    s_num_storage_drops = 0;
  }

  return success;
}

uint32_t memfault_serializer_helper_read_drop_count(void) {
  const uint32_t drop_count = s_last_drop_count + s_num_storage_drops;
  s_last_drop_count = 0;
  s_num_storage_drops = 0;
  return drop_count;
}

size_t memfault_serializer_helper_compute_size(sMemfaultCborEncoder *encoder,
    MemfaultSerializerHelperEncodeCallback encode_callback, void *ctx) {
  memfault_cbor_encoder_size_only_init(encoder);
  encode_callback(encoder, ctx);
  return memfault_cbor_encoder_deinit(encoder);
}

bool memfault_serializer_helper_check_storage_size(
    const sMemfaultEventStorageImpl *storage_impl, size_t (compute_worst_case_size)(void), const char *event_type) {
  // Check to see if the backing storage can hold at least one event
  // and return an error code in this situation so it's easier for an end user to catch it:
  const size_t storage_max_size = storage_impl->get_storage_size_cb();
  const size_t worst_case_size_needed = compute_worst_case_size();
  if (worst_case_size_needed > storage_max_size) {
    MEMFAULT_LOG_WARN("Event storage (%d) smaller than largest %s event (%d)",
                      (int)storage_max_size, event_type, (int)worst_case_size_needed);
    return false;
  }
  return true;
}
