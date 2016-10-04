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

#ifndef _MGMT_MGMT_H_
#define _MGMT_MGMT_H_

#include <inttypes.h>
#include <json/json.h>

/* First 64 groups are reserved for system level newtmgr commands.
 * Per-user commands are then defined after group 64.
 */
#define MGMT_GROUP_ID_DEFAULT   (0)
#define MGMT_GROUP_ID_IMAGE     (1)
#define MGMT_GROUP_ID_STATS     (2)
#define MGMT_GROUP_ID_CONFIG    (3)
#define MGMT_GROUP_ID_LOGS      (4)
#define MGMT_GROUP_ID_CRASH     (5)
#define MGMT_GROUP_ID_SPLIT     (6)
#define MGMT_GROUP_ID_PERUSER   (64)

struct mgmt_jbuf {
    /* json_buffer must be first element in the structure */
    struct json_buffer mjb_buf;
    struct json_encoder mjb_enc;
    struct os_mbuf *mjb_in_m;
    struct os_mbuf *mjb_out_m;
    void *mjb_arg;
    uint16_t mjb_off;
    uint16_t mjb_end;
};

typedef int (*mgmt_handler_func_t)(struct mgmt_jbuf *);

struct mgmt_handler {
    mgmt_handler_func_t nh_read;
    mgmt_handler_func_t nh_write;
};

struct mgmt_group {
    struct mgmt_handler *mg_handlers;
    uint16_t mg_handlers_count;
    uint16_t mg_group_id;
    STAILQ_ENTRY(mgmt_group) mg_next;
};

#endif /* _MGMT_MGMT_H_ */
