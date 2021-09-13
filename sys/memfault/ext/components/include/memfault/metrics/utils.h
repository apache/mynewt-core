#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! Utilities to assist with querying Memfault Metric info
//!
//! @note A user of the Memfault SDK should _never_ call any
//! of these routines directly

#include "memfault/metrics/metrics.h"

#ifdef __cplusplus
extern "C" {
#endif

union MemfaultMetricValue {
  uint32_t u32;
  int32_t i32;
};

typedef struct {
  MemfaultMetricId key;
  eMemfaultMetricType type;
  union MemfaultMetricValue val;
} sMemfaultMetricInfo;

//! The callback invoked when "memfault_metrics_heartbeat_iterate" is called
//!
//! @param ctx The context provided to "memfault_metrics_heartbeat_iterate"
//! @param metric_info Info for the particular metric being returned in the callback
//! See struct for more details
//!
//! @return bool to continue iterating, else false
typedef bool (*MemfaultMetricIteratorCallback)(void *ctx, const sMemfaultMetricInfo *metric_info);

void memfault_metrics_heartbeat_iterate(MemfaultMetricIteratorCallback cb, void *ctx);

//! @return the number of metrics being required
size_t memfault_metrics_heartbeat_get_num_metrics(void);

#ifdef __cplusplus
}
#endif
