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
#if MYNEWT_VAL(MCU_STM32H7)
#include <bsp/stm32h7xx_hal_conf.h>
#include <mcu/stm32h7_bsp.h>

#define ETH_RX_BUF_SIZE           (ETH_MAX_PACKET_SIZE)
#define PHY_BSR                   ((uint16_t)0x0001U)
#define PHY_LINKED_STATUS         ((uint16_t)0x0004U)

#define __HAL_RCC_ETH_CLK_ENABLE()       do { \
        __HAL_RCC_ETH1MAC_CLK_ENABLE();   \
        __HAL_RCC_ETH1TX_CLK_ENABLE();    \
        __HAL_RCC_ETH1RX_CLK_ENABLE();    \
} while (0)
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
#define STM32_ETH_RX_BUFFER_CNT   MYNEWT_VAL(STM32_ETH_RX_BUFFER_CNT)

/*
 * Phy specific registers
 */
#define SMSC_8710_ISR 29
#define SMSC_8710_IMR 30

#define SMSC_8710_ISR_AUTO_DONE 0x40
#define SMSC_8710_ISR_LINK_DOWN 0x10

struct rx_buff {
    struct pbuf_custom pbuf_custom;
    uint8_t buff[(ETH_RX_BUF_SIZE + 31) & ~31];
};

struct stm32_eth_state {
    struct netif st_nif;
    ETH_HandleTypeDef st_eth;
    ETH_DMADescTypeDef st_rx_descs[ETH_RX_DESC_CNT];
    ETH_DMADescTypeDef st_tx_descs[ETH_TX_DESC_CNT];
    ETH_TxPacketConfig st_tx_cfg;
    ETH_MACConfigTypeDef st_mac_cfg;
    struct hal_timer st_phy_tmr;
    const struct stm32_eth_cfg *cfg;
};

static struct stm32_eth_state stm32_eth_state;
static bool rx_alloc_failed;

LWIP_MEMPOOL_DECLARE(RX_POOL, STM32_ETH_RX_BUFFER_CNT, sizeof(struct rx_buff), "Zero-copy RX PBUF pool");

static void
stm32_eth_input(struct stm32_eth_state *ses)
{
    struct pbuf *p = NULL;

    if (rx_alloc_failed) {
        return;
    }

    HAL_ETH_ReadData(&ses->st_eth, (void **)&p);
    if (p != NULL) {
        if (ses->st_nif.input(p, &ses->st_nif) != ERR_OK) {
            pbuf_free(p);
        }
    }
}

void
HAL_ETH_RxCpltCallback(ETH_HandleTypeDef *heth)
{
    struct stm32_eth_state *ses = &stm32_eth_state;

    stm32_eth_input(ses);
}

void
pbuf_free_custom(struct pbuf *p)
{
    struct pbuf_custom *custom_pbuf = (struct pbuf_custom *)p;
    struct stm32_eth_state *ses = &stm32_eth_state;
    LWIP_MEMPOOL_FREE(RX_POOL, custom_pbuf);

    if (rx_alloc_failed) {
        rx_alloc_failed = 0;
        stm32_eth_input(ses);
    }
}

void
HAL_ETH_RxAllocateCallback(uint8_t **buff)
{
    struct pbuf_custom *p;

    p = LWIP_MEMPOOL_ALLOC(RX_POOL);
    if (p != NULL) {
        *buff = ((struct rx_buff *)p)->buff;
        p->custom_free_function = pbuf_free_custom;
        pbuf_alloced_custom(PBUF_RAW, 0, PBUF_REF, p, *buff, ETH_RX_BUF_SIZE);
    } else {
        rx_alloc_failed = 1;
    }
}

void
HAL_ETH_RxLinkCallback(void **p_start, void **p_end, uint8_t *buff, uint16_t len)
{
    struct pbuf **pp_start = (struct pbuf **)p_start;
    struct pbuf **pp_end = (struct pbuf **)p_end;
    struct pbuf *p = NULL;

    /* Get the struct pbuf from the buff address. */
    p = (struct pbuf *)(buff - offsetof(struct rx_buff, buff));
    p->next = NULL;
    p->tot_len = 0;
    p->len = len;

    /* Chain the buffer. */
    if (!*pp_start) {
        /* The first buffer of the packet. */
        *pp_start = p;
    } else {
        /* Chain the buffer to the end of the packet. */
        (*pp_end)->next = p;
    }
    *pp_end = p;

    /* Update the total length of all the buffers of the chain. Each pbuf in the chain should have its tot_len
     * set to its own length, plus the length of all the following pbufs in the chain. */
    for (p = *pp_start; p != NULL; p = p->next) {
        p->tot_len += len;
    }
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

static err_t
stm32_eth_output(struct netif *nif, struct pbuf *p)
{
    int i = 0;
    struct pbuf *q;
    err_t errval = ERR_OK;
    ETH_BufferTypeDef tx_buffer[ETH_TX_DESC_CNT] = {0};
    struct stm32_eth_state *ses = &stm32_eth_state;

    memset(tx_buffer, 0, ETH_TX_DESC_CNT * sizeof(ETH_BufferTypeDef));

    for (q = p; q != NULL; q = q->next) {
        if (i >= ETH_TX_DESC_CNT) {
            return ERR_IF;
        }

        tx_buffer[i].buffer = q->payload;
        tx_buffer[i].len = q->len;

        if (i > 0) {
            tx_buffer[i - 1].next = &tx_buffer[i];
        }

        if (q->next == NULL) {
            tx_buffer[i].next = NULL;
        }

        i++;
    }

    ses->st_tx_cfg.Length = p->tot_len;
    ses->st_tx_cfg.TxBuffer = tx_buffer;
    ses->st_tx_cfg.pData = p;

    HAL_ETH_Transmit(&ses->st_eth, &ses->st_tx_cfg, 20);

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
    const struct stm32_eth_cfg *cfg;
    uint32_t reg;

    cfg = ses->cfg;

    HAL_ETH_ReadPHYRegister(&ses->st_eth, cfg->sec_phy_addr, PHY_BSR, &reg);

    if ((reg & PHY_LINKED_STATUS) &&
        (ses->st_nif.flags & NETIF_FLAG_LINK_UP) == 0) {
        netif_set_link_up(&ses->st_nif);
    } else if ((reg & PHY_LINKED_STATUS) == 0 &&
               ses->st_nif.flags & NETIF_FLAG_LINK_UP) {
        netif_set_link_down(&ses->st_nif);
    }
    switch (ses->cfg->sec_phy_type) {
    case SMSC_8710_RMII:
    case LAN_8742_RMII:
        HAL_ETH_ReadPHYRegister(&ses->st_eth, cfg->sec_phy_addr, SMSC_8710_ISR, &reg);
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

    LWIP_MEMPOOL_INIT(RX_POOL);

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
    ses->st_eth.Init.RxDesc = ses->st_rx_descs;
    ses->st_eth.Init.TxDesc = ses->st_tx_descs;
    ses->st_eth.Init.RxBuffLen = ETH_RX_BUF_SIZE;

    switch (cfg->sec_phy_type) {
    case SMSC_8710_RMII:
    case LAN_8742_RMII:
        ses->st_eth.Init.MediaInterface = HAL_ETH_RMII_MODE;
    }

    ses->st_tx_cfg.Attributes = ETH_TX_PACKETS_FEATURES_CSUM | ETH_TX_PACKETS_FEATURES_CRCPAD;
    ses->st_tx_cfg.ChecksumCtrl = ETH_CHECKSUM_IPHDR_PAYLOAD_INSERT_PHDR_CALC;
    ses->st_tx_cfg.CRCPadCtrl = ETH_CRC_PAD_INSERT;

    /*
     * XXX pass all multicast traffic for now
     */
#if MYNEWT_VAL(MCU_STM32H7)
    ses->st_eth.Instance->MACPFR |= ETH_MACPFR_PM;
#else
    ses->st_eth.Instance->MACFFR |= ETH_MULTICASTFRAMESFILTER_NONE;
#endif

    if (HAL_ETH_Init(&ses->st_eth) == HAL_ERROR) {
        return ERR_IF;
    }

    /*
     * Generate an interrupt when link state changes
     */
    if (cfg->sec_phy_irq >= 0) {
        switch (cfg->sec_phy_type) {
        case SMSC_8710_RMII:
            HAL_ETH_ReadPHYRegister(&ses->st_eth, cfg->sec_phy_addr, SMSC_8710_IMR, &reg);
            reg |= (SMSC_8710_ISR_AUTO_DONE | SMSC_8710_ISR_LINK_DOWN);
            HAL_ETH_WritePHYRegister(&ses->st_eth, cfg->sec_phy_addr, SMSC_8710_IMR, reg);
        case LAN_8742_RMII:
            /* XXX */
            break;
        }
    } else {
        os_cputime_timer_init(&ses->st_phy_tmr, stm32_phy_poll, ses);
        os_cputime_timer_relative(&ses->st_phy_tmr, STM32_PHY_POLL_FREQ);
    }

    HAL_ETH_GetMACConfig(&ses->st_eth, &ses->st_mac_cfg);
    ses->st_mac_cfg.DuplexMode = ETH_FULLDUPLEX_MODE;
    ses->st_mac_cfg.Speed = ETH_SPEED_100M;
    HAL_ETH_SetMACConfig(&ses->st_eth, &ses->st_mac_cfg);

    NVIC_EnableIRQ(ETH_IRQn);
    if (HAL_ETH_Start_IT(&ses->st_eth) == HAL_ERROR) {
        return ERR_IF;
    }

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
    const struct stm32_eth_cfg *cfg;
    uint32_t reg;
    int rc;

    cfg = ses->cfg;

    for (i = 0; i <= 6; i++) {
        rc = HAL_ETH_ReadPHYRegister(&ses->st_eth, cfg->sec_phy_addr, i, &reg);
        func("%d: %x (%d)\n", i, reg, rc);
    }
    for (i = 17; i <= 18; i++) {
        rc = HAL_ETH_ReadPHYRegister(&ses->st_eth, cfg->sec_phy_addr, i, &reg);
        func("%d: %x (%d)\n", i, reg, rc);
    }
    for (i = 26; i <= 31; i++) {
        rc = HAL_ETH_ReadPHYRegister(&ses->st_eth, cfg->sec_phy_addr, i, &reg);
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

    stm32_eth_set_hwaddr(MYNEWT_VAL(STM32_MAC_ADDR));

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
