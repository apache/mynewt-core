//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! ESP32 CLI implementation for demo application

#include "driver/periph_ctrl.h"
#include "driver/timer.h"
#include "esp_console.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_wifi.h"

#include "memfault/config.h"
#include "memfault/core/data_export.h"
#include "memfault/core/debug_log.h"
#include "memfault/core/math.h"
#include "memfault/core/platform/debug_log.h"
#include "memfault/demo/cli.h"
#include "memfault/esp_port/cli.h"
#include "memfault/esp_port/http_client.h"
#include "memfault/http/platform/http_client.h"
#include "memfault/metrics/metrics.h"
#include "memfault/panics/assert.h"
#include "memfault/panics/platform/coredump.h"

#define TIMER_DIVIDER         16  //  Hardware timer clock divider
#define TIMER_SCALE_TICKS_PER_MS    ((TIMER_BASE_CLK / TIMER_DIVIDER) / 1000)  // convert counter value to milliseconds

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

static void IRAM_ATTR prv_timer_group0_isr(void *para) {
  // Always clear the interrupt:
  TIMERG0.int_clr_timers.t0 = 1;

  // Crash from ISR:
  ESP_ERROR_CHECK(-1);
}

static void prv_timer_init(void)
{
  const timer_config_t config = {
      .divider = TIMER_DIVIDER,
      .counter_dir = TIMER_COUNT_UP,
      .counter_en = TIMER_PAUSE,
      .alarm_en = TIMER_ALARM_EN,
      .intr_type = TIMER_INTR_LEVEL,
      .auto_reload = false,
  };
  timer_init(TIMER_GROUP_0, TIMER_0, &config);
  timer_enable_intr(TIMER_GROUP_0, TIMER_0);
  timer_isr_register(TIMER_GROUP_0, TIMER_0, prv_timer_group0_isr, NULL, ESP_INTR_FLAG_IRAM, NULL);
}

static void prv_timer_start(uint32_t timer_interval_ms) {
  timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0x00000000ULL);
  timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, timer_interval_ms * TIMER_SCALE_TICKS_PER_MS);
  timer_set_alarm(TIMER_GROUP_0, TIMER_0, TIMER_ALARM_EN);
  timer_start(TIMER_GROUP_0, TIMER_0);
}

static int prv_esp32_crash_example(int argc, char** argv) {
  int crash_type =  0;

  if (argc >= 2) {
    crash_type = atoi(argv[1]);
  }

  if (crash_type == 0) {
    ESP_ERROR_CHECK(10);
  } else if (crash_type == 2) {
    // Crash in timer ISR:
    prv_timer_start(10);
  } else if (crash_type == 3) {
    prv_recursive_crash(0);
  } else if (crash_type == 4) {
    prv_check4();
  }
  return 0;
}

static int prv_esp32_memfault_heartbeat_dump(int argc, char** argv) {
  memfault_metrics_heartbeat_debug_print();
  return 0;
}

static bool prv_wifi_connected_check(const char *op) {
  if (memfault_esp_port_wifi_connected()) {
    return true;
  }

  MEMFAULT_LOG_ERROR("Must be connected to WiFi to %s. Use 'join <ssid> <pass>'", op);
  return false;
}

#if MEMFAULT_ESP_HTTP_CLIENT_ENABLE

typedef struct {
  bool perform_ota;
} sMemfaultOtaUserCtx;

static bool prv_handle_ota_upload_available(void *user_ctx) {
  sMemfaultOtaUserCtx *ctx = (sMemfaultOtaUserCtx *)user_ctx;
  MEMFAULT_LOG_DEBUG("OTA Update Available");

  if (ctx->perform_ota) {
    MEMFAULT_LOG_INFO("Starting OTA download ...");
  }
  return ctx->perform_ota;
}

static bool prv_handle_ota_download_complete(void *user_ctx) {
  MEMFAULT_LOG_INFO("OTA Update Complete, Rebooting System");
  esp_restart();
  return true;
}

static int prv_memfault_ota(sMemfaultOtaUserCtx *ctx) {
  if (!prv_wifi_connected_check("perform an OTA")) {
    return -1;
  }

  sMemfaultOtaUpdateHandler handler = {
    .user_ctx = ctx,
    .handle_update_available = prv_handle_ota_upload_available,
    .handle_download_complete = prv_handle_ota_download_complete,
  };

  MEMFAULT_LOG_DEBUG("Checking for OTA Update");

  int rv = memfault_esp_port_ota_update(&handler);
  if (rv == 0) {
    MEMFAULT_LOG_DEBUG("Up to date!");
  } else if (rv < 0) {
    MEMFAULT_LOG_ERROR("OTA update failed, rv=%d", rv);
  }

  return rv;
}

static int prv_memfault_ota_perform(int argc, char **argv) {
  sMemfaultOtaUserCtx user_ctx = {
    .perform_ota = true,
  };
  return prv_memfault_ota(&user_ctx);
}

static int prv_memfault_ota_check(int argc, char **argv) {
  sMemfaultOtaUserCtx user_ctx = {
    .perform_ota = false,
  };
  return prv_memfault_ota(&user_ctx);
}

static int prv_post_memfault_data(int argc, char **argv) {
  return memfault_esp_port_http_client_post_data();
}
#endif /* MEMFAULT_ESP_HTTP_CLIENT_ENABLE */

static int prv_chunk_data_export(int argc, char **argv) {
  memfault_data_export_dump_chunks();
  return 0;
}

void memfault_register_cli(void) {
  prv_timer_init();

  ESP_ERROR_CHECK( esp_console_cmd_register(&(esp_console_cmd_t) {
      .command = "crash",
      .help = "Trigger a crash",
      .hint = "crash type",
      .func = memfault_demo_cli_cmd_crash,
  }));

  ESP_ERROR_CHECK( esp_console_cmd_register(&(esp_console_cmd_t) {
      .command = "esp_crash",
      .help = "Trigger a timer isr crash",
      .hint = NULL,
      .func = prv_esp32_crash_example,
  }));

  ESP_ERROR_CHECK( esp_console_cmd_register(&(esp_console_cmd_t) {
      .command = "test_log",
      .help = "Writes test logs to log buffer",
      .hint = NULL,
      .func = memfault_demo_cli_cmd_test_log,
  }));

  ESP_ERROR_CHECK( esp_console_cmd_register(&(esp_console_cmd_t) {
      .command = "trigger_logs",
      .help = "Trigger capture of current log buffer contents",
      .hint = NULL,
      .func = memfault_demo_cli_cmd_trigger_logs,
  }));

  ESP_ERROR_CHECK( esp_console_cmd_register(&(esp_console_cmd_t) {
      .command = "clear_core",
      .help = "Clear an existing coredump",
      .hint = NULL,
      .func = memfault_demo_cli_cmd_clear_core,
  }));

  ESP_ERROR_CHECK( esp_console_cmd_register(&(esp_console_cmd_t) {
      .command = "get_core",
      .help = "Get coredump info",
      .hint = NULL,
      .func = memfault_demo_cli_cmd_get_core,
  }));

  ESP_ERROR_CHECK( esp_console_cmd_register(&(esp_console_cmd_t) {
      .command = "get_device_info",
      .help = "Display device information",
      .hint = NULL,
      .func = memfault_demo_cli_cmd_get_device_info,
  }));

  ESP_ERROR_CHECK( esp_console_cmd_register(&(esp_console_cmd_t) {
      .command = "print_chunk",
      .help = "Get next Memfault data chunk to send and print as a curl command",
      .hint = "curl | hex",
      .func = memfault_demo_cli_cmd_print_chunk,
  }));
  ESP_ERROR_CHECK( esp_console_cmd_register(&(esp_console_cmd_t) {
      .command = "export",
      .help = "Can be used to dump chunks to console or post via GDB",
      .hint = NULL,
      .func = prv_chunk_data_export,
  }));

  ESP_ERROR_CHECK( esp_console_cmd_register(&(esp_console_cmd_t) {
      .command = "heartbeat_dump",
      .help = "Dump current Memfault metrics heartbeat state",
      .hint = NULL,
      .func = prv_esp32_memfault_heartbeat_dump,
  }));

#if MEMFAULT_ESP_HTTP_CLIENT_ENABLE
  ESP_ERROR_CHECK( esp_console_cmd_register(&(esp_console_cmd_t) {
      .command = "post_chunks",
      .help = "Post Memfault data to cloud",
      .hint = NULL,
      .func = prv_post_memfault_data,
  }));

  ESP_ERROR_CHECK( esp_console_cmd_register(&(esp_console_cmd_t) {
      .command = "memfault_ota_check",
      .help = "Checks Memfault to see if a new OTA is available",
      .hint = NULL,
      .func = prv_memfault_ota_check,
  }));

  ESP_ERROR_CHECK( esp_console_cmd_register(&(esp_console_cmd_t) {
      .command = "memfault_ota_perform",
      .help = "Performs an OTA is an updates is available from Memfault",
      .hint = NULL,
      .func = prv_memfault_ota_perform,
  }));
#endif /* MEMFAULT_ESP_HTTP_CLIENT_ENABLE */
}
