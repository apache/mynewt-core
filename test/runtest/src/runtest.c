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
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "sysinit/sysinit.h"
#include "syscfg/syscfg.h"
#include "os/os.h"
#include "console/console.h"

#include "runtest/runtest.h"
#include "runtest_priv.h"

#if MYNEWT_VAL(RUNTEST_CLI)
#include "shell/shell.h"
struct shell_cmd runtest_cmd_struct;
#endif

#if MYNEWT_VAL(RUNTEST_NEWTMGR)
#include "mgmt/mgmt.h"
struct mgmt_group runtest_nmgr_group;
#endif

static struct os_eventq *run_evq;

extern int run_nmgr_register_group();

/**
 * Retrieves the event queue used by the runtest package.
 */
struct os_eventq *
run_evq_get(void)
{
    return run_evq;
}

void
run_evq_set(struct os_eventq *evq)
{
    run_evq = evq;
}

/*
 * Package init routine to register newtmgr "run" commands
 */
void
runtest_init(void)
{
    int rc;

    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

#if MYNEWT_VAL(RUNTEST_CLI)
    shell_cmd_register(&runtest_cmd_struct);
#endif

#if MYNEWT_VAL(RUNTEST_NEWTMGR)
    rc = run_nmgr_register_group();
    SYSINIT_PANIC_ASSERT(rc == 0);
#endif

    run_evq_set(os_eventq_dflt_get());
}
