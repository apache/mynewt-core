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
#ifndef __LOG_H__ 
#define __LOG_H__

#include "util/cbmem.h"

#include <os/queue.h>

struct log;

typedef int (*log_walk_func_t)(struct log *, void *arg, void *offset, 
        uint16_t len);

typedef int (*lh_read_func_t)(struct log *, void *dptr, void *buf, 
        uint16_t offset, uint16_t len);
typedef int (*lh_append_func_t)(struct log *, void *buf, int len);
typedef int (*lh_walk_func_t)(struct log *, 
        log_walk_func_t walk_func, void *arg);
typedef int (*lh_flush_func_t)(struct log *);

#define LOG_TYPE_STREAM  (0)
#define LOG_TYPE_MEMORY  (1) 
#define LOG_TYPE_STORAGE (2) 

struct log_handler {
    int log_type;
    lh_read_func_t log_read;
    lh_append_func_t log_append;
    lh_walk_func_t log_walk;
    lh_flush_func_t log_flush;
    void *log_arg;
};

struct log_entry_hdr {
    int64_t ue_ts;
}; 

struct log {
    char *l_name;
    struct log_handler *l_log;
    STAILQ_ENTRY(log) l_next;
};

/* Log system level functions (for all logs.) */
int log_init(void);
struct log *log_list_get_next(struct log *);

/* Log functions, manipulate a single log */
int log_register(char *name, struct log *log, struct log_handler *);
int log_append(struct log *log, uint8_t *data, uint16_t len);
int log_read(struct log *log, void *dptr, void *buf, uint16_t off, 
        uint16_t len);
int log_walk(struct log *log, log_walk_func_t walk_func, 
        void *arg);
int log_flush(struct log *log);


/* CBMEM exports */

int log_cbmem_handler_init(struct log_handler *, struct cbmem *);


#endif /* __LOG_H__ */
