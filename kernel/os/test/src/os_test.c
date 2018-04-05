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

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <setjmp.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>

#include "os/mynewt.h"
#include "testutil/testutil.h"
#include "os_test_priv.h"

uint32_t stack1_size;
uint32_t stack2_size;
uint32_t stack3_size;
uint32_t stack4_size;

/*
 * Most of this file is the driver for the kernel selftest running in sim
 * In the sim environment, we can initialize and restart mynewt at will
 * where that is not the case when the test cases are run in a target env.
 */
#if MYNEWT_VAL(SELFTEST)
void
os_test_restart(void)
{
    struct sigaction sa;
    struct itimerval it;
    int rc;

    g_os_started = 0;

    memset(&sa, 0, sizeof sa);
    sa.sa_handler = SIG_IGN;

    sigaction(SIGALRM, &sa, NULL);
    sigaction(SIGVTALRM, &sa, NULL);

    memset(&it, 0, sizeof(it));
    rc = setitimer(ITIMER_VIRTUAL, &it, NULL);
    if (rc != 0) {
        perror("Cannot set itimer");
        abort();
    }

   tu_restart();
}

/*
 * sysinit and os_start are only called if running in a sim environment
 * (ie MYNEWT_VAL(SELFTEST) is set)
 */
void
os_selftest_pretest_cb(void* arg)
{
    os_init(NULL);
    sysinit();
}

void
os_selftest_posttest_cb(void *arg)
{
    os_start();
}

extern void os_mempool_test_init(void *arg);
extern void os_sem_test_init(void *arg);
extern void os_mutex_test_init(void *arg);

int
os_test_all(void)
{

    tu_suite_set_init_cb(os_mempool_test_init, NULL);
    os_mempool_test_suite();
#if 1
    tu_suite_set_init_cb(os_mutex_test_init, NULL);
    os_mutex_test_suite();
#endif
    tu_suite_set_init_cb(os_sem_test_init, NULL);
    os_sem_test_suite();

    os_mbuf_test_suite();

    os_eventq_test_suite();

    os_callout_test_suite();

    return tu_case_failed;
}

int
main(int argc, char **argv)
{
    sysinit();

    os_test_all();

    return tu_any_failed;
}

#else
/*
 * Leave this as an implemented function for non-sim test environments
 */
void
os_test_restart(void)
{
    return;
}
#endif /* MYNEWT_VAL(SELFTEST) */
