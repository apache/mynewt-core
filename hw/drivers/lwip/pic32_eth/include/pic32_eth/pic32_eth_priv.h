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

#ifndef __PIC32_ETH_PRIV_H__
#define __PIC32_ETH_PRIV_H__

#include <stdint.h>
#include <os/mynewt.h>

#include <pic32_eth/pic32_eth.h>
#include <lwip/netif.h>

#ifdef __cplusplus
extern "C" {
#endif

struct pic32_eth_stats {
    uint32_t oframe;
    uint32_t odone;
    uint32_t oerr;
    uint32_t iframe;
    uint32_t imem;
};

struct pic32_eth_desc {
    volatile union {
        struct {
            uint32_t : 7;
            uint32_t EOWN : 1;
            uint32_t NPV : 1;
            uint32_t : 7;
            uint32_t BYTE_COUNT : 11;
            uint32_t : 3;
            uint32_t EOP : 1;
            uint32_t SOP : 1;
        };
        uint32_t w;
    } hdr;
    uint32_t data_buffer_address;
    volatile union {
        struct {
            uint16_t bytes_transmitted_on_wire;
            uint8_t control_frame : 1;
            uint8_t pause_control_frame : 1;
            uint8_t back_pressure_applied : 1;
            uint8_t vlan_tagged_frame : 1;
            uint8_t reserved : 4;
            uint8_t user;
            uint16_t transmitted_byte_count;
            uint8_t collision_count : 4;
            uint8_t crc_error : 1;
            uint8_t length_check_error : 1;
            uint8_t length_out_of_range : 1;
            uint8_t transmit_done : 1;
            uint8_t transmit_multicast : 1;
            uint8_t transmit_broadcast : 1;
            uint8_t transmit_packet_deffer : 1;
            uint8_t transmit_excessive_deffer : 1;
            uint8_t transmit_maximum_collision : 1;
            uint8_t transmit_late_collision : 1;
            uint8_t transmit_giant : 1;
            uint8_t transmit_under_run : 1;
        };
        struct {
            uint32_t tsv32_51;
            uint32_t tsv0_31;
        };
        uint64_t stat;
    };
    struct pic32_eth_desc *next_ed;
} __attribute__ ((__packed__));

struct pic32_eth_state {
    struct netif nif;
    struct tcpip_callback_msg *phy_isr_msg;
    struct os_mutex lock;
    /* aliases to descriptors _rx_descs within KSEG1 addresses */
    struct pic32_eth_desc *tx_descs;
    /* aliases to descriptors _tx_descs within KSEG1 addresses */
    struct pic32_eth_desc *rx_descs;
    /* buffers references by tx_descs */
    struct pbuf *tx_bufs[PIC32_ETH_TX_DESC_COUNT];
    /* buffers references by tx_descs */
    struct pbuf *rx_bufs[PIC32_ETH_RX_DESC_COUNT];
    uint8_t rx_head;
    uint8_t rx_tail;
    uint8_t tx_head;
    uint8_t tx_tail;
    /* Timer to use for check PHY interrupt if pin is not available */
    struct hal_timer phy_tmr;
    const struct pic32_eth_cfg *cfg;
    /* TX descriptors */
    struct pic32_eth_desc _tx_descs[PIC32_ETH_TX_DESC_COUNT];
    /* RX descriptors */
    struct pic32_eth_desc _rx_descs[PIC32_ETH_RX_DESC_COUNT];
};

extern struct pic32_eth_stats pic32_eth_stats;
extern struct pic32_eth_state pic32_eth_state;

#ifdef __cplusplus
}
#endif

#endif /* __PIC32_ETH_PRIV_H__ */
