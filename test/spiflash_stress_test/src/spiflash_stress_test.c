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

#include <inttypes.h>
#include "os/mynewt.h"
#include <console/console.h>
#include <hal/hal_bsp.h>
#include <hal/hal_flash.h>
#include <hal/hal_flash_int.h>
#include <shell/shell.h>
#include <stdio.h>
#include <string.h>

#define SPIFLASH_STRESS_TEST_TASK_COUNT 3

struct runtest_task {
    struct os_task task;
    char name[sizeof "taskX"];
    OS_TASK_STACK_DEFINE_NOSTATIC(stack, MYNEWT_VAL(SPIFLASH_STRESS_TEST_STACK_SIZE));
};

static struct runtest_task runtest_tasks[SPIFLASH_STRESS_TEST_TASK_COUNT];

static int runtest_next_task_idx;

struct task_cfg {
    uint8_t flash_area_id;
    uint32_t flash_area_offset;
    uint32_t flash_area_size;
    int increment;
    int pin;
} task_args[] = {
    { MYNEWT_VAL(SPIFLASH_STRESS_TEST_FLASH_AREA_ID), 0x00000, 0x01000, 1, 11 },
    { MYNEWT_VAL(SPIFLASH_STRESS_TEST_FLASH_AREA_ID), 0x02000, 0x06000, 7, 12 },
    { MYNEWT_VAL(SPIFLASH_STRESS_TEST_FLASH_AREA_ID), 0x08000, 0x08000, 13, 13 },
};


struct os_task *
runtest_init_task(os_task_func_t task_func, uint8_t prio)
{
    struct os_task *task = NULL;
#if 1
    os_stack_t *stack;
    char *name;
    int rc;

    if (runtest_next_task_idx >= SPIFLASH_STRESS_TEST_TASK_COUNT) {
        assert("No more test tasks");
        return NULL;
    }

    task = &runtest_tasks[runtest_next_task_idx].task;
    stack = runtest_tasks[runtest_next_task_idx].stack;
    name = runtest_tasks[runtest_next_task_idx].name;

    strcpy(name, "task");
    name[4] = '0' + runtest_next_task_idx;
    name[5] = '\0';

    rc = os_task_init(task, name, task_func, &task_args[runtest_next_task_idx],
                      prio, OS_WAIT_FOREVER, stack,
                      MYNEWT_VAL(SPIFLASH_STRESS_TEST_STACK_SIZE));
    assert(rc == 0);

#endif
    runtest_next_task_idx++;
    return task;
}

const uint8_t pattern[] = "1234567890 We choose to go to the moon in this decade and do the other things, not because they are easy, but because they are hard.";

struct os_sem sem;

void flash_test_task1(void *arg)
{
    const struct flash_area *fa;
    struct task_cfg *cfg = arg;
    int i;
    int increment;
    uint8_t pat_buf[sizeof(pattern)];

    g_current_task->t_pad = cfg->pin;
    if (flash_area_open(cfg->flash_area_id, &fa))
        return;

    while (1) {
        os_sem_pend(&sem, OS_TIMEOUT_NEVER);
        console_printf("Task %d starts\n", os_sched_get_current_task()->t_taskid);
        flash_area_erase(fa, cfg->flash_area_offset, cfg->flash_area_size);
        increment = cfg->increment;
        int chunk = 0;
        for (i = 0 ; i < cfg->flash_area_size; ) {
            chunk += increment;
            if (chunk < 0 || chunk > sizeof(pattern)) {
                increment = -increment;
                chunk += increment;
            }
            if (i + chunk > cfg->flash_area_size)
                chunk = cfg->flash_area_size - i;
            flash_area_write(fa, cfg->flash_area_offset + i, pattern, chunk);
            i += chunk;
            os_time_delay(1);
        }

        increment = cfg->increment;
        chunk = 0;
        for (i = 0 ; i < cfg->flash_area_size; ) {
            chunk += increment;
            if (chunk < 0 || chunk > sizeof(pattern)) {
                increment = -increment;
                chunk += increment;
            }
            memset(pat_buf, 0xDA, chunk);
            if (i + chunk > cfg->flash_area_size)
                chunk = cfg->flash_area_size - i;
            flash_area_read(fa, cfg->flash_area_offset + i, pat_buf, chunk);
            if (memcmp(pattern, pat_buf, chunk) != 0) {
                console_printf("Flast write/read failed\n");
            }
            os_time_delay(1);
            i += chunk;
        }
        console_printf("Task %d finished and waits for next start\n",
                       os_sched_get_current_task()->t_taskid);
    }
}

static int spiflash_stress_test_cli_cmd(int argc, char **argv);
static struct shell_cmd spiflash_stress_cmd_struct = {
    .sc_cmd = "flashstress",
    .sc_cmd_func = spiflash_stress_test_cli_cmd
};

static int
spiflash_stress_test_cli_cmd(int argc, char **argv)
{
    if (argc > 1 && (!strcmp(argv[1], "?") || !strcmp(argv[1], "help"))) {
        console_printf("Commands Available\n");
        console_printf("start\n");
        return 0;
    }

    os_sem_release(&sem);
    os_sem_release(&sem);
    os_sem_release(&sem);

    return 0;
}

/*
 * Initialize the package. Only called from sysinit().
 */
void
spiflash_stress_test_init(void)
{
    os_sem_init(&sem, 0);
    runtest_init_task(flash_test_task1, MYNEWT_VAL(OS_MAIN_TASK_PRIO) + 1);
    runtest_init_task(flash_test_task1, MYNEWT_VAL(OS_MAIN_TASK_PRIO) + 2);
    runtest_init_task(flash_test_task1, MYNEWT_VAL(OS_MAIN_TASK_PRIO) + 3);

    shell_cmd_register(&spiflash_stress_cmd_struct);
}
