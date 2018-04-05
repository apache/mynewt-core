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

#if MYNEWT_VAL(WIFI_MGMT_CLI)

#include <stddef.h>
#include <string.h>
#include <assert.h>

#include <shell/shell.h>
#include <console/console.h>

#include "wifi_mgmt/wifi_mgmt.h"

#include "wifi_priv.h"

static int
wifi_cli(int argc, char **argv)
{
    struct wifi_if *wi;
    int i;

    if (argc < 2) {
        goto usage;
    }
    wi = wifi_if_lookup(0);

    if (!strcmp(argv[1], "start")) {
        wifi_start(wi);
    } else if (!strcmp(argv[1], "stop")) {
        wifi_stop(wi);
    } else if (!strcmp(argv[1], "scan")) {
        wifi_scan_start(wi);
    } else if (!strcmp(argv[1], "aps")) {
        int i;
        struct wifi_ap *ap;

        console_printf("   %32s %4s %4s %s\n", "SSID", "RSSI", "chan", "sec");
        for (i = 0; i < wi->wi_scan_cnt; i++) {
            ap = (struct wifi_ap *)&wi->wi_scan[i];
            console_printf("%2d:%32s %4d %4d %s\n",
              i, ap->wa_ssid, ap->wa_rssi, ap->wa_channel,
              ap->wa_key_type ? "X" : "");
        }
    } else if (!strcmp(argv[1], "connect")) {
        if (argc < 2) {
            goto conn_usage;
        }
        i = strlen(argv[2]);
        if (i >= sizeof(wi->wi_ssid)) {
            goto conn_usage;
        }
        if (argc > 2) {
            i = strlen(argv[2]);
            if (i >= sizeof(wi->wi_key)) {
                goto conn_usage;
            }
            strcpy(wi->wi_key, argv[3]);
        }
        strcpy(wi->wi_ssid, argv[2]);
        if (wifi_connect(wi)) {
conn_usage:
            console_printf("%s %s <ssid> [<key>]\n",
              argv[0], argv[1]);
        }
    } else {
usage:
        console_printf("start|stop|scan|aps|connect <ssid> [<key>]\n");
    }
    return 0;
}

struct shell_cmd wifi_cli_cmd = {
    .sc_cmd = "wifi",
    .sc_cmd_func = wifi_cli
};

#endif
