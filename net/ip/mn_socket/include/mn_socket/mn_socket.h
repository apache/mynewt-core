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
#ifndef __SYS_MN_SOCKET_H_
#define __SYS_MN_SOCKET_H_

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Address/protocol family.
 */
#define MN_AF_INET              4
#define MN_PF_INET              MN_AF_INET
#define MN_AF_INET6             6
#define MN_PF_INET6             MN_AF_INET6

/*
 * Socket types
 */
#define MN_SOCK_STREAM          1
#define MN_SOCK_DGRAM           2

/*
 * Error codes from mn_socket interface.
 */
#define MN_EAFNOSUPPORT         1
#define MN_EPROTONOSUPPORT      2
#define MN_ENOBUFS              3
#define MN_EINVAL               4
#define MN_ENOTCONN             5
#define MN_ECONNABORTED         6
#define MN_EDESTADDRREQ         7
#define MN_EADDRINUSE           8
#define MN_ETIMEDOUT            9
#define MN_EAGAIN               10
#define MN_EUNKNOWN             11
#define MN_EADDRNOTAVAIL        12
#define MN_ENETUNREACH          13
#define MN_OPNOSUPPORT          14

/*
 * Multicast macros
 */
#define MN_IN_MULTICAST(a)                                              \
    ((((uint32_t)(a)) & 0xf0000000) == 0xe0000000)

#define MN_IN6_IS_ADDR_MULTICAST(a)                                     \
    ((a)->s_addr[0] == 0xff)

struct mn_socket;
struct mn_socket_ops;
struct mn_sock_cb;
struct os_mbuf;

struct mn_socket {
    const union mn_socket_cb *ms_cbs;          /* filled in by user */
    void *ms_cb_arg;                           /* filled in by user */
    const struct mn_socket_ops *ms_ops;        /* filled in by mn_socket */
};

/*
 * Callbacks. Socket callbacks are for sockets which exchange
 * data. Listen callback is for TCP listen sockets.
 */
union mn_socket_cb {
    struct {
        void (*readable)(void *cb_arg, int err);
        void (*writable)(void *cb_arg, int err);
    } socket;
    struct {
        int (*newconn)(void *cb_arg, struct mn_socket *new);
    } listen;
};

struct mn_sockaddr {
    uint8_t msa_len;
    uint8_t msa_family;
    char    msa_data[2];
};

struct mn_in_addr {
    uint32_t s_addr;
};

struct mn_sockaddr_in {
    uint8_t msin_len;
    uint8_t msin_family;
    uint16_t msin_port;
    struct mn_in_addr msin_addr;
};

struct mn_in6_addr {
    uint8_t s_addr[16];
};

struct mn_sockaddr_in6 {
    uint8_t msin6_len;
    uint8_t msin6_family;
    uint16_t msin6_port;
    uint32_t msin6_flowinfo;
    struct mn_in6_addr msin6_addr;
    uint32_t msin6_scope_id;
};

extern const uint32_t nm_in6addr_any[4];

/*
 * Structure for multicast join/leave
 */
struct mn_mreq {
    uint8_t mm_idx;			/* interface index; must not be 0 */
    uint8_t mm_family;			/* address family */
    union {
        struct mn_in_addr v4;
        struct mn_in6_addr v6;
    } mm_addr;
};

#define MN_SO_LEVEL                     0xfe

#define MN_MCAST_JOIN_GROUP             1
#define MN_MCAST_LEAVE_GROUP            2
#define MN_MCAST_IF                     3
#define MN_REUSEADDR                    4

/*
 * Socket calls.
 *
 * mn_connect() for TCP is asynchronous. Once connection has been established,
 * socket callback (*writable) will be called.
 *
 * mn_sendto() is asynchronous as well. If it fails due to buffer shortage,
 * socket provider should call (*writable) when more data can be sent.
 *
 * mn_recvfrom() returns immediatelly if no data is available. If data arrives,
 * the callback (*readable) will be called. Once that happens, owner of the
 * socket should keep calling mn_recvfrom() until it has drained all the
 * data from the socket.
 *
 * If remote end closes the socket, socket callback (*readable) will be
 * called.
 */
int mn_socket(struct mn_socket **, uint8_t domain, uint8_t type, uint8_t proto);
int mn_bind(struct mn_socket *, struct mn_sockaddr *);
int mn_connect(struct mn_socket *, struct mn_sockaddr *);
int mn_listen(struct mn_socket *, uint8_t qlen);

int mn_recvfrom(struct mn_socket *, struct os_mbuf **,
  struct mn_sockaddr *from);
int mn_sendto(struct mn_socket *, struct os_mbuf *, struct mn_sockaddr *to);

int mn_getsockopt(struct mn_socket *, uint8_t level, uint8_t optname,
  void *optval);
int mn_setsockopt(struct mn_socket *, uint8_t level, uint8_t optname,
  void *optval);

int mn_getsockname(struct mn_socket *, struct mn_sockaddr *);
int mn_getpeername(struct mn_socket *, struct mn_sockaddr *);

int mn_close(struct mn_socket *);

#define mn_socket_set_cbs(sock, cb_arg, cbs)                            \
    do {                                                                \
        (sock)->ms_cbs = (cbs);                                         \
        (sock)->ms_cb_arg = (cb_arg);                                   \
    } while (0)

/*
 * Address conversion
 */
int mn_inet_pton(int af, const char *src, void *dst);
const char *mn_inet_ntop(int af, const void *src, void *dst, int len);

/*
 * Info about interfaces.
 */
#define MN_ITF_NAME_MAX    8

/*
 * Interface flags
 */
#define MN_ITF_F_UP        1
#define MN_ITF_F_MULTICAST 2
#define MN_ITF_F_LINK      4

struct mn_itf {
    char mif_name[MN_ITF_NAME_MAX];
    uint8_t mif_idx;
    uint8_t mif_flags;
};

struct mn_itf_addr {
    uint8_t mifa_family;
    uint8_t mifa_plen;
    union {
        struct mn_in_addr v4;
        struct mn_in6_addr v6;
    } mifa_addr;
};

/*
 * Iterate through interfaces, and their addresses
 */
int mn_itf_getnext(struct mn_itf *);
int mn_itf_addr_getnext(struct mn_itf *, struct mn_itf_addr *);

/*
 * Find specific interface, given name.
 */
int mn_itf_get(char *name, struct mn_itf *mi);

#ifdef __cplusplus
}
#endif

#endif /* __SYS_MN_SOCKET_H_ */
