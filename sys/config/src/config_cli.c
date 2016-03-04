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

#include <stddef.h>

#include "config/config.h"
#include "config_priv.h"

#ifdef SHELL_PRESENT
#include <string.h>

#include <shell/shell.h>
#include <console/console.h>

static int shell_conf_command(int argc, char **argv);

static struct shell_cmd shell_conf_cmd = {
    .sc_cmd = "config",
    .sc_cmd_func = shell_conf_command
};

static int
shell_conf_command(int argc, char **argv)
{
    char *name = NULL;
    char *val = NULL;
    char tmp_buf[16];
    int rc;

    switch (argc) {
    case 2:
        name = argv[1];
        break;
    case 3:
        name = argv[1];
        val = argv[2];
        break;
    default:
        goto err;
    }

    if (!strcmp(name, "commit")) {
        rc = conf_commit(val);
        if (rc) {
            val = "Failed to commit\n";
        } else {
            val = "Done\n";
        }
        console_printf("%s", val);
        return 0;
    }
    if (!val) {
        val = conf_get_value(name, tmp_buf, sizeof(tmp_buf));
        if (!val) {
            console_printf("Cannot display value\n");
            goto err;
        }
        console_printf("%s\n", val);
    } else {
        rc = conf_set_value(name, val);
        if (rc) {
            console_printf("Failed to set\n");
            goto err;
        }
    }
    return 0;
err:
    console_printf("Invalid args\n");
    return 0;
}

int
conf_cli_register(void)
{
    return shell_cmd_register(&shell_conf_cmd);
}
#endif

