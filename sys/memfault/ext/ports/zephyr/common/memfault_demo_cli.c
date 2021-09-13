//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! Adds a basic set of commands for interacting with Memfault SDK

#include "memfault/demo/cli.h"

#include <shell/shell.h>

#include "memfault/core/data_export.h"
#include "memfault/core/debug_log.h"
#include "memfault/core/platform/core.h"
#include "memfault/core/reboot_tracking.h"
#include "memfault/core/trace_event.h"
#include "memfault/metrics/metrics.h"
#include "memfault/ports/zephyr/http.h"

static int prv_clear_core_cmd(const struct shell *shell, size_t argc, char **argv) {
  return memfault_demo_cli_cmd_clear_core(argc, argv);
}

static int prv_get_core_cmd(const struct shell *shell, size_t argc, char **argv) {
  return memfault_demo_cli_cmd_get_core(argc, argv);
}

static int prv_crash_example(const struct shell *shell, size_t argc, char **argv) {
  return memfault_demo_cli_cmd_crash(argc, argv);
}

static int prv_test_log(const struct shell *shell, size_t argc, char **argv) {
  return memfault_demo_cli_cmd_test_log(argc, argv);
}

static int prv_trigger_logs(const struct shell *shell, size_t argc, char **argv) {
  return memfault_demo_cli_cmd_trigger_logs(argc, argv);
}

static int prv_get_device_info(const struct shell *shell, size_t argc, char **argv) {
  return memfault_demo_cli_cmd_get_device_info(argc, argv);
}

static int prv_hang_example(const struct shell *shell, size_t argc, char **argv) {
#if !CONFIG_WATCHDOG
  MEMFAULT_LOG_WARN("No watchdog configured, this will hang forever");
#else
  MEMFAULT_LOG_DEBUG("Hanging system and waiting for watchdog!");
#endif
  while (1) { }
  return -1;
}

static int prv_chunk_data_export(const struct shell *shell, size_t argc, char **argv) {
  memfault_data_export_dump_chunks();
  return 0;
}

static int prv_example_trace_event_capture(const struct shell *shell, size_t argc, char **argv) {
  // For more information on user-defined error reasons, see
  // the MEMFAULT_TRACE_REASON_DEFINE macro in trace_reason_user.h .
  MEMFAULT_TRACE_EVENT_WITH_LOG(MemfaultCli_Test, "Num args: %d", (int)argc);
  MEMFAULT_LOG_DEBUG("Trace Event Generated!");
  return 0;
}

static int prv_post_data(const struct shell *shell, size_t argc, char **argv) {
  // For more information on user-defined error reasons, see
  // the MEMFAULT_TRACE_REASON_DEFINE macro in trace_reason_user.h .
#if defined(CONFIG_MEMFAULT_HTTP_ENABLE)
  MEMFAULT_LOG_INFO("Posting Memfault Data");
  return memfault_zephyr_port_post_data();
#else
  shell_print(shell, "CONFIG_MEMFAULT_HTTP_ENABLE not enabled");
  return 0;
#endif
}

#if defined(CONFIG_MEMFAULT_HTTP_ENABLE)
typedef struct {
  const struct shell *shell;
  size_t total_size;
} sMemfaultShellOtaDownloadCtx;

static bool prv_handle_update_available(const sMemfaultOtaInfo *info, void *user_ctx) {
  sMemfaultShellOtaDownloadCtx *ctx = (sMemfaultShellOtaDownloadCtx *)user_ctx;
  shell_print(ctx->shell, "Downloading OTA payload, size=%d bytes", (int)info->size);
  return true;
}

static bool prv_handle_data(void *buf, size_t buf_len, void *user_ctx) {
  // this is an example cli command so we just drop the data on the floor
  // a real implementation could save the data in this callback!
  return true;
}

static bool prv_handle_download_complete(void *user_ctx) {
  sMemfaultShellOtaDownloadCtx *ctx = (sMemfaultShellOtaDownloadCtx *)user_ctx;
  shell_print(ctx->shell, "OTA download complete!");
  return true;
}
#endif /* CONFIG_MEMFAULT_HTTP_ENABLE */

static int prv_check_and_fetch_ota_payload_cmd(const struct shell *shell, size_t argc, char **argv) {
#if defined(CONFIG_MEMFAULT_HTTP_ENABLE)
  uint8_t working_buf[256];

  sMemfaultShellOtaDownloadCtx user_ctx = {
    .shell = shell,
  };

  sMemfaultOtaUpdateHandler handler = {
    .buf = working_buf,
    .buf_len = sizeof(working_buf),
    .user_ctx = &user_ctx,
    .handle_update_available = prv_handle_update_available,
    .handle_data = prv_handle_data,
    .handle_download_complete = prv_handle_download_complete,
  };

  shell_print(shell, "Checking for OTA update");
  int rv = memfault_zephyr_port_ota_update(&handler);
  if (rv == 0) {
    shell_print(shell, "Up to date!");
  } else if (rv < 0) {
    shell_print(shell, "OTA update failed, rv=%d, errno=%d", rv, errno);
  }
  return rv;
#else
  shell_print(shell, "CONFIG_MEMFAULT_HTTP_ENABLE not enabled");
  return 0;
#endif
}

static int prv_trigger_heartbeat(const struct shell *shell, size_t argc, char **argv) {
#if CONFIG_MEMFAULT_METRICS
  shell_print(shell, "Triggering Heartbeat");
  memfault_metrics_heartbeat_debug_trigger();
  return 0;
#else
  shell_print(shell, "CONFIG_MEMFAULT_METRICS not enabled");
  return 0;
#endif
}

static int prv_test_reboot(const struct shell *shell, size_t argc, char **argv) {
  memfault_reboot_tracking_mark_reset_imminent(kMfltRebootReason_UserReset, NULL);
  memfault_platform_reboot();
  return 0; // should be unreachable
}

SHELL_STATIC_SUBCMD_SET_CREATE(
    sub_memfault_cmds,
    SHELL_CMD(reboot, NULL, "trigger a reboot and record it using memfault", prv_test_reboot),
    SHELL_CMD(get_core, NULL, "gets the core", prv_get_core_cmd),
    SHELL_CMD(clear_core, NULL, "clear the core", prv_clear_core_cmd),
    SHELL_CMD(crash, NULL, "trigger a crash", prv_crash_example),
    SHELL_CMD(test_log, NULL, "Writes test logs to log buffer", prv_test_log),
    SHELL_CMD(trigger_logs, NULL, "Trigger capture of current log buffer contents", prv_trigger_logs),
    SHELL_CMD(hang, NULL, "trigger a hang to test watchdog functionality", prv_hang_example),
    SHELL_CMD(export, NULL, "dump chunks collected by Memfault SDK using https://mflt.io/chunk-data-export", prv_chunk_data_export),
    SHELL_CMD(trace, NULL, "Capture an example trace event", prv_example_trace_event_capture),
    SHELL_CMD(get_device_info, NULL, "display device information", prv_get_device_info),
    SHELL_CMD(post_chunks, NULL, "Post Memfault data to cloud", prv_post_data),
    SHELL_CMD(trigger_heartbeat, NULL, "Trigger an immediate capture of all heartbeat metrics", prv_trigger_heartbeat),
    SHELL_CMD(get_latest_release, NULL, "checks to see if new ota payload is available", prv_check_and_fetch_ota_payload_cmd),
    SHELL_SUBCMD_SET_END /* Array terminated. */
);

SHELL_CMD_REGISTER(mflt, &sub_memfault_cmds, "Memfault Test Commands", NULL);
