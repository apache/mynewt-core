#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! Configuration settings available in the SDK.
//!
//! All settings can be set from the platform configuration file,
//! "memfault_platform_config.h". If no setting is specified, the default values
//! below will get picked up.
//!
//! The configuration file has three settings which can be overridden by adding a
//! compiler time define to your CFLAG list:
//!  1. MEMFAULT_PLATFORM_CONFIG_FILE can be used to change the default name of
//!     the platform configuration file, "memfault_platform_config.h"
//!  2. MEMFAULT_PLATFORM_CONFIG_DISABLED can be used to disable inclusion of
//!     a platform configuration file.
//!  3. MEMFAULT_PLATFORM_CONFIG_STRICT can be used to force a user of the SDK to
//!     explicitly define every configuration constant rather than pick up defaults.
//!     When the Wundef compiler option is used, any undefined configuration will
//!     be caught at compilation time.

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MEMFAULT_PLATFORM_CONFIG_FILE
#define MEMFAULT_PLATFORM_CONFIG_FILE "memfault_platform_config.h"
#endif

#ifndef MEMFAULT_PLATFORM_CONFIG_DISABLED
#include MEMFAULT_PLATFORM_CONFIG_FILE
#endif

#ifndef MEMFAULT_PLATFORM_CONFIG_STRICT
#define MEMFAULT_PLATFORM_CONFIG_STRICT 0
#endif

#if !MEMFAULT_PLATFORM_CONFIG_STRICT
#define MEMFAULT_PLATFORM_CONFIG_INCLUDE_DEFAULTS
#include "memfault/default_config.h"
#undef MEMFAULT_PLATFORM_CONFIG_INCLUDE_DEFAULTS
#endif

#ifdef __cplusplus
}
#endif
