#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! Thin wrapper around rom/uart which was refactored between the v3.x and v4.x esp-idf releases

#include "rom/uart.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MEMFAULT_ESP32_CONSOLE_UART_NUM CONFIG_CONSOLE_UART_NUM

#ifdef __cplusplus
}
#endif
