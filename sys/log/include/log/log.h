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
#ifndef __UTIL_LOG_H__ 
#define __UTIL_LOG_H__

#include "util/cbmem.h"
#include <os/queue.h>

struct util_log;

typedef int (*util_log_walk_func_t)(struct util_log *, void *arg, void *offset, 
        uint16_t len);

typedef int (*ulh_read_func_t)(struct util_log *, void *dptr, void *buf, 
        uint16_t offset, uint16_t len);
typedef int (*ulh_append_func_t)(struct util_log *, void *buf, int len);
typedef int (*ulh_walk_func_t)(struct util_log *, 
        util_log_walk_func_t walk_func, void *arg);
typedef int (*ulh_flush_func_t)(struct util_log *);

struct ul_handler {
    ulh_read_func_t ulh_read;
    ulh_append_func_t ulh_append;
    ulh_walk_func_t ulh_walk;
    ulh_flush_func_t ulh_flush;
    void *ulh_arg;
};

struct ul_entry_hdr {
    int64_t ue_ts;
}; 

struct util_log {
    char *ul_name;
    struct ul_handler *ul_ulh;
    STAILQ_ENTRY(util_log) ul_next;
};

int util_log_cbmem_handler_init(struct ul_handler *, struct cbmem *);
int util_log_register(char *name, struct util_log *log, struct ul_handler *);
int util_log_append(struct util_log *log, uint8_t *data, uint16_t len);
int util_log_read(struct util_log *log, void *dptr, void *buf, uint16_t off, 
        uint16_t len);
int util_log_walk(struct util_log *log, util_log_walk_func_t walk_func, 
        void *arg);
int util_log_flush(struct util_log *log);

#endif /* __UTIL_LOG_H__ */
