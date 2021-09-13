#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//! ESP32 Memfault Demo Cli Port

#ifdef __cplusplus
extern "C" {
#endif

//! Call on boot to enabled the mflt demo cli
//! See https://mflt.io/demo-cli for more info
void memfault_register_cli(void);

#ifdef __cplusplus
}
#endif
