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

#include "os/mynewt.h"

#if MYNEWT_VAL(CRASH_TEST_CLI)
#include <inttypes.h>
#include <console/console.h>
#include <shell/shell.h>
#include <stdio.h>
#include <string.h>

#include "crash_test/crash_test.h"
#include "crash_test_priv.h"

static int crash_cli_cmd(int argc, char **argv);
struct shell_cmd crash_cmd_struct = {
    .sc_cmd = "crash",
    .sc_cmd_func = crash_cli_cmd
};

static int
crash_cli_cmd(int argc, char **argv)
{
    if (argc >= 2 && crash_device(argv[1]) == 0) {
        return 0;
    }
    console_printf("Usage crash [div0|jump0|ref0|assert|wdog]\n");
    return 0;
}

#endif /* MYNEWT_VAL(CRASH_TEST_CLI) */
