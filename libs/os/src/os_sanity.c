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

#include <assert.h>
#include <string.h> 

#include "os/os.h" 

SLIST_HEAD(, os_sanity_check) g_os_sanity_check_list = 
    SLIST_HEAD_INITIALIZER(os_sanity_check_list); 

struct os_mutex g_os_sanity_check_mu; 

int g_os_sanity_num_secs;

struct os_task g_os_sanity_task;
os_stack_t g_os_sanity_task_stack[OS_STACK_ALIGN(OS_SANITY_STACK_SIZE)];


/**
 * Initialize a sanity check
 *
 * @param sc The sanity check to initialize
 *
 * @return 0 on success, error code on failure.
 */
int 
os_sanity_check_init(struct os_sanity_check *sc)
{
    memset(sc, 0, sizeof(*sc)); 

    return (0);
}

/**
 * Lock the sanity check list
 *
 * @return 0 on success, error code on failure. 
 */
static int
os_sanity_check_list_lock(void)
{
    int rc; 

    if (!g_os_started) {
        return (0);
    }

    rc = os_mutex_pend(&g_os_sanity_check_mu, OS_WAIT_FOREVER);
    if (rc != OS_OK) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

/**
 * Unlock the sanity check list 
 *
 * @return 0 on success, error code on failure
 */
static int 
os_sanity_check_list_unlock(void)
{
    int rc; 

    if (!g_os_started) {
        return (0);
    }

    rc = os_mutex_release(&g_os_sanity_check_mu);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

/**
 * Provide a "task checkin" for the sanity task.
 *
 * @param t The task to check in
 *
 * @return 0 on success, error code on failure
 */
int 
os_sanity_task_checkin(struct os_task *t)
{
    int rc; 

    if (t == NULL) {
        t = os_sched_get_current_task();
    }

    rc = os_sanity_check_reset(&t->t_sanity_check);
    if (rc != OS_OK) {
        goto err;
    }

    return (0);
err:
    return (rc);
}


/**
 * Register a sanity check 
 *
 * @param sc The sanity check to register 
 *
 * @return 0 on success, error code on failure
 */
int 
os_sanity_check_register(struct os_sanity_check *sc)
{
    int rc;

    rc = os_sanity_check_list_lock();
    if (rc != OS_OK) {
        goto err;
    }

    SLIST_INSERT_HEAD(&g_os_sanity_check_list, sc, sc_next);

    rc = os_sanity_check_list_unlock();
    if (rc != OS_OK) {
        goto err;
    }

    return (0);
err:
    return (rc);
}


/**
 * Reset the os sanity check, so that it doesn't trip up the 
 * sanity timer.
 *
 * @param sc The sanity check to reset 
 *
 * @return 0 on success, error code on failure 
 */
int 
os_sanity_check_reset(struct os_sanity_check *sc)
{
    int rc;

    rc = os_sanity_check_list_lock();
    if (rc != OS_OK) {
        goto err;
    }

    sc->sc_checkin_last = os_time_get();

    rc = os_sanity_check_list_unlock();
    if (rc != OS_OK) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

/**
 * The main sanity check task loop.  This executes every SANITY_CHECK_NUM_SECS
 * and goes through to see if any of the registered sanity checks are expired.
 * If the sanity checks have expired, it restarts the operating system.
 *
 * @param arg unused 
 *
 * @return never 
 */
static void
os_sanity_task_loop(void *arg)
{
    struct os_sanity_check *sc; 
    int rc; 

    while (1) {
        rc = os_sanity_check_list_lock();
        if (rc != 0) {
            assert(0);
        }

        SLIST_FOREACH(sc, &g_os_sanity_check_list, sc_next) {
            rc = OS_OK; 

            if (sc->sc_func) {
                rc = sc->sc_func(sc, sc->sc_arg);
                if (rc == OS_OK) {
                    sc->sc_checkin_last = os_time_get();
                    continue;
                }
            }

            if (OS_TIME_TICK_GT(os_time_get() - sc->sc_checkin_last, 
                        sc->sc_checkin_itvl)) {
                assert(0);
            }
        }


        rc = os_sanity_check_list_unlock();
        if (rc != 0) {
            assert(0);
        }

        os_time_delay(g_os_sanity_num_secs); 
    }
}

/**
 * Initialize the sanity task and mutex. 
 *
 * @return 0 on success, error code on failure
 */
int 
os_sanity_task_init(int num_secs)
{
    int rc;

    g_os_sanity_num_secs = num_secs * OS_TICKS_PER_SEC;

    rc = os_mutex_init(&g_os_sanity_check_mu); 
    if (rc != 0) {
        goto err;
    }

    rc = os_task_init(&g_os_sanity_task, "os_sanity", os_sanity_task_loop, 
            NULL, OS_SANITY_PRIO, OS_WAIT_FOREVER, g_os_sanity_task_stack, 
            OS_STACK_ALIGN(OS_SANITY_STACK_SIZE));
    if (rc != OS_OK) {
        goto err;
    }

    return (0);
err:
    return (rc);
}
