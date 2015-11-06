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

typedef int32_t ul_off_t;

/* Forward declare for function pointers */
struct ul_storage;

typedef int (*ul_storage_read_func_t)(struct ul_storage *, ul_off_t off, 
        void *buf, int len);
typedef int (*ul_storage_write_func_t)(struct ul_storage *, ul_off_t off, 
        void *buf, int len);
typedef int (*ul_storage_size_func_t)(struct ul_storage *uls, ul_off_t *size);

struct uls_mem {
    uint8_t *um_buf;
    ul_off_t um_buf_size;
};

struct ul_storage {
    ul_storage_read_func_t uls_read;
    ul_storage_write_func_t uls_write;
    ul_storage_size_func_t uls_size;
    void *uls_arg;
};

struct ul_entry_hdr {
    uint64_t ue_ts;
    uint16_t ue_len;
    uint16_t ue_flags;
}; 
#define UL_ENTRY_SIZE(__ue) (sizeof(struct ul_entry_hdr) + (__ue)->ue_len)

struct util_log {
    char *ul_name;
    struct ul_storage *ul_uls;
    ul_off_t ul_start_off;
    ul_off_t ul_last_off;
    ul_off_t ul_cur_end_off;
    ul_off_t ul_end_off;
    STAILQ_ENTRY(util_log) ul_next;
};

int uls_mem_init(struct ul_storage *uls, struct uls_mem *umem);
int util_log_register(char *name, struct util_log *log, struct ul_storage *uls);
int util_log_write(struct util_log *log, uint8_t *data, uint16_t len);

#endif /* __UTIL_LOG_H__ */
