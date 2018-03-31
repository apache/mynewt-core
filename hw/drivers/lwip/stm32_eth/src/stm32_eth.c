/**
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

#include <hal/hal_gpio.h>
#include <hal/hal_timer.h>
#include <mcu/cmsis_nvic.h>

#if MYNEWT_VAL(MCU_STM32F4)
#include <bsp/stm32f4xx_hal_conf.h>
#include <mcu/stm32f4_bsp.h>
#endif
#if MYNEWT_VAL(MCU_STM32F7)
#include <bsp/stm32f7xx_hal_conf.h>
#include <mcu/stm32f7_bsp.h>
#endif

#include <netif/etharp.h>
#include <netif/ethernet.h>
#include <lwip/tcpip.h>
#include <lwip/ethip6.h>
#include <string.h>

#include "stm32_eth/stm32_eth.h"
#include "stm32_eth/stm32_eth_cfg.h"

/*
 * PHY polling frequency, when HW has no interrupt line to report link status
 * changes.
 *
 * Note STM32F767ZI errata regarding RMII sometimes corrupting RX.
 * This shows up as MMCRFCECR going up, and no valid RX happening.
 */
#define STM32_PHY_POLL_FREQ os_cputime_usecs_to_ticks(1500000) /* 1.5 secs */

/*
 * Phy specific registers
 */
#define SMSC_8710_ISR 29
#define SMSC_8710_IMR 30

#define SMSC_8710_ISR_AUTO_DONE 0x40
#define SMSC_8710_ISR_LINK_DOWN 0x10

#define STM32_ETH_RX_DESC_SZ 3
#define STM32_ETH_TX_DESC_SZ 4

struct stm32_eth_desc {
    volatile ETH_DMADescTypeDef desc;
    struct pbuf *p;
};

struct stm32_eth_state {
    struct netif st_nif;
    ETH_HandleTypeDef st_eth;
    struct stm32_eth_desc st_rx_descs[STM32_ETH_RX_DESC_SZ];
    struct stm32_eth_desc st_tx_descs[STM32_ETH_TX_DESC_SZ];
    uint8_t st_rx_head;
    uint8_t st_rx_tail;
    uint8_t st_tx_head;
    uint8_t st_tx_tail;
    struct hal_timer st_phy_tmr;
    const struct stm32_eth_cfg *cfg;
};

struct stm32_eth_stats {
    uint32_t oframe;
    uint32_t odone;
    uint32_t oerr;
    uint32_t iframe;
    uint32_t imem;
} stm32_eth_stats;

static struct stm32_eth_state stm32_eth_state;

static void
stm32_eth_setup_descs(struct stm32_eth_desc *descs, int cnt)
{
    int i;

    for (i = 0; i < cnt - 1; i++) {
        descs[i].desc.Status = 0;
        descs[i].desc.Buffer2NextDescAddr = (uint32_t)&descs[i + 1].desc;
    }
    descs[cnt - 1].desc.Status = 0;
    descs[cnt - 1].desc.Buffer2NextDescAddr = (uint32_t)&descs[0].desc;
}

static void
stm32_eth_fill_rx(struct stm32_eth_state *ses)
{
    struct stm32_eth_desc *sed;
    struct pbuf *p;

    while (1) {
        sed = &ses->st_rx_descs[ses->st_rx_tail];
        if (sed->p) {
            break;
        }
        p = pbuf_alloc(PBUF_RAW, ETH_MAX_PACKET_SIZE, PBUF_POOL);
        if (!p) {
            ++stm32_eth_stats.imem;
            break;
        }
        sed->p = p;
        sed->desc.Status = 0;
        sed->desc.ControlBufferSize = ETH_DMARXDESC_RCH | ETH_MAX_PACKET_SIZE;
        sed->desc.Buffer1Addr = (uint32_t)p->payload;
        sed->desc.Status = ETH_DMARXDESC_OWN;

        ses->st_rx_tail++;
        if (ses->st_rx_tail >= STM32_ETH_RX_DESC_SZ) {
            ses->st_rx_tail = 0;
        }
    }
}

static void
stm32_eth_input(struct stm32_eth_state *ses)
{
    struct stm32_eth_desc *sed;
    struct pbuf *p;
    struct netif *nif;

    nif = &ses->st_nif;

    while (1) {
        sed = &ses->st_rx_descs[ses->st_rx_head];
        if (!sed->p) {
            break;
        }
        if (sed->desc.Status & ETH_DMARXDESC_OWN) {
            break;
        }
        p = sed->p;
        sed->p = NULL;
        if (!(sed->desc.Status & ETH_DMARXDESC_LS)) {
            /*
             * Incoming data spans multiple pbufs. XXX support later
             */
            pbuf_free(p);
            continue;
        }
        p->len = p->tot_len = (sed->desc.Status & ETH_DMARXDESC_FL) >> 16;
        ++stm32_eth_stats.iframe;
        nif->input(p, nif);
        ses->st_rx_head++;
        if (ses->st_rx_head >= STM32_ETH_RX_DESC_SZ) {
            ses->st_rx_head = 0;
        }
    }

    stm32_eth_fill_rx(ses);

    if (ses->st_eth.Instance->DMASR & ETH_DMASR_RBUS)  {
        /*
         * Resume DMA reception
         */
        ses->st_eth.Instance->DMASR = ETH_DMASR_RBUS;
        ses->st_eth.Instance->DMARPDR = 0;
    }
}

void
HAL_ETH_RxCpltCallback(ETH_HandleTypeDef *heth)
{
    stm32_eth_input(&stm32_eth_state);
}

/*
 * Hardware configuration. Should be called from BSP init.
 */
int
stm32_eth_init(const void *cfg)
{
    stm32_eth_state.cfg = (const struct stm32_eth_cfg *)cfg;
    return 0;
}

/*
 * XXX do proper multicast filtering at some stage
 */
#if LWIP_IGMP
static err_t
stm32_igmp_mac_filter(struct netif *nif, const ip4_addr_t *group,
  enum netif_mac_filter_action action)
{
    return -1;
}
#endif

#if LWIP_IPV6 && LWIP_IPV6_MLD
static err_t
stm32_mld_mac_filter(struct netif *nif, const ip6_addr_t *group,
  enum netif_mac_filter_action action)
{
    return -1;
}
#endif

static void
stm32_eth_output_done(struct stm32_eth_state *ses)
{
    struct stm32_eth_desc *sed;

    while (1) {
        sed = &ses->st_tx_descs[ses->st_tx_tail];
        if (!sed->p) {
            break;
        }
        if (sed->desc.Status & ETH_DMATXDESC_OWN) {
            /*
             * Belongs to board
             */
            break;
        }
        if (sed->desc.Status & ETH_DMATXDESC_ES) {
            ++stm32_eth_stats.oerr;
        } else {
            ++stm32_eth_stats.odone;
        }
        pbuf_free(sed->p);
        sed->p = NULL;
        ses->st_tx_tail++;
        if (ses->st_tx_tail >= STM32_ETH_TX_DESC_SZ) {
            ses->st_tx_tail = 0;
        }
    }
}

static err_t
stm32_eth_output(struct netif *nif, struct pbuf *p)
{
    struct stm32_eth_state *ses = (struct stm32_eth_state *)nif;
    struct stm32_eth_desc *sed;
    uint32_t reg;
    struct pbuf *q;
    err_t errval;

    stm32_eth_output_done(ses);

    ++stm32_eth_stats.oframe;
    sed = &ses->st_tx_descs[ses->st_tx_head];
    for (q = p; q; q = q->next) {
        if (!q->len) {
            continue;
        }
        if (sed->desc.Status & ETH_DMATXDESC_OWN) {
            /*
             * Not enough space.
             */
            errval = ERR_MEM;
            goto error;
        }
        sed = (struct stm32_eth_desc *)sed->desc.Buffer2NextDescAddr;
    }

    for (q = p; q; q = q->next) {
        if (!q->len) {
            continue;
        }
        sed = &ses->st_tx_descs[ses->st_tx_head];
        if (q == p) {
            reg = ETH_DMATXDESC_FS | ETH_DMATXDESC_TCH;
        } else {
            reg = ETH_DMATXDESC_TCH;
        }
        if (q->next == NULL) {
            reg |= ETH_DMATXDESC_LS;
        }
        sed->desc.Status = reg;
        sed->desc.ControlBufferSize = p->len;
        sed->desc.Buffer1Addr = (uint32_t)p->payload;
        sed->p = p;
        pbuf_ref(p);
        sed->desc.Status = reg | ETH_DMATXDESC_OWN;
        ses->st_tx_head++;
        if (ses->st_tx_head >= STM32_ETH_TX_DESC_SZ) {
            ses->st_tx_head = 0;
        }
    }

    if (ses->st_eth.Instance->DMASR & ETH_DMASR_TBUS) {
        /*
         * Resume DMA transmission.
         */
        ses->st_eth.Instance->DMASR = ETH_DMASR_TBUS;
        ses->st_eth.Instance->DMATPDR = 0U;
    }

    return ERR_OK;
error:
    ++stm32_eth_stats.oerr;
    return errval;
}

static void
stm32_eth_isr(void)
{
    HAL_ETH_IRQHandler(&stm32_eth_state.st_eth);
}

static void
stm32_phy_isr(void *arg)
{
    struct stm32_eth_state *ses = (void *)arg;
    uint32_t reg;

    HAL_ETH_ReadPHYRegister(&ses->st_eth, PHY_BSR, &reg);
    if (reg & PHY_LINKED_STATUS &&
        (ses->st_nif.flags & NETIF_FLAG_LINK_UP) == 0) {
        netif_set_link_up(&ses->st_nif);
    } else if ((reg & PHY_LINKED_STATUS) == 0 &&
               ses->st_nif.flags & NETIF_FLAG_LINK_UP) {
        netif_set_link_down(&ses->st_nif);
    }
    switch (ses->cfg->sec_phy_type) {
    case SMSC_8710_RMII:
    case LAN_8742_RMII:
        HAL_ETH_ReadPHYRegister(&ses->st_eth, SMSC_8710_ISR, &reg);
    }
}

static void
stm32_phy_poll(void *arg)
{
    struct stm32_eth_state *ses = (void *)arg;

    stm32_phy_isr(arg);
    os_cputime_timer_relative(&ses->st_phy_tmr, STM32_PHY_POLL_FREQ);
}

static err_t
stm32_lwip_init(struct netif *nif)
{
    struct stm32_eth_state *ses = &stm32_eth_state;
    int i, j;
    const struct stm32_eth_cfg *cfg;
    uint32_t reg;

    /*
     * LwIP clears most of these field in netif_add() before calling
     * this init routine. So we need to fill them in here.
     */
    memcpy(nif->name, "st", 2);
    nif->output = etharp_output;
#if LWIP_IPV6
    nif->output_ip6 = ethip6_output;
#endif
    nif->linkoutput = stm32_eth_output;
    nif->mtu = 1500;
    nif->hwaddr_len = ETHARP_HWADDR_LEN;
    nif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP;

#if LWIP_IGMP
    nif->flags |= NETIF_FLAG_IGMP;
    nif->igmp_mac_filter = stm32_igmp_mac_filter;
#endif
#if LWIP_IPV6 && LWIP_IPV6_MLD
    nif->flags |= NETIF_FLAG_MLD6;
    nif->mld_mac_filter = stm32_mld_mac_filter;
#endif

    cfg = ses->cfg;

    /*
     * Now take the BSP specific HW config and set up the hardware.
     */
    for (i = 0; i < STM32_MAX_PORTS; i++) {
        for (j = 0; j < 32; j++) {
            if ((cfg->sec_port_mask[i] & (1 << j)) == 0) {
                continue;
            }
            hal_gpio_init_af(i * 16 + j, GPIO_AF11_ETH, GPIO_NOPULL, 0);
        }
    }

    NVIC_SetVector(ETH_IRQn, (uint32_t)stm32_eth_isr);
    __HAL_RCC_ETH_CLK_ENABLE();

    ses->st_eth.Instance = ETH;

    ses->st_eth.Init.AutoNegotiation = ETH_AUTONEGOTIATION_ENABLE;
    ses->st_eth.Init.Speed = ETH_SPEED_100M;
    ses->st_eth.Init.DuplexMode = ETH_MODE_FULLDUPLEX;
    ses->st_eth.Init.PhyAddress = cfg->sec_phy_addr;
    ses->st_eth.Init.RxMode = ETH_RXINTERRUPT_MODE;
    ses->st_eth.Init.ChecksumMode = ETH_CHECKSUM_BY_HARDWARE;

    switch (cfg->sec_phy_type) {
    case SMSC_8710_RMII:
    case LAN_8742_RMII:
        ses->st_eth.Init.MediaInterface = ETH_MEDIA_INTERFACE_RMII;
    }

    ses->st_rx_head = 0;
    ses->st_rx_tail = 0;
    ses->st_tx_head = 0;
    ses->st_tx_tail = 0;

    stm32_eth_setup_descs(ses->st_rx_descs, STM32_ETH_RX_DESC_SZ);
    stm32_eth_setup_descs(ses->st_tx_descs, STM32_ETH_TX_DESC_SZ);
    stm32_eth_fill_rx(ses);

    if (HAL_ETH_Init(&ses->st_eth) == HAL_ERROR) {
        return ERR_IF;
    }

    /*
     * XXX pass all multicast traffic for now
     */
    ses->st_eth.Instance->MACFFR |= ETH_MULTICASTFRAMESFILTER_NONE;
    ses->st_eth.Instance->DMATDLAR = (uint32_t)ses->st_tx_descs;
    ses->st_eth.Instance->DMARDLAR = (uint32_t)ses->st_rx_descs;

    /*
     * Generate an interrupt when link state changes
     */
    if (cfg->sec_phy_irq >= 0) {
        switch (cfg->sec_phy_type) {
        case SMSC_8710_RMII:
            HAL_ETH_ReadPHYRegister(&ses->st_eth, SMSC_8710_IMR, &reg);
            reg |= (SMSC_8710_ISR_AUTO_DONE | SMSC_8710_ISR_LINK_DOWN);
            HAL_ETH_WritePHYRegister(&ses->st_eth, SMSC_8710_IMR, reg);
        case LAN_8742_RMII:
            /* XXX */
            break;
        }
    } else {
        os_cputime_timer_init(&ses->st_phy_tmr, stm32_phy_poll, ses);
        os_cputime_timer_relative(&ses->st_phy_tmr, STM32_PHY_POLL_FREQ);
    }
    NVIC_EnableIRQ(ETH_IRQn);
    HAL_ETH_Start(&ses->st_eth);

    /*
     * Check for link.
     */
    stm32_phy_isr(ses);
    return ERR_OK;
}

int
stm32_mii_dump(int (*func)(const char *fmt, ...))
{
    int i;
    struct stm32_eth_state *ses = &stm32_eth_state;
    uint32_t reg;
    int rc;

    for (i = 0; i <= 6; i++) {
        rc = HAL_ETH_ReadPHYRegister(&ses->st_eth, i, &reg);
        func("%d: %x (%d)\n", i, reg, rc);
    }
    for (i = 17; i <= 18; i++) {
        rc = HAL_ETH_ReadPHYRegister(&ses->st_eth, i, &reg);
        func("%d: %x (%d)\n", i, reg, rc);
    }
    for (i = 26; i <= 31; i++) {
        rc = HAL_ETH_ReadPHYRegister(&ses->st_eth, i, &reg);
        func("%d: %x (%d)\n", i, reg, rc);
    }
    return 0;
}

int
stm32_eth_set_hwaddr(uint8_t *addr)
{
    struct stm32_eth_state *ses = &stm32_eth_state;

    if (ses->st_nif.name[0] != '\0') {
        /*
         * Needs to be called before stm32_eth_open() is called.
         */
        return -1;
    }
    memcpy(ses->st_nif.hwaddr, addr, ETHARP_HWADDR_LEN);
    ses->st_eth.Init.MACAddr = ses->st_nif.hwaddr;
    return 0;
}

int
stm32_eth_open(void)
{
    struct stm32_eth_state *ses = &stm32_eth_state;
    struct netif *nif;
    struct ip4_addr addr;
    int rc;

    if (ses->cfg == NULL) {
        /*
         * HW configuration not passed in.
         */
        return -1;
    }
    if (ses->st_eth.Init.MACAddr == NULL) {
        /*
         * MAC address not set
         */
        return -1;
    }

    if (ses->cfg->sec_phy_irq >= 0) {
        rc = hal_gpio_irq_init(ses->cfg->sec_phy_irq, stm32_phy_isr, ses,
                               HAL_GPIO_TRIG_FALLING, HAL_GPIO_PULL_UP);
        assert(rc == 0);
    }

    /*
     * Register network interface with LwIP.
     */
    memset(&addr, 0, sizeof(addr));
    nif = netif_add(&ses->st_nif, &addr, &addr, &addr, NULL, stm32_lwip_init,
                    tcpip_input);
    assert(nif);
    return 0;
}
