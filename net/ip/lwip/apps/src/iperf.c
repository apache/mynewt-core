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

#include <os/mynewt.h>
#include <stdio.h>
#include <lwip/apps/lwiperf.h>

void
iperf_cb(void *arg, enum lwiperf_report_type report_type, const ip_addr_t *local_addr,
         u16_t local_port, const ip_addr_t *remote_addr, u16_t remote_port,
         u32_t bytes_transferred, u32_t ms_duration, u32_t bandwidth_kbitpsec)
{
    int MB10 = 10 * bytes_transferred / (1024 * 1024);
    int Mb10 = 10 * bandwidth_kbitpsec / 1000;

    printf("[ ID] Interval       Transfer    Bandwidth\n");
    printf("[  1] 0.00-%d.%02d sec %d.%d MBytes %d.%d Mbits/sec\n",
           (int)ms_duration / 1000, (int)ms_duration / 10 % 100, MB10 / 10,
           MB10 % 10, Mb10 / 10, Mb10 % 10);
}

void
iperf_start(void)
{
    printf("\n------------------------------------------------------------\n"
           "Server listening on TCP port %d\n"
           "TCP window size: 64.0 KByte (default)\n"
           "------------------------------------------------------------\n",
           LWIPERF_TCP_PORT_DEFAULT);
    lwiperf_start_tcp_server_default(iperf_cb, NULL);
}

void
iperf_init(void)
{
    iperf_start();
}
