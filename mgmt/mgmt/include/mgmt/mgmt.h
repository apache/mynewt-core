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

#ifndef _MGMT_MGMT_H_
#define _MGMT_MGMT_H_

#include <inttypes.h>
#include "os/mynewt.h"

#ifdef __cplusplus
extern "C" {
#endif

/* MTU for newtmgr responses */
#define MGMT_MAX_MTU 1024

#ifndef STR
/* Stringification of constants */
#define STR(x) #x
#endif

#define NMGR_OP_READ            (0)
#define NMGR_OP_READ_RSP        (1)
#define NMGR_OP_WRITE           (2)
#define NMGR_OP_WRITE_RSP       (3)

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
#define MGMT_GROUP_ID_RUN       (7)
#define MGMT_GROUP_ID_FS        (8)
#define MGMT_GROUP_ID_PERUSER   (64)

/**
 * Newtmgr error codes
 */
#define MGMT_ERR_EOK        (0)
#define MGMT_ERR_EUNKNOWN   (1)
#define MGMT_ERR_ENOMEM     (2)
#define MGMT_ERR_EINVAL     (3)
#define MGMT_ERR_ETIMEOUT   (4)
#define MGMT_ERR_ENOENT     (5)
#define MGMT_ERR_EBADSTATE  (6)     /* Current state disallows command. */
#define MGMT_ERR_EPERUSER   (256)

#define NMGR_HDR_SIZE           (8)

struct nmgr_hdr {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    uint8_t  nh_op:3;           /* NMGR_OP_XXX */
    uint8_t  _res1:5;
#endif
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    uint8_t  _res1:5;
    uint8_t  nh_op:3;           /* NMGR_OP_XXX */
#endif
    uint8_t  nh_flags;          /* XXX reserved for future flags */
    uint16_t nh_len;            /* length of the payload */
    uint16_t nh_group;          /* NMGR_GROUP_XXX */
    uint8_t  nh_seq;            /* sequence number */
    uint8_t  nh_id;             /* message ID within group */
};

struct mgmt_cbuf;

typedef int (*mgmt_handler_func_t)(struct mgmt_cbuf *);

struct mgmt_handler {
    mgmt_handler_func_t mh_read;
    mgmt_handler_func_t mh_write;
};

struct mgmt_group {
    const struct mgmt_handler *mg_handlers;
    uint16_t mg_handlers_count;
    uint16_t mg_group_id;
    STAILQ_ENTRY(mgmt_group) mg_next;
};

#define MGMT_GROUP_SET_HANDLERS(__group, __handlers)       \
    (__group)->mg_handlers = (__handlers);                 \
    (__group)->mg_handlers_count = (sizeof((__handlers)) / \
            sizeof(struct mgmt_handler));

int mgmt_group_register(struct mgmt_group *group);
int mgmt_cbuf_setoerr(struct mgmt_cbuf *njb, int errcode);
const struct mgmt_handler *mgmt_find_handler(uint16_t group_id,
  uint16_t handler_id);

struct os_eventq *mgmt_evq_get(void);
void mgmt_evq_set(struct os_eventq *evq);

#ifdef __cplusplus
}
#endif

#endif /* _MGMT_MGMT_H_ */
