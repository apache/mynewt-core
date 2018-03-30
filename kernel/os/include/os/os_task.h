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


/**
 * @addtogroup OSKernel
 * @{
 *   @defgroup OSTask Tasks
 *   @{
 */

#ifndef _OS_TASK_H
#define _OS_TASK_H

#include "os/os.h"
#include "os/os_sanity.h"
#include "os/os_arch.h"
#include "os/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OS_TASK_STACK_DEFINE(__name, __size) \
    static os_stack_t __name [OS_STACK_ALIGN(__size)] \
        __attribute__((aligned(OS_STACK_ALIGNMENT)));

/** Highest priority task */
#define OS_TASK_PRI_HIGHEST (0)
/** Lowest priority task */
#define OS_TASK_PRI_LOWEST  (0xff)

/*
 * Generic "object" structure. All objects that a task can wait on must
 * have a SLIST_HEAD(, os_task) head_name as the first element in the object
 * structure. The element 'head_name' can be any name. See os_mutex.h or
 * os_sem.h for an example.
 */
struct os_task_obj {
    SLIST_HEAD(, os_task) obj_head;     /* chain of waiting tasks */
};

/** Task states */
typedef enum os_task_state {
    /** Task is ready to run */
    OS_TASK_READY = 1,
    /** Task is sleeping */
    OS_TASK_SLEEP = 2,
} os_task_state_t;

/* Task flags */
#define OS_TASK_FLAG_NO_TIMEOUT     (0x01U)
/** Task waiting on a semaphore */
#define OS_TASK_FLAG_SEM_WAIT       (0x02U)
/** Task waiting on a mutex */
#define OS_TASK_FLAG_MUTEX_WAIT     (0x04U)
/** Task waiting on a event queue */
#define OS_TASK_FLAG_EVQ_WAIT       (0x08U)

typedef void (*os_task_func_t)(void *);

#define OS_TASK_MAX_NAME_LEN (32)

/**
 * Structure containing information about a running task
 */
struct os_task {
    /** Current stack pointer for this task */
    os_stack_t *t_stackptr;
    /** Pointer to top of this task's stack */
    os_stack_t *t_stacktop;
    /** Size of this task's stack */
    uint16_t t_stacksize;
    /** Task ID */
    uint8_t t_taskid;
    /** Task Priority */
    uint8_t t_prio;
    /* Task state, either READY or SLEEP */
    uint8_t t_state;
    /** Task flags, bitmask */
    uint8_t t_flags;
    uint8_t t_lockcnt;
    uint8_t t_pad;

    /** Task name */
    const char *t_name;
    /** Task function that executes */
    os_task_func_t t_func;
    /** Argument to pass to task function when called */
    void *t_arg;

    /** Current object task is waiting on, either a semaphore or mutex */
    void *t_obj;

    /** Default sanity check for this task */
    struct os_sanity_check t_sanity_check;

    /** Next scheduled wakeup if this task is sleeping */
    os_time_t t_next_wakeup;
    /** Total task run time */
    os_time_t t_run_time;
    /**
     * Total number of times this task has been context switched during
     * execution.
     */
    uint32_t t_ctx_sw_cnt;

    STAILQ_ENTRY(os_task) t_os_task_list;
    TAILQ_ENTRY(os_task) t_os_list;
    SLIST_ENTRY(os_task) t_obj_list;
};

/** @cond INTERNAL_HIDDEN */
STAILQ_HEAD(os_task_stailq, os_task);

extern struct os_task_stailq g_os_task_list;
/** @endcond */

/**
 * Initialize a task.
 *
 * This function initializes the task structure pointed to by t,
 * clearing and setting it's stack pointer, provides sane defaults
 * and sets the task as ready to run, and inserts it into the operating
 * system scheduler.
 *
 * @param t The task to initialize
 * @param name The name of the task to initialize
 * @param func The task function to call
 * @param arg The argument to pass to this task function
 * @param prio The priority at which to run this task
 * @param sanity_itvl The time at which this task should check in with the
 *                    sanity task.  OS_WAIT_FOREVER means never check in
 *                    here.
 * @param stack_bottom A pointer to the bottom of a task's stack
 * @param stack_size The overall size of the task's stack.
 *
 * @return 0 on success, non-zero on failure.
 */
int os_task_init(struct os_task *, const char *, os_task_func_t, void *,
        uint8_t, os_time_t, os_stack_t *, uint16_t);

/**
 * Removes specified task
 * XXX
 * NOTE: This interface is currently experimental and not ready for common use
 */
int os_task_remove(struct os_task *t);

/**
 * Return the number of tasks initialized.
 *
 * @return number of tasks initialized
 */
uint8_t os_task_count(void);

/**
 * Information about an individual task, returned for management APIs.
 */
struct os_task_info {
    /** Task priority */
    uint8_t oti_prio;
    /** Task identifier */
    uint8_t oti_taskid;
    /** Task state, either READY or SLEEP */
    uint8_t oti_state;
    /** Task stack usage */
    uint16_t oti_stkusage;
    /** Task stack size */
    uint16_t oti_stksize;
    /** Task context switch count */
    uint32_t oti_cswcnt;
    /** Task runtime */
    uint32_t oti_runtime;
    /** Last time this task checked in with sanity */
    os_time_t oti_last_checkin;
    /** Next time this task is scheduled to check-in with sanity */
    os_time_t oti_next_checkin;
    /** Name of this task */
    char oti_name[OS_TASK_MAX_NAME_LEN];
};

/**
 * Iterate through tasks, and return the following information about them:
 *
 * - Priority
 * - Task ID
 * - State (READY, SLEEP)
 * - Total Stack Usage
 * - Stack Size
 * - Context Switch Count
 * - Runtime
 * - Last & Next Sanity checkin
 * - Task Name
 *
 * To get the first task in the list, call os_task_info_get_next() with a
 * NULL pointer in the prev argument, and os_task_info_get_next() will
 * return a pointer to the task structure, and fill out the os_task_info
 * structure pointed to by oti.
 *
 * To get the next task in the list, provide the task structure returned
 * by the previous call to os_task_info_get_next(), and os_task_info_get_next()
 * will fill out the task structure pointed to by oti again, and return
 * the next task in the list.
 *
 * @param prev The previous task returned by os_task_info_get_next(), or NULL
 *             to begin iteration.
 * @param oti  The OS task info structure to fill out.
 *
 * @return A pointer to the OS task that has been read, or NULL when finished
 *         iterating through all tasks.
 */
struct os_task *os_task_info_get_next(const struct os_task *,
        struct os_task_info *);

#ifdef __cplusplus
}
#endif

#endif /* _OS_TASK_H */


/**
 *   @} OSTask
 * @} OSKernel
 */
