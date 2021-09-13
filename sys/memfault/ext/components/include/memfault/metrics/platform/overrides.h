#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! Functions that can be optionally overriden by the user of the SDK.
//!
//! Default "weak function" stub definitions are provided for each of these functions
//! in the SDK itself

#ifdef __cplusplus
extern "C" {
#endif

//! Function invoked every time a heartbeat interval has passed
//!
//! It can (optionally) be overridden by the application to collect and update
//! the data for the heartbeat before it is serialized and stored.
void memfault_metrics_heartbeat_collect_data(void);

#ifdef __cplusplus
}
#endif
