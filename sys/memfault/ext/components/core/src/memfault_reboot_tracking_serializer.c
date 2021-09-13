//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! Reads the current reboot tracking information and converts it into an "trace" event which can
//! be sent to the Memfault cloud

#include "memfault_reboot_tracking_private.h"

#include <string.h>

#include "memfault/core/data_packetizer_source.h"
#include "memfault/core/debug_log.h"
#include "memfault/core/event_storage.h"
#include "memfault/core/event_storage_implementation.h"
#include "memfault/core/platform/device_info.h"
#include "memfault/core/serializer_helper.h"
#include "memfault/core/serializer_key_ids.h"
#include "memfault/util/cbor.h"

#define MEMFAULT_REBOOT_TRACKING_BAD_PARAM (-1)
#define MEMFAULT_REBOOT_TRACKING_STORAGE_TOO_SMALL (-2)

static bool prv_serialize_reboot_info(sMemfaultCborEncoder *e,
                                      const sMfltResetReasonInfo *info) {
  const size_t extra_event_info_pairs = 1 /* coredump_saved */ +
                                        ((info->reset_reason_reg0 != 0) ? 1 : 0);
  const sMemfaultTraceEventHelperInfo helper_info = {
      .reason_key = kMemfaultTraceInfoEventKey_Reason,
      .reason_value = info->reason,
      .pc = info->pc,
      .lr = info->lr,
      .extra_event_info_pairs = extra_event_info_pairs,
  };
  bool success = memfault_serializer_helper_encode_trace_event(e, &helper_info);

  if (success && info->reset_reason_reg0) {
    success = memfault_serializer_helper_encode_uint32_kv_pair(e,
        kMemfaultTraceInfoEventKey_McuReasonRegister, info->reset_reason_reg0);
  }

  if (success) {
    success = memfault_serializer_helper_encode_uint32_kv_pair(
        e, kMemfaultTraceInfoEventKey_CoredumpSaved, info->coredump_saved);
  }

  return success;
}

static bool prv_encode_cb(sMemfaultCborEncoder *encoder, void *ctx) {
  const sMfltResetReasonInfo *info = (const sMfltResetReasonInfo *)ctx;
  return prv_serialize_reboot_info(encoder, info);
}

size_t memfault_reboot_tracking_compute_worst_case_storage_size(void) {
  // a reset reason with maximal values so we can compute the worst case encoding size
  sMfltResetReasonInfo reset_reason = {
    .reason = kMfltRebootReason_HardFault,
    .pc = UINT32_MAX,
    .lr = UINT32_MAX,
    .reset_reason_reg0 = UINT32_MAX,
    .coredump_saved = 1,
  };

  sMemfaultCborEncoder encoder = { 0 };
  return memfault_serializer_helper_compute_size(&encoder, prv_encode_cb, &reset_reason);
}

int memfault_reboot_tracking_collect_reset_info(const sMemfaultEventStorageImpl *impl) {
  if (impl == NULL) {
    return MEMFAULT_REBOOT_TRACKING_BAD_PARAM;
  }

  memfault_serializer_helper_check_storage_size(
      impl, memfault_reboot_tracking_compute_worst_case_storage_size, "reboot");
  // we'll fall through and try to encode anyway and later return a failure
  // code if the event could not be stored. This line is here to give the user
  // an idea of how they should size things

  sMfltResetReasonInfo info;
  if (!memfault_reboot_tracking_read_reset_info(&info)) {
    // Two ways we get here:
    //  1. memfault_reboot_tracking_boot() has not yet been called
    //  2. memfault_reboot_tracking_boot() was called but there's no info
    //     about the last reboot reason. To fix this, pass bootup_info when
    //     calling memfault_reboot_tracking_boot()
    // For more details about reboot tracking in general see https://mflt.io//2QlOlgH
    MEMFAULT_LOG_WARN("%s: No reset info collected", __func__);
    return 0;
  }

  sMemfaultCborEncoder encoder = { 0 };
  const bool success = memfault_serializer_helper_encode_to_storage(
      &encoder, impl, prv_encode_cb, &info);

  if (!success) {
    const size_t storage_max_size = impl->get_storage_size_cb();
    const size_t worst_case_size_needed =
        memfault_reboot_tracking_compute_worst_case_storage_size();
    MEMFAULT_LOG_WARN("Event storage (%d) smaller than largest reset reason (%d)",
                      (int)storage_max_size, (int)worst_case_size_needed);
    return MEMFAULT_REBOOT_TRACKING_STORAGE_TOO_SMALL;
  }

  memfault_reboot_tracking_clear_reset_info();
  return 0;
}
