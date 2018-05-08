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
#ifndef __LWIP_LWIPOPTS_H__
#define __LWIP_LWIPOPTS_H__

#ifdef __cplusplus
extern "C" {
#endif

#define MEM_LIBC_MALLOC			1	/* use platform malloc */
#define LWIP_NETIF_TX_SINGLE_PBUF 	1
#define LWIP_NETIF_LOOPBACK		1	/* yes loopback interface */

#define TCPIP_THREAD_PRIO		5
#define TCPIP_THREAD_STACKSIZE	((6 * 1024) / sizeof(portSTACK_TYPE))
#define TCPIP_MBOX_SIZE			10

#define MEMP_NUM_ARP_QUEUE		4
#define MEMP_NUM_RAW_PCB		1
#define LWIP_SO_RCVTIMEO                1
#define LWIP_IGMP                       1
#define SO_REUSE                        1
#define LWIP_DNS                        1
#define DNS_TABLE_SIZE                  3
#define DNS_MAX_NAME_LENGTH             64
#define LWIP_NETIF_HOSTNAME             1

#define LWIP_NETIF_STATUS_CALLBACK      1
#define LWIP_NETIF_LINK_CALLBACK        1
#define LWIP_TCP_KEEPALIVE              1

#define LWIP_AUTOIP                     0
#define LWIP_DHCP_AUTOIP_COOP           0

#define LWIP_IPV6                       1
#define LWIP_ND6                        0
#define MEMP_NUM_ND6_QUEUE              4

#ifndef LWIP_DEBUG
#define LWIP_NOASSERT                   1
#endif

#define LWIP_NETIF_API                  0

#define SYS_LIGHTWEIGHT_PROT            1

#define TCP_LISTEN_BACKLOG     	        (1)

/* ---------- Memory options ---------- */
/* MEM_ALIGNMENT: should be set to the alignment of the CPU for which
   lwIP is compiled. 4 byte alignment -> define MEM_ALIGNMENT to 4, 2
   byte alignment -> define MEM_ALIGNMENT to 2. */
#define MEM_ALIGNMENT                   4

/* MEMP_NUM_PBUF: the number of memp struct pbufs. If the application
   sends a lot of data out of ROM (or other static memory), this
   should be set high. */
#define MEMP_NUM_PBUF                   20
/* MEMP_NUM_UDP_PCB: the number of UDP protocol control blocks. One
   per active UDP "connection". */
#define MEMP_NUM_UDP_PCB                4

/* MEMP_NUM_TCP_PCB: the number of simulatenously active TCP
   connections. */
#define MEMP_NUM_TCP_PCB                3	/* XXX */
/* MEMP_NUM_TCP_PCB_LISTEN: the number of listening TCP
   connections. */
#define MEMP_NUM_TCP_PCB_LISTEN         3
/* MEMP_NUM_TCP_SEG: the number of simultaneously queued TCP
   segments. */
#define MEMP_NUM_TCP_SEG                (TCP_SND_QUEUELEN + 1)
/* MEMP_NUM_SYS_TIMEOUT: the number of simulateously active
   timeouts. */
#define MEMP_NUM_SYS_TIMEOUT            12

#define MEMP_NUM_TCPIP_MSG_API          16


/* The following four are used only with the sequential API and can be
   set to 0 if the application only will use the raw API. */
/* MEMP_NUM_NETBUF: the number of struct netbufs. */
#define MEMP_NUM_NETBUF                 0
/* MEMP_NUM_NETCONN: the number of struct netconns. */
#define MEMP_NUM_NETCONN                0

/* ---------- Pbuf options ---------- */
/* PBUF_POOL_SIZE: the number of buffers in the pbuf pool. */
#ifndef PBUF_POOL_SIZE
#define PBUF_POOL_SIZE                  6
#endif

/* PBUF_POOL_BUFSIZE: the size of each pbuf in the pbuf pool. */
#define PBUF_POOL_BUFSIZE               1580

/*
 * Disable this; causes excessive stack use in device drivers calling
 * pbuf_alloc()
 */
#define PBUF_POOL_FREE_OOSEQ            0

/* PBUF_LINK_HLEN: the number of bytes that should be allocated for a
   link level header. */
#define PBUF_LINK_HLEN                  16

/* ---------- TCP options ---------- */
#define LWIP_TCP                        1
#define TCP_TTL                         255

/* Controls if TCP should queue segments that arrive out of
   order. Define to 0 if your device is low on memory. */
#define TCP_QUEUE_OOSEQ                 1

/* TCP Maximum segment size. */
#define TCP_MSS                         1460

/* TCP sender buffer space (bytes). */
#define TCP_SND_BUF                     (3 * TCP_MSS)

/* TCP sender buffer space (pbufs). This must be at least = 2 *
   TCP_SND_BUF/TCP_MSS for things to work. */
#define TCP_SND_QUEUELEN                (6 * TCP_SND_BUF / TCP_MSS)

/* TCP receive window. */
#define TCP_WND                         (3 * TCP_MSS)

/* Maximum number of retransmissions of data segments. */
#define TCP_MAXRTX                      12

/* Maximum number of retransmissions of SYN segments. */
#define TCP_SYNMAXRTX                   4

/* ---------- ARP options ---------- */
#define ARP_TABLE_SIZE                  10
#define ARP_QUEUEING                    1

/* ---------- IP options ---------- */
/* Define IP_FORWARD to 1 if you wish to have the ability to forward
   IP packets across network interfaces. If you are going to run lwIP
   on a device with only one network interface, define this to 0. */
#define IP_FORWARD                      0

/* If defined to 1, IP options are allowed (but not parsed). If
   defined to 0, all packets with IP options are dropped. */
#define IP_OPTIONS                      1

/* ---------- ICMP options ---------- */
#define ICMP_TTL                        255


/* ---------- DHCP options ---------- */
/* Define LWIP_DHCP to 1 if you want DHCP configuration of
   interfaces. DHCP is not implemented in lwIP 0.5.1, however, so
   turning this on does currently not work. */
#define LWIP_DHCP                       1

/* 1 if you want to do an ARP check on the offered address
   (recommended). */
#define DHCP_DOES_ARP_CHECK             1

/* ---------- UDP options ---------- */
#define LWIP_UDP                        1
#define UDP_TTL                         255


#define LWIP_SOCKET                     0

/* ---------- Statistics options ---------- */
/* XXX hook into sys/stats */
#define LWIP_STATS                           0

#if LWIP_STATS
#define LINK_STATS                      1
#define IP_STATS                        1
#define ICMP_STATS                      1
#define UDP_STATS                       1
#define TCP_STATS                       1
#define MEM_STATS                       1
#define MEMP_STATS                      1
#define PBUF_STATS                      1
#define SYS_STATS                       1
#endif /* STATS */

#define LWIP_PROVIDE_ERRNO 1
#define LWIP_DONT_PROVIDE_BYTEORDER_FUNCTIONS 1
#define ERRNO                     0

#ifdef __cplusplus
}
#endif

#endif /*  __LWIP_LWIPOPTS_H__ */
