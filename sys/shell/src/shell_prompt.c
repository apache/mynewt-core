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
#include <console/prompt.h>
static char shell_prompt = '>';

void console_set_prompt(char p);

/**
 * Handles the 'prompt' command
 * with set argument, sets the prompt to the provided char
 * with the show argument, echos the current prompt
 * otherwise echos the prompt and the usage message
 */
int
shell_prompt_cmd(int argc, char **argv)
{
    int rc;

    rc = shell_cmd_list_lock();
    if (rc != 0) {
        return -1;
    }
    if (argc > 1) {
        if (!strcmp(argv[1], "show")) {
            console_printf(" Prompt character: %c\n", shell_prompt);
        }   
        else if (!strcmp(argv[1],"set")) {
            shell_prompt = argv[2][0];
            console_printf(" Prompt set to: %c\n", argv[2][0]);
            console_set_prompt(argv[2][0]);
        }
        else if (!strcmp(argv[1], "on")) {
            console_yes_prompt();
            console_printf(" Prompt now on.\n");
        }   
        else if (!strcmp(argv[1], "off")) {
            console_no_prompt();
            console_printf(" Prompt now off.\n");
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
    console_printf("Usage: prompt [on|off]|[set|show] [prompt_char]\n");
    shell_cmd_list_unlock();
    return 0;
  
}
 