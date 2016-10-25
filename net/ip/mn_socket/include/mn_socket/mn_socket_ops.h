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
#ifndef __SYS_MN_SOCKET_OPS_H_
#define __SYS_MN_SOCKET_OPS_H_

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Interface for socket providers.
 * - mso_create() creates a socket, memory allocation has to be done by
 *   the socket provider.
 * - mso_close() closes the socket, memory should be freed. User should not
 *   be using the socket pointer once it has been closed.
 */
struct mn_socket_ops {
    int (*mso_create)(struct mn_socket **, uint8_t domain, uint8_t type,
      uint8_t protocol);
    int (*mso_close)(struct mn_socket *);

    int (*mso_bind)(struct mn_socket *, struct mn_sockaddr *);
    int (*mso_connect)(struct mn_socket *, struct mn_sockaddr *);
    int (*mso_listen)(struct mn_socket *, uint8_t qlen);

    int (*mso_sendto)(struct mn_socket *, struct os_mbuf *,
      struct mn_sockaddr *to);
    int (*mso_recvfrom)(struct mn_socket *, struct os_mbuf **,
      struct mn_sockaddr *from);

    int (*mso_getsockopt)(struct mn_socket *, uint8_t level, uint8_t name,
      void *val);
    int (*mso_setsockopt)(struct mn_socket *, uint8_t level, uint8_t name,
      void *val);

    int (*mso_getsockname)(struct mn_socket *, struct mn_sockaddr *);
    int (*mso_getpeername)(struct mn_socket *, struct mn_sockaddr *);

    int (*mso_itf_getnext)(struct mn_itf *);
    int (*mso_itf_addr_getnext)(struct mn_itf *, struct mn_itf_addr *);
};

int mn_socket_ops_reg(const struct mn_socket_ops *ops);

static inline void
mn_socket_writable(struct mn_socket *s, int error)
{
    if (s->ms_cbs && s->ms_cbs->socket.writable) {
        s->ms_cbs->socket.writable(s->ms_cb_arg, error);
    }
}

static inline void
mn_socket_readable(struct mn_socket *s, int error)
{
    if (s->ms_cbs && s->ms_cbs->socket.readable) {
        s->ms_cbs->socket.readable(s->ms_cb_arg, error);
    }
}

static inline int
mn_socket_newconn(struct mn_socket *s, struct mn_socket *new)
{
    if (s->ms_cbs && s->ms_cbs->listen.newconn) {
        return s->ms_cbs->listen.newconn(s->ms_cb_arg, new);
    } else {
        return -1;
    }
}

#ifdef __cplusplus
}
#endif

#endif /* __SYS_MN_SOCKET_OPS_H_ */
