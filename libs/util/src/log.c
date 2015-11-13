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

#include "util/log.h" 
#include "util/cbmem.h" 

#include <stdio.h>

#ifdef SHELL_PRESENT

#include <shell/shell.h>
#include <console/console.h> 

struct shell_cmd shell_log_cmd;
uint8_t shell_registered;

#endif 

static STAILQ_HEAD(, util_log) g_util_log_list = 
    STAILQ_HEAD_INITIALIZER(g_util_log_list);


#ifdef SHELL_PRESENT 

static int 
shell_log_dump_entry(struct util_log *log, void *arg, void *dptr, uint16_t len) 
{
    struct ul_entry_hdr ueh;
    char data[128];
    int dlen;
    int rc;

    rc = util_log_read(log, dptr, &ueh, 0, sizeof(ueh)); 
    if (rc != sizeof(ueh)) {
        goto err;
    }

    dlen = min(len-sizeof(ueh), 128);

    rc = util_log_read(log, dptr, data, sizeof(ueh), dlen);
    if (rc < 0) {
        goto err;
    }
    data[rc] = 0;

    /* XXX: This is evil.  newlib printf does not like 64-bit 
     * values, and this causes memory to be overwritten.  Cast to a 
     * unsigned 32-bit value for now.
     */
    console_printf("[%d] %s\n", (uint32_t) ueh.ue_ts, data);

    return (0);
err:
    return (rc);
}

static int 
shell_log_dump_all(int argc, char **argv)
{
    struct util_log *log;
    int rc;

    STAILQ_FOREACH(log, &g_util_log_list, ul_next) {
        rc = util_log_walk(log, shell_log_dump_entry, NULL);
        if (rc != 0) {
            goto err;
        }
    }

    return (0);
err:
    return (rc);
}

#endif 

static int 
ulh_cbmem_append(struct util_log *log, void *buf, int len) 
{
    struct cbmem *cbmem;
    int rc;

    cbmem = (struct cbmem *) log->ul_ulh->ulh_arg;

    rc = cbmem_append(cbmem, buf, len);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

static int 
ulh_cbmem_read(struct util_log *log, void *dptr, void *buf, uint16_t offset, 
        uint16_t len) 
{
    struct cbmem *cbmem;
    struct cbmem_entry_hdr *hdr;
    int rc;

    cbmem = (struct cbmem *) log->ul_ulh->ulh_arg;
    hdr = (struct cbmem_entry_hdr *) dptr;

    rc = cbmem_read(cbmem, hdr, buf, offset, len);

    return (rc);
}

static int 
ulh_cbmem_walk(struct util_log *log, util_log_walk_func_t walk_func, void *arg)
{
    struct cbmem *cbmem;
    struct cbmem_entry_hdr *hdr;
    struct cbmem_iter iter;
    int rc;

    cbmem = (struct cbmem *) log->ul_ulh->ulh_arg;

    rc = cbmem_lock_acquire(cbmem);
    if (rc != 0) {
        goto err;
    }
    
    cbmem_iter_start(cbmem, &iter);
    while (1) {
        hdr = cbmem_iter_next(cbmem, &iter);
        if (!hdr) {
            break;
        }

        rc = walk_func(log, arg, (void *) hdr, hdr->ceh_len);
        if (rc == 1) {
            break;
        }
    }

    rc = cbmem_lock_release(cbmem);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

static int 
ulh_cbmem_flush(struct util_log *log)
{
    struct cbmem *cbmem;
    int rc;

    cbmem = (struct cbmem *) log->ul_ulh->ulh_arg;
    
    rc = cbmem_flush(cbmem);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

int 
util_log_cbmem_handler_init(struct ul_handler *handler, struct cbmem *cbmem)
{
    handler->ulh_read = ulh_cbmem_read;
    handler->ulh_append = ulh_cbmem_append;
    handler->ulh_walk = ulh_cbmem_walk;
    handler->ulh_flush = ulh_cbmem_flush;
    handler->ulh_arg = (void *) cbmem;

    return (0);
}

int 
util_log_register(char *name, struct util_log *log, struct ul_handler *ulh)
{
#ifdef SHELL_PRESENT 
    if (!shell_registered) {
        /* register the shell */
        
        shell_registered = 1;
        shell_cmd_register(&shell_log_cmd, "log", shell_log_dump_all);
    }
#endif

    log->ul_name = name;
    log->ul_ulh = ulh;

    STAILQ_INSERT_TAIL(&g_util_log_list, log, ul_next);

    return (0);
}

int
util_log_append(struct util_log *log, uint8_t *data, uint16_t len)
{
    struct ul_entry_hdr *ue;
    int rc;

    ue = (struct ul_entry_hdr *) data;
    ue->ue_ts = (int64_t) os_time_get();

    rc = log->ul_ulh->ulh_append(log, data, len + sizeof(*ue));
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

int 
util_log_walk(struct util_log *log, util_log_walk_func_t walk_func, void *arg)
{
    int rc;

    rc = log->ul_ulh->ulh_walk(log, walk_func, arg);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}

int 
util_log_read(struct util_log *log, void *dptr, void *buf, uint16_t off, 
        uint16_t len)
{
    int rc;

    rc = log->ul_ulh->ulh_read(log, dptr, buf, off, len);

    return (rc);
}

int 
util_log_flush(struct util_log *log)
{
    int rc;

    rc = log->ul_ulh->ulh_flush(log);
    if (rc != 0) {
        goto err;
    } 

    return (0);
err:
    return (rc);
}
