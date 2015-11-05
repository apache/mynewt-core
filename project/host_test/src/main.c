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
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "os/os.h"
#include "nimble/ble.h"
#include "host/host_hci.h"
#include "host/ble_hs.h"

#define HOST_TASK_PRIO (1) 
#define HOST_STACK_SIZE (OS_STACK_ALIGN(4096))

static struct os_task host_task;
static os_stack_t host_stack[HOST_STACK_SIZE];

static void
host_test_task_handler(void *arg)
{
    int rc;

    host_init();

    while (1) {
        os_time_delay(1000);
        if (!ble_host_listen_enabled) {
            rc = ble_host_send_data_connectionless(1, 4, (uint8_t *)"BLAH", 4);
            printf("ble_host_send_data_connectionless(); rc=%d\n", rc);
        } else {
            ble_host_poll();
        }
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
    os_task_init(&host_task, "host", host_test_task_handler, NULL, 
            HOST_TASK_PRIO, OS_WAIT_FOREVER, host_stack, HOST_STACK_SIZE);

    return (0);
}

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
    /* Initialize OS */
    os_init();

    /* Init tasks */
    init_tasks();

    if (argc > 1 && strcmp(argv[1], "-l") == 0) {
        ble_host_listen_enabled = 1;
    }

    /* Start the OS */
    os_start();

    /* os start should never return. If it does, this should be an error */
    assert(0);

    return (0);
}
