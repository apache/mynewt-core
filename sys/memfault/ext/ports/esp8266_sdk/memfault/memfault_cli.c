//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! ESP8266 CLI commands to facilitate the Memfault SDK Integration

#include "sdkconfig.h"

#if CONFIG_MEMFAULT_CLI_ENABLED

#include "esp_console.h"
#include "esp_err.h"
#include "esp_system.h"

#include "esp_wifi.h"

#include "memfault/core/data_export.h"
#include "memfault/core/debug_log.h"
#include "memfault/core/math.h"
#include "memfault/core/platform/debug_log.h"
#include "memfault/core/trace_event.h"
#include "memfault/demo/cli.h"
#include "memfault/esp8266_port/http_client.h"
#include "memfault/http/platform/http_client.h"
#include "memfault/metrics/metrics.h"
#include "memfault/panics/assert.h"
#include "memfault/panics/coredump.h"
#include "memfault/panics/platform/coredump.h"

static void IRAM_ATTR prv_recursive_crash(int depth) {
  if (depth == 15) {
    MEMFAULT_ASSERT_RECORD(depth);
  }

  // an array to create some stack depth variability
  int dummy_array[depth + 1];
  for (size_t i = 0; i < MEMFAULT_ARRAY_SIZE(dummy_array); i++) {
    dummy_array[i] = (depth << 24) | i;
  }
  dummy_array[depth] = depth + 1;
  prv_recursive_crash(dummy_array[depth]);
}

void prv_check1(const void *buf) {
  MEMFAULT_ASSERT_RECORD(sizeof(buf));
}

void prv_check2(const void *buf) {
  uint8_t buf2[200];
  prv_check1(buf2);
}

void prv_check3(const void *buf) {
  uint8_t buf3[300];
  prv_check2(buf3);
}

void prv_check4(void) {
  uint8_t buf4[400];
  prv_check3(buf4);
}

static int prv_crash_example(int argc, char** argv) {
  int crash_type =  0;

  if (argc >= 2) {
    crash_type = atoi(argv[1]);
  }

  if (crash_type == 0) {
    void (*bad_func_call)(void) = (void *)0xbadcafe;
    bad_func_call();
  } else if (crash_type == 1) {
    ESP_ERROR_CHECK(10);
  } else if (crash_type == 2) {
    prv_recursive_crash(0);
  } else if (crash_type == 3) {
    prv_check4();
  }
  return 0;
}

static int prv_post_memfault_data(int argc, char **argv) {
  return memfault_esp_port_http_client_post_data();
}

static int  prv_export_memfault_data(int agrc, char **argv) {
  memfault_data_export_dump_chunks();
  return 0;
}

static int prv_get_core_cmd(int argc, char **argv) {
  size_t total_size = 0;
  if (!memfault_coredump_has_valid_coredump(&total_size)) {
    MEMFAULT_LOG_INFO("No coredump present!");
    return 0;
  }
  MEMFAULT_LOG_INFO("Has coredump with size: %d", (int)total_size);
  return 0;
}

static int prv_clear_core_cmd(int argc, char **argv) {
  memfault_platform_coredump_storage_clear();
  return 0;
}

static int prv_collect_metric_data(int argc, char **argv) {
  memfault_metrics_heartbeat_debug_trigger();
  return 0;
}

static int prv_dump_metric_data(int argc, char **argv) {
  memfault_metrics_heartbeat_debug_print();
  return 0;
}

static int prv_coredump_storage_test(int argc, char **argv) {
#if CONFIG_MEMFAULT_CLI_COREDUMP_STORAGE_TEST_CMD
  // Storage needs to work even if interrupts are disabled
  vPortEnterCritical();
  memfault_coredump_storage_debug_test_begin();
  const bool success = memfault_coredump_storage_debug_test_finish();
  vPortExitCritical();

  return success ? 0 : -1;
#else
  MEMFAULT_LOG_INFO("Disabled. Set CONFIG_MEMFAULT_CLI_COREDUMP_STORAGE_TEST_CMD=y");
  return -1;
#endif
}

static int prv_trace_event_test(int argc, char **argv) {
  MEMFAULT_TRACE_EVENT_WITH_LOG(MemfaultCli_Test, "Num Args: %d", argc);
  return 0;
}

void memfault_register_cli(void) {
  ESP_ERROR_CHECK( esp_console_cmd_register(&(esp_console_cmd_t) {
      .command = "crash",
      .help = "Trigger a crash to test coredump collection",
      .hint = NULL,
      .func = prv_crash_example,
  }));

  ESP_ERROR_CHECK( esp_console_cmd_register(&(esp_console_cmd_t) {
      .command = "get_core",
      .help = "Get coredump info",
      .hint = NULL,
      .func = prv_get_core_cmd,
  }));

 ESP_ERROR_CHECK( esp_console_cmd_register(&(esp_console_cmd_t) {
      .command = "test_core_storage",
      .help = "Test that data can be written to coredump storage region",
      .hint = NULL,
      .func = prv_coredump_storage_test,
  }));


  ESP_ERROR_CHECK( esp_console_cmd_register(&(esp_console_cmd_t) {
      .command = "trace",
      .help = "Generate a test trace event",
      .hint = NULL,
      .func = prv_trace_event_test,
  }));

  ESP_ERROR_CHECK( esp_console_cmd_register(&(esp_console_cmd_t) {
      .command = "clear_core",
      .help = "Invalidate Coredump",
      .hint = NULL,
      .func = prv_clear_core_cmd,
  }));

  ESP_ERROR_CHECK( esp_console_cmd_register(&(esp_console_cmd_t) {
      .command = "get_device_info",
      .help = "Display device information",
      .hint = NULL,
      .func = memfault_demo_cli_cmd_get_device_info,
  }));

  ESP_ERROR_CHECK( esp_console_cmd_register(&(esp_console_cmd_t) {
      .command = "post_chunks",
      .help = "Post Memfault data to cloud",
      .hint = NULL,
      .func = prv_post_memfault_data,
  }));

  ESP_ERROR_CHECK( esp_console_cmd_register(&(esp_console_cmd_t) {
      .command = "export_data",
      .help = "Extract Memfault data via CLI",
      .hint = NULL,
      .func = prv_export_memfault_data,
  }));

  ESP_ERROR_CHECK( esp_console_cmd_register(&(esp_console_cmd_t) {
      .command = "dump_metrics",
      .help = "Dump current Memfault Metrics via CLI",
      .hint = NULL,
      .func = prv_dump_metric_data,
  }));

  ESP_ERROR_CHECK( esp_console_cmd_register(&(esp_console_cmd_t) {
      .command = "collect_metrics",
      .help = "Force the generation of a metric event",
      .hint = NULL,
      .func = prv_collect_metric_data,
  }));
}

#endif /* MEMFAULT_CLI_ENABLED */
