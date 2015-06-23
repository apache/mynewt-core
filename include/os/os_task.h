/**
 * Copyright (c) 2015 Stack Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _OS_TASK_H
#define _OS_TASK_H

#include "os/os.h"
#include "os/queue.h"

#ifndef OS_TASK_NAME_SIZE
#define OS_TASK_NAME_SIZE (36) 
#endif 

typedef enum os_task_state {
    OS_TASK_READY = 1, 
    OS_TASK_SLEEP = 2
} os_task_state_t;

/* Task flags */
#define OS_TASK_FLAG_NO_TIMEOUT     (0x0001U)

typedef void (*os_task_func_t)(void *);

struct os_task {
    os_stack_t *t_stackptr;
    uint16_t t_stacksize;
    uint16_t t_flags;

    uint8_t t_taskid;
    uint8_t t_prio;
    uint8_t t_pad[2];

    char *t_name;
    os_task_func_t t_func;
    void *t_arg;

    struct os_mutex *t_mutex;

    os_task_state_t t_state;
    os_time_t t_next_wakeup; 
    TAILQ_ENTRY(os_task) t_run_list;
    TAILQ_ENTRY(os_task) t_sleep_list;
    SLIST_ENTRY(os_task) t_mutex_list;
};

int os_task_init(struct os_task *, char *, os_task_func_t, void *, uint8_t, 
        os_stack_t *, uint16_t);


#endif /* _OS_TASK_H */
