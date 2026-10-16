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

/*
 * USB network example.
 *
 * USB network function (NCM by default) is added to lwIP by
 * hw/usb/tinyusb/net_lwip with static address USBD_NET_IP_ADDR and a DHCP
 * server for the host.  If the target enables Ethernet (ETH_0) it runs in
 * the same lwIP stack, gets its address from DHCP and stays the default
 * route.  Echo (7), discard (9) and chargen (19) TCP/UDP services are
 * reachable on both interfaces.  Shell commands "lsif" and "ip" show
 * interface state.
 */

#include <os/mynewt.h>
#include <console/console.h>
#include <inet_def_service/inet_def_service.h>

#include <lwip/netif.h>
#include <lwip/ip4_addr.h>
#include <net_lwip/net_lwip.h>

int
mynewt_main(int argc, char **argv)
{
    struct netif *nif;
    char addr[IP4ADDR_STRLEN_MAX];

    sysinit();

    inet_def_service_init(os_eventq_dflt_get());

    nif = usb_net_lwip_netif();
    ip4addr_ntoa_r(netif_ip4_addr(nif), addr, sizeof(addr));
    console_printf("usb_net: USB interface %c%c%u at %s\n", nif->name[0],
                   nif->name[1], nif->num, addr);

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }

    return 0;
}
