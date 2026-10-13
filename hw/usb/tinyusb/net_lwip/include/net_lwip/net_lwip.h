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

#ifndef H_NET_LWIP_
#define H_NET_LWIP_

#ifdef __cplusplus
extern "C" {
#endif

struct netif;

/**
 * Returns lwIP network interface of USB network function.
 *
 * Interface is added during sysinit (USBD_NET_SYSINIT_STAGE) with static
 * IPv4 address USBD_NET_IP_ADDR.  Its link is up while USB host has the
 * network function configured.
 *
 * @return pointer to lwIP network interface
 */
struct netif *usb_net_lwip_netif(void);

#ifdef __cplusplus
}
#endif

#endif /* H_NET_LWIP_ */
