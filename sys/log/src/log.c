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

#include <string.h>

#include <util/cbmem.h>

#include "log/log.h"

#include <stdio.h>
#include <stdarg.h>

#ifdef SHELL_PRESENT
#include <shell/shell.h>
#endif 

static STAILQ_HEAD(, log) g_log_list = STAILQ_HEAD_INITIALIZER(g_log_list);

#ifdef SHELL_PRESENT
struct shell_cmd g_shell_log_cmd;
int shell_log_dump_all_cmd(int, char **);
#endif 

int 
log_init(void)
{
#ifdef SHELL_PRESENT 
    shell_cmd_register(&g_shell_log_cmd, "log", shell_log_dump_all_cmd);
#endif

    return (0);
}

struct log *
log_list_get_next(struct log *log)
{
    struct log *next;

    if (log == NULL) {
        next = STAILQ_FIRST(&g_log_list);
    } else {
        next = STAILQ_NEXT(log, l_next);
    }

    return (next);
}

int 
log_register(char *name, struct log *log, struct log_handler *lh)
{
    log->l_name = name;
    log->l_log = lh;

    STAILQ_INSERT_TAIL(&g_log_list, log, l_next);

    return (0);
}

int
log_append(struct log *log, uint16_t module, uint16_t level, uint8_t *data, 
        uint16_t len)
{
    struct log_entry_hdr *ue;
    int rc;

    ue = (struct log_entry_hdr *) data;
    ue->ue_ts = (int64_t) os_time_get();
    ue->ue_level = level;
    ue->ue_module = module;

    rc = log->l_log->log_append(log, data, len + LOG_ENTRY_HDR_SIZE);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

void 
log_printf(struct log *log, uint16_t module, uint16_t level, char *msg,
        ...)
{
    va_list args;
    char buf[LOG_ENTRY_HDR_SIZE + LOG_PRINTF_MAX_ENTRY_LEN];
    int len;

    va_start(args, msg);
    len = vsnprintf(&buf[LOG_ENTRY_HDR_SIZE], LOG_PRINTF_MAX_ENTRY_LEN, msg, 
            args);
    if (len >= LOG_PRINTF_MAX_ENTRY_LEN) {
        len = LOG_PRINTF_MAX_ENTRY_LEN-1;
    }

    log_append(log, module, level, (uint8_t *) buf, len);
}

int 
log_walk(struct log *log, log_walk_func_t walk_func, void *arg)
{
    int rc;

    rc = log->l_log->log_walk(log, walk_func, arg);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

int 
log_read(struct log *log, void *dptr, void *buf, uint16_t off, 
        uint16_t len)
{
    int rc;

    rc = log->l_log->log_read(log, dptr, buf, off, len);

    return (rc);
}

int 
log_flush(struct log *log)
{
    int rc;

    rc = log->l_log->log_flush(log);
    if (rc != 0) {
        goto err;
    } 

    return (0);
err:
    return (rc);
}
