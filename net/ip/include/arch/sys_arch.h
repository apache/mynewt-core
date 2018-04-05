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
#ifndef __LWIP_SYS_ARCH_H__
#define __LWIP_SYS_ARCH_H__

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#undef LITTLE_ENDIAN
#include "os/mynewt.h"
#include <ip/os_queue.h>

#define SYS_MBOX_NULL NULL
#define SYS_SEM_NULL  NULL

struct os_queue;

typedef struct os_sem sys_sem_t;
typedef struct os_mutex sys_mutex_t;
typedef struct os_queue sys_mbox_t;
typedef struct os_task * sys_thread_t;
typedef os_stack_t portSTACK_TYPE;

typedef int sys_prot_t;

static inline int
sys_arch_protect(void)
{
    int sr;

    OS_ENTER_CRITICAL(sr);
    return sr;
}

static inline void
sys_arch_unprotect(int prev_sr)
{
    OS_EXIT_CRITICAL(prev_sr);
}

static inline void
sys_init(void)
{
}

static inline err_t
sys_sem_new(sys_sem_t *sem, u8_t count)
{
    if (os_sem_init(sem, count)) {
        return ERR_VAL;
    }
    return ERR_OK;
}

static inline void
sys_sem_signal(sys_sem_t *sem)
{
    os_sem_release(sem);
}

static inline err_t
sys_mutex_new(sys_mutex_t *mutex)
{
    if (os_mutex_init(mutex)) {
        return ERR_VAL;
    }
    return ERR_OK;
}

static inline void
sys_mutex_lock(sys_mutex_t *mutex)
{
    os_mutex_pend(mutex, OS_WAIT_FOREVER);
}

static inline void
sys_mutex_unlock(sys_mutex_t *mutex)
{
    os_mutex_release(mutex);
}

static inline uint32_t
sys_now(void)
{
    uint64_t t;

    t = os_time_get();
    return t * 1000 / OS_TICKS_PER_SEC;
}

static inline err_t
sys_mbox_new(sys_mbox_t *mbox, int size)
{
    if (os_queue_init(mbox, sizeof(void *), size)) {
        return ERR_MEM;
    }
    return ERR_OK;
}

static inline void
sys_mbox_post(sys_mbox_t *mbox, void *msg)
{
    os_queue_put(mbox, &msg, OS_WAIT_FOREVER);
}

static inline err_t
sys_mbox_trypost(sys_mbox_t *mbox, void *msg)
{
    if (os_queue_put(mbox, &msg, 0)) {
        return ERR_MEM;
    }
    return ERR_OK;
}

static inline sys_thread_t
sys_thread_new(const char *name, void (*thread)(void *arg), void *arg,
  int stacksize, int prio)
{
    struct os_task *task;
    os_stack_t *stack;

    task = (struct os_task *)os_malloc(sizeof(*task));
    assert(task);
    stack = (os_stack_t *)os_malloc(stacksize * sizeof(os_stack_t));
    assert(stack);
    if (os_task_init(task, name, thread, arg, prio, OS_WAIT_FOREVER, stack,
        stacksize)) {
        assert(0);
    }
    return task;
}

#ifdef __cplusplus
}
#endif

#endif /* __LWIP_SYS_ARCH_H__ */

