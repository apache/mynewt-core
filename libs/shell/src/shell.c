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

#include <shell/shell.h>
#include <os/endian.h>
#include <util/base64.h>

#include <stdio.h>
#include <string.h>
#include <assert.h>

static shell_nlip_input_func_t g_shell_nlip_in_func;
static void *g_shell_nlip_in_arg;

static struct os_mqueue g_shell_nlip_mq;

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

static struct os_mbuf *g_nlip_mbuf;
static uint16_t g_nlip_expected_len;


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
        sc->sc_cmd_func(argc, argv);
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
shell_nlip_process(char *data, int len)
{
    uint16_t copy_len;
    int rc;

    rc = base64_decode(data, data);
    if (rc < 0) {
        goto err;
    }
    len = rc;

    if (g_nlip_mbuf == NULL) {
        if (len < 2) {
            rc = -1;
            goto err;
        }

        g_nlip_expected_len = ntohs(*(uint16_t *) data);
        g_nlip_mbuf = os_msys_get_pkthdr(g_nlip_expected_len, 0);
        if (!g_nlip_mbuf) {
            rc = -1;
            goto err;
        }

        data += sizeof(uint16_t);
        len -= sizeof(uint16_t);
    }

    copy_len = min(g_nlip_expected_len - OS_MBUF_PKTHDR(g_nlip_mbuf)->omp_len,
            len);

    rc = os_mbuf_copyinto(g_nlip_mbuf, OS_MBUF_PKTHDR(g_nlip_mbuf)->omp_len, 
            data, copy_len);
    if (rc != 0) {
        goto err;
    }

    if (OS_MBUF_PKTHDR(g_nlip_mbuf)->omp_len == g_nlip_expected_len) {
        if (g_shell_nlip_in_func) {
            g_shell_nlip_in_func(g_nlip_mbuf, g_shell_nlip_in_arg);
        } else {
            os_mbuf_free_chain(g_nlip_mbuf);
        }
        g_nlip_mbuf = NULL;
        g_nlip_expected_len = 0;
    }

    return (0);
err:
    return (rc);
}

static int 
shell_nlip_mtx(struct os_mbuf *m)
{
#define SHELL_NLIP_MTX_BUF_SIZE (12)
    uint8_t readbuf[SHELL_NLIP_MTX_BUF_SIZE];
    char encodebuf[BASE64_ENCODE_SIZE(SHELL_NLIP_MTX_BUF_SIZE)];
    uint8_t esc_seq[2];
    uint8_t pkt_seq[2];
    uint16_t totlen;
    uint16_t dlen;
    uint16_t off;
    int elen;
    uint16_t nwritten;
    int rc;

    /* Convert the mbuf into a packet.
     *
     * starts with 06 09 
     * base64 encode:
     *  - total packet length (uint16_t) 
     *  - data 
     * base64 encoded data must be less than 122 bytes per line to 
     * avoid overflows and adhere to convention.
     *
     * continuation packets are preceded by 04 20 until the entire 
     * buffer has been sent. 
     */
    totlen = OS_MBUF_PKTHDR(m)->omp_len;
    nwritten = 0;
    off = 0;

    pkt_seq[0] = SHELL_NLIP_PKT_START1;
    pkt_seq[1] = SHELL_NLIP_PKT_START2;

    esc_seq[0] = SHELL_NLIP_DATA_START1;
    esc_seq[1] = SHELL_NLIP_DATA_START2;

    /* Start a packet */
    console_write((char *) pkt_seq, sizeof(pkt_seq));

    while (totlen > 0) {
        dlen = min(SHELL_NLIP_MTX_BUF_SIZE, totlen);

        if (nwritten != 0 && 
                ((nwritten + BASE64_ENCODE_SIZE(dlen)) % 122) == 0) {
            console_write((char *) esc_seq, sizeof(esc_seq));
            console_write("\n", sizeof("\n")-1);
        }

        rc = os_mbuf_copydata(m, off, dlen, readbuf);
        if (rc != 0) {
            goto err;
        }
        off += dlen;

        elen = base64_encode(readbuf, dlen, encodebuf, 
                totlen - dlen > 0 ? 0 : 1);

        console_write(encodebuf, elen);

        nwritten += elen;
        totlen -= dlen;
        
    }

    return (0);
err:
    return (rc);
}

static void
shell_nlip_mqueue_process(void)
{
    struct os_mbuf *m;

    /* Copy data out of the mbuf 12 bytes at a time and write it to 
     * the console.
     */
    while (1) {
        m = os_mqueue_get(&g_shell_nlip_mq);
        if (!m) {
            break;
        }

        (void) shell_nlip_mtx(m);

        os_mbuf_free_chain(m);
    }
}

int
shell_nlip_input_register(shell_nlip_input_func_t nf, void *arg)
{
    g_shell_nlip_in_func = nf;
    g_shell_nlip_in_arg = arg;

    return (0);
}

int 
shell_nlip_output(struct os_mbuf *m)
{
    int rc;

    rc = os_mqueue_put(&g_shell_nlip_mq, &shell_evq, m);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
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

        if (rc > 2) {
            if (shell_line[0] == SHELL_NLIP_PKT_START1 && 
                    shell_line[1] == SHELL_NLIP_PKT_START2) {
                if (g_nlip_mbuf) {
                    os_mbuf_free_chain(g_nlip_mbuf);
                    g_nlip_mbuf = NULL;
                }
                g_nlip_expected_len = 0;

                rc = shell_nlip_process(&shell_line[2], rc-2);
            } else if (shell_line[0] == SHELL_NLIP_DATA_START1 && 
                    shell_line[1] == SHELL_NLIP_DATA_START2) {
                rc = shell_nlip_process(&shell_line[2], rc-2);
            } else {
                rc = shell_process_command(shell_line, rc);
                if (rc != 0) {
                    goto err;
                }
            }
        } else {
            rc = shell_process_command(shell_line, rc);
            if (rc != 0) {
                goto err;
            }
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
    os_mqueue_init(&g_shell_nlip_mq, NULL);
    
    console_rdy_ev.ev_type = OS_EVENT_T_CONSOLE_RDY;

    while (1) {
        ev = os_eventq_get(&shell_evq);
        assert(ev != NULL);

        switch (ev->ev_type) {
            case OS_EVENT_T_CONSOLE_RDY: 
                // Read and process all available lines on the console.
                (void) shell_read_console();
                break;
            case OS_EVENT_T_MQUEUE_DATA:
                shell_nlip_mqueue_process();
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
shell_echo_cmd(int argc, char **argv)
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

