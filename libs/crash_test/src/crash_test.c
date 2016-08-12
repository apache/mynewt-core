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
#include <inttypes.h>
#include <os/os.h>
#include <console/console.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "crash_test/crash_test.h"
#include "crash_test_priv.h"

#ifdef SHELL_PRESENT
#include <shell/shell.h>
#endif
#ifdef NEWTMGR_PRESENT
#include <newtmgr/newtmgr.h>
#endif

int
crash_device(char *how)
{
    volatile int val1, val2, val3;

    if (!strcmp(how, "div0")) {

        val1 = 42;
        val2 = 0;

        val3 = val1 / val2;
        console_printf("42/0 = %d\n", val3);
    } else if (!strcmp(how, "jump0")) {
        ((void (*)(void))0)();
    } else if (!strcmp(how, "ref0")) {
        val1 = *(int *)0;
    } else if (!strcmp(how, "assert")) {
        assert(0);
    } else {
        return -1;
    }
    return 0;
}

int
crash_test_init(void)
{
#ifdef SHELL_PRESENT
    shell_cmd_register(&crash_cmd_struct);
#endif
#ifdef NEWTMGR_PRESENT
    nmgr_group_register(&crash_test_nmgr_group);
#endif
    return 0;
}
