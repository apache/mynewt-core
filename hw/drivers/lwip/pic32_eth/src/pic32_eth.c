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

#include <string.h>
#include <os/mynewt.h>

#include <hal/hal_gpio.h>
#include <hal/hal_timer.h>

#include <netif/etharp.h>
#include <netif/ethernet.h>
#include <lwip/tcpip.h>
#include <lwip/ethip6.h>
#include <lwip/dhcp.h>
#include <lwip/snmp.h>

/* Include for KVA_TO_PA and PA_TO_KVA0 */
#include <sys/kmem.h>

#include <pic32_eth/pic32_eth.h>
#include <pic32_eth/pic32_eth_priv.h>

/*
 * PHY polling frequency, when HW has no interrupt line to report link status
 * changes.
 */
#define PIC32_PHY_POLL_FREQ             os_cputime_usecs_to_ticks(1500000) /* 1.5 secs */

#define ETH_MAX_PACKET_SIZE             1518

/*
 * Phy specific registers
 */
#define PHY_BCR                         0

#define PHY_RESET                       ((uint16_t)0x8000)  /*!< PHY Reset */
#define PHY_LOOPBACK                    ((uint16_t)0x4000)  /*!< Select loop-back mode */
#define PHY_FULLDUPLEX_100M             ((uint16_t)0x2100)  /*!< Set the full-duplex mode at 100 Mb/s */
#define PHY_HALFDUPLEX_100M             ((uint16_t)0x2000)  /*!< Set the half-duplex mode at 100 Mb/s */
#define PHY_FULLDUPLEX_10M              ((uint16_t)0x0100)  /*!< Set the full-duplex mode at 10 Mb/s  */
#define PHY_HALFDUPLEX_10M              ((uint16_t)0x0000)  /*!< Set the half-duplex mode at 10 Mb/s  */
#define PHY_AUTONEGOTIATION             ((uint16_t)0x1000)  /*!< Enable auto-negotiation function     */
#define PHY_RESTART_AUTONEGOTIATION     ((uint16_t)0x0200)  /*!< Restart auto-negotiation function    */
#define PHY_POWERDOWN                   ((uint16_t)0x0800)  /*!< Select the power down mode           */
#define PHY_ISOLATE                     ((uint16_t)0x0400)  /*!< Isolate PHY from MII                 */

#define PHY_BSR                         1

#define PHY_AUTONEGOTIATION_ABILITY     ((uint16_t)0x0008)  /*!< Able to perform auto-negotiation function */
#define PHY_LINKED_STATUS               ((uint16_t)0x0004)  /*!< Valid link established               */

#define LAN_87xx_MODE_CONTROL           17
#define PHY_ALTINT                      ((uint16_t)0x0040)  /*!< Alternate interrupt mode             */
#define LAN_87xx_SPECIAL_MODES          18

#define PHY_RMII                        0x4000
#define PHY_MODE_MASK                   0x000E
#define PHY_PHY_ADD_MASK                0x001F

#define LAN_87xx_ISR                    29
#define LAN_87xx_IMR                    30

#define LAN_87xx_ISR_AUTO_DONE          0x40
#define LAN_87xx_ISR_LINK_DOWN          0x10

struct pic32_eth_stats pic32_eth_stats;

struct pic32_eth_state pic32_eth_state;

/*
 * Hardware configuration. Should be called from BSP init.
 */
int
pic32_eth_init(const struct pic32_eth_cfg *cfg)
{
    pic32_eth_state.cfg = cfg;

    return 0;
}

int
pic32_eth_phy_read_register(uint8_t phy_addr, uint8_t reg_addr, uint16_t *reg_value)
{
    int sr;
    int on = ETHCON1bits.ON;
    ETHCON1bits.ON = 1;

    EMAC1MADRbits.PHYADDR = phy_addr;
    EMAC1MADRbits.REGADDR = reg_addr;
    OS_ENTER_CRITICAL(sr);
    EMAC1MCMDbits.READ = 1;
    while (!EMAC1MINDbits.MIIMBUSY);
    OS_EXIT_CRITICAL(sr);
    while (EMAC1MINDbits.MIIMBUSY);
    EMAC1MCMDbits.READ = 0;
    *reg_value = (uint16_t)EMAC1MRDD;

    ETHCON1bits.ON = on;

    return 0;
}

int
pic32_eth_phy_write_register(uint8_t phy_addr, uint8_t reg_addr, uint16_t reg_value)
{
    int on = ETHCON1bits.ON;
    ETHCON1bits.ON = 1;

    EMAC1MADRbits.PHYADDR = phy_addr;
    EMAC1MADRbits.REGADDR = reg_addr;
    EMAC1MWTDbits.MWTD = reg_value;
    while (EMAC1MINDbits.MIIMBUSY);

    ETHCON1bits.ON = on;

    return 0;
}

static void
pic32_eth_setup_descs(struct pic32_eth_desc *descs, int cnt)
{
    int i;

    for (i = 0; i < cnt - 1; i++) {
        descs[i].hdr.w = 0;
        descs[i].hdr.NPV = 1;
        descs[i].next_ed = (void *)KVA_TO_PA(&descs[i + 1]);
    }
    descs[i].hdr.w = 0;
    descs[i].hdr.NPV = 1;
    descs[i].next_ed = (void *)KVA_TO_PA(&descs[0]);
}

static void
pic32_eth_fill_rx(struct pic32_eth_state *pes)
{
    struct pic32_eth_desc *ped;
    struct pbuf *p;

    while (1) {
        if (pes->rx_bufs[pes->rx_tail]) {
            break;
        }
        p = pbuf_alloc(PBUF_RAW, ETH_MAX_PACKET_SIZE, PBUF_POOL);
        if (p == NULL) {
            ++pic32_eth_stats.imem;
            break;
        }
        dcache_flush_area(p->payload, ETH_MAX_PACKET_SIZE);
        pes->rx_bufs[pes->rx_tail] = p;
        ped = &pes->rx_descs[pes->rx_tail];
        ped->stat = 0;
        ped->data_buffer_address = KVA_TO_PA(p->payload);
        ped->hdr.EOWN = 1;

        pes->rx_tail++;
        if (pes->rx_tail >= PIC32_ETH_RX_DESC_COUNT) {
            pes->rx_tail = 0;
        }
    }
}

static void
pic32_eth_input(struct pic32_eth_state *pes)
{
    struct pic32_eth_desc *sed;
    struct pbuf *p;
    struct netif *nif;
    int rx_head = pes->rx_head;

    nif = &pes->nif;

    while (1) {
        if (pes->rx_bufs[rx_head] == NULL) {
            break;
        }
        sed = &pes->rx_descs[rx_head];
        if (sed->hdr.EOWN) {
            break;
        }
        p = pes->rx_bufs[rx_head];
        pes->rx_bufs[rx_head] = NULL;
        if (sed->hdr.EOP == 0 || sed->hdr.SOP == 0) {
            /*
             * Incoming data spans multiple pbufs. XXX support later
             */
            pbuf_free(p);
            continue;
        } else {
            p->len = p->tot_len = sed->hdr.BYTE_COUNT;
            ++pic32_eth_stats.iframe;
            LINK_STATS_INC(link.recv);
            nif->input(p, nif);
        }
        rx_head++;
        if (rx_head >= PIC32_ETH_RX_DESC_COUNT) {
            rx_head = 0;
        }
        ETHCON1SET = _ETHCON1_BUFCDEC_MASK;
    }

    pes->rx_head = rx_head;
    pic32_eth_fill_rx(pes);

    sed = &pes->rx_descs[rx_head];

    if (pes->rx_bufs[rx_head] && sed->hdr.EOWN) {
        if (ETHSTATbits.RXBUSY == 0 && sed->hdr.EOWN) {
            ETHRXST = KVA_TO_PA(sed);
            ETHCON1SET = _ETHCON1_RXEN_MASK;
        }
    }
}

/*
 * TODO: do proper multicast filtering at some stage
 */
#if LWIP_IGMP
static err_t
pic32_igmp_mac_filter(struct netif *nif, const ip4_addr_t *group,
                      enum netif_mac_filter_action action)
{
    return -1;
}
#endif

#if LWIP_IPV6 && LWIP_IPV6_MLD
static err_t
pic32_mld_mac_filter(struct netif *nif, const ip6_addr_t *group,
                     enum netif_mac_filter_action action)
{
    return -1;
}
#endif

static void
pic32_eth_lock(struct pic32_eth_state *pes)
{
    os_mutex_pend(&pes->lock, OS_TIMEOUT_NEVER);
}

static void
pic32_eth_unlock(struct pic32_eth_state *pes)
{
    os_mutex_release(&pes->lock);
}

static void
pic32_eth_output_done(struct pic32_eth_state *pes)
{
    struct pic32_eth_desc *sed;

    pic32_eth_lock(pes);

    while (1) {
        if (pes->tx_bufs[pes->tx_tail] == NULL) {
            break;
        }
        sed = &pes->tx_descs[pes->tx_tail];
        if (sed->hdr.EOWN) {
            break;
        }
        if (sed->transmit_done) {
            ++pic32_eth_stats.odone;
            LINK_STATS_INC(link.xmit);
        } else {
            ++pic32_eth_stats.oerr;
            LINK_STATS_INC(link.err);
        }
        pbuf_free(pes->tx_bufs[pes->tx_tail]);
        pes->tx_bufs[pes->tx_tail] = NULL;
        pes->tx_tail++;
        if (pes->tx_tail >= PIC32_ETH_TX_DESC_COUNT) {
            pes->tx_tail = 0;
        }
    }

    pic32_eth_unlock(pes);
}

static err_t
pic32_eth_output(struct netif *nif, struct pbuf *p)
{
    struct pic32_eth_state *pes = (struct pic32_eth_state *)nif;
    struct pic32_eth_desc *sed;
    struct pbuf *q;
    err_t errval;
    uint32_t tx_head;

    /* Update SNMP stats (only if you use SNMP) */
    MIB2_STATS_NETIF_ADD(netif, ifoutoctets, p->tot_len);
    int unicast = ((*(uint8_t *)(p->payload) & 0x01) == 0);
    if (unicast) {
        MIB2_STATS_NETIF_INC(netif, ifoutucastpkts);
    } else {
        MIB2_STATS_NETIF_INC(netif, ifoutnucastpkts);
    }

    pic32_eth_output_done(pes);

    pic32_eth_lock(pes);

    tx_head = pes->tx_head;
    ++pic32_eth_stats.oframe;
    sed = &pes->tx_descs[tx_head];
    /* Check if there is enough descriptors for pbuf chain */
    for (q = p; q; q = q->next) {
        if (q->len == 0) {
            continue;
        }
        if (sed->hdr.EOWN) {
            LINK_STATS_INC(link.drop);
            LINK_STATS_INC(link.memerr);
            errval = ERR_MEM;
            goto error;
        }
        sed = (struct pic32_eth_desc *)PA_TO_KVA0((uint32_t)sed->next_ed);
    }

    for (q = p; q; q = q->next) {
        if (!q->len) {
            continue;
        }
        sed = &pes->tx_descs[tx_head];
        sed->data_buffer_address = KVA_TO_PA((uint32_t)q->payload);
        pes->tx_bufs[tx_head] = q;
        pbuf_ref(q);
        sed->hdr.BYTE_COUNT = q->len;
        sed->hdr.EOP = q->next == NULL ? 1 : 0;
        sed->hdr.SOP = q == p ? 1 : 0;
        dcache_flush_area(q->payload, q->len);
        /* Don't set EOWN in first packet yet */
        if (p != q) {
            sed->hdr.EOWN = 1;
        }
        tx_head++;
        if (tx_head >= PIC32_ETH_TX_DESC_COUNT) {
            tx_head = 0;
        }
    }

    /* Pass first buffer to ETH */
    sed->hdr.EOWN = 1;
    /* If TXRTS is on buffer will be used without starting anew */
    if (ETHCON1bits.TXRTS == 0) {
        /* TX is not started, just check that buffer was not consumed in the meantime */
        if (sed->hdr.EOWN) {
            /* Set first descriptor address */
            ETHTXST = KVA_TO_PA((uint32_t)&pes->tx_descs[pes->tx_head]);
            ETHCON1SET = _ETHCON1_TXRTS_MASK;
        }
    }
    pes->tx_head = tx_head;

    pic32_eth_unlock(pes);

    return ERR_OK;
error:
    ++pic32_eth_stats.oerr;
    pic32_eth_unlock(pes);

    return errval;
}

void
__attribute__((interrupt(IPL4AUTO), vector(_ETHERNET_VECTOR)))
pic32_eth_isr(void)
{
    /* Enabled pending interrupt requests */
    uint32_t irq = ETHIRQ & ETHIEN;
    /* Acknowledge the interrupt flags */
    ETHIRQCLR = irq;

    pic32_eth_input(&pic32_eth_state);

    IFS4CLR = _IFS4_ETHIF_MASK;
}

static void
pic32_phy_isr_task(void *arg)
{
    struct pic32_eth_state *pes = (void *)arg;
    uint16_t reg;
    uint8_t phy_addr = pes->cfg->phy_addr;

    pic32_eth_phy_read_register(phy_addr, PHY_BSR, &reg);
    if (reg & PHY_LINKED_STATUS &&
        (pes->nif.flags & NETIF_FLAG_LINK_UP) == 0) {
        netif_set_link_up(&pes->nif);
    } else if ((reg & PHY_LINKED_STATUS) == 0 &&
               pes->nif.flags & NETIF_FLAG_LINK_UP) {
        netif_set_link_down(&pes->nif);
    }
    switch (pes->cfg->phy_type) {
    case LAN_8710:
    case LAN_8720:
    case LAN_8740:
    case LAN_8742:
        pic32_eth_phy_read_register(phy_addr, LAN_87xx_ISR, &reg);
        break;
    default:
        break;
    }
}

static void
pic32_phy_isr(void *arg)
{
    struct pic32_eth_state *pes = (void *)arg;

    tcpip_callbackmsg_trycallback(pes->phy_isr_msg);
}

static void
pic32_phy_poll(void *arg)
{
    struct pic32_eth_state *pes = (void *)arg;

    pic32_phy_isr_task(arg);
    os_cputime_timer_relative(&pes->phy_tmr, PIC32_PHY_POLL_FREQ);
}

static err_t
pic32_lwip_init(struct netif *nif)
{
    struct pic32_eth_state *pes = &pic32_eth_state;
    const struct pic32_eth_cfg *cfg;
    uint16_t reg;
    uint16_t new_reg;

    /*
     * LwIP clears most of these field in netif_add() before calling
     * this init routine. So we need to fill them in here.
     */
    memcpy(nif->name, "et", 2);
    nif->output = etharp_output;
#if LWIP_IPV6
    nif->output_ip6 = ethip6_output;
#endif
    nif->linkoutput = pic32_eth_output;
    nif->mtu = 1500;
    nif->hwaddr_len = ETHARP_HWADDR_LEN;
    nif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP;

#if LWIP_IGMP
    nif->flags |= NETIF_FLAG_IGMP;
    nif->igmp_mac_filter = pic32_igmp_mac_filter;
#endif
#if LWIP_IPV6 && LWIP_IPV6_MLD
    nif->flags |= NETIF_FLAG_MLD6;
    nif->mld_mac_filter = pic32_mld_mac_filter;
#endif

    cfg = pes->cfg;

    pes->rx_head = 0;
    pes->rx_tail = 0;
    pes->tx_head = 0;
    pes->tx_tail = 0;
    pes->rx_descs = (struct pic32_eth_desc *)PA_TO_KVA1((uint32_t)&pes->_rx_descs);
    pes->tx_descs = (struct pic32_eth_desc *)PA_TO_KVA1((uint32_t)&pes->_tx_descs);

    /* 35.4.10 Ethernet Initialization Sequence */
    /*  1. Ethernet Controller Initialization: */
    /*   1.a */
    IEC4CLR = _IEC4_ETHIE_MASK;
    /*   1.b */
    ETHCON1CLR = _ETHCON1_ON_MASK;
    ETHCON1CLR = _ETHCON1_RXEN_MASK | _ETHCON1_TXRTS_MASK;
    /*   1.c */
    while (ETHSTATbits.BUSY);
    /*   1.d */
    IFS4CLR = _IFS4_ETHIF_MASK;
    /*   1.e */
    ETHIEN = 0;
    /*   1.f */
    ETHTXST = 0;
    ETHRXST = 0;

    ETHRXFC = _ETHRXFC_CRCOKEN_MASK | _ETHRXFC_RUNTEN_MASK | _ETHRXFC_UCEN_MASK | _ETHRXFC_BCEN_MASK |
              _ETHRXFC_MCEN_MASK;

    /*  2. MAC initialization */
    /*   2.a */
    EMAC1CFG1SET = _EMAC1CFG1_SOFTRESET_MASK;
    os_cputime_delay_usecs(100);
    EMAC1CFG1CLR = _EMAC1CFG1_SOFTRESET_MASK;
    /*   2.b */
    /*   2.c */
    /*   2.d */
#if (__PIC32_PIN_COUNT == 64)
    ANSELECLR = 0xFF;
    ANSELBCLR = 0x8000;
#elif (__PIC32_PIN_COUNT == 100)
#else
    ANSELDCLR = 0x0840;
    ANSELHCLR = 0x2130;
    ANSELJCLR = 0x0B02;
#endif
    if (DEVCFG3bits.FMIIEN == 0) {
        /*   2.e.i Reset RMII and set 100 Mbps */
        EMAC1SUPPSET = _EMAC1SUPP_RESETRMII_MASK | _EMAC1SUPP_SPEEDRMII_MASK;
        EMAC1SUPPCLR = _EMAC1SUPP_RESETRMII_MASK;
    } else {
        /* 8720 nor 8742 does not have MII */
        assert(cfg->phy_type == LAN_8710 || cfg->phy_type == LAN_8740);
    }
    /*   2.e.ii MIIM block reset */
    EMAC1MCFGSET = _EMAC1MCFG_RESETMGMT_MASK;
    os_cputime_delay_usecs(100);
    EMAC1MCFGCLR = _EMAC1MCFG_RESETMGMT_MASK;
    EMAC1MCFGCLR = _EMAC1MCFG_NOPRE_MASK;

    ETHCON1SET = _ETHCON1_ON_MASK;

    /*   2.e.iii Divider = 20 */
    EMAC1MCFGbits.CLKSEL = 10;

    /* 3. PHY Initialization */
    /*   3.a Reset the PHY */
    pic32_eth_phy_write_register(cfg->phy_addr, PHY_BCR, PHY_RESET);

    os_time_delay(os_time_ms_to_ticks32(1));
    /*   3.b Set the MII/RMII operation mode */
    /* Just verify that PHY is aligned with what is selected in APP, it applies to chips that can be MII and RMII */
    if (pes->cfg->phy_type == LAN_8710 || pes->cfg->phy_type == LAN_8740) {
        pic32_eth_phy_read_register(cfg->phy_addr, LAN_87xx_SPECIAL_MODES, &reg);
        assert(((reg & PHY_RMII) != 0) != ((DEVCFG3 & _DEVCFG3_FMIIEN_MASK) == 0));
    }
    /*   3.c Automatic MDIX is the default, nothing to do */
    /*   3.d,e,f  */
    pic32_eth_phy_read_register(cfg->phy_addr, PHY_BSR, &reg);

    /* Enable auto-negotiation if it's possible */
    if (reg & PHY_AUTONEGOTIATION_ABILITY) {
        pic32_eth_phy_read_register(cfg->phy_addr, PHY_BCR, &reg);
        if ((reg & PHY_AUTONEGOTIATION) == 0) {
            reg |= PHY_AUTONEGOTIATION;
            pic32_eth_phy_write_register(cfg->phy_addr, PHY_BCR, reg);
        }
    }

    /* 4. MAC configuration */
    /*   4.a Enable rx */
    EMAC1CFG1SET = _EMAC1CFG1_RXENABLE_MASK | _EMAC1CFG1_RXPAUSE_MASK | _EMAC1CFG1_TXPAUSE_MASK;
    /*   4.b Select the desired automatic padding and CRC capabilities, enabling of the Huge
             frames, and the Duplex */
    EMAC1CFG2SET = _EMAC1CFG2_PADENABLE_MASK | _EMAC1CFG2_CRCENABLE_MASK | _EMAC1CFG2_FULLDPLX_MASK;
    /*   4.c Program the EMAC1IPGT register with the back-to-back inter-packet gap. */
    EMAC1IPGT = 21;
    /*   4.d Setting the non-back-to-back inter-packet gap */
    EMAC1IPGR = 0x0C12;
    /*   4.e MAC collision window/retry */
    EMAC1CLRT = 0x370F;
    /*   4.f MAC Max frame length */
    EMAC1MAXF = 1518;
    /*   4.g Use factory default MAC address */
    /* 5. Ethernet controller initialization */
    /* 5.a skip this step */
    /* 5.b  */
    ETHRXWMbits.RXFWM = 0x0005;
    ETHRXWMbits.RXEWM = 0x0000;
    /* 5.c  */
    ETHCON1SET = _ETHCON1_AUTOFC_MASK;
    ETHCON1CLR = _ETHCON1_MANFC_MASK;
    /* 5.d skip  */
    if (DEVCFG3bits.FMIIEN) {

    }

    pic32_eth_setup_descs(pes->rx_descs, PIC32_ETH_RX_DESC_COUNT);
    pic32_eth_setup_descs(pes->tx_descs, PIC32_ETH_TX_DESC_COUNT);
    pic32_eth_fill_rx(pes);

    ETHRXST = KVA_TO_PA(&pes->rx_descs[0]);
    ETHCON2 = ETH_MAX_PACKET_SIZE;
    ETHCON1SET = _ETHCON1_ON_MASK | _ETHCON1_RXEN_MASK;

    /*
     * Generate an interrupt when link state changes
     */
    if (cfg->phy_irq_pin >= 0) {
        pes->phy_isr_msg = tcpip_callbackmsg_new(pic32_phy_isr_task, pes);
        assert(pes->phy_isr_msg);

        switch (cfg->phy_type) {
        case LAN_8710:
        case LAN_8720:
        case LAN_8740:
        case LAN_8742:
            pic32_eth_phy_read_register(cfg->phy_addr, LAN_87xx_IMR, &reg);
            new_reg = reg | LAN_87xx_ISR_AUTO_DONE | LAN_87xx_ISR_LINK_DOWN;
            pic32_eth_phy_write_register(cfg->phy_addr, LAN_87xx_IMR, new_reg);
            break;
        default:
            break;
        }
        hal_gpio_irq_enable(cfg->phy_irq_pin);
    } else {
        os_cputime_timer_init(&pes->phy_tmr, pic32_phy_poll, pes);
        os_cputime_timer_relative(&pes->phy_tmr, PIC32_PHY_POLL_FREQ);
    }

    /* Enable interrupts except watermark for now */
    ETHIEN = _ETHIEN_TXABORTIE_MASK | _ETHIEN_PKTPENDIE_MASK | _ETHIEN_RXBUFNAIE_MASK |
             _ETHIEN_RXBUSEIE_MASK | _ETHIEN_RXDONEIE_MASK | _ETHIEN_RXOVFLWIE_MASK |
             _ETHIEN_TXBUSEIE_MASK;
    IEC4SET = _IEC4_ETHIE_MASK;

    return ERR_OK;
}

int
pic32_eth_set_hwaddr(uint8_t *addr)
{
    struct pic32_eth_state *pes = &pic32_eth_state;

    if (pes->nif.name[0] != '\0') {
        /*
         * Needs to be called before pic32_eth_open() is called.
         */
        return -1;
    }
    memcpy(pes->nif.hwaddr, addr, ETHARP_HWADDR_LEN);
    EMAC1SA0bits.STNADDR6 = addr[5];
    EMAC1SA0bits.STNADDR5 = addr[4];
    EMAC1SA1bits.STNADDR4 = addr[3];
    EMAC1SA1bits.STNADDR3 = addr[2];
    EMAC1SA2bits.STNADDR2 = addr[1];
    EMAC1SA2bits.STNADDR1 = addr[0];

    return 0;
}

static int
pic32_eth_up(struct netif *nif)
{
    err_t err;

    netif_set_up(nif);
    err = dhcp_start(nif);
#if LWIP_IPV6
    nif->ip6_autoconfig_enabled = 1;
    netif_create_ip6_linklocal_address(nif, 1);
#endif
    if (nif->flags & NETIF_FLAG_LINK_UP) {
        netif_set_default(nif);
        return err ? -1 : 0;
    }

    return 0;
}

int
pic32_eth_open(void)
{
    struct pic32_eth_state *pes = &pic32_eth_state;
    struct netif *nif;
    struct ip4_addr addr;
    int rc;

    if (pes->cfg == NULL) {
        /*
         * HW configuration not passed in.
         */
        return -1;
    }

    os_mutex_init(&pes->lock);

    /* set ETH interrupt priority */
    IPC38CLR = _IPC38_ETHIP_MASK;
    IPC38SET = (4 << _IPC38_ETHIP_POSITION); /* priority 4 */
    /* set ETH interrupt subpriority */
    IPC38CLR = _IPC38_ETHIS_MASK;
    IPC38SET = (0 << _IPC38_ETHIS_POSITION); /* subpriority 0 */

    /* Microchip provided MAC address */
    pes->nif.hwaddr[5] = EMAC1SA0bits.STNADDR6;
    pes->nif.hwaddr[4] = EMAC1SA0bits.STNADDR5;
    pes->nif.hwaddr[3] = EMAC1SA1bits.STNADDR4;
    pes->nif.hwaddr[2] = EMAC1SA1bits.STNADDR3;
    pes->nif.hwaddr[1] = EMAC1SA2bits.STNADDR2;
    pes->nif.hwaddr[0] = EMAC1SA2bits.STNADDR1;
    pes->nif.hwaddr_len = ETHARP_HWADDR_LEN;

    if (pes->cfg->phy_irq_pin >= 0) {
        rc = hal_gpio_irq_init(pes->cfg->phy_irq_pin, pic32_phy_isr, pes, HAL_GPIO_TRIG_FALLING,
                               pes->cfg->phy_irq_pin_pull_up ? HAL_GPIO_PULL_UP : HAL_GPIO_PULL_NONE);
        assert(rc == 0);
    }

    /*
     * Register network interface with LwIP.
     */
    memset(&addr, 0, sizeof(addr));
    nif = netif_add(&pes->nif, &addr, &addr, &addr, NULL, pic32_lwip_init,
                    tcpip_input);
    assert(nif);

    if (MYNEWT_VAL(PIC32_ETH_0_AUTO_UP)) {
        pic32_eth_up(nif);
    }

    return 0;
}
