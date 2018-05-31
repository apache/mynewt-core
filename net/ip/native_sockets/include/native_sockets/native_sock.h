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

#ifndef H_NATIVE_SOCKETS_
#define H_NATIVE_SOCKETS_

#include <sys/socket.h>
struct mn_socket;
struct mn_sockaddr;

#define MN_AF_LOCAL         255
#define MN_PF_LOCAL         MN_AF_LOCAL

struct mn_sockaddr_un {
    uint8_t msun_len;
    uint8_t msun_family;
    char msun_path[104];
};

int native_sock_create(struct mn_socket **sp, uint8_t domain,
  uint8_t type, uint8_t proto);
int native_sock_close(struct mn_socket *);
int native_sock_connect(struct mn_socket *, struct mn_sockaddr *);
int native_sock_bind(struct mn_socket *, struct mn_sockaddr *);
int native_sock_listen(struct mn_socket *, uint8_t qlen);
int native_sock_sendto(struct mn_socket *, struct os_mbuf *,
  struct mn_sockaddr *);
int native_sock_recvfrom(struct mn_socket *, struct os_mbuf **,
  struct mn_sockaddr *);
int native_sock_getsockopt(struct mn_socket *, uint8_t level,
  uint8_t name, void *val);
int native_sock_setsockopt(struct mn_socket *, uint8_t level,
  uint8_t name, void *val);
int native_sock_getsockname(struct mn_socket *, struct mn_sockaddr *);
int native_sock_getpeername(struct mn_socket *, struct mn_sockaddr *);

#endif
