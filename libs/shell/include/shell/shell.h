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
#ifndef __SHELL_H__
#define __SHELL_H__

#include <os/os.h>

typedef int (*shell_cmd_func_t)(int argc, char **argv);
struct shell_cmd {
    char *sc_cmd;
    shell_cmd_func_t sc_cmd_func;
    STAILQ_ENTRY(shell_cmd) sc_next;
};

int shell_cmd_register(struct shell_cmd *sc);

#define SHELL_NLIP_PKT_START1 (6)
#define SHELL_NLIP_PKT_START2 (9)
#define SHELL_NLIP_DATA_START1 (4)
#define SHELL_NLIP_DATA_START2 (20)

typedef int (*shell_nlip_input_func_t)(struct os_mbuf *, void *arg);
int shell_nlip_input_register(shell_nlip_input_func_t nf, void *arg);
int shell_nlip_output(struct os_mbuf *m);

void shell_console_rx_cb(void);
int shell_task_init(uint8_t prio, os_stack_t *stack, uint16_t stack_size,
                    int max_input_length);

#endif /* __SHELL_H__ */
