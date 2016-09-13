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

#include "syscfg/syscfg.h"
#include <os/os.h>
#include <bsp/bsp.h>
#include <hal/hal_gpio.h>
#include <hal/hal_flash.h>
#include <console/console.h>
#include <shell/shell.h>
#include <log/log.h>
#include <stats/stats.h>
#include <config/config.h>
#include <hal/flash_map.h>
#include <hal/hal_system.h>
#if MYNEWT_PKG_FS_FS
#include <fs/fs.h>
#include <nffs/nffs.h>
#include <config/config_file.h>
#elif MYNEWT_PKG_SYS_FCB
#include <fcb/fcb.h>
#include <config/config_fcb.h>
#else
#error "Need NFFS or FCB for config storage"
#endif
#include <newtmgr/newtmgr.h>
#include <bootutil/image.h>
#include <bootutil/bootutil_misc.h>
#include <imgmgr/imgmgr.h>
#include <assert.h>
#include <string.h>
#include <json/json.h>
#include <flash_test/flash_test.h>
#include <reboot/log_reboot.h>
#include <os/os_time.h>
#include <id/id.h>

#ifdef ARCH_sim
#include <mcu/mcu_sim.h>
#endif

/* Init all tasks */
volatile int tasks_initialized;
int init_tasks(void);

/* Task 1 */
#define TASK1_PRIO (8)
#define TASK1_STACK_SIZE    OS_STACK_ALIGN(192)
#define MAX_CBMEM_BUF 600
struct os_task task1;
os_stack_t stack1[TASK1_STACK_SIZE];
static volatile int g_task1_loops;

/* Task 2 */
#define TASK2_PRIO (9)
#define TASK2_STACK_SIZE    OS_STACK_ALIGN(128)
struct os_task task2;
os_stack_t stack2[TASK2_STACK_SIZE];

struct log_handler log_cbmem_handler;
struct log my_log;

static volatile int g_task2_loops;

/* Global test semaphore */
struct os_sem g_test_sem;

/* For LED toggling */
int g_led_pin;

STATS_SECT_START(gpio_stats)
STATS_SECT_ENTRY(toggles)
STATS_SECT_END

STATS_SECT_DECL(gpio_stats) g_stats_gpio_toggle;

STATS_NAME_START(gpio_stats)
STATS_NAME(gpio_stats, toggles)
STATS_NAME_END(gpio_stats)

static char *test_conf_get(int argc, char **argv, char *val, int max_len);
static int test_conf_set(int argc, char **argv, char *val);
static int test_conf_commit(void);
static int test_conf_export(void (*export_func)(char *name, char *val),
  enum conf_export_tgt tgt);

static struct conf_handler test_conf_handler = {
    .ch_name = "test",
    .ch_get = test_conf_get,
    .ch_set = test_conf_set,
    .ch_commit = test_conf_commit,
    .ch_export = test_conf_export
};

static uint8_t test8;
static uint8_t test8_shadow;
static char test_str[32];
static uint32_t cbmem_buf[MAX_CBMEM_BUF];
struct cbmem cbmem;

static char *
test_conf_get(int argc, char **argv, char *buf, int max_len)
{
    if (argc == 1) {
        if (!strcmp(argv[0], "8")) {
            return conf_str_from_value(CONF_INT8, &test8, buf, max_len);
        } else if (!strcmp(argv[0], "str")) {
            return test_str;
        }
    }
    return NULL;
}

static int
test_conf_set(int argc, char **argv, char *val)
{
    if (argc == 1) {
        if (!strcmp(argv[0], "8")) {
            return CONF_VALUE_SET(val, CONF_INT8, test8_shadow);
        } else if (!strcmp(argv[0], "str")) {
            return CONF_VALUE_SET(val, CONF_STRING, test_str);
        }
    }
    return OS_ENOENT;
}

static int
test_conf_commit(void)
{
    test8 = test8_shadow;

    return 0;
}

static int
test_conf_export(void (*func)(char *name, char *val), enum conf_export_tgt tgt)
{
    char buf[4];

    conf_str_from_value(CONF_INT8, &test8, buf, sizeof(buf));
    func("test/8", buf);
    func("test/str", test_str);
    return 0;
}

void
task1_handler(void *arg)
{
    struct os_task *t;
    int prev_pin_state, curr_pin_state;
    struct image_version ver;

    /* Set the led pin for the E407 devboard */
    g_led_pin = LED_BLINK_PIN;
    hal_gpio_init_out(g_led_pin, 1);

    if (imgr_my_version(&ver) == 0) {
        console_printf("\nSlinky %u.%u.%u.%u\n",
          ver.iv_major, ver.iv_minor, ver.iv_revision,
          (unsigned int)ver.iv_build_num);
    } else {
        console_printf("\nSlinky\n");
    }

    while (1) {
        t = os_sched_get_current_task();
        assert(t->t_func == task1_handler);

        ++g_task1_loops;

        /* Wait one second */
        os_time_delay(1000);

        /* Toggle the LED */
        prev_pin_state = hal_gpio_read(g_led_pin);
        curr_pin_state = hal_gpio_toggle(g_led_pin);
        LOG_INFO(&my_log, LOG_MODULE_DEFAULT, "GPIO toggle from %u to %u",
            prev_pin_state, curr_pin_state);
        STATS_INC(g_stats_gpio_toggle, toggles);

        /* Release semaphore to task 2 */
        os_sem_release(&g_test_sem);
    }
}

void
task2_handler(void *arg)
{
    struct os_task *t;

    while (1) {
        /* just for debug; task 2 should be the running task */
        t = os_sched_get_current_task();
        assert(t->t_func == task2_handler);

        /* Increment # of times we went through task loop */
        ++g_task2_loops;

        /* Wait for semaphore from ISR */
        os_sem_pend(&g_test_sem, OS_TIMEOUT_NEVER);
    }
}

/**
 * init_tasks
 *
 * Called by main.c after os_init(). This function performs initializations
 * that are required before tasks are running.
 *
 * @return int 0 success; error otherwise.
 */
int
init_tasks(void)
{
    /* Initialize global test semaphore */
    os_sem_init(&g_test_sem, 0);

    os_task_init(&task1, "task1", task1_handler, NULL,
            TASK1_PRIO, OS_WAIT_FOREVER, stack1, TASK1_STACK_SIZE);

    os_task_init(&task2, "task2", task2_handler, NULL,
            TASK2_PRIO, OS_WAIT_FOREVER, stack2, TASK2_STACK_SIZE);

    tasks_initialized = 1;
    return 0;
}

#if !MYNEWT_VAL(CONFIG_NFFS)

static void
setup_for_fcb(void)
{
    int cnt;
    int rc;

    rc = flash_area_to_sectors(FLASH_AREA_NFFS, &cnt, NULL);
    assert(rc == 0);
    assert(cnt <= sizeof(conf_fcb_area) / sizeof(conf_fcb_area[0]));
    flash_area_to_sectors(FLASH_AREA_NFFS, &cnt, conf_fcb_area);

    my_conf.cf_fcb.f_sector_cnt = cnt;

    rc = conf_fcb_src(&my_conf);
    if (rc) {
        for (cnt = 0; cnt < my_conf.cf_fcb.f_sector_cnt; cnt++) {
            flash_area_erase(&conf_fcb_area[cnt], 0,
              conf_fcb_area[cnt].fa_size);
        }
        rc = conf_fcb_src(&my_conf);
    }
    assert(rc == 0);
    rc = conf_fcb_dst(&my_conf);
    assert(rc == 0);
}

#endif

/**
 * main
 *
 * The main function for the project. This function initializes the os, calls
 * init_tasks to initialize tasks (and possibly other objects), then starts the
 * OS. We should not return from os start.
 *
 * @return int NOTE: this function should never return!
 */
int
main(int argc, char **argv)
{
    int rc;

#ifdef ARCH_sim
    mcu_sim_parse_args(argc, argv);
#endif

    os_init();

    rc = conf_register(&test_conf_handler);
    assert(rc == 0);

    cbmem_init(&cbmem, cbmem_buf, MAX_CBMEM_BUF);
    log_cbmem_handler_init(&log_cbmem_handler, &cbmem);
    log_register("log", &my_log, &log_cbmem_handler);

#if !MYNEWT_VAL(CONFIG_NFFS)
    setup_for_fcb();
#endif

    stats_init(STATS_HDR(g_stats_gpio_toggle),
               STATS_SIZE_INIT_PARMS(g_stats_gpio_toggle, STATS_SIZE_32),
               STATS_NAME_INIT_PARMS(gpio_stats));

    stats_register("gpio_toggle", STATS_HDR(g_stats_gpio_toggle));

    flash_test_init();

    conf_load();

    log_reboot(HARD_REBOOT);

    rc = init_tasks();

    os_start();

    /* os start should never return. If it does, this should be an error */
    assert(0);

    return rc;
}

