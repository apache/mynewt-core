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

#include <string.h>
#include <errno.h>
#include "sysinit/sysinit.h"
#include "console/console.h"
#include "hal/hal_gpio.h"
#include "lps33hw/lps33hw.h"
#include "lps33hw_priv.h"

#if MYNEWT_VAL(LPS33HW_CLI)

#include "shell/shell.h"
#include "parse/parse.h"

static int lps33hw_shell_cmd(int argc, char **argv);

static struct shell_cmd lps33hw_shell_cmd_struct = {
    .sc_cmd = "lps33hw",
    .sc_cmd_func = lps33hw_shell_cmd
};

static struct sensor_itf g_sensor_itf = {
    .si_type = MYNEWT_VAL(LPS33HW_SHELL_ITF_TYPE),
    .si_num = MYNEWT_VAL(LPS33HW_SHELL_ITF_NUM),
    .si_cs_pin = MYNEWT_VAL(LPS33HW_SHELL_CSPIN),
    .si_addr = MYNEWT_VAL(LPS33HW_SHELL_ITF_ADDR)
};

static int
lps33hw_shell_cmd_read(int argc, char **argv)
{
    return 0;
}

static int
lps33hw_shell_cmd(int argc, char **argv)
{
    if (argc == 1) {
        return lps33hw_shell_help();
    }

    /* Read command (get a new data sample) */
    if (argc > 1 && strcmp(argv[1], "r") == 0) {
        return lps33hw_shell_cmd_read(argc, argv);
    }


    return lps33hw_shell_err_unknown_arg(argv[1]);
}

int
lps33hw_shell_init(void)
{
    int rc;

    rc = shell_cmd_register(&lps33hw_shell_cmd_struct);
    SYSINIT_PANIC_ASSERT(rc == 0);

    return rc;
}

#endif
