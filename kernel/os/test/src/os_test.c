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
#include "syscfg/syscfg.h"
#include "testutil/testutil.h"
#include "os/os_test.h"
#include "os_test_priv.h"

#include <stdio.h>
#include <setjmp.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include "os/os.h"

#if MYNEWT_VAL(SELFTEST) /* these parameters only work in a native env */

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

#if MYNEWT_VAL(SELFTEST)
   tu_restart();
#endif
}

extern void os_mempool_test_init(void *arg);
extern void os_sem_test_init(void *arg);
extern void os_mutex_test_init(void *arg);

int
os_test_all(void)
{

    tu_suite_set_init_cb(os_mempool_test_init, NULL);
    os_mempool_test_suite();

    tu_suite_set_init_cb(os_mutex_test_init, NULL);
    os_mutex_test_suite();

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
    ts_config.ts_print_results = 1;
    tu_parse_args(argc, argv);
    tu_init();

    os_test_all();

    return tu_any_failed;
}
#else
void
os_test_restart(void)
{
    return;
}
#endif
