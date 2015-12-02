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

#include "os/os.h"

#include "os/queue.h"

#include "console/console.h"
#ifdef SHELL_PRESENT
#include "shell/shell.h" 
#endif

#include <string.h>

struct os_task_info_walk {
    struct os_task_info *info;
    int info_cnt;
};

#define OS_TASK_INFO_NAME_SIZE (32)
struct os_task_info {
    uint8_t oti_tid;
    uint8_t oti_prio;
    uint8_t oti_state;
    uint8_t oti_pad1;
    uint16_t oti_flags;
    uint16_t oti_stack_size;
    uint32_t oti_csw_cnt;
    os_time_t oti_next_wakeup;
    os_time_t oti_run_time;
    char oti_name[OS_TASK_INFO_NAME_SIZE];
};

int os_task_info_get(struct os_task_info *info, int info_cnt);

#ifdef SHELL_PRESENT
struct shell_cmd shell_os_tasks_display_cmd;
#endif


#ifdef SHELL_PRESENT
/* Code to register with shell, and display the results of os_getinfo() 
 */


int 
shell_os_tasks_display(int argc, char **argv)
{
    struct os_task_info *info; 
    char *name;
    int i;
    int found;
    int rc;
    uint8_t tcount;

    name = NULL;
    found = 0;

    if (argc > 1 && strcmp(argv[1], "")) {
        name = argv[1];
    }   

    tcount = os_task_count();

    info = (struct os_task_info *) os_malloc(
            sizeof(struct os_task_info) * tcount);
    if (!info) {
        rc = -1;
        goto err;
    }

    rc = os_task_info_get(info, tcount);

    console_printf("%d tasks: \n", rc);
    for (i = 0; i < rc; i++) {
        if (name) {
            if (strcmp(name, info[i].oti_name)) {
                continue;
            } else {
                found = 1;
            }
        }

        console_printf("  %s (prio: %u, nw: %u, flags: 0x%x, "
                "ssize: %u, cswcnt: %lu, tot_run_time: %ums)\n", 
                info[i].oti_name, 
                info[i].oti_prio, info[i].oti_next_wakeup, info[i].oti_flags, 
                info[i].oti_stack_size,
                info[i].oti_csw_cnt, info[i].oti_run_time);

    }

    if (name && !found) {
        console_printf("Couldn't find task with name %s\n", name);
    }

    os_free(info);

    return (0);
err:
    return (rc);
}
    

#endif 

static int
_os_task_copy_info(struct os_task *t, void *arg)
{
    struct os_task_info_walk *walk;

    walk = (struct os_task_info_walk *) arg;

    if (walk->info_cnt == 0) {
        /* Stored all the elements we can fit, exit out */
        return (1);
    }

    walk->info->oti_tid = t->t_taskid;
    walk->info->oti_prio = t->t_prio;
    strncpy(walk->info->oti_name, t->t_name, OS_TASK_INFO_NAME_SIZE);
    walk->info->oti_state = (uint8_t) t->t_state;
    walk->info->oti_next_wakeup = t->t_next_wakeup;
    walk->info->oti_flags = t->t_flags;
    walk->info->oti_stack_size = t->t_stacksize;
    walk->info->oti_csw_cnt = t->t_ctx_sw_cnt;
    walk->info->oti_run_time = t->t_run_time;

    walk->info += 1;
    walk->info_cnt--;

    return (0);
}

int
os_task_info_get(struct os_task_info *info, int info_cnt)
{
    struct os_task_info_walk walk;

    walk.info = info;
    walk.info_cnt = info_cnt;

    os_sched_walk(_os_task_copy_info, (void *) &walk);
    return (info_cnt - walk.info_cnt);
}


int 
os_info_init(void)
{
#ifdef SHELL_PRESENT 
    shell_cmd_register(&shell_os_tasks_display_cmd, "tasks", 
            shell_os_tasks_display);
#endif
    return (0);
}
