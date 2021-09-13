//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
// Logging depends on how your configuration does logging. See
// https://docs.memfault.com/docs/mcu/self-serve/#logging-dependency

#ifdef __cplusplus
extern "C" {
#endif

//! @file memfault_platform_log_config.h
#pragma once

// #define MEMFAULT_LOG_DEBUG(fmt, ...) LOG(DEBUG, fmt, ## __VA_ARGS__)
// #define MEMFAULT_LOG_INFO(fmt, ...)  LOG(INFO,  fmt, ## __VA_ARGS__)
// #define MEMFAULT_LOG_WARN(fmt, ...)  LOG(WARN, fmt, ## __VA_ARGS__)
// #define MEMFAULT_LOG_ERROR(fmt, ...) LOG(ERROR, fmt, ## __VA_ARGS__)

#define MEMFAULT_LOG_DEBUG(fmt, ...) LOG(CRITICAL, fmt, ## __VA_ARGS__)
#define MEMFAULT_LOG_INFO(fmt, ...)  LOG(CRITICAL,  fmt, ## __VA_ARGS__)
#define MEMFAULT_LOG_WARN(fmt, ...)  LOG(CRITICAL, fmt, ## __VA_ARGS__)
#define MEMFAULT_LOG_ERROR(fmt, ...) LOG(CRITICAL, fmt, ## __VA_ARGS__)

#ifdef __cplusplus
}
#endif
