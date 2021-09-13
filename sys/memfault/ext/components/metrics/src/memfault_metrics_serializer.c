//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! Utility responsible for serializing out a Memfault Heartbeat. Today, the serialization format
//! used is cbor but the actual format used is opaque to an end user and no assumptions should be
//! made in user code based on it.

#include "memfault/metrics/serializer.h"

#include <stdio.h>
#include <stddef.h>

#include "memfault/core/compiler.h"
#include "memfault/core/debug_log.h"
#include "memfault/core/event_storage.h"
#include "memfault/core/event_storage_implementation.h"
#include "memfault/core/platform/device_info.h"
#include "memfault/core/serializer_helper.h"
#include "memfault/core/serializer_key_ids.h"
#include "memfault/metrics/utils.h"
#include "memfault/util/cbor.h"

typedef struct {
  sMemfaultCborEncoder encoder;
  bool compute_worst_case_size;
  bool encode_success;
} sMemfaultSerializerState;

static bool prv_metric_heartbeat_writer(void *ctx, const sMemfaultMetricInfo *metric_info) {
  sMemfaultSerializerState *state = (sMemfaultSerializerState *)ctx;
  sMemfaultCborEncoder *encoder = &state->encoder;

  // encode the value
  switch (metric_info->type) {
    case kMemfaultMetricType_Timer:
    case kMemfaultMetricType_Unsigned: {
      const uint32_t value = state->compute_worst_case_size ? UINT32_MAX : metric_info->val.u32;
      state->encode_success = memfault_cbor_encode_unsigned_integer(encoder, value);
      break;
    }
    case kMemfaultMetricType_Signed: {
      const int32_t value = state->compute_worst_case_size ? INT32_MIN : metric_info->val.i32;
      state->encode_success = memfault_cbor_encode_signed_integer(encoder, value);
      break;
    }
    case kMemfaultMetricType_NumTypes: // silence error with -Wswitch-enum
    default:
      break;
  }

  // only continue iterating if the encode was successful
  return state->encode_success;
}

static bool prv_serialize_latest_heartbeat_and_deinit(sMemfaultSerializerState *state) {
  bool success = false;

  sMemfaultCborEncoder *encoder = &state->encoder;
  if (!memfault_serializer_helper_encode_metadata(encoder, kMemfaultEventType_Heartbeat)) {
    goto cleanup;
  }

  // Encode up to "metrics:" section
  if (!memfault_cbor_encode_unsigned_integer(encoder, kMemfaultEventKey_EventInfo) ||
      !memfault_cbor_encode_dictionary_begin(encoder, 1) ||
      !memfault_cbor_encode_unsigned_integer(encoder, kMemfaultHeartbeatInfoKey_Metrics) ||
      !memfault_cbor_encode_array_begin(encoder, memfault_metrics_heartbeat_get_num_metrics())) {
    goto cleanup;
  }

  memfault_metrics_heartbeat_iterate(prv_metric_heartbeat_writer, state);
  success = state->encode_success;

cleanup:
  return success;
}

static bool prv_encode_cb(MEMFAULT_UNUSED sMemfaultCborEncoder *encoder, void *ctx) {
  sMemfaultSerializerState *state = (sMemfaultSerializerState *)ctx;
  return prv_serialize_latest_heartbeat_and_deinit(state);
}

size_t memfault_metrics_heartbeat_compute_worst_case_storage_size(void) {
  sMemfaultSerializerState state = { .compute_worst_case_size = true };
  return memfault_serializer_helper_compute_size(&state.encoder, prv_encode_cb, &state);
}

bool memfault_metrics_heartbeat_serialize(const sMemfaultEventStorageImpl *storage_impl) {
  // Build a heartbeat event, which looks like this:
  // {
  //    "type": "heartbeat",
  //    "device_serial": "DAABBCCDD",
  //    "software_type": "main",
  //    "software_version": "1.2.3",
  //    "hardware_version": "evt_24",
  //    "event_info": {
  //         "metrics": {
  //          ... heartbeat metrics ...
  //    }
  // }
  // NOTE: "sdk_version" is not included, but derived from the CborSchemaVersion

  // NOTE: We'll always attempt to serialize the heartbeat and rollback if we are out of space
  // avoiding the need to serialize the data twice
  sMemfaultSerializerState state = { 0 };
  const bool success = memfault_serializer_helper_encode_to_storage(
      &state.encoder, storage_impl, prv_encode_cb, &state);

  return success;
}
