/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "sysinit/sysinit.h"
#include "os/os.h"

#include "memfault/core/debug_log.h"
#include "memfault/core/event_storage.h"
#include "memfault/core/trace_event.h"
#include "memfault/metrics/metrics.h"
#include "memfault/metrics/platform/timer.h"
#include "memfault/panics/reboot_tracking.h"
#include "memfault/panics/platform/coredump.h"

#include "memfault_common.h"

/* Your .ld file:
 * MEMORY
 * {
 *     [...]
 *     NOINIT (rw) :  ORIGIN = <RAM_REGION_END>, LENGTH = 64
 * }
 * SECTIONS
 * {
 *     .noinit (NOLOAD): { KEEP(*(*.mflt_reboot_info)) } > NOINIT
 * }
 */
static uint8_t s_reboot_tracking[MEMFAULT_REBOOT_TRACKING_REGION_SIZE]
    __attribute__((section(".mflt_reboot_info")));

static struct os_callout metrics_callout;
static uint32_t metrics_period_sec;
static MemfaultPlatformTimerCallback *metrics_callback;

static void
metrics_callout_cb(struct os_event *ev)
{
    if (metrics_callback) {
        metrics_callback();
    }
    os_callout_reset(&metrics_callout,
                     os_time_ms_to_ticks32(metrics_period_sec * 1000));
}

void
memfault_platform_halt_if_debugging(void)
{
    if (hal_debugger_connected()) {
        __asm("bkpt");
    }
}

void
memfault_platform_reboot(void)
{
    os_reboot(HAL_RESET_REQUESTED);
    MEMFAULT_UNREACHABLE;
}

bool
memfault_platform_metrics_timer_boot(uint32_t period_sec,
                                     MemfaultPlatformTimerCallback callback)
{
    metrics_period_sec = period_sec;
    metrics_callback = callback;
    metrics_callout_cb(NULL);
    return true;
}

uint64_t
memfault_platform_get_time_since_boot_ms(void)
{
    return (uint64_t) (os_get_uptime_usec() / 1000);
}

void
memfault_platform_core_init(void)
{
    static uint8_t s_event_storage[MYNEWT_VAL(MEMFAULT_EVENT_STORAGE_SIZE)];
    int rc;

    SYSINIT_ASSERT_ACTIVE();
    os_callout_init(&metrics_callout, os_eventq_dflt_get(),
                    metrics_callout_cb, NULL);
    SYSINIT_PANIC_ASSERT(metrics_callout.c_evq != NULL);

    const sResetBootupInfo reset_reason = {
        .reset_reason_reg = NRF_POWER->RESETREAS,
    };

    memfault_reboot_tracking_boot(s_reboot_tracking, &reset_reason);
    /* Note: MCU reset reason register bits are usually "sticky" and
     * need to be cleared.
     */
    NRF_POWER->RESETREAS |= NRF_POWER->RESETREAS;

    const sMemfaultEventStorageImpl *evt_storage =
        memfault_events_storage_boot(s_event_storage, sizeof(s_event_storage));

    /* Pass the storage to collect reset info */
    memfault_reboot_tracking_collect_reset_info(evt_storage);
    /* Pass the storage to initialize the trace event module */
    memfault_trace_event_boot(evt_storage);

    /* NOTE: crash count represents the number of unexpected reboots since the
     * last heartbeat was reported. In the simplest case if reboots are
     * unexpected, this can just be set to 1 but see memfault/metrics/metrics.h
     * how this can be tracked with the reboot_tracking module.
     */
    sMemfaultMetricBootInfo boot_info = {
        .unexpected_reboot_count = memfault_reboot_tracking_get_crash_count()
    };
    rc = memfault_metrics_boot(evt_storage, &boot_info);
    /* NOTE: a non-zero value indicates a configuration error took place */
    SYSINIT_PANIC_ASSERT(rc == 0);

    memfault_reboot_tracking_reset_crash_count();

    sMfltCoredumpStorageInfo info;
    memfault_platform_coredump_storage_get_info(&info);

    shell_mflt_register();
}

