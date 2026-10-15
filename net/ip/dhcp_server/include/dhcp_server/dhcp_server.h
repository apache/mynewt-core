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

#ifndef H_DHCP_SERVER_
#define H_DHCP_SERVER_

#include <stdint.h>
#include <lwip/netif.h>

#ifdef __cplusplus
extern "C" {
#endif

struct dhcp_server_cfg {
    /** Host part of first leased address, e.g. 2 gives 192.168.7.2 */
    uint32_t pool_start;
    /** Number of leased addresses, at most DHCP_SERVER_MAX_LEASES */
    uint16_t pool_size;
    /** Lease time in seconds */
    uint32_t lease_time;
};

/**
 * Starts DHCP server on network interface.
 *
 * Server answers only on given interface and leases addresses from the
 * interface subnet.  Router and DNS server options are not sent, clients
 * do not route their traffic through the device.  Must be called from
 * tcpip thread or with tcpip core lock held, after interface address is
 * set.
 *
 * @param nif  Network interface to serve.
 * @param cfg  Address pool and lease time; it must stay valid while the
 *             server runs.
 *
 * @return 0 on success, SYS_EINVAL when pool does not fit in interface
 *         subnet or contains interface address, SYS_ENOMEM when all
 *         server instances are in use or UDP PCB can't be allocated,
 *         SYS_EBUSY when DHCP server port is already in use.
 */
int dhcp_server_start(struct netif *nif, const struct dhcp_server_cfg *cfg);

#ifdef __cplusplus
}
#endif

#endif /* H_DHCP_SERVER_ */
