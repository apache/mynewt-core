/**
 * Copyright (c) 2015 Runtime Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <os/os.h>

#include <console/console.h>

#include "shell/shell.h" 

#include <stdio.h>
#include <string.h>
#include <assert.h>

#define OS_EVENT_T_CONSOLE_RDY (OS_EVENT_T_PERUSER)

static struct shell_cmd g_shell_echo_cmd;

static struct os_task shell_task;
static struct os_eventq shell_evq;
static struct os_event console_rdy_ev;

static struct os_mutex g_shell_cmd_list_lock; 

char shell_line[128];
char *argv[20];

static STAILQ_HEAD(, shell_cmd) g_shell_cmd_list = 
    STAILQ_HEAD_INITIALIZER(g_shell_cmd_list);

static int 
shell_cmd_list_lock(void)
{
    int rc;

    if (!os_started()) {
        return (0);
    }

    rc = os_mutex_pend(&g_shell_cmd_list_lock, OS_WAIT_FOREVER);
    if (rc != 0) {
        goto err;
    }
    return (0);
err:
    return (rc);
}

static int 
shell_cmd_list_unlock(void)
{
    int rc;

    if (!os_started()) {
        return (0);
    }

    rc = os_mutex_release(&g_shell_cmd_list_lock);
    if (rc != 0) {
        goto err;
    }
    return (0);
err:
    return (rc);
}

int
shell_cmd_register(struct shell_cmd *sc, char *cmd, shell_cmd_func_t func)
{
    int rc;

    /* Create the command that is being registered. */
    sc->sc_cmd = cmd;
    sc->sc_cmd_func = func;
    STAILQ_NEXT(sc, sc_next) = NULL;

    rc = shell_cmd_list_lock();
    if (rc != 0) {
        goto err;
    }

    STAILQ_INSERT_TAIL(&g_shell_cmd_list, sc, sc_next);

    rc = shell_cmd_list_unlock();
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

static int 
shell_cmd(char *cmd, char **argv, int argc)
{
    struct shell_cmd *sc;
    int rc;

    rc = shell_cmd_list_lock();
    if (rc != 0) {
        goto err;
    }

    STAILQ_FOREACH(sc, &g_shell_cmd_list, sc_next) {
        if (!strcmp(sc->sc_cmd, cmd)) {
            break;
        }
    }

    rc = shell_cmd_list_unlock();
    if (rc != 0) {
        goto err;
    }

    if (sc) {
        sc->sc_cmd_func(argv, argc);
    } else {
        console_printf("Unknown command %s\n", cmd);
    }

    return (0);
err:
    return (rc);
}

static int 
shell_process_command(char *line, int len)
{
    char *tok;
    char *tok_ptr;
    int argc;

    tok_ptr = NULL;
    tok = strtok_r(line, " ", &tok_ptr);
    argc = 0;
    while (tok != NULL) {
        argv[argc++] = tok;

        tok = strtok_r(NULL, " ", &tok_ptr);
    }

    (void) shell_cmd(argv[0], argv, argc);
    
    return (0);
}

static int 
shell_read_console(void)
{
    int rc;

    while (1) {
        rc = console_read(shell_line, sizeof(shell_line));
        if (rc < 0) {
            goto err;
        }
        if (rc == 0) {
            break;
        }

        rc = shell_process_command(shell_line, rc);
        if (rc != 0) {
            goto err;
        }
    }

    return (0);
err:
    return (rc);
}

static void
shell_task_func(void *arg) 
{
    struct os_event *ev;

    os_eventq_init(&shell_evq);
    
    console_rdy_ev.ev_type = OS_EVENT_T_CONSOLE_RDY;

    while (1) {
        ev = os_eventq_get(&shell_evq);
        assert(ev != NULL);

        switch (ev->ev_type) {
            case OS_EVENT_T_CONSOLE_RDY: 
                // Read and process all available lines on the console.
                (void) shell_read_console();
                break;
        }
    }
}

/**
 * This function is called from the console APIs when data is available 
 * to be read.  This is either a full line (full_line = 1), or when the 
 * console buffer (default = 128) is full.   At the moment, we assert() 
 * when the buffer is filled to avoid double buffering this data. 
 */
void
shell_console_rx_cb(int full_line)
{
    assert(full_line);

    os_eventq_put(&shell_evq, &console_rdy_ev);
}

static int
shell_echo_cmd(char **argv, int argc)
{
    int i; 

    for (i = 1; i < argc; i++) {
        console_write(argv[i], strlen(argv[i]));
        console_write(" ", sizeof(" ")-1);
    }
    console_write("\n", sizeof("\n")-1);

    return (0);
}


int 
shell_task_init(uint8_t prio, os_stack_t *stack, uint16_t stack_size)
{
    int rc;
   
    rc = os_mutex_init(&g_shell_cmd_list_lock);
    if (rc != 0) {
        goto err;
    }

    rc = shell_cmd_register(&g_shell_echo_cmd, "echo", shell_echo_cmd);
    if (rc != 0) {
        goto err;
    }

    rc = os_task_init(&shell_task, "shell", shell_task_func, 
            NULL, prio, OS_WAIT_FOREVER, stack, stack_size);
    if (rc != 0) {
        goto err;
    }


    return (0);
err:
    return (rc);
}

