/**
 * Copyright (c) 2015 Runtime Inc.
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
#include "os/os_sanity.h" 
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
#define OS_TASK_FLAG_SEM_WAIT       (0x0002U)

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

    struct os_sanity_check t_sanity_check; 

    os_task_state_t t_state;
    os_time_t t_next_wakeup;
    
    /* Used to chain task to either the run or sleep list */ 
    TAILQ_ENTRY(os_task) t_os_list;

    /* Used to chain task to an object such as a semaphore or mutex */
    SLIST_ENTRY(os_task) t_obj_list;
};

int os_task_init(struct os_task *, char *, os_task_func_t, void *, uint8_t,
        os_time_t, os_stack_t *, uint16_t);

int os_task_sanity_checkin(struct os_task *);


#endif /* _OS_TASK_H */
