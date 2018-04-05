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

#include <inttypes.h>
#include <assert.h>
#include <string.h>

#include "os/mynewt.h"

#include "mn_socket/mn_socket.h"
#include "mn_socket/mn_socket_ops.h"

/*
 * Currently there can be just one provider of sockets.
 */
static const struct mn_socket_ops *mn_sock_tgt;

/** All zeros. */
const uint32_t nm_in6addr_any[4];

int
mn_socket_ops_reg(const struct mn_socket_ops *ops)
{
    if (mn_sock_tgt) {
        /*
         * XXXX for now.
         */
        return -1;
    }
    mn_sock_tgt = ops;
    return 0;
}

int
mn_socket(struct mn_socket **sp, uint8_t domain, uint8_t type, uint8_t proto)
{
    int rc;

    *sp = NULL;
    /*
     * XXX Look up where socket should go.
     */
    if (!mn_sock_tgt) {
        return MN_EINVAL;
    }
    rc = mn_sock_tgt->mso_create(sp, domain, type, proto);
    if (*sp) {
        (*sp)->ms_ops = mn_sock_tgt;
    }
    return rc;
}

int
mn_bind(struct mn_socket *s, struct mn_sockaddr *addr)
{
    return s->ms_ops->mso_bind(s, addr);
}

int
mn_connect(struct mn_socket *s, struct mn_sockaddr *addr)
{
    return s->ms_ops->mso_connect(s, addr);
}

int
mn_listen(struct mn_socket *s, uint8_t qlen)
{
    return s->ms_ops->mso_listen(s, qlen);
}

int
mn_recvfrom(struct mn_socket *s, struct os_mbuf **mp, struct mn_sockaddr *from)
{
    return s->ms_ops->mso_recvfrom(s, mp, from);
}

int
mn_sendto(struct mn_socket *s, struct os_mbuf *m, struct mn_sockaddr *to)
{
    return s->ms_ops->mso_sendto(s, m, to);
}

int
mn_getsockopt(struct mn_socket *s, uint8_t level, uint8_t name, void *val)
{
    return s->ms_ops->mso_getsockopt(s, level, name, val);
}

int
mn_setsockopt(struct mn_socket *s, uint8_t level, uint8_t name, void *val)
{
    return s->ms_ops->mso_setsockopt(s, level, name, val);
}

int
mn_getsockname(struct mn_socket *s, struct mn_sockaddr *addr)
{
    return s->ms_ops->mso_getsockname(s, addr);
}

int
mn_getpeername(struct mn_socket *s, struct mn_sockaddr *addr)
{
    return s->ms_ops->mso_getpeername(s, addr);
}

int
mn_close(struct mn_socket *s)
{
    return s->ms_ops->mso_close(s);
}

int
mn_itf_getnext(struct mn_itf *mi)
{
    return mn_sock_tgt->mso_itf_getnext(mi);
}

int
mn_itf_addr_getnext(struct mn_itf *mi, struct mn_itf_addr *mia)
{
    return mn_sock_tgt->mso_itf_addr_getnext(mi, mia);
}

int
mn_itf_get(char *name, struct mn_itf *mi)
{
    memset(mi, 0, sizeof(*mi));
    while (1) {
        if (mn_itf_getnext(mi)) {
            break;
        }
        if (!strcmp(name, mi->mif_name)) {
            return 0;
        }
    }
    return -1;
}
