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
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <poll.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include <stdio.h>

#include <sysinit/sysinit.h>
#include <os/os.h>
#include <os/os_mbuf.h>
#include "mn_socket/mn_socket.h"
#include "mn_socket/mn_socket_ops.h"
#include "native_sockets/native_sock.h"

#include "native_sock_priv.h"

static int native_sock_create(struct mn_socket **sp, uint8_t domain,
  uint8_t type, uint8_t proto);
static int native_sock_close(struct mn_socket *);
static int native_sock_connect(struct mn_socket *, struct mn_sockaddr *);
static int native_sock_bind(struct mn_socket *, struct mn_sockaddr *);
static int native_sock_listen(struct mn_socket *, uint8_t qlen);
static int native_sock_sendto(struct mn_socket *, struct os_mbuf *,
  struct mn_sockaddr *);
static int native_sock_recvfrom(struct mn_socket *, struct os_mbuf **,
  struct mn_sockaddr *);
static int native_sock_getsockopt(struct mn_socket *, uint8_t level,
  uint8_t name, void *val);
static int native_sock_setsockopt(struct mn_socket *, uint8_t level,
  uint8_t name, void *val);

static int native_sock_getsockname(struct mn_socket *, struct mn_sockaddr *);
static int native_sock_getpeername(struct mn_socket *, struct mn_sockaddr *);

static struct native_sock {
    struct mn_socket ns_sock;
    int ns_fd;
    unsigned int ns_poll:1;
    unsigned int ns_listen:1;
    uint8_t ns_type;
    uint8_t ns_pf;
    struct os_sem ns_sem;
    STAILQ_HEAD(, os_mbuf_pkthdr) ns_rx;
    struct os_mbuf *ns_tx;
} native_socks[MYNEWT_VAL(NATIVE_SOCKETS_MAX)];

static struct native_sock_state {
    struct pollfd poll_fds[MYNEWT_VAL(NATIVE_SOCKETS_MAX)];
    int poll_fd_cnt;
    struct os_mutex mtx;
    struct os_task task;
} native_sock_state;

static const struct mn_socket_ops native_sock_ops = {
    .mso_create = native_sock_create,
    .mso_close = native_sock_close,

    .mso_bind = native_sock_bind,
    .mso_connect = native_sock_connect,
    .mso_listen = native_sock_listen,

    .mso_sendto = native_sock_sendto,
    .mso_recvfrom = native_sock_recvfrom,

    .mso_getsockopt = native_sock_getsockopt,
    .mso_setsockopt = native_sock_setsockopt,

    .mso_getsockname = native_sock_getsockname,
    .mso_getpeername = native_sock_getpeername,

    .mso_itf_getnext = native_sock_itf_getnext,
    .mso_itf_addr_getnext = native_sock_itf_addr_getnext
};

static struct native_sock *
native_get_sock(void)
{
    int i;
    struct native_sock *ns;

    for (i = 0; i < MYNEWT_VAL(NATIVE_SOCKETS_MAX); i++) {
        if (native_socks[i].ns_fd < 0) {
            ns = &native_socks[i];
            ns->ns_poll = 0;
            ns->ns_listen = 0;
            return ns;
        }
    }
    return NULL;
}

static struct native_sock *
native_find_sock(int fd)
{
    int i;

    for (i = 0; i < MYNEWT_VAL(NATIVE_SOCKETS_MAX); i++) {
        if (native_socks[i].ns_fd == fd) {
            return &native_socks[i];
        }
    }
    return NULL;
}

static void
native_sock_poll_rebuild(struct native_sock_state *nss)
{
    struct native_sock *ns;
    int i;
    int j;

    os_mutex_pend(&nss->mtx, OS_WAIT_FOREVER);
    for (i = 0, j = 0; i < MYNEWT_VAL(NATIVE_SOCKETS_MAX); i++) {
        ns = &native_socks[i];
        if (ns->ns_fd < 0) {
            continue;
        }
        if (!ns->ns_poll) {
            continue;
        }
        nss->poll_fds[j].fd = ns->ns_fd;
        nss->poll_fds[j].events = POLLIN | POLLOUT;
        nss->poll_fds[j].revents = 0;
        j++;
    }
    nss->poll_fd_cnt = j;
    os_mutex_release(&nss->mtx);
}

int
native_sock_err_to_mn_err(int err)
{
    switch (err) {
    case 0:
        return 0;
    case EAGAIN:
    case EINPROGRESS:
        return MN_EAGAIN;
    case ENOTCONN:
        return MN_ENOTCONN;
    case ETIMEDOUT:
        return MN_ETIMEDOUT;
    case ENOMEM:
        return MN_ENOBUFS;
    case EADDRINUSE:
        return MN_EADDRINUSE;
    case EADDRNOTAVAIL:
        return MN_EADDRNOTAVAIL;
    default:
        return MN_EINVAL;
    }
}

static int
native_sock_mn_addr_to_addr(struct mn_sockaddr *ms, struct sockaddr *sa,
  int *sa_len)
{
    struct sockaddr_un *sun = (struct sockaddr_un *)sa;
    struct sockaddr_in *sin = (struct sockaddr_in *)sa;
    struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)sa;
    struct mn_sockaddr_un *msun = (struct mn_sockaddr_un *)ms;
    struct mn_sockaddr_in *msin = (struct mn_sockaddr_in *)ms;
    struct mn_sockaddr_in6 *msin6 = (struct mn_sockaddr_in6 *)ms;

    switch (ms->msa_family) {
    case MN_AF_INET:
        sin->sin_family = AF_INET;
#ifndef MN_LINUX
        sin->sin_len = sizeof(*sin);
#endif
        sin->sin_addr.s_addr = msin->msin_addr.s_addr;
        sin->sin_port = msin->msin_port;
        *sa_len = sizeof(*sin);
        break;
    case MN_AF_INET6:
        sin6->sin6_family = AF_INET6;
#ifndef MN_LINUX
        sin6->sin6_len = sizeof(*sin6);
#endif
        sin6->sin6_port = msin6->msin6_port;
        sin6->sin6_flowinfo = msin6->msin6_flowinfo;
        memcpy(&sin6->sin6_addr, &msin6->msin6_addr,
               sizeof(msin6->msin6_addr));
        sin6->sin6_scope_id = msin6->msin6_scope_id;
        *sa_len = sizeof(*sin6);
        break;
    case MN_AF_LOCAL:
        sun->sun_family = AF_LOCAL;
#ifndef MN_LINUX
        sun->sun_len = sizeof(*sun);
#endif
        sun->sun_path[0] = '\0';
        strncat(sun->sun_path, msun->msun_path, sizeof sun->sun_path);
        if (strcmp(sun->sun_path, msun->msun_path) != 0) {
            /* Path too long. */
            return MN_EINVAL;
        }
        *sa_len = sizeof(*sun);
        break;
    default:
        return MN_EPROTONOSUPPORT;
    }
    return 0;
}

static int
native_sock_addr_to_mn_addr(struct sockaddr *sa, struct mn_sockaddr *ms)
{
    struct mn_sockaddr_un *msun = (struct mn_sockaddr_un *)ms;
    struct mn_sockaddr_in *msin = (struct mn_sockaddr_in *)ms;
    struct mn_sockaddr_in6 *msin6 = (struct mn_sockaddr_in6 *)ms;
    struct sockaddr_un *sun = (struct sockaddr_un *)sa;
    struct sockaddr_in *sin = (struct sockaddr_in *)sa;
    struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)sa;

    switch (sa->sa_family) {
    case AF_INET:
        msin->msin_family = MN_AF_INET;
        msin->msin_len = sizeof(*msin);
        msin->msin_addr.s_addr = sin->sin_addr.s_addr;
        msin->msin_port = sin->sin_port;
        break;
    case AF_INET6:
        msin6->msin6_family = MN_AF_INET6;
        msin6->msin6_len = sizeof(*msin6);
        msin6->msin6_port = sin6->sin6_port;
        msin6->msin6_flowinfo = sin6->sin6_flowinfo;
        memcpy(&msin6->msin6_addr, &sin6->sin6_addr,
               sizeof(msin6->msin6_addr));
        msin6->msin6_scope_id = sin6->sin6_scope_id;
        break;
    case AF_LOCAL:
        msun->msun_family = MN_AF_LOCAL;
        strncpy(msun->msun_path, sun->sun_path, sizeof msun->msun_path);
        if (strcmp(msun->msun_path, sun->sun_path) != 0) {
            /* Path too long. */
            return MN_EINVAL;
        }
        break;
    default:
        return MN_EPROTONOSUPPORT;
    }
    return 0;
}

static int
native_sock_create(struct mn_socket **sp, uint8_t domain,
  uint8_t type, uint8_t proto)
{
    struct native_sock_state *nss = &native_sock_state;
    struct native_sock *ns;
    int idx;
    int rc;

    switch (domain) {
    case MN_PF_INET:
        domain = PF_INET;
        break;
    case MN_PF_INET6:
        domain = PF_INET6;
        break;
    case MN_PF_LOCAL:
        domain = PF_LOCAL;
        break;
    default:
        return MN_EPROTONOSUPPORT;
    }

    switch (type) {
    case MN_SOCK_DGRAM:
        type = SOCK_DGRAM;
        break;
    case MN_SOCK_STREAM:
        type = SOCK_STREAM;
        break;
    case 0:
        break;
    default:
        return MN_EPROTONOSUPPORT;
    }

    os_mutex_pend(&nss->mtx, OS_WAIT_FOREVER);
    ns = native_get_sock();
    if (!ns) {
        os_mutex_release(&nss->mtx);
        return MN_ENOBUFS;
    }
    os_sem_init(&ns->ns_sem, 0);
    idx = socket(domain, type, proto);

    /* Make the socket nonblocking. */
    rc = fcntl(idx, F_SETFL, fcntl(idx, F_GETFL, 0) | O_NONBLOCK);
    assert(rc == 0);

    ns->ns_fd = idx;
    ns->ns_pf = domain;
    ns->ns_type = type;
    os_mutex_release(&nss->mtx);
    if (idx < 0) {
        return MN_ENOBUFS;
    }
    *sp = &ns->ns_sock;
    return 0;
}

static int
native_sock_close(struct mn_socket *s)
{
    struct native_sock_state *nss = &native_sock_state;
    struct native_sock *ns = (struct native_sock *)s;
    struct os_mbuf_pkthdr *m;

    os_mutex_pend(&nss->mtx, OS_WAIT_FOREVER);
    close(ns->ns_fd);
    ns->ns_fd = -1;

    /*
     * When socket is closed, we must free all mbufs which might be
     * queued in it.
     */
    while ((m = STAILQ_FIRST(&ns->ns_rx))) {
        STAILQ_REMOVE_HEAD(&ns->ns_rx, omp_next);
        os_mbuf_free_chain(OS_MBUF_PKTHDR_TO_MBUF(m));
    }
    os_mbuf_free_chain(ns->ns_tx);
    native_sock_poll_rebuild(nss);
    os_mutex_release(&nss->mtx);
    return 0;
}

static int
native_sock_connect(struct mn_socket *s, struct mn_sockaddr *addr)
{
    struct native_sock_state *nss = &native_sock_state;
    struct native_sock *ns = (struct native_sock *)s;
    struct sockaddr_storage ss;
    struct sockaddr *sa = (struct sockaddr *)&ss;
    int rc;
    int sa_len;

    rc = native_sock_mn_addr_to_addr(addr, sa, &sa_len);
    if (rc) {
        return rc;
    }
    os_mutex_pend(&nss->mtx, OS_WAIT_FOREVER);
    if (connect(ns->ns_fd, sa, sa_len)) {
        rc = errno;
        os_mutex_release(&nss->mtx);
        return native_sock_err_to_mn_err(rc);
    }
    ns->ns_poll = 1;
    native_sock_poll_rebuild(nss);
    os_mutex_release(&nss->mtx);
    mn_socket_writable(s, 0);
    return 0;
}

static int
native_sock_bind(struct mn_socket *s, struct mn_sockaddr *addr)
{
    struct native_sock_state *nss = &native_sock_state;
    struct native_sock *ns = (struct native_sock *)s;
    struct sockaddr_un ss;
    struct sockaddr *sa = (struct sockaddr *)&ss;
    int rc;
    int sa_len;
    int val = 1;

    rc = native_sock_mn_addr_to_addr(addr, sa, &sa_len);
    if (rc) {
        return rc;
    }

    os_mutex_pend(&nss->mtx, OS_WAIT_FOREVER);
    if (ns->ns_type == SOCK_STREAM) {
        rc = setsockopt(ns->ns_fd, SOL_SOCKET, SO_REUSEADDR, &val,
          sizeof(val));
        if (rc) {
            goto err;
        }
    }
    rc = ioctl(ns->ns_fd, FIONBIO, (char *)&val);
    if (rc) {
        goto err;
    }
    if (bind(ns->ns_fd, sa, sa_len)) {
        goto err;
    }
    if (ns->ns_type == SOCK_DGRAM) {
        ns->ns_poll = 1;
        native_sock_poll_rebuild(nss);
    }
    os_mutex_release(&nss->mtx);
    return 0;
err:
    rc = errno;
    os_mutex_release(&nss->mtx);
    return native_sock_err_to_mn_err(rc);
}

static int
native_sock_listen(struct mn_socket *s, uint8_t qlen)
{
    struct native_sock_state *nss = &native_sock_state;
    struct native_sock *ns = (struct native_sock *)s;
    int rc;

    os_mutex_pend(&nss->mtx, OS_WAIT_FOREVER);
    if (listen(ns->ns_fd, qlen)) {
        rc = errno;
        os_mutex_release(&nss->mtx);
        return native_sock_err_to_mn_err(rc);
    }
    ns->ns_poll = 1;
    ns->ns_listen = 1;
    native_sock_poll_rebuild(nss);
    os_mutex_release(&nss->mtx);
    return 0;
}

/*
 * TX routine for stream sockets (TCP). The data to send is pointed
 * by ns_tx.
 * Keep sending mbufs until socket says that it can't take anymore.
 * then wait for send event notification before continuing.
 */
static int
native_sock_stream_tx(struct native_sock *ns, int notify)
{
    struct native_sock_state *nss = &native_sock_state;
    struct os_mbuf *m;
    struct os_mbuf *n;
    int rc;

    rc = 0;

    os_mutex_pend(&nss->mtx, OS_TIMEOUT_NEVER);
    while (ns->ns_tx && rc == 0) {
        m = ns->ns_tx;
        n = SLIST_NEXT(m, om_next);

        errno = 0;
        rc = write(ns->ns_fd, m->om_data, m->om_len);
        if (rc == m->om_len) {
            /* Complete write. */
            ns->ns_tx = n;
            os_mbuf_free(m);
            rc = 0;
        } else if (rc != -1) {
            /* Partial write. */
            os_mbuf_adj(m, m->om_len - rc);
            rc = 0;
        } else {
            /* Error. */
            rc = errno;
            if (rc == EAGAIN) {
                rc = 0;
            } else {
                /*
                 * Socket had an error. User should close it.
                 */
                os_mbuf_free_chain(ns->ns_tx);
                ns->ns_tx = NULL;
                rc = native_sock_err_to_mn_err(rc);
            }
            break;
        }
    }
    os_mutex_release(&nss->mtx);
    if (notify) {
        mn_socket_writable(&ns->ns_sock, rc);
    }
    return rc;
}

static int
native_sock_set_tx_buf(struct native_sock *ns, struct os_mbuf *om)
{
    struct native_sock_state *nss = &native_sock_state;
    int rc;

    os_mutex_pend(&nss->mtx, OS_WAIT_FOREVER);
    if (ns->ns_tx) {
        rc = MN_EAGAIN;
    } else {
        ns->ns_tx = om;
        rc = 0;
    }
    os_mutex_release(&nss->mtx);

    return rc;
}

static int
native_sock_sendto(struct mn_socket *s, struct os_mbuf *m,
  struct mn_sockaddr *addr)
{
    struct native_sock *ns = (struct native_sock *)s;
    struct sockaddr_storage ss;
    struct sockaddr *sa = (struct sockaddr *)&ss;
    uint8_t tmpbuf[MYNEWT_VAL(NATIVE_SOCKETS_MAX_UDP)];
    struct os_mbuf *o;
    int sa_len;
    int off;
    int rc;

    if (ns->ns_type == SOCK_DGRAM) {
        rc = native_sock_mn_addr_to_addr(addr, sa, &sa_len);
        if (rc) {
            return rc;
        }
        off = 0;
        for (o = m; o; o = SLIST_NEXT(o, om_next)) {
            if (off + o->om_len > sizeof(tmpbuf)) {
                return MN_ENOBUFS;
            }
            os_mbuf_copydata(o, 0, o->om_len, &tmpbuf[off]);
            off += o->om_len;
        }
        rc = sendto(ns->ns_fd, tmpbuf, off, 0, sa, sa_len);
        if (rc != off) {
            return native_sock_err_to_mn_err(errno);
        }
        os_mbuf_free_chain(m);
        return 0;
    } else {
        rc = native_sock_set_tx_buf(ns, m);
        if (rc != 0) {
            return rc;
        }

        rc = native_sock_stream_tx(ns, 0);
        return rc;
    }
}

static int
native_sock_recvfrom(struct mn_socket *s, struct os_mbuf **mp,
  struct mn_sockaddr *addr)
{
    struct native_sock *ns = (struct native_sock *)s;
    struct sockaddr_storage ss;
    struct sockaddr *sa = (struct sockaddr *)&ss;
    uint8_t tmpbuf[MYNEWT_VAL(NATIVE_SOCKETS_MAX_UDP)];
    struct os_mbuf *m;
    socklen_t slen;
    int rc;

    slen = sizeof(ss);
    if (ns->ns_type == SOCK_DGRAM) {
        rc = recvfrom(ns->ns_fd, tmpbuf, sizeof(tmpbuf), 0, sa, &slen);
    } else {
        rc = getpeername(ns->ns_fd, sa, &slen);
        if (rc == 0) {
            rc = read(ns->ns_fd, tmpbuf, sizeof(tmpbuf));
        }
    }
    if (rc < 0) {
        return native_sock_err_to_mn_err(errno);
    }
    if (ns->ns_type == SOCK_STREAM && rc == 0) {
        mn_socket_readable(&ns->ns_sock, MN_ECONNABORTED);
        ns->ns_poll = 0;
        native_sock_poll_rebuild(&native_sock_state);
        return MN_ECONNABORTED;
    }

    m = os_msys_get_pkthdr(rc, 0);
    if (!m) {
        return MN_ENOBUFS;
    }
    os_mbuf_copyinto(m, 0, tmpbuf, rc);
    *mp = m;
    if (addr) {
        native_sock_addr_to_mn_addr(sa, addr);
    }
    return 0;
}

static int
native_sock_getsockopt(struct mn_socket *s, uint8_t level, uint8_t name,
  void *val)
{
    return MN_EPROTONOSUPPORT;
}

static int
native_sock_setsockopt(struct mn_socket *s, uint8_t level, uint8_t name,
  void *val)
{
    struct native_sock *ns = (struct native_sock *)s;
    int rc;
    uint32_t val32;
    struct group_req greq;
    struct sockaddr_in *sin;
    struct sockaddr_in6 *sin6;
    struct mn_mreq *mreq;

    if (level == MN_SO_LEVEL) {
        switch (name) {
        case MN_MCAST_JOIN_GROUP:
        case MN_MCAST_LEAVE_GROUP:
            mreq = val;
            memset(&greq, 0, sizeof(greq));
            greq.gr_interface = mreq->mm_idx;
            if (mreq->mm_family == MN_AF_INET) {
                sin = (struct sockaddr_in *)&greq.gr_group;
#ifndef MN_LINUX
                sin->sin_len = sizeof(*sin);
#endif
                sin->sin_family = AF_INET;
                memcpy(&sin->sin_addr, &mreq->mm_addr, sizeof(struct in_addr));
                level = IPPROTO_IP;
            } else {
                sin6 = (struct sockaddr_in6 *)&greq.gr_group;
#ifndef MN_LINUX
                sin6->sin6_len = sizeof(*sin6);
#endif
                sin6->sin6_family = AF_INET6;
                memcpy(&sin6->sin6_addr, &mreq->mm_addr,
                  sizeof(struct in6_addr));
                level = IPPROTO_IPV6;
            }

            if (name == MN_MCAST_JOIN_GROUP) {
                name = MCAST_JOIN_GROUP;
            } else {
                name = MCAST_LEAVE_GROUP;
            }
            rc = setsockopt(ns->ns_fd, level, name, &greq, sizeof(greq));
            if (rc) {
                return native_sock_err_to_mn_err(errno);
            }
            return 0;
        case MN_MCAST_IF:
            if (ns->ns_pf == AF_INET) {
                level = IPPROTO_IP;
                name = IP_MULTICAST_IF;
                rc = native_sock_itf_addr(*(int *)val, &val32);
                if (rc) {
                    return rc;
                }
            } else {
                level = IPPROTO_IPV6;
                name = IPV6_MULTICAST_IF;
                val32 = *(uint32_t *)val;
            }
            rc = setsockopt(ns->ns_fd, level, name, &val32, sizeof(val32));
            if (rc) {
                return native_sock_err_to_mn_err(errno);
            }
            return 0;

        case MN_REUSEADDR:
            name = SO_REUSEADDR;
            val32 = *(uint32_t *)val;
            rc = setsockopt(ns->ns_fd, level, name, &val32, sizeof(val32));
            if (rc) {
                return native_sock_err_to_mn_err(errno);
            }
            return 0;
        }
    }
    return MN_EPROTONOSUPPORT;
}

static int
native_sock_getsockname(struct mn_socket *s, struct mn_sockaddr *addr)
{
    struct native_sock *ns = (struct native_sock *)s;
    struct sockaddr_storage ss;
    struct sockaddr *sa = (struct sockaddr *)&ss;
    socklen_t len;
    int rc;

    len = sizeof(struct sockaddr_storage);
    rc = getsockname(ns->ns_fd, sa, &len);
    if (rc) {
        return native_sock_err_to_mn_err(errno);
    }
    rc = native_sock_addr_to_mn_addr(sa, addr);
    if (rc) {
        return rc;
    }
    return 0;
}

static int
native_sock_getpeername(struct mn_socket *s, struct mn_sockaddr *addr)
{
    struct native_sock *ns = (struct native_sock *)s;
    struct sockaddr_storage ss;
    struct sockaddr *sa = (struct sockaddr *)&ss;
    socklen_t len;
    int rc;

    len = sizeof(struct sockaddr_storage);
    rc = getpeername(ns->ns_fd, sa, &len);
    if (rc) {
        return native_sock_err_to_mn_err(errno);
    }
    rc = native_sock_addr_to_mn_addr(sa, addr);
    if (rc) {
        return rc;
    }
    return 0;
}

/*
 * XXX should do this task with SIGIO as well.
 */
static void
socket_task(void *arg)
{
    struct native_sock_state *nss = arg;
    struct native_sock *ns, *new_ns;
    struct sockaddr_storage ss;
    struct sockaddr *sa = (struct sockaddr *)&ss;
    int revents;
    int i;
    socklen_t slen;
    int rc;

    os_mutex_pend(&nss->mtx, OS_WAIT_FOREVER);
    while (1) {
        os_mutex_release(&nss->mtx);
        os_time_delay(MYNEWT_VAL(NATIVE_SOCKETS_POLL_ITVL));
        os_mutex_pend(&nss->mtx, OS_WAIT_FOREVER);
        if (nss->poll_fd_cnt) {
            rc = poll(nss->poll_fds, nss->poll_fd_cnt, 0);
        } else {
            rc = 0;
        }
        if (rc == 0) {
            continue;
        }
        for (i = 0; i < nss->poll_fd_cnt; i++) {
            if (nss->poll_fds[i].revents == 0) {
                continue;
            }

            revents = nss->poll_fds[i].revents;
            nss->poll_fds[i].revents = 0;

            ns = native_find_sock(nss->poll_fds[i].fd);
            assert(ns);

            if (revents & POLLIN) {
                if (ns->ns_listen) {
                    new_ns = native_get_sock();
                    if (!new_ns) {
                        continue;
                    }
                    slen = sizeof(ss);
                    new_ns->ns_fd = accept(ns->ns_fd, sa, &slen);
                    if (new_ns->ns_fd < 0) {
                        continue;
                    }
                    new_ns->ns_type = ns->ns_type;
                    new_ns->ns_sock.ms_ops = &native_sock_ops;
                    os_mutex_release(&nss->mtx);
                    if (mn_socket_newconn(&ns->ns_sock, &new_ns->ns_sock)) {
                        /*
                         * should close
                         */
                    }
                    os_mutex_pend(&nss->mtx, OS_WAIT_FOREVER);
                    new_ns->ns_poll = 1;
                    native_sock_poll_rebuild(nss);
                } else {
                    mn_socket_readable(&ns->ns_sock, 0);
                }
            }

            if (revents & POLLOUT) {
                if (ns->ns_type == SOCK_STREAM && ns->ns_tx) {
                    native_sock_stream_tx(ns, 1);
                }
            }
        }
    }
}

int
native_sock_init(void)
{
    struct native_sock_state *nss = &native_sock_state;
    int i;
    os_stack_t *sp;

    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    for (i = 0; i < MYNEWT_VAL(NATIVE_SOCKETS_MAX); i++) {
        native_socks[i].ns_fd = -1;
        STAILQ_INIT(&native_socks[i].ns_rx);
    }
    sp = malloc(sizeof(os_stack_t) * MYNEWT_VAL(NATIVE_SOCKETS_STACK_SZ));
    if (!sp) {
        return -1;
    }
    os_mutex_init(&nss->mtx);
    i = os_task_init(&nss->task, "socket", socket_task, &native_sock_state,
      MYNEWT_VAL(NATIVE_SOCKETS_PRIO), OS_WAIT_FOREVER, sp,
      MYNEWT_VAL(NATIVE_SOCKETS_STACK_SZ));
    if (i) {
        return -1;
    }
    i = mn_socket_ops_reg(&native_sock_ops);
    if (i) {
        return -1;
    }

    return 0;
}
