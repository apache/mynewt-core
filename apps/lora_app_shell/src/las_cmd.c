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

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "sysinit/sysinit.h"
#include "syscfg/syscfg.h"
#include "console/console.h"
#include "shell/shell.h"
#include "parse/parse.h"
#include "node/lora_priv.h"
#include "node/lora.h"

extern void
lora_app_shell_txd_func(uint8_t port, LoRaMacEventInfoStatus_t status,
                        Mcps_t pkt_type, struct os_mbuf *om);

extern void
lora_app_shell_rxd_func(uint8_t port, LoRaMacEventInfoStatus_t status,
                        Mcps_t pkt_type, struct os_mbuf *om);

extern void
lora_app_shell_join_cb(LoRaMacEventInfoStatus_t status, uint8_t attempts);

extern void
lora_app_shell_link_chk_cb(LoRaMacEventInfoStatus_t status, uint8_t num_gw,
                           uint8_t demod_margin);

#define LORA_APP_SHELL_MAX_APP_PAYLOAD  (250)
static uint8_t las_cmd_app_tx_buf[LORA_APP_SHELL_MAX_APP_PAYLOAD];

struct mib_pair {
    char *mib_name;
    Mib_t mib_param;
};

static struct mib_pair lora_mib[] = {
    {"device_class",    MIB_DEVICE_CLASS},
    {"nwk_joined",      MIB_NETWORK_JOINED},
    {"adr",             MIB_ADR},
    {"net_id",          MIB_NET_ID},
    {"dev_addr",        MIB_DEV_ADDR},
    {"nwk_skey",        MIB_NWK_SKEY},
    {"app_skey",        MIB_APP_SKEY},
    {"pub_nwk",         MIB_PUBLIC_NETWORK},
    {"repeater",        MIB_REPEATER_SUPPORT},
    {"rx2_chan",        MIB_RX2_CHANNEL},
    {"rx2_def_chan",    MIB_RX2_DEFAULT_CHANNEL},
    {"chan_mask",       MIB_CHANNELS_MASK},
    {"chan_def_mask",   MIB_CHANNELS_DEFAULT_MASK},
    {"chan_nb_rep",     MIB_CHANNELS_NB_REP},
    {"max_rx_win_dur",  MIB_MAX_RX_WINDOW_DURATION},
    {"rx_delay1",       MIB_RECEIVE_DELAY_1},
    {"rx_delay2",       MIB_RECEIVE_DELAY_2},
    {"join_acc_delay1", MIB_JOIN_ACCEPT_DELAY_1},
    {"join_acc_delay2", MIB_JOIN_ACCEPT_DELAY_2},
    {"chan_dr",         MIB_CHANNELS_DATARATE},
    {"chan_def_dr",     MIB_CHANNELS_DEFAULT_DATARATE},
    {"chan_tx_pwr",     MIB_CHANNELS_TX_POWER},
    {"chan_def_tx_pwr", MIB_CHANNELS_DEFAULT_TX_POWER},
    {"uplink_cntr",     MIB_UPLINK_COUNTER},
    {"downlink_cntr",   MIB_DOWNLINK_COUNTER},
    {"multicast_chan",  MIB_MULTICAST_CHANNEL},
    {"sys_max_rx_err",  MIB_SYSTEM_MAX_RX_ERROR},
    {"min_rx_symbols",  MIB_MIN_RX_SYMBOLS},
    {NULL, (Mib_t)0}
};

static int las_cmd_wr_mib(int argc, char **argv);
static int las_cmd_rd_mib(int argc, char **argv);
static int las_cmd_wr_dev_eui(int argc, char **argv);
static int las_cmd_rd_dev_eui(int argc, char **argv);
static int las_cmd_wr_app_eui(int argc, char **argv);
static int las_cmd_rd_app_eui(int argc, char **argv);
static int las_cmd_wr_app_key(int argc, char **argv);
static int las_cmd_rd_app_key(int argc, char **argv);
static int las_cmd_app_port(int argc, char **argv);
static int las_cmd_app_tx(int argc, char **argv);
static int las_cmd_join(int argc, char **argv);
static int las_cmd_link_chk(int argc, char **argv);

static struct shell_cmd las_cmds[] = {
    {
        .sc_cmd = "las_wr_mib",
        .sc_cmd_func = las_cmd_wr_mib,
    },
    {
        .sc_cmd = "las_rd_mib",
        .sc_cmd_func = las_cmd_rd_mib,
    },
    {
        .sc_cmd = "las_rd_dev_eui",
        .sc_cmd_func = las_cmd_rd_dev_eui,
    },
    {
        .sc_cmd = "las_wr_dev_eui",
        .sc_cmd_func = las_cmd_wr_dev_eui,
    },
    {
        .sc_cmd = "las_rd_app_eui",
        .sc_cmd_func = las_cmd_rd_app_eui,
    },
    {
        .sc_cmd = "las_wr_app_eui",
        .sc_cmd_func = las_cmd_wr_app_eui,
    },
    {
        .sc_cmd = "las_rd_app_key",
        .sc_cmd_func = las_cmd_rd_app_key,
    },
    {
        .sc_cmd = "las_wr_app_key",
        .sc_cmd_func = las_cmd_wr_app_key,
    },
    {
        .sc_cmd = "las_app_port",
        .sc_cmd_func = las_cmd_app_port,
    },
    {
        .sc_cmd = "las_app_tx",
        .sc_cmd_func = las_cmd_app_tx,
    },
    {
        .sc_cmd = "las_join",
        .sc_cmd_func = las_cmd_join,
    },
    {
        .sc_cmd = "las_link_chk",
        .sc_cmd_func = las_cmd_link_chk,
    },
    {
        NULL, NULL,
#if MYNEWT_VAL(SHELL_CMD_HELP)
        NULL
#endif
    },
};

#define LAS_NUM_CLI_CMDS  (sizeof las_cmds / sizeof las_cmds[0])

void
las_cmd_disp_byte_str(uint8_t *bytes, int len)
{
    int i;

    if (len > 0) {
        for (i = 0; i < len - 1; ++i) {
            console_printf("%02x:", bytes[i]);
        }
        console_printf("%02x\n", bytes[len - 1]);
    }
}

static void
las_cmd_disp_chan_mask(uint16_t *mask)
{
    int i;
    int len;

    if (!mask) {
        return;
    }

    len = LORA_MAX_NB_CHANNELS / 16;
    if ((len * 16) != LORA_MAX_NB_CHANNELS) {
        len += 1;
    }

    for (i = 0; i < len - 1; ++i) {
        console_printf("%04x:", mask[i]);
    }
    console_printf("%04x\n", mask[len - 1]);
}

/**
 * Display list of MAC mibs
 *
 */
static void
las_cmd_show_mibs(void)
{
    struct mib_pair *mp;

    mp = &lora_mib[0];
    while (mp->mib_name != NULL) {
        console_printf("%s\n", mp->mib_name);
        ++mp;
    }
}

static struct mib_pair *
las_find_mib_by_name(char *mibname)
{
    struct mib_pair *mp;

    mp = &lora_mib[0];
    while (mp->mib_name != NULL) {
        if (!strcmp(mibname, mp->mib_name)) {
            return mp;
        }
        ++mp;
    }

    return NULL;
}

static void
las_cmd_wr_mib_help(void)
{
    console_printf("las_wr_mib <mib_name> <val> where mib_name is one of:\n");
    las_cmd_show_mibs();
}

static int
las_parse_bool(char *str)
{
    int rc;

    if (!strcmp(str, "0")) {
        rc = 0;
    } else if (!strcmp(str, "1")) {
        rc = 1;
    } else {
        console_printf("Invalid value. Valid values are 0 or 1\n");
        rc = -1;
    }

    return rc;
}

static int
las_cmd_wr_mib(int argc, char **argv)
{
    int rc;
    int plen;
    uint8_t key[LORA_KEY_LEN];
    uint16_t mask[6];
    int mask_len;
    struct mib_pair *mp;
    MibRequestConfirm_t mib;

    if (argc < 3) {
        console_printf("Invalid # of arguments\n");
        goto wr_mib_err;
    }

    if (strcmp(argv[1], "help") == 0) {
        las_cmd_wr_mib_help();
        return 0;
    }

    mp = las_find_mib_by_name(argv[1]);
    if (mp == NULL) {
        console_printf("No mib named %s\n",argv[1]);
        goto wr_mib_err;
    }

    /* parse value */
    mib.Type = mp->mib_param;
    switch (mib.Type) {
        case MIB_DEVICE_CLASS:
            if (!strcmp(argv[2], "A")) {
                mib.Param.Class = CLASS_A;
            } else if (!strcmp(argv[2], "B")) {
                console_printf("Class B devices currently not supported\n");
                return 0;
            } else if (!strcmp(argv[2], "C")) {
                mib.Param.Class = CLASS_C;
            } else {
                console_printf("Invalid value. Valid values are A, B or C\n");
                return 0;
            }
            break;
        case MIB_NETWORK_JOINED:
            rc = las_parse_bool(argv[2]);
            if (rc == 0) {
                mib.Param.IsNetworkJoined = false;
            } else if (rc == 1) {
                mib.Param.IsNetworkJoined = true;
            } else {
                return 0;
            }
            break;
        case MIB_ADR:
            rc = las_parse_bool(argv[2]);
            if (rc == 0) {
                mib.Param.AdrEnable = false;
            } else if (rc == 1) {
                mib.Param.AdrEnable = true;
            } else {
                return 0;
            }
            break;
        case MIB_NET_ID:
            mib.Param.NetID = (uint32_t)parse_ull(argv[2], &rc);
            if (rc) {
                console_printf("Unable to parse value\n");
                return 0;
            }
            break;
        case MIB_DEV_ADDR:
            mib.Param.DevAddr = (uint32_t)parse_ull(argv[2], &rc);
            if (rc) {
                console_printf("Unable to parse value\n");
                return 0;
            }
            break;
        case MIB_NWK_SKEY:
            rc = parse_byte_stream(argv[2], LORA_KEY_LEN, key, &plen);
            if (rc || (plen != LORA_KEY_LEN)) {
                console_printf("Key does not parse. Must be 16 bytes"
                               " and separated by : or -\n");
                return 0;
            }
            mib.Param.NwkSKey = key;
            break;
        case MIB_APP_SKEY:
            rc = parse_byte_stream(argv[2], LORA_KEY_LEN, key, &plen);
            if (rc || (plen != LORA_KEY_LEN)) {
                console_printf("Key does not parse. Must be 16 bytes"
                               " and separated by : or -\n");
                return 0;
            }
            mib.Param.AppSKey = key;
            break;
        case MIB_PUBLIC_NETWORK:
            rc = las_parse_bool(argv[2]);
            if (rc == 0) {
                mib.Param.EnablePublicNetwork = false;
            } else if (rc == 1) {
                mib.Param.EnablePublicNetwork = true;
            } else {
                return 0;
            }
            break;
        case MIB_REPEATER_SUPPORT:
            rc = las_parse_bool(argv[2]);
            if (rc == 0) {
                mib.Param.EnableRepeaterSupport = false;
            } else if (rc == 1) {
                mib.Param.EnableRepeaterSupport = true;
            } else {
                return 0;
            }
            break;
        case MIB_CHANNELS:
            //mib.Param.ChannelList;
            break;
        case MIB_RX2_CHANNEL:
            //mib.Param.Rx2Channel;
            break;
        case MIB_RX2_DEFAULT_CHANNEL:
            //mib.Param.Rx2Channel;
            break;
        case MIB_CHANNELS_DEFAULT_MASK:
            /* NOTE: fall-through intentional */
        case MIB_CHANNELS_MASK:
            memset(mask, 0, sizeof(mask));
            mask_len = LORA_MAX_NB_CHANNELS / 8;
            if ((mask_len * 8) != LORA_MAX_NB_CHANNELS) {
                mask_len += 1;
            }

            /* NOTE: re-use of key here for temp buffer storage */
            rc = parse_byte_stream(argv[2], mask_len, key, &plen);
            if (rc || (plen != mask_len)) {
                console_printf("Mask does not parse. Must be %d bytes"
                               " and separated by : or -\n", mask_len);
                return 0;
            }

            /* construct mask from byte stream */
            rc = 0;
            for (plen = 0; plen < mask_len; plen += 2) {
                mask[rc] = key[plen];
                if ((mask_len & 1) == 0) {
                    mask[rc] += ((uint16_t)key[plen + 1]) << 8;
                }
                ++rc;
            }

            if (mib.Type == MIB_CHANNELS_DEFAULT_MASK) {
                mib.Param.ChannelsDefaultMask = mask;
            } else {
                mib.Param.ChannelsMask = mask;
            }
            break;
        case MIB_CHANNELS_NB_REP:
            //mib.Param.ChannelNbRep;
            break;
        case MIB_MAX_RX_WINDOW_DURATION:
            //mib.Param.MaxRxWindow;
            break;
        case MIB_RECEIVE_DELAY_1:
            //mib.Param.ReceiveDelay1;
            break;
        case MIB_RECEIVE_DELAY_2:
            //mib.Param.ReceiveDelay2;
            break;
        case MIB_JOIN_ACCEPT_DELAY_1:
            //mib.Param.JoinAcceptDelay1;
            break;
        case MIB_JOIN_ACCEPT_DELAY_2:
            //mib.Param.JoinAcceptDelay2;
            break;
        case MIB_CHANNELS_DEFAULT_DATARATE:
            mib.Param.ChannelsDefaultDatarate = parse_ll(argv[2], &rc);
            if (rc) {
                console_printf("Unable to parse value\n");
                return 0;
            }
            break;
        case MIB_CHANNELS_DATARATE:
            mib.Param.ChannelsDatarate = parse_ll(argv[2], &rc);
            if (rc) {
                console_printf("Unable to parse value\n");
                return 0;
            }
            break;
        case MIB_CHANNELS_DEFAULT_TX_POWER:
            //mibGet.Param.ChannelsDefaultTxPower;
            break;
        case MIB_CHANNELS_TX_POWER:
            //mib.Param.ChannelsTxPower;
            break;
        case MIB_UPLINK_COUNTER:
            //mib.Param.UpLinkCounter;
            break;
        case MIB_DOWNLINK_COUNTER:
            //mib.Param.DownLinkCounter;
            break;
        case MIB_MULTICAST_CHANNEL:
            //mib.Param.MulticastList = MulticastChannels;
            break;
        default:
            assert(0);
            break;
    }

    if (LoRaMacMibSetRequestConfirm(&mib) != LORAMAC_STATUS_OK) {
        console_printf("Mib not able to be set\n");
        return 0;
    }

    console_printf("mib %s set\n", mp->mib_name);

    return 0;

wr_mib_err:
    las_cmd_wr_mib_help();
    return 0;
}

static void
las_cmd_rd_mib_help(void)
{
    console_printf("las_rd_mib <mib_name> where mib_name is one of:\n");
    las_cmd_show_mibs();
}

static int
las_cmd_rd_mib(int argc, char **argv)
{
    struct mib_pair *mp;
    LoRaMacStatus_t stat;
    MibRequestConfirm_t mibGet;

    if (argc != 2) {
        console_printf("Invalid # of arguments\n");
        goto rd_mib_err;
    }

    if (strcmp(argv[1], "help") == 0) {
        las_cmd_rd_mib_help();
        return 0;
    }

    mp = las_find_mib_by_name(argv[1]);
    if (mp == NULL) {
        console_printf("No mib named %s\n",argv[1]);
        goto rd_mib_err;
    }

    /* Read the mib value */
    mibGet.Type = mp->mib_param;
    stat = LoRaMacMibGetRequestConfirm(&mibGet);
    if (stat != LORAMAC_STATUS_OK) {
        console_printf("Mib lookup failure\n");
        goto rd_mib_err;
    }

    console_printf("%s=", mp->mib_name);
    /* Display the value */
    switch (mibGet.Type) {
        case MIB_DEVICE_CLASS:
            console_printf("%c\n", 'A' + mibGet.Param.Class);
            break;
        case MIB_NETWORK_JOINED:
            console_printf("%d\n", mibGet.Param.IsNetworkJoined);
            break;
        case MIB_ADR:
            console_printf("%d\n", mibGet.Param.AdrEnable);
            break;
        case MIB_NET_ID:
            console_printf("%08lx\n", mibGet.Param.NetID);
            break;
        case MIB_DEV_ADDR:
            console_printf("%08lx\n", mibGet.Param.DevAddr);
            break;
        case MIB_NWK_SKEY:
            las_cmd_disp_byte_str(mibGet.Param.NwkSKey, LORA_KEY_LEN);
            break;
        case MIB_APP_SKEY:
            las_cmd_disp_byte_str(mibGet.Param.AppSKey, LORA_KEY_LEN);
            break;
        case MIB_PUBLIC_NETWORK:
            console_printf("%d\n", mibGet.Param.EnablePublicNetwork);
            break;
        case MIB_REPEATER_SUPPORT:
            console_printf("%d\n", mibGet.Param.EnableRepeaterSupport);
            break;
        case MIB_CHANNELS:
            //mibGet.Param.ChannelList;
            break;
        case MIB_RX2_CHANNEL:
            //mibGet.Param.Rx2Channel;
            break;
        case MIB_RX2_DEFAULT_CHANNEL:
            //mibGet.Param.Rx2Channel;
            break;
        case MIB_CHANNELS_DEFAULT_MASK:
            las_cmd_disp_chan_mask(mibGet.Param.ChannelsDefaultMask);
            break;
        case MIB_CHANNELS_MASK:
            las_cmd_disp_chan_mask(mibGet.Param.ChannelsMask);
            break;
        case MIB_CHANNELS_NB_REP:
            console_printf("%u\n", mibGet.Param.ChannelNbRep);
            break;
        case MIB_MAX_RX_WINDOW_DURATION:
            console_printf("%lu\n", mibGet.Param.MaxRxWindow);
            break;
        case MIB_RECEIVE_DELAY_1:
            console_printf("%lu\n", mibGet.Param.ReceiveDelay1);
            break;
        case MIB_RECEIVE_DELAY_2:
            console_printf("%lu\n", mibGet.Param.ReceiveDelay2);
            break;
        case MIB_JOIN_ACCEPT_DELAY_1:
            console_printf("%lu\n", mibGet.Param.JoinAcceptDelay1);
            break;
        case MIB_JOIN_ACCEPT_DELAY_2:
            console_printf("%lu\n", mibGet.Param.JoinAcceptDelay2);
            break;
        case MIB_CHANNELS_DEFAULT_DATARATE:
            console_printf("%d\n", mibGet.Param.ChannelsDefaultDatarate);
            break;
        case MIB_CHANNELS_DATARATE:
            console_printf("%d\n", mibGet.Param.ChannelsDatarate);
            break;
        case MIB_CHANNELS_DEFAULT_TX_POWER:
            console_printf("%d\n", mibGet.Param.ChannelsDefaultTxPower);
            break;
        case MIB_CHANNELS_TX_POWER:
            console_printf("%d\n", mibGet.Param.ChannelsTxPower);
            break;
        case MIB_UPLINK_COUNTER:
            console_printf("%lu\n", mibGet.Param.UpLinkCounter);
            break;
        case MIB_DOWNLINK_COUNTER:
            console_printf("%lu\n", mibGet.Param.DownLinkCounter);
            break;
        case MIB_MULTICAST_CHANNEL:
            //mibGet.Param.MulticastList = MulticastChannels;
            break;
        default:
            assert(0);
            break;
    }
    return 0;

rd_mib_err:
    las_cmd_rd_mib_help();
    return 0;
}

static int
las_cmd_rd_dev_eui(int argc, char **argv)
{
    if (argc != 1) {
        console_printf("Invalid # of arguments. Usage: las_rd_dev_eui\n");
        return 0;
    }

    las_cmd_disp_byte_str(g_lora_dev_eui, LORA_EUI_LEN);
    return 0;
}

static int
las_cmd_wr_dev_eui(int argc, char **argv)
{
    int rc;
    int plen;
    uint8_t eui[LORA_EUI_LEN];

    if (argc < 2) {
        console_printf("Invalid # of arguments."
                       " Usage: las_wr_dev_eui <xx:xx:xx:xx:xx:xx:xx:xx>\n");
    }

    rc = parse_byte_stream(argv[1], LORA_EUI_LEN, eui, &plen);
    if (rc || (plen != LORA_EUI_LEN)) {
        console_printf("EUI does not parse. Must be 8 bytes"
                       " and separated by : or -\n");
    } else {
        memcpy(g_lora_dev_eui, eui, LORA_EUI_LEN);
    }

    return 0;
}

static int
las_cmd_rd_app_eui(int argc, char **argv)
{
    if (argc != 1) {
        console_printf("Invalid # of arguments. Usage: las_rd_app_eui\n");
        return 0;
    }

    las_cmd_disp_byte_str(g_lora_app_eui, LORA_EUI_LEN);
    return 0;
}

static int
las_cmd_wr_app_eui(int argc, char **argv)
{
    int rc;
    int plen;
    uint8_t eui[LORA_EUI_LEN];

    if (argc < 2) {
        console_printf("Invalid # of arguments."
                       " Usage: las_wr_app_eui <xx:xx:xx:xx:xx:xx:xx:xx>\n");
    }

    rc = parse_byte_stream(argv[1], LORA_EUI_LEN, eui, &plen);
    if (rc || (plen != LORA_EUI_LEN)) {
        console_printf("EUI does not parse. Must be 8 bytes"
                       " and separated by : or -\n");
    } else {
        memcpy(g_lora_app_eui, eui, LORA_EUI_LEN);
    }

    return 0;
}

static int
las_cmd_rd_app_key(int argc, char **argv)
{
    if (argc != 1) {
        console_printf("Invalid # of arguments. Usage: las_rd_app_key\n");
        return 0;
    }

    las_cmd_disp_byte_str(g_lora_app_key, LORA_KEY_LEN);
    return 0;
}

static int
las_cmd_wr_app_key(int argc, char **argv)
{
    int rc;
    int plen;
    uint8_t key[LORA_KEY_LEN];

    if (argc < 2) {
        console_printf("Invalid # of arguments."
                       " Usage: las_wr_app_key <xx:xx:xx:xx:xx:xx:xx:xx:xx:xx"
                       ":xx:xx:xx:xx:xx:xx\n");
    }

    rc = parse_byte_stream(argv[1], LORA_KEY_LEN, key, &plen);
    if (rc || (plen != LORA_KEY_LEN)) {
        console_printf("Key does not parse. Must be 16 bytes and separated by"
                       " : or -\n");
        return 0;
    } else {
        memcpy(g_lora_app_key, key, LORA_KEY_LEN);
    }

    return 0;
}

static int
las_cmd_app_port(int argc, char **argv)
{
    int rc;
    uint8_t port;
    uint8_t retries;

    if (argc < 3) {
        console_printf("Invalid # of arguments.\n");
        goto cmd_app_port_err;
    }

    port = parse_ull_bounds(argv[2], 1, 255, &rc);
    if (rc != 0) {
        console_printf("Invalid port %s. Must be 1 - 255\n", argv[2]);
        return 0;
    }

    if (!strcmp(argv[1], "open")) {
        rc = lora_app_port_open(port, lora_app_shell_txd_func,
                                lora_app_shell_rxd_func);
        if (rc == LORA_APP_STATUS_OK) {
            console_printf("Opened app port %u\n", port);
        } else {
            console_printf("Failed to open app port %u err=%d\n", port, rc);
        }
    } else if (!strcmp(argv[1], "close")) {
        rc = lora_app_port_close(port);
        if (rc == LORA_APP_STATUS_OK) {
            console_printf("Closed app port %u\n", port);
        } else {
            console_printf("Failed to close app port %u err=%d\n", port, rc);
        }
    } else if (!strcmp(argv[1], "cfg")) {
        if (argc != 4) {
            console_printf("Invalid # of arguments.\n");
            goto cmd_app_port_err;
        }
        retries = parse_ull_bounds(argv[3], 1, MAX_ACK_RETRIES, &rc);
        if (rc) {
            console_printf("Invalid # of retries. Must be between 1 and "
                           "%d (inclusve)\n", MAX_ACK_RETRIES);
            return 0;
        }

        rc = lora_app_port_cfg(port, retries);
        if (rc == LORA_APP_STATUS_OK) {
            console_printf("App port %u configured w/retries=%u\n",
                           port, retries);
        } else {
            console_printf("Cannot configure port %u err=%d\n", port, rc);
        }
    } else if (!strcmp(argv[1], "show")) {
        if (rc == LORA_APP_STATUS_OK) {
            console_printf("app port %u\n", port);
            /* XXX: implement */
        } else {
            console_printf("Cannot show app port %u err=%d\n", port, rc);
        }
    } else {
        console_printf("Invalid port command.\n");
        goto cmd_app_port_err;
    }

    return 0;

cmd_app_port_err:
    console_printf("Usage:\n");
    console_printf("\tlas_app_port open <port num>\n");
    console_printf("\tlas_app_port close <port num>\n");
    console_printf("\tlas_app_port cfg <port num> <retries>\n");
    console_printf("\not implemented! las_app_port show <port num | all>\n");
    return 0;
}

static int
las_cmd_app_tx(int argc, char **argv)
{
    int rc;
    uint8_t port;
    uint8_t len;
    uint8_t pkt_type;
    struct os_mbuf *om;
    Mcps_t mcps_type;

    if (argc < 4) {
        console_printf("Invalid # of arguments\n");
        goto cmd_app_tx_err;
    }

    port = parse_ull_bounds(argv[1], 1, 255, &rc);
    if (rc != 0) {
        console_printf("Invalid port %s. Must be 1 - 255\n", argv[2]);
        return 0;
    }
    len = parse_ull_bounds(argv[2], 1, LORA_APP_SHELL_MAX_APP_PAYLOAD, &rc);
    if (rc != 0) {
        console_printf("Invalid length. Must be 1 - %u\n",
                       LORA_APP_SHELL_MAX_APP_PAYLOAD);
        return 0;
    }
    pkt_type = parse_ull_bounds(argv[3], 0, 1, &rc);
    if (rc != 0) {
        console_printf("Invalid type. Must be 0 (unconfirmed) or 1 (confirmed)"
                       "\n");
        return 0;
    }

    if (lora_app_mtu() < len) {
        console_printf("Can send at max %d bytes\n", lora_app_mtu());
        return 0;
    }

    /* Attempt to allocate a mbuf */
    om = lora_pkt_alloc();
    if (!om) {
        console_printf("Unable to allocate mbuf\n");
        return 0;
    }

    /* Get correct packet type. */
    if (pkt_type == 0) {
        mcps_type = MCPS_UNCONFIRMED;
    } else {
        mcps_type = MCPS_CONFIRMED;
    }

    rc = os_mbuf_copyinto(om, 0, las_cmd_app_tx_buf, len);
    assert(rc == 0);

    rc = lora_app_port_send(port, mcps_type, om);
    if (rc) {
        console_printf("Failed to send to port %u err=%d\n", port, rc);
        os_mbuf_free_chain(om);
    } else {
        console_printf("Packet sent on port %u\n", port);
    }

    return 0;

cmd_app_tx_err:
    console_printf("Usage:\n");
    console_printf("\tlas_app_tx <port> <len> <type>\n");
    console_printf("Where:\n");
    console_printf("\tport = port number on which to send\n");
    console_printf("\tlen = size n bytes of app data\n");
    console_printf("\ttype = 0 for unconfirmed, 1 for confirmed\n");
    console_printf("\tex: las_app_tx 10 20 1\n");

    return 0;
}

static int
las_cmd_link_chk(int argc, char **argv)
{
    int rc;

    rc = lora_app_link_check();
    if (rc) {
        console_printf("Link check start failure err=%d\n", rc);
    } else {
        console_printf("Sending link check\n");
    }
    return 0;
}

static int
las_cmd_join(int argc, char **argv)
{
    int rc;
    uint8_t attempts;

    /* Get the number of attempts */
    if (argc != 2) {
        console_printf("Invalid # of arguments\n");
        goto cmd_join_err;
    }

    attempts = parse_ull_bounds(argv[1], 0, 255, &rc);
    if (rc) {
        console_printf("Error: could not parse attempts. Must be 0 - 255\n");

    }

    rc = lora_app_join(g_lora_dev_eui, g_lora_app_eui, g_lora_app_key,attempts);
    if (rc) {
        console_printf("Join attempt start failure err=%d\n", rc);
    } else {
        console_printf("Attempting to join...\n");
    }
    return 0;

cmd_join_err:
    console_printf("Usage:\n");
    console_printf("\tlas_join <attempts>\n");
    console_printf("Where:\n");
    console_printf("\tattempts = # of join requests to send before failure"
                   " (0 -255)ß\n");
    console_printf("\tex: las_join 10\n");
    return 0;
}

void
las_cmd_init(void)
{
    int i;
    int rc;

    /* Set the join callback */
    lora_app_set_join_cb(lora_app_shell_join_cb);

    /* Set link check callback */
    lora_app_set_link_check_cb(lora_app_shell_link_chk_cb);

    for (i = 0; i < LAS_NUM_CLI_CMDS; i++) {
        rc = shell_cmd_register(las_cmds + i);
        SYSINIT_PANIC_ASSERT_MSG(
            rc == 0, "Failed to register lora app shell CLI commands");
    }

    /* Init app tx payload to incrementing pattern */
    for (i = 0; i < LORA_APP_SHELL_MAX_APP_PAYLOAD; ++i) {
        las_cmd_app_tx_buf[i] = i;
    }
}
