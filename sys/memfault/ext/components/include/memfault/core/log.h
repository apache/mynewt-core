#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//!
//! A lightweight set of log utilities which can be wrapped around pre-existing logging
//! infrastructure to capture events or errors that transpired leading up to an issue.
//! See https://mflt.io/logging for detailed integration steps.
//!
//! @note These utilities are already integrated into memfault/core/debug_log.h module. If your
//! project does not have a logging subsystem, see the notes in that header about how to leverage
//! the debug_log.h module for that!
//!
//! @note The thread-safety of the module depends on memfault_lock/unlock() API. If calls can be
//! made from multiple tasks, these APIs must be implemented. Locks are _only_ held while copying
//! data into the backing circular buffer so durations will be very quick.

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "memfault/core/compiler.h"
#include "memfault/core/platform/debug_log.h" // For eMemfaultPlatformLogLevel

#ifdef __cplusplus
extern "C" {
#endif

//! Must be called on boot to initialize the Memfault logging module
//!
//! @param buffer The ram buffer to save logs into
//! @param buf_len The length of the buffer. There's no length restriction but
//!  the more space that is available, the longer the trail of breadcrumbs that will
//!  be available upon crash
//!
//! @note Until this function is called, all other calls to the module will be no-ops
//! @return true if call was successful and false if the parameters were bad or
//!  memfault_log_boot has already been called
bool memfault_log_boot(void *buffer, size_t buf_len);

//! Change the minimum level log saved to the circular buffer
//!
//! By default, any logs >= kMemfaultPlatformLogLevel_Info can be saved
void memfault_log_set_min_save_level(eMemfaultPlatformLogLevel min_log_level);

//! Macro which can be called from a platforms pre-existing logging macro.
//!
//! For example, if your platform already has something like this
//!
//! #define YOUR_PLATFORM_LOG_ERROR(...)
//!   my_platform_log_error(__VA_ARGS__)
//!
//! the error data could be automatically recorded by making the
//! following modification
//!
//! #define YOUR_PLATFORM_LOG_ERROR(...)
//!  do {
//!    MEMFAULT_LOG_SAVE(kMemfaultPlatformLogLevel_Error, __VA_ARGS__);
//!    your_platform_log_error(__VA_ARGS__)
//! } while (0)
#define MEMFAULT_LOG_SAVE(_level, ...) memfault_log_save(_level, __VA_ARGS__)

//! Function which can be called to save a log after it has been formatted
//!
//! Typically a user should be able to use the MEMFAULT_LOG_SAVE macro but if your platform does
//! not have logging macros and you are just using newlib or dlib & printf, you could make
//! the following changes to the _write dependency function:
//!
//! int _write(int fd, char *ptr, int len) {
//!   // ... other code such as printing to console ...
//!   eMemfaultPlatformLogLevel level =
//!       (fd == 2) ? kMemfaultPlatformLogLevel_Error : kMemfaultPlatformLogLevel_Info;
//!   memfault_log_save_preformatted(level, ptr, len);
//!   // ...
//! }
void memfault_log_save_preformatted(eMemfaultPlatformLogLevel level, const char *log,
                                    size_t log_len);

//! Maximum length a log record can occupy
#define MEMFAULT_LOG_MAX_LINE_SAVE_LEN 128

typedef struct {
  // the level of the message
  eMemfaultPlatformLogLevel level;
  // the length of the msg (not including NUL character)
  uint32_t msg_len;
  // the message to print which will always be NUL terminated
  char msg[MEMFAULT_LOG_MAX_LINE_SAVE_LEN + 1 /* '\0' */];
} sMemfaultLog;

//! Returns the oldest unread log in memfault log storage
//!
//! @param log[out] When a new log is available, populated with its info
//! @return true if a log was found, false if there were no logs to read.
//!
//! @note For some timing sensitive applications, logs may be written into RAM and later dumped out
//! over UART and/or saved to flash on a lower priority background task. The memfault_log_read()
//! API is designed to be easy to utilize in these situations. For example:
//!
//! Any task:
//!   Call MEMFAULT_SAVE_LOG() to quickly write a log into RAM.
//!
//!   Optional: Anytime a new log is saved, memfault_log_handle_saved_callback is called by the
//!   memfault log module. A platform can choose to implement something like:
//!
//!   void memfault_log_handle_saved_callback(void) {
//!       my_rtos_schedule_log_read()
//!   }
//!
//! Task responsible for flushing logs out to slower mediums (UART, NOR/EMMC Flash, etc):
//!   // .. RTOS code to wait for log read event ..
//!   sMemfaultLog log = { 0 };
//!   const bool log_found = memfault_log_read(&log);
//!   if (log_found) {
//!       my_platform_uart_println(log.level, log, log.msg_len);
//!   }
bool memfault_log_read(sMemfaultLog *log);

//! Invoked every time a new log has been saved
//!
//! @note By default this is a weak function which behaves as a no-op. Platforms which dispatch
//! console logging to a low priority thread can implement this callback to have a "hook" from
//! where a job to drain new logs can easily be scheduled
extern void memfault_log_handle_saved_callback(void);

//! Formats the provided string and saves it to backing storage
//!
//! @note: Should only be called via MEMFAULT_LOG_SAVE macro
MEMFAULT_PRINTF_LIKE_FUNC(2, 3)
void memfault_log_save(eMemfaultPlatformLogLevel level, const char *fmt, ...);

//! Formats the provided string from a variable argument list
//!
//! @note Prefer saving logs via MEMFAULT_LOG_SAVE() when possible
MEMFAULT_PRINTF_LIKE_FUNC(2, 0)
void memfault_vlog_save(eMemfaultPlatformLogLevel level, const char *fmt, va_list args);

//! Freezes the contents of the log buffer in preparation of uploading the logs to Memfault.
//!
//! Once the log buffer contents have been uploaded, the buffer is unfrozen. While the buffer is
//! frozen, logs can still be added, granted enough space is available in the buffer. If the buffer
//! is full, newly logs will get dropped. Once the buffer is unfrozen again, the oldest logs will be
//! expunged again upon writing new logs that require the space.
//! @note This function must not be called from an ISR context.
void memfault_log_trigger_collection(void);

#ifdef __cplusplus
}
#endif
