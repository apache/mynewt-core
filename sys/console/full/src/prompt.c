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


#include "console/console.h"
#include "console/prompt.h"
#include <syscfg/syscfg.h>

/* console prompt, always followed by a space */
static char console_prompt[] = " > ";
static char do_prompt = MYNEWT_VAL(CONSOLE_PROMPT);


/* set the prompt character, leave the space */
void
console_set_prompt(char p)
{
    do_prompt = 1;
    console_prompt[1] = p;
}

void
console_no_prompt(void)
{
    do_prompt = 0;
}

/* print the prompt to the console */
void
console_print_prompt(void)
{
    if (do_prompt) {
        console_printf("%s", console_prompt);
    }
}
