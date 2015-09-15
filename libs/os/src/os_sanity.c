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

#include <assert.h>
#include <string.h> 

#include "os/os.h" 

SLIST_HEAD(, os_sanity_check) g_os_sanity_check_list = 
    SLIST_HEAD_INITIALIZER(os_sanity_check_list); 

struct os_mutex g_os_sanity_check_mu; 

struct os_task g_os_sanity_task;
os_stack_t g_os_sanity_task_stack[OS_STACK_ALIGN(OS_SANITY_STACK_SIZE)];


int 
os_sanity_check_init(struct os_sanity_check *sc)
{
    memset(sc, 0, sizeof(*sc)); 

    return (0);
}

static int
os_sanity_check_list_lock(void)
{
    int rc; 

    if (!g_os_started) {
        return (OS_OK);
    }

    rc = os_mutex_pend(&g_os_sanity_check_mu, OS_WAIT_FOREVER);
    if (rc != OS_OK) {
        goto err;
    }

    return (OS_OK);
err:
    return (rc);
}

static int 
os_sanity_check_list_unlock(void)
{
    int rc; 

    if (!g_os_started) {
        return (OS_OK);
    }

    rc = os_mutex_release(&g_os_sanity_check_mu);
    if (rc != 0) {
        goto err;
    }

    return (OS_OK);
err:
    return (rc);
}


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

        os_time_delay(OS_TICKS_PER_SEC); 
    }
}


int 
os_sanity_task_init(void)
{
    int rc;

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
