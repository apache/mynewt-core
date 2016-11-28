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
#include "shell/shell.h"
#include <console/console.h>
#include <console/ticks.h>
#include "syscfg/syscfg.h"

/**
 * Handles the 'ticks' command
 */
int
shell_ticks_cmd(int argc, char **argv)
{
    int rc;

    rc = shell_cmd_list_lock();
    if (rc != 0) {
        return -1;
    }
    if (argc > 1) {
        if (!strcmp(argv[1], "on")) {
            console_yes_ticks();
            console_printf(" Console Ticks on\n");
        }   
        else if (!strcmp(argv[1],"off")) {
            console_printf(" Console Ticks off\n");
            console_no_ticks();
        }
        else {
            goto usage;
        }
    } 
    else {
        goto usage;
    }
    shell_cmd_list_unlock();
    return 0;
usage:
    console_printf(" Usage: ticks [on|off]\n");
    shell_cmd_list_unlock();
    return 0;
  
}
 