#pragma once

//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief CLI console commands for Memfault demo apps
//!
//! The first element in argv is expected to be the command name that was invoked.
//! The elements following after that are expected to be the arguments to the command.

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//! Command to crash the device, for example, to trigger a coredump to be captured.
//! It takes one number argument, which is the crash type:
//! - 0: crash by MEMFAULT_ASSERT(0)
//! - 1: crash by jumping to 0xbadcafe
//! - 2: crash by unaligned memory store
int memfault_demo_cli_cmd_crash(int argc, char *argv[]);

//! Command to exercise the MEMFAULT_TRACE_EVENT API, capturing a
//! Trace Event with the error reason set to "MemfaultDemoCli_Error".
int memfault_demo_cli_cmd_trace_event_capture(int argc, char *argv[]);

//! Command to insert test logs into the RAM log buffer. One log for each log level
//! is written. The command takes no arguments.
//! @note By default, the minimum save level is >= Info. Use the
//! memfault_log_set_min_save_level() API to control this.
int memfault_demo_cli_cmd_test_log(int argc, char *argv[]);

//! Command to trigger "freeze" the current contents of the RAM log buffer and
//! allow them to be collected through the data transport (see memfault_demo_drain_chunk_data()).
//! It takes no arguments.
int memfault_demo_cli_cmd_trigger_logs(int argc, char *argv[]);

//! Command to get whether a coredump is currently stored and how large it is.
//! It takes no arguments.
int memfault_demo_cli_cmd_get_core(int argc, char *argv[]);

//! Command to post a coredump using the http client.
//! It takes no arguments.
int memfault_demo_cli_cmd_post_core(int argc, char *argv[]);

//! Command to print out the next Memfault data to send and print as a curl command.
//! It takes zero or one string argument, which can be:
//! - curl : (default) prints a shell command to post the next data chunk to Memfault's API (using echo, xxd and curl)
//! - hex : hexdumps the data
int memfault_demo_cli_cmd_print_chunk(int argc, char *argv[]);

//! Command to clear a coredump.
//! It takes no arguments.
int memfault_demo_cli_cmd_clear_core(int argc, char *argv[]);

//! Command to print device info, as obtained through memfault_platform_get_device_info().
//! It takes no arguments.
int memfault_demo_cli_cmd_get_device_info(int argc, char *argv[]);

//! Reboots the system
//!
//! This command takes no arguments and demonstrates how to use the reboot tracking module to
//! track the occurrence of intentional reboots.
int memfault_demo_cli_cmd_system_reboot(int argc, char *argv[]);

//! Drains _all_ queued up chunks by calling user_transport_send_chunk_data
//!
//! @note user_transport_send_chunk_data is defined as a weak function so it can be overriden.
//! The default implementation is a no-op.
//! @note When "memfault install_chunk_handler" has been run, this can be used as a way to post
//! chunks to the Memfault cloud directly from GDB. See https://mflt.io/posting-chunks-with-gdb
//! for more details
int memfault_demo_drain_chunk_data(int argc, char *argv[]);
void user_transport_send_chunk_data(void *chunk_data, size_t chunk_data_len);

//! Output base64 encoded chunks. Chunks can be uploaded via the Memfault CLI or
//! manually via the Chunks Debug in the UI.
int memfault_demo_cli_cmd_export(int argc, char *argv[]);

#ifdef __cplusplus
}
#endif
