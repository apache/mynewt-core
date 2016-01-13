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

#ifndef _NEWTMGR_H_
#define _NEWTMGR_H_

#include <inttypes.h>
#include <os/os.h>

/* First 64 groups are reserved for system level newtmgr commands.
 * Per-user commands are then defined after group 64.
 */
#define NMGR_GROUP_ID_DEFAULT   (0)
#define NMGR_GROUP_ID_IMAGE     (1)
#define NMGR_GROUP_ID_PERUSER   (64)

#define NMGR_OP_READ            (0)
#define NMGR_OP_READ_RSP        (1)
#define NMGR_OP_WRITE           (2)
#define NMGR_OP_WRITE_RSP       (3)

/*
 * Id's for default group commands
 */
#define NMGR_ID_ECHO	        0
#define NMGR_ID_CONS_ECHO_CTRL  1

struct nmgr_hdr {
    uint8_t nh_op;
    uint8_t nh_flags;
    uint16_t nh_len;
    uint16_t nh_group;
    uint16_t nh_id;
};

typedef int (*nmgr_handler_func_t)(struct nmgr_hdr *nmr, struct os_mbuf *req, 
        uint16_t srcoff, struct nmgr_hdr *rsp_hdr, struct os_mbuf *rsp);

#define NMGR_HANDLER_FUNC(__name)                                           \
    int __name(struct nmgr_hdr *nmr, struct os_mbuf *req, uint16_t srcoff,  \
            struct os_mbuf *rsp)

struct nmgr_handler {
    nmgr_handler_func_t nh_read;
    nmgr_handler_func_t nh_write;
};

struct nmgr_group {
    struct nmgr_handler *ng_handlers;
    uint16_t ng_handlers_count;
    uint16_t ng_group_id;
    STAILQ_ENTRY(nmgr_group) ng_next;
};

#define NMGR_GROUP_SET_HANDLERS(__group, __handlers)       \
    (__group)->ng_handlers = (__handlers);                 \
    (__group)->ng_handlers_count = (sizeof((__handlers)) / \
            sizeof(struct nmgr_handler)); 

struct nmgr_transport;
typedef int (*nmgr_transport_out_func_t)(struct nmgr_transport *nt, 
        struct os_mbuf *m);

struct nmgr_transport {
    struct os_mqueue nt_imq;
    nmgr_transport_out_func_t nt_output; 
};

int nmgr_task_init(uint8_t, os_stack_t *, uint16_t);
int nmgr_rsp_extend(struct nmgr_hdr *, struct os_mbuf *, void *data, uint16_t);
int nmgr_group_register(struct nmgr_group *group);

#endif /* _NETMGR_H */
