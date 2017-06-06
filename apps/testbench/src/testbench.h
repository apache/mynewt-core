/**
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
#ifndef _H_TESTBENCH
#define _H_TESTBENCH

#include "syscfg/syscfg.h"
#include "sysinit/sysinit.h"
#include "sysflash/sysflash.h"

#include <os/os.h>
#include <bsp/bsp.h>
#include <hal/hal_gpio.h>
#include <hal/hal_flash.h>
#include <console/console.h>
#if MYNEWT_VAL(SHELL_TASK)
#include <shell/shell.h>
#endif
#include <log/log.h>
#include <stats/stats.h>
#include <config/config.h>
#include "flash_map/flash_map.h"
#include <hal/hal_system.h>
#if MYNEWT_VAL(SPLIT_LOADER)
#include "split/split.h"
#endif
#include <bootutil/image.h>
#include <bootutil/bootutil.h>
#include <imgmgr/imgmgr.h>
#include <assert.h>
#include <string.h>
#include <json/json.h>
#include <reboot/log_reboot.h>
#include <os/os_time.h>
#include <id/id.h>

#include "testutil/testutil.h"

#if MYNEWT_VAL(CONFIG_NFFS)
#include <nffs/nffs.h>
#include "nffs/nffs_test.h"
#endif

#if MYNEWT_VAL(CONFIG_FCB)
#include <fcb/fcb.h>
#endif

#include "os/os_test.h"
#include "bootutil/bootutil_test.h"

#include <stddef.h>
#include <config/config_file.h>
#include "mbedtls/mbedtls_test.h"

#ifdef __cplusplus
#extern "C" {
#endif

#ifndef IMGMGR_HASH_LEN
#define IMGMGR_HASH_LEN 32
#endif

extern char image_id[IMGMGR_HASH_LEN * 2 + 1];

/* For LED toggling */
int g_led_pin;

#if MYNEWT_VAL(CONFIG_NFFS)
/*
 * NFFS
 */
int testbench_nffs();
extern struct nffs_area_desc *nffs_current_area_descs;
extern struct log nffs_log; /* defined in the OS module */
#endif

#if 0 /* not ready for this yet */
#if MYNEWT_VAL(CONFIG_FCB)
static struct flash_area conf_fcb_area[NFFS_AREA_MAX + 1];

static struct conf_fcb my_conf = {
    .cf_fcb.f_magic = 0xc09f6e5e,
    .cf_fcb.f_sectors = conf_fcb_area
};
#endif
#endif

/*
 * Test log cbmem buf
 */
extern uint32_t *cbmem_buf;
extern struct cbmem cbmem;

extern struct log testlog;

#define TESTBENCH_BUILDID_SZ 64
extern char buildID[TESTBENCH_BUILDID_SZ];

/*
 * defaults if not specified
 */
#ifndef BUILD_ID
#define BUILD_ID "1.2.3.4"
#endif

#ifndef BUILD_TARGET
#define BUILD_TARGET    "ARDUINO_ZERO"
#endif

/*
 * Pool of test worker tasks and matching stacks
 */
struct os_task task1;
struct os_task task2;
struct os_task task3;
struct os_task task4;

/*
 * stacks are re-used to minimize space on the target
 * Set the size to the biggest we'll need to run tests
 * We assume that no more than 4 test tasks are needed
 */
#define TESTHANDLER_STACK_SIZE  OS_STACK_ALIGN(256)

extern os_stack_t *stack1;
#define TASK1_STACK_SIZE TESTHANDLER_STACK_SIZE

extern os_stack_t *stack2;
#define TASK2_STACK_SIZE TESTHANDLER_STACK_SIZE

extern os_stack_t *stack3;
#define TASK3_STACK_SIZE TESTHANDLER_STACK_SIZE

extern os_stack_t *stack4;
#define TASK4_STACK_SIZE TESTHANDLER_STACK_SIZE

/*
 * Generic routines for testsuite and testcase callbacks
 */
void testbench_ts_init(void *arg);
void testbench_ts_pretest(void* arg);
void testbench_ts_posttest(void* arg);
void testbench_ts_pass(char *msg, void *arg);
void testbench_ts_fail(char *msg, void *arg);

void testbench_tc_pretest(void* arg);
void testbench_tc_postest(void* arg);

#define TESTBENCH_TOD_DELAY    1

/* XXX hack to allow the idle task to run and update the TOD */
#define TESTBENCH_UPDATE_TOD os_time_delay(TESTBENCH_TOD_DELAY)

#ifdef __cplusplus
}
#endif

#endif /* _H_TESTBENCH */
