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

#include <util/tpq.h>

#define NMGR_OP_READ (0)
#define NMGR_OP_READ_RSP (1) 
#define NMGR_OP_WRITE (2)
#define NMGR_OP_WRITE_RSP (3) 

struct nmgr_hdr {
    uint8_t nh_op; 
    uint8_t nh_flags;
    uint16_t nh_len;
    uint16_t nh_group;
    uint16_t nh_id;
};

typedef int (*nmgr_handler_func_t)(struct nmgr_hdr *hdr, struct os_mbuf *req, 
        uint16_t srcoff, struct os_mbuf *rsp);

struct nmgr_handler {
    nmgr_handle_func_t nh_read;
    nmgr_handle_func_t nh_write;
};

struct nmgr_pkt {
    struct tpq_element np_tpq_elem;
    struct os_mbuf np_m;
};

struct nmgr_group {
    struct nmgr_handler *ng_handlers;
    uint16_t ng_handlers_count;
    uint16_t ng_group_id;
    STAILQ_ENTRY(nmgr_group) ng_next;
};

typedef int (*nmgr_transport_out_func_t)(struct nmgr_transport *nt, 
        struct os_mbuf *m);

struct nmgr_transport {
    struct os_mqueue nt_imq;
    nmgr_transport_out_func_t nt_output; 
};

#endif /* _NETMGR_H */
