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

#include "os/mynewt.h"

#if MYNEWT_VAL(LWIP_CLI)
#include <string.h>

#include <mn_socket/mn_socket.h>
#include <console/console.h>
#include <shell/shell.h>

#include <lwip/tcpip.h>
#include <lwip/dhcp.h>

#include "ip_priv.h"

static void
lwip_nif_print(struct mn_itf *itf)
{
    int rc;
    int flags = 0;
    struct mn_itf_addr itf_addr;
    char addr_str[48];

    console_printf("%d: %s %x(", itf->mif_idx, itf->mif_name, itf->mif_flags);
    if (itf->mif_flags & MN_ITF_F_UP) {
        flags = 1;
        console_printf("up");
    }
    if (itf->mif_flags & MN_ITF_F_MULTICAST) {
        if (flags) {
            console_printf("|");
        }
        flags = 1;
        console_printf("mcast");
    }
    if (itf->mif_flags & MN_ITF_F_LINK) {
        if (flags) {
            console_printf("|");
        }
        flags = 1;
        console_printf("link");
    }
    console_printf(")\n");

    memset(&itf_addr, 0, sizeof(itf_addr));
    while (1) {
        rc = mn_itf_addr_getnext(itf, &itf_addr);
        if (rc) {
            break;
        }
        mn_inet_ntop(itf_addr.mifa_family, &itf_addr.mifa_addr,
                     addr_str, sizeof(addr_str));
        console_printf(" %s/%d\n", addr_str, itf_addr.mifa_plen);
    }
}

int
lwip_nif_up(const char *name)
{
    struct netif *nif;
    err_t err;

    nif = netif_find(name);
    if (!nif) {
        return MN_EINVAL;
    }
    if (nif->flags & NETIF_FLAG_LINK_UP) {
        netif_set_up(nif);
        netif_set_default(nif);
#if LWIP_IPV6
        nif->ip6_autoconfig_enabled = 1;
        netif_create_ip6_linklocal_address(nif, 1);
#endif
        err = dhcp_start(nif);
        return lwip_err_to_mn_err(err);
    }
    return 0;
}

static int
lwip_cli(int argc, char **argv)
{
    int rc;
    struct mn_itf itf;

    if (argc == 1 || !strcmp(argv[1], "listif")) {
        memset(&itf, 0, sizeof(itf));
        while (1) {
            rc = mn_itf_getnext(&itf);
            if (rc) {
                break;
            }
            lwip_nif_print(&itf);
        }
    } else if (mn_itf_get(argv[1], &itf) == 0) {
        if (argc == 2) {
            lwip_nif_print(&itf);
            return 0;
        }
        if (!strcmp(argv[2], "up")) {
            rc = lwip_nif_up(argv[1]);
            console_printf("lwip_nif_up() = %d\n", rc);
        } else if (!strcmp(argv[2], "down")) {
        } else {
            console_printf("unknown cmd\n");
        }
    } else {
        console_printf("unknown cmd\n");
    }
    return 0;
}

struct shell_cmd lwip_cli_cmd = {
    .sc_cmd = "ip",
    .sc_cmd_func = lwip_cli
};

void
lwip_cli_init(void)
{
    shell_cmd_register(&lwip_cli_cmd);
}
#endif
