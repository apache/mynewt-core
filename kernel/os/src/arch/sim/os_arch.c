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

#include "os/mynewt.h"
#include "sim/sim.h"

/*
 * Assert that 'sf_mainsp' and 'sf_jb' are at the specific offsets where
 * os_arch_frame_init() expects them to be.
 */
CTASSERT(offsetof(struct stack_frame, sf_mainsp) == 0);
CTASSERT(offsetof(struct stack_frame, sf_jb) == 4);

void
os_arch_task_start(struct stack_frame *sf, int rc)
{
    sim_task_start(sf, rc);
}

os_stack_t *
os_arch_task_stack_init(struct os_task *t, os_stack_t *stack_top, int size)
{
    return sim_task_stack_init(t, stack_top, size);
}

os_error_t
os_arch_os_start(void)
{
    return sim_os_start();
}

void
os_arch_os_stop(void)
{
    sim_os_stop();
}

os_error_t
os_arch_os_init(void)
{
    return sim_os_init();
}

void
os_arch_ctx_sw(struct os_task *next_t)
{
    sim_ctx_sw(next_t);
}

os_sr_t
os_arch_save_sr(void)
{
    return sim_save_sr();
}

void
os_arch_restore_sr(os_sr_t osr)
{
    sim_restore_sr(osr);
}

int
os_arch_in_critical(void)
{
    return sim_in_critical();
}

void
os_tick_idle(os_time_t ticks)
{
    sim_tick_idle(ticks);
}

void
__assert_func(const char *file, int line, const char *func, const char *e)
{
    OS_PRINT_ASSERT_SIM(file, line, func, e);
    _Exit(1);
}
