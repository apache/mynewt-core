/*
 * Copyright 2022 Jerzy Kasenberg
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "syscfg/syscfg.h"
#include "lwip/netif.h"
#include "lwip/dhcp.h"
#include "mn_socket/mn_socket.h"
#include "pic32_eth/pic32_eth_priv.h"
#include "parse/parse.h"

#if MYNEWT_VAL(PIC32_ETH_SHELL)

#include <shell/shell.h>
#include <console/console.h>
#include <pic32_eth/pic32_eth.h>

static const struct shell_cmd_help pic32_eth_stats_help = {
    .summary = "print eth stats counters",
    .usage = "stats",
    .params = NULL,
};

static const struct shell_cmd_help pic32_eth_phy_help = {
    .summary = "read write phy registers",
    .usage = "phy [0-31 [<reg_value]]",
    .params = NULL,
};

static const struct shell_cmd_help pic32_eth_up_help = {
    .summary = "turns eth interface on",
    .usage = "up",
    .params = NULL,
};

static const struct shell_cmd_help pic32_eth_down_help = {
    .summary = "turns eth interface down",
    .usage = "down",
    .params = NULL,
};

static const struct shell_cmd_help pic32_eth_desc_help = {
    .summary = "dump buffer descriptors",
    .usage = "desc",
    .params = NULL,
};

static const struct shell_cmd_help pic32_eth_dump_help = {
    .summary = "dumps ETH registers",
    .usage = "dump",
    .params = NULL,
};

static int
pic32_eth_down_cmd(const struct shell_cmd *cmd, int argc, char **argv, struct streamer *streamer)
{
    (void)argc;
    (void)argv;

    struct netif *nif;
    err_t err;

    nif = netif_find("et1");
    if (!nif) {
        return MN_EINVAL;
    }
    if ((nif->flags & NETIF_FLAG_LINK_UP) == 0) {
        dhcp_stop(nif);
        netif_set_down(nif);
    }
    return 0;
}

static int
pic32_eth_up_cmd(const struct shell_cmd *cmd, int argc, char **argv, struct streamer *streamer)
{
    (void)argc;
    (void)argv;

    struct netif *nif;
    err_t err;

    nif = netif_find("et1");
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
        return err ? MN_EUNKNOWN : 0;
    }
    return 0;
}

static int
pic32_eth_dump_cmd(const struct shell_cmd *cmd, int argc, char **argv, struct streamer *streamer)
{
    (void)argc;
    (void)argv;

    streamer_printf(streamer, "   ETHCON1 0x%08" PRIx32 "\n", ETHCON1);
    streamer_printf(streamer, "   ETHCON2 0x%08" PRIx32 "\n", ETHCON2);
    streamer_printf(streamer, "   ETHTXST 0x%08" PRIx32 "\n", ETHTXST);
    streamer_printf(streamer, "   ETHRXST 0x%08" PRIx32 "\n", ETHRXST);
    streamer_printf(streamer, "    ETHHT0 0x%08" PRIx32 "\n", ETHHT0);
    streamer_printf(streamer, "    ETHHT1 0x%08" PRIx32 "\n", ETHHT1);
    streamer_printf(streamer, "   ETHPMM0 0x%08" PRIx32 "\n", ETHPMM0);
    streamer_printf(streamer, "   ETHPMM1 0x%08" PRIx32 "\n", ETHPMM1);
    streamer_printf(streamer, "   ETHPMCS 0x%08" PRIx32 "\n", ETHPMCS);
    streamer_printf(streamer, "    ETHPMO 0x%08" PRIx32 "\n", ETHPMO);
    streamer_printf(streamer, "   ETHRXFC 0x%08" PRIx32 "\n", ETHRXFC);
    streamer_printf(streamer, "   ETHRXWM 0x%08" PRIx32 "\n", ETHRXWM);
    streamer_printf(streamer, "    ETHIEN 0x%08" PRIx32 "\n", ETHIEN);
    streamer_printf(streamer, "    ETHIRQ 0x%08" PRIx32 "\n", ETHIRQ);
    streamer_printf(streamer, "   ETHSTAT 0x%08" PRIx32 "\n", ETHSTAT);
    streamer_printf(streamer, "THRXOVFLOW 0x%08" PRIx32 "\n", ETHRXOVFLOW);
    streamer_printf(streamer, "ETHFRMTXOK 0x%08" PRIx32 "\n", ETHFRMTXOK);
    streamer_printf(streamer, "ETHFRMRXOK 0x%08" PRIx32 "\n", ETHFRMRXOK);
    streamer_printf(streamer, "ETHSCOLFRM 0x%08" PRIx32 "\n", ETHSCOLFRM);
    streamer_printf(streamer, "ETHMCOLFRM 0x%08" PRIx32 "\n", ETHMCOLFRM);
    streamer_printf(streamer, "ETHFRMRXOK 0x%08" PRIx32 "\n", ETHFRMRXOK);
    streamer_printf(streamer, " ETHFCSERR 0x%08" PRIx32 "\n", ETHFCSERR);
    streamer_printf(streamer, "ETHALGNERR 0x%08" PRIx32 "\n", ETHALGNERR);
    streamer_printf(streamer, " EMAC1CFG1 0x%08" PRIx32 "\n", EMAC1CFG1);
    streamer_printf(streamer, " EMAC1CFG2 0x%08" PRIx32 "\n", EMAC1CFG2);
    streamer_printf(streamer, " EMAC1IPGT 0x%08" PRIx32 "\n", EMAC1IPGT);
    streamer_printf(streamer, " EMAC1IPGR 0x%08" PRIx32 "\n", EMAC1IPGR);
    streamer_printf(streamer, " EMAC1MADR 0x%08" PRIx32 "\n", EMAC1MADR);

    return 0;
}

static int
pic32_eth_stats_cmd(const struct shell_cmd *cmd, int argc, char **argv, struct streamer *streamer)
{
    (void)argc;
    (void)argv;

    streamer_printf(streamer, "oframe %d\n", (int)pic32_eth_stats.oframe);
    streamer_printf(streamer, "odone %d\n", (int)pic32_eth_stats.odone);
    streamer_printf(streamer, "oerr %d\n", (int)pic32_eth_stats.oerr);
    streamer_printf(streamer, "iframe %d\n", (int)pic32_eth_stats.iframe);
    streamer_printf(streamer, "imem %d\n", (int)pic32_eth_stats.imem);

    return 0;
}

static void
pic32_eth_phy_dump(struct streamer *streamer)
{
    int i;
    int rc;
    int phy_addr = MYNEWT_VAL(PIC32_ETH_0_PHY_ADDR);
    uint16_t reg;

    for (i = 0; i <= 6; i++) {
        rc = pic32_eth_phy_read_register(phy_addr, i, &reg);
        streamer_printf(streamer, "%d: %x (%d)\n", i, reg, rc);
    }
    for (i = 17; i <= 18; i++) {
        rc = pic32_eth_phy_read_register(phy_addr, i, &reg);
        streamer_printf(streamer, "%d: %x (%d)\n", i, reg, rc);
    }
    for (i = 26; i <= 31; i++) {
        rc = pic32_eth_phy_read_register(phy_addr, i, &reg);
        streamer_printf(streamer, "%d: %x (%d)\n", i, reg, rc);
    }
}

static int
pic32_eth_phy_cmd(const struct shell_cmd *cmd, int argc, char **argv, struct streamer *streamer)
{
    int status;
    int reg;
    uint16_t regval;

    if (argc == 1) {
        pic32_eth_phy_dump(streamer);
        return 0;
    }
    reg = (int)parse_ull_bounds(argv[1], 0, 31, &status);
    if (status) {
        streamer_printf(streamer, "Invalid register number.\n"
                                  "Valid range 0-31\n");
        return 0;
    }
    if (argc > 2) {
        regval = (uint16_t)(uint16_t)parse_ull_bounds(argv[2], 0, 0xFFFF, &status);
        if (status) {
            streamer_printf(streamer, "Invalid register value.\n"
                                      "Valid range 0-31\n");
            return 0;
        }
        pic32_eth_phy_write_register(MYNEWT_VAL(PIC32_ETH_0_PHY_ADDR), reg, regval);
    } else {
        pic32_eth_phy_read_register(MYNEWT_VAL(PIC32_ETH_0_PHY_ADDR), reg, &regval);
        streamer_printf(streamer, "0x%04x\n", regval);
    }

    return 0;
}

static int
pic32_eth_desc_cmd(const struct shell_cmd *cmd, int argc, char **argv, struct streamer *streamer)
{
    int i;
    (void)argc;
    (void)argv;

    streamer_printf(streamer, "----------------RX_DESC-> %04X ---------\n", ETHRXST);
    streamer_printf(streamer, "N  ADDR SOP EOP BYTES NPV EOWN DATA NEXT\n");
    for (i = 0; i < PIC32_ETH_RX_DESC_COUNT; ++i) {
        streamer_printf(streamer,
                        "%02d %04x %d   %d  %6d %d   %d    %04x %04x\n", i,
                        ((uint32_t)&pic32_eth_state.rx_descs[i]) & 0xFFFF,
                        pic32_eth_state.rx_descs[i].hdr.SOP,
                        pic32_eth_state.rx_descs[i].hdr.EOP,
                        pic32_eth_state.rx_descs[i].hdr.BYTE_COUNT,
                        pic32_eth_state.rx_descs[i].hdr.NPV,
                        pic32_eth_state.rx_descs[i].hdr.EOWN,
                        ((uint32_t)pic32_eth_state.rx_descs[i].data_buffer_address) & 0xFFFF,
                        ((uint32_t)pic32_eth_state.rx_descs[i].next_ed) & 0xFFFF);
    }
    streamer_printf(streamer, "----------------RX_DESC-> %04X ---------\n", ETHTXST);
    streamer_printf(streamer, "N  ADDR SOP EOP BYTES NPV EOWN DATA NEXT\n");
    for (i = 0; i < PIC32_ETH_TX_DESC_COUNT; ++i) {
        streamer_printf(streamer, "%02d %04x %d   %d  %6d %d   %d    %04x %04x\n", i,
                        ((uint32_t)&pic32_eth_state.tx_descs[i]) & 0xFFFF,
                        pic32_eth_state.tx_descs[i].hdr.SOP,
                        pic32_eth_state.tx_descs[i].hdr.EOP,
                        pic32_eth_state.tx_descs[i].hdr.BYTE_COUNT,
                        pic32_eth_state.tx_descs[i].hdr.NPV,
                        pic32_eth_state.tx_descs[i].hdr.EOWN,
                        ((uint32_t)pic32_eth_state.tx_descs[i].data_buffer_address) & 0xFFFF,
                        ((uint32_t)pic32_eth_state.tx_descs[i].next_ed) & 0xFFFF);
    }

    return 0;
}

static const struct shell_cmd pic32_eth_commands[] = {
    SHELL_CMD_EXT("stats", pic32_eth_stats_cmd, &pic32_eth_stats_help),
    SHELL_CMD_EXT("phy", pic32_eth_phy_cmd, &pic32_eth_phy_help),
    SHELL_CMD_EXT("dump", pic32_eth_dump_cmd, &pic32_eth_dump_help),
    SHELL_CMD_EXT("desc", pic32_eth_desc_cmd, &pic32_eth_desc_help),
    SHELL_CMD_EXT("up", pic32_eth_up_cmd, &pic32_eth_up_help),
    SHELL_CMD_EXT("down", pic32_eth_down_cmd, &pic32_eth_down_help),
    { 0 },
};

static int
pic32_eth_compat_cmd(const struct shell_cmd *cmd, int argc, char **argv, struct streamer *streamer)
{
    int rc;
    int i;

    if (argc < 2) {
        rc = SYS_EINVAL;
    } else {
        for (i = 0; pic32_eth_commands[i].sc_cmd; ++i) {
            if (strcmp(pic32_eth_commands[i].sc_cmd, argv[1]) == 0) {
                rc = pic32_eth_commands[i].sc_cmd_func(argc - 1, argv + 1);
                break;
            }
        }
        /* No command found */
        if (pic32_eth_commands[i].sc_cmd == NULL) {
            streamer_printf(streamer, "Invalid command.\n");
            rc = -1;
        }
    }

    return rc;
}

static const struct shell_cmd pic32_eth_cmd = {
    .sc_cmd = "eth",
    .sc_cmd_ext_func = pic32_eth_compat_cmd,
};

void
pic32_eth_shell_register(void)
{
    int rc;

    rc = shell_register("eth", pic32_eth_commands);
    rc = shell_cmd_register(&pic32_eth_cmd);

    SYSINIT_PANIC_ASSERT_MSG(rc == 0, "Failed to register eth shell");
}

#endif
