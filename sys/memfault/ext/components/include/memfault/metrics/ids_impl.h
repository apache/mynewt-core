#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! NOTE: The internals of the metric APIs make use of "X-Macros" to enable more flexibility
//! improving and extending the internal implementation without impacting the externally facing API

#ifdef __cplusplus
extern "C" {
#endif

#include "memfault/config.h"

//! Generate extern const char * declarations for all IDs (used in key names):
#define MEMFAULT_METRICS_KEY_DEFINE_WITH_RANGE(key_name, value_type, _min, _max) \
  MEMFAULT_METRICS_KEY_DEFINE(key_name, value_type)

#define MEMFAULT_METRICS_KEY_DEFINE(key_name, value_type) \
  extern const char * const g_memfault_metrics_id_##key_name;
#include "memfault/metrics/heartbeat_config.def"
#include MEMFAULT_METRICS_USER_HEARTBEAT_DEFS_FILE
#undef MEMFAULT_METRICS_KEY_DEFINE
#undef MEMFAULT_METRICS_KEY_DEFINE_WITH_RANGE

//! Generate an enum for all IDs (used for indexing into values)
#define MEMFAULT_METRICS_KEY_DEFINE_WITH_RANGE(key_name, value_type, min_value, max_value) \
  MEMFAULT_METRICS_KEY_DEFINE(key_name, value_type)

#define MEMFAULT_METRICS_KEY_DEFINE(key_name, value_type) \
  kMfltMetricsIndex_##key_name,

typedef enum MfltMetricsIndex {
  #include "memfault/metrics/heartbeat_config.def"
  #include MEMFAULT_METRICS_USER_HEARTBEAT_DEFS_FILE
  #undef MEMFAULT_METRICS_KEY_DEFINE
  #undef MEMFAULT_METRICS_KEY_DEFINE_WITH_RANGE
} eMfltMetricsIndex;

#define _MEMFAULT_METRICS_KEY_DEFINE(key_name, value_type) \
  MEMFAULT_STATIC_ASSERT(false, \
    "MEMFAULT_METRICS_KEY_DEFINE should only be used in " MEMFAULT_METRICS_USER_HEARTBEAT_DEFS_FILE)

//! NOTE: Access to a key should _always_ be made via the MEMFAULT_METRICS_KEY() macro to ensure source
//! code compatibility with future APIs updates
//!
//! The struct wrapper does not have any function, except for preventing one from passing a C
//! string to the API:
typedef struct {
  int _impl;
} MemfaultMetricId;

#define _MEMFAULT_METRICS_ID_CREATE(id) \
  { kMfltMetricsIndex_##id }

#define _MEMFAULT_METRICS_ID(id) \
  ((MemfaultMetricId) { kMfltMetricsIndex_##id })

#ifdef __cplusplus
}
#endif
