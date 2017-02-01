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

#include <stddef.h>

#include "sysinit/sysinit.h"
#include "os/os.h"

#include "shell_priv.h"
#include "shell.h"

#define SHELL_TASK_PRIO         (3)
#define SHELL_STACK_SIZE        (512)
static struct os_task shell_task;
static os_stack_t shell_task_stack[SHELL_STACK_SIZE];
#define SHELL_PROMPT "shell> "

static void
init_task(void)
{
    /* Initialize the task */
    os_task_init(&shell_task, "shell", shell, NULL,
                 SHELL_TASK_PRIO, OS_WAIT_FOREVER,
                 shell_task_stack, SHELL_STACK_SIZE);
    shell_init(SHELL_PROMPT);
}

int
main(int argc, char **argv)
{
    int rc;

    sysinit();
    init_task();

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
    /* Never exit */

    return rc;
}
