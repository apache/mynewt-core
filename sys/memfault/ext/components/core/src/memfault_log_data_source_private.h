#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! Internal logging data source

#include <stdbool.h>

#include "memfault/util/cbor.h"

#ifdef __cplusplus
extern "C" {
#endif

//! @note Internal function
bool memfault_log_data_source_has_been_triggered(void);

//! Reset the state of the logging data source
//!
//! @note Internal function only intended for use with unit tests
void memfault_log_data_source_reset(void);

//! @note Internal function only intended for use with unit tests
size_t memfault_log_data_source_count_unsent_logs(void);

#ifdef __cplusplus
}
#endif
