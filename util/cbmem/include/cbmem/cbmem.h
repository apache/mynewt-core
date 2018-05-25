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
#ifndef __UTIL_CBMEM_H__ 
#define __UTIL_CBMEM_H__

#include "os/mynewt.h"

#ifdef __cplusplus
extern "C" {
#endif

struct cbmem_entry_hdr {
    uint16_t ceh_len;
    uint16_t ceh_flags;
} __attribute__((packed));

struct cbmem {
    struct os_mutex c_lock;

    struct cbmem_entry_hdr *c_entry_start;
    struct cbmem_entry_hdr *c_entry_end;
    uint8_t *c_buf;
    uint8_t *c_buf_end;
    uint8_t *c_buf_cur_end;
};

struct cbmem_iter {
    struct cbmem_entry_hdr *ci_start;
    struct cbmem_entry_hdr *ci_cur;
    struct cbmem_entry_hdr *ci_end;
};

#define CBMEM_ENTRY_SIZE(__p) (sizeof(struct cbmem_entry_hdr) \
        + ((struct cbmem_entry_hdr *) (__p))->ceh_len)
#define CBMEM_ENTRY_NEXT(__p) ((struct cbmem_entry_hdr *) \
        ((uint8_t *) (__p) + CBMEM_ENTRY_SIZE(__p)))

typedef int (*cbmem_walk_func_t)(struct cbmem *, struct cbmem_entry_hdr *, 
        void *arg);

int cbmem_lock_acquire(struct cbmem *cbmem);
int cbmem_lock_release(struct cbmem *cbmem);
int cbmem_init(struct cbmem *cbmem, void *buf, uint32_t buf_len);
int cbmem_append(struct cbmem *cbmem, void *data, uint16_t len);
int cbmem_append_mbuf(struct cbmem *cbmem, struct os_mbuf *om);
void cbmem_iter_start(struct cbmem *cbmem, struct cbmem_iter *iter);
struct cbmem_entry_hdr *cbmem_iter_next(struct cbmem *cbmem, 
        struct cbmem_iter *iter);
int cbmem_read(struct cbmem *cbmem, struct cbmem_entry_hdr *hdr, void *buf, 
        uint16_t off, uint16_t len);
int cbmem_read_mbuf(struct cbmem *cbmem, struct cbmem_entry_hdr *hdr,
                    struct os_mbuf *om, uint16_t off, uint16_t len);
int cbmem_walk(struct cbmem *cbmem, cbmem_walk_func_t walk_func, void *arg);

int cbmem_flush(struct cbmem *);

#ifdef __cplusplus
}
#endif

#endif /* __UTIL_CBMEM_H__ */
