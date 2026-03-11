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

#include <ip_priv.h>
#include <lwip/priv/tcp_priv.h>

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
    (void)err;

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
#if LWIP_DHCP
        err = dhcp_start(nif);
        return lwip_err_to_mn_err(err);
#endif
    }
    return 0;
}

static int
lwip_lsif_cmd(int argc, char **argv)
{
    struct mn_itf itf = {};
    int rc;

    while (1) {
        rc = mn_itf_getnext(&itf);
        if (rc) {
            break;
        }
        lwip_nif_print(&itf);
    }
    return 0;
}

static int
lwip_cli(int argc, char **argv)
{
    int rc;
    struct mn_itf itf;

    if (argc == 1 || !strcmp(argv[1], "listif")) {
        lwip_lsif_cmd(argc - 1, argv + 1);
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

static const char *const tcp_state_str[] = {
    "CLOSED",      "LISTEN",     "SYN_SENT",   "SYN_RCVD",
    "ESTABLISHED", "FIN_WAIT_1", "FIN_WAIT_2", "CLOSE_WAIT",
    "CLOSING",     "LAST_ACK",   "TIME_WAIT",
};

static void
lwip_netstat_cmd(int argc, char **argv)
{
    char buf1[48];
    char buf2[48];
    const struct tcp_pcb *tcp_pcb;
    const struct udp_pcb *udp_pcb;
    int len;

    for (int i = 0; i < 4; ++i) {
        for (tcp_pcb = *tcp_pcb_lists[i]; tcp_pcb != NULL; tcp_pcb = tcp_pcb->next) {
            ipaddr_ntoa_r(&tcp_pcb->local_ip, buf1, sizeof(buf1));
            len = strlen(buf1);
            sprintf(buf1 + len, ":%d", tcp_pcb->local_port);
            if (tcp_pcb->state >= ESTABLISHED) {
                ipaddr_ntoa_r(&tcp_pcb->remote_ip, buf2, sizeof(buf2));
                len = strlen(buf2);
                sprintf(buf2 + len, ":%d", tcp_pcb->remote_port);
            } else {
                strcpy(buf2, "*.*");
            }
            if (IP_IS_V4_VAL(tcp_pcb->local_ip)) {
                console_printf("TCP   %-22s  %-22s  %s\n", buf1, buf2,
                               tcp_state_str[tcp_pcb->state]);
            } else {
                console_printf("TCP   %s  %s  %s\n", buf1, buf2,
                               tcp_state_str[tcp_pcb->state]);
            }
        }
    }

    for (udp_pcb = udp_pcbs; udp_pcb != NULL; udp_pcb = udp_pcb->next) {
        ipaddr_ntoa_r(&udp_pcb->local_ip, buf1, sizeof(buf1));
        len = strlen(buf1);
        sprintf(buf1 + len, ":%d", udp_pcb->local_port);
        if (IP_IS_V4_VAL(udp_pcb->local_ip)) {
            console_printf("UDP   %-22s  *.*\n", buf1);
        } else {
            console_printf("UDP   %s\n", buf1);
        }
    }
}

struct shell_cmd lwip_cli_cmd = {
    .sc_cmd = "ip",
    .sc_cmd_func = lwip_cli
};

#if LWIP_IPV6 || LWIP_IPV4
SHELL_MODULE_CMD(lwip, netstat, lwip_netstat_cmd, NULL);
#endif
SHELL_MODULE_CMD(lwip, lsif, lwip_lsif_cmd, NULL);
SHELL_MODULE_CMD(lwip, ip, lwip_cli, NULL);

SHELL_MODULE_WITH_LINK_TABLE(lwip);

void
lwip_cli_init(void)
{
    shell_cmd_register(&lwip_cli_cmd);
}
#endif
