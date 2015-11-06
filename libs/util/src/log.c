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

#ifdef SHELL_PRESENT

#include <shell/shell.h>
#include <console/console.h> 

struct shell_cmd shell_log_cmd;
int shell_registered;
#endif 

static STAILQ_HEAD(, util_log) g_util_log_list = 
    STAILQ_HEAD_INITIALIZER(g_util_log_list);


#ifdef SHELL_PRESENT 
static void 
shell_dump_log(struct util_log *log)
{
    struct ul_entry_hdr hdr; 
    struct ul_storage *uls;
    uint8_t bla[256];
    ul_off_t cur_off;
    int rc;

    uls = log->ul_uls;

    cur_off = log->ul_start_off;

    while (cur_off < log->ul_cur_end_off) {
        rc = uls->uls_read(uls, cur_off, &hdr, sizeof(hdr));
        if (rc != 0) {
            goto err;
        }

        rc = uls->uls_read(uls, cur_off + sizeof(hdr), bla, 
                hdr.ue_len);
        if (rc != 0) {
            goto err;
        }
        bla[hdr.ue_len] = 0;

        console_printf("[%d] [%d] [%x] %s", hdr.ue_ts, hdr.ue_len, 
                hdr.ue_flags, bla);

        cur_off += UL_ENTRY_SIZE(&hdr);
    }

    return;
err:
    return;
}

static int 
shell_log_dump_all(char **argv, int argc)
{
    struct util_log *log;

    STAILQ_FOREACH(log, &g_util_log_list, ul_next) {
        shell_dump_log(log);
    }

    return (0);
}
#endif 

static int 
uls_mem_read(struct ul_storage *uls, ul_off_t off, void *buf, int len)
{
    struct uls_mem *umem;
    int rc;

    umem = (struct uls_mem *) uls->uls_arg;

    if (off + len > umem->um_buf_size) {
        rc = -1;
        goto err;
    }

    memcpy(buf, umem->um_buf + off, len);

    return (0);
err:
    return (rc);
}

static int 
uls_mem_write(struct ul_storage *uls, ul_off_t off, void *buf, int len)
{
    struct uls_mem *umem;
    int rc;

    umem = (struct uls_mem *) uls->uls_arg;

    if (off + len > umem->um_buf_size) {
        rc = -1;
        goto err;
    }

    memcpy(umem->um_buf + off, buf, len);

    return (0);
err:
    return (rc);
}

static int 
uls_mem_size(struct ul_storage *uls, ul_off_t *size)
{
    *size = ((struct uls_mem *) uls->uls_arg)->um_buf_size;

    return (0);
}


int 
uls_mem_init(struct ul_storage *uls, struct uls_mem *umem)
{
    uls->uls_arg = umem;
    uls->uls_read = uls_mem_read;
    uls->uls_write = uls_mem_write;
    uls->uls_size = uls_mem_size;

    return (0);
}

int 
util_log_register(char *name, struct util_log *log, struct ul_storage *uls)
{
    int rc;

#ifdef SHELL_PRESENT 
    if (!shell_registered) {
        /* register the shell */
        
        shell_registered = 1;
        shell_cmd_register(&shell_log_cmd, "log", shell_log_dump_all);
    }
#endif

    log->ul_name = name;
    log->ul_start_off = 0;
    log->ul_last_off = -1;
    log->ul_cur_end_off = -1;

    log->ul_uls = uls;

    rc = uls->uls_size(uls, &log->ul_end_off);
    if (rc != 0) {
        goto err;
    }

    STAILQ_INSERT_TAIL(&g_util_log_list, log, ul_next);

    return (0);
err:
    return (rc);
}


static int
ul_insert_next(struct util_log *log, struct ul_entry_hdr *ue, 
        uint8_t *data)
{
    struct ul_storage *uls;
    struct ul_entry_hdr last;
    ul_off_t write_off;
    int rc;

    uls = log->ul_uls;

    if (log->ul_last_off != -1) {
        rc = uls->uls_read(uls, log->ul_last_off, &last, sizeof(last));
        if (rc != 0) {
            goto err;
        }

        write_off = log->ul_last_off + UL_ENTRY_SIZE(&last);
    } else {
        write_off = 0;
    }

    rc = uls->uls_write(uls, write_off, ue, sizeof(*ue));
    if (rc != 0) {
        goto err;
    }

    rc = uls->uls_write(uls, write_off + sizeof(*ue), 
            data, ue->ue_len);
    if (rc != 0) {
        goto err;
    }

    log->ul_last_off = write_off;
    log->ul_cur_end_off = write_off + UL_ENTRY_SIZE(ue);

    return (0);
err:
    return (rc);
}

int
util_log_write(struct util_log *log, uint8_t *data, uint16_t len)
{
    struct ul_entry_hdr ue;
    int rc;

    ue.ue_ts = os_time_get();
    ue.ue_len = len;
    ue.ue_flags = 0;

    rc = ul_insert_next(log, &ue, data);
    if (rc != 0) {
        goto err;
    }

    return (0);
err:
    return (rc);
}


