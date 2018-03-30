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

#ifndef _OS_H
#define _OS_H

#include <stdlib.h>
#include <inttypes.h>

#include <syscfg/syscfg.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef min
#define min(a, b) ((a)<(b)?(a):(b))
#endif

#ifndef max
#define max(a, b) ((a)>(b)?(a):(b))
#endif

#define os_get_return_addr() (__builtin_return_address(0))

#define OS_ALIGN(__n, __a) (                             \
        (((__n) & ((__a) - 1)) == 0)                   ? \
            (__n)                                      : \
            ((__n) + ((__a) - ((__n) & ((__a) - 1))))    \
        )


#define CTASSERT(x) typedef int __ctasssert ## __LINE__[(x) ? 1 : -1]


/**
 * @cond INTERNAL_HIDDEN
 */
/**
 * Whether or not the operating system has been started.  Set to
 * 1 right before first task is run.
 */
extern int g_os_started;

int os_info_init(void);

/* XXX: Not sure if this should go here; I want to differentiate API that
 * should be called by application developers as those that should not. */
void os_init_idle_task(void);
/**
 * @endcond
 */

/**
 * Check whether or not the OS has been started.
 *
 * @return 1 if the OS has been started and 0 if it has not yet been started.
 */
int os_started(void);

/**
 * Definition used for functions that take timeouts to specify
 * waiting indefinitely.
 */
#define OS_WAIT_FOREVER (-1)

/**
 * Priority of the IDLE task.  Always the lowest priority task in teh system.
 */
#define OS_IDLE_PRIO (0xff)

/**
 * Main task priority, defined by sysconfig.
 */
#define OS_MAIN_TASK_PRIO       MYNEWT_VAL(OS_MAIN_TASK_PRIO)
/**
 * Main task stack size, defined by sysconfig.
 */
#define OS_MAIN_STACK_SIZE      MYNEWT_VAL(OS_MAIN_STACK_SIZE)

/**
 * Initialize the OS, including memory areas and housekeeping functions.
 * This calls into the architecture specific OS initialization.
 *
 * @param fn The system "main" function to start the main task with.
 */
void os_init(int (*fn)(int argc, char **argv));

/**
 * Start the OS and begin processing.
 */
void os_start(void);

#include "os/endian.h"
#include "os/os_callout.h"
#include "os/os_cfg.h"
#include "os/os_cputime.h"
#include "os/os_dev.h"
#include "os/os_error.h"
#include "os/os_eventq.h"
#include "os/os_fault.h"
#include "os/os_heap.h"
#include "os/os_mbuf.h"
#include "os/os_mempool.h"
#include "os/os_mutex.h"
#include "os/os_sanity.h"
#include "os/os_sched.h"
#include "os/os_sem.h"
#include "os/os_task.h"
#include "os/os_test.h"
#include "os/os_time.h"
#include "os/os_trace_api.h"
#include "os/queue.h"

#ifdef __cplusplus
}
#endif

#endif /* _OS_H */
