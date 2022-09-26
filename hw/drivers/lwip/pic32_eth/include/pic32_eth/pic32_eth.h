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

#ifndef __PIC32_ETH_H__
#define __PIC32_ETH_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#ifndef PIC32_ETH_RX_DESC_COUNT
#define PIC32_ETH_RX_DESC_COUNT         4
#endif
#ifndef PIC32_ETH_TX_DESC_COUNT
#define PIC32_ETH_TX_DESC_COUNT         4
#endif

/*
 * BSP specific ethernet settings.
 */
struct pic32_eth_cfg {
    enum {
        LAN_8710, /* MII/RMII */
        LAN_8720, /* RMII */
        LAN_8740, /* MII/RMII */
        LAN_8742, /* RMII */
    } phy_type;
    int phy_irq_pin;
    uint8_t phy_irq_pin_pull_up;
    uint8_t phy_addr;
};

/*
 * Initialize the ethernet device with cfg.
 *
 * @param cfg HW specific configuration.
 *
 * @return int 0 on success, non-zero error code on failure.
 */
int pic32_eth_init(const struct pic32_eth_cfg *cfg);

/*
 * Set the MAC address for ethernet to use.
 *
 * @param addr Byte array of 6 bytes to use as MAC address.
 *
 * @return int 0 on success, non-zero error code on failure.
 */
int pic32_eth_set_hwaddr(uint8_t *addr);

/**
 * Reads PHY registers
 *
 * @param phy_addr phy address
 * @param reg_addr phy register number (0-31)
 * @param reg_value pointer to buffer for register value
 * @return int 0 on success, non-zero error code on failure.
 */
int pic32_eth_phy_read_register(uint8_t phy_addr, uint8_t reg_addr, uint16_t *reg_value);

/**
 * Writes PHY register
 *
 * @param phy_addr phy address
 * @param reg_addr phy register number (0-31)
 * @param reg_value new register value
 * @return int 0 on success, non-zero error code on failure.
 */
int pic32_eth_phy_write_register(uint8_t phy_addr, uint8_t reg_addr, uint16_t reg_value);

#ifdef __cplusplus
}
#endif

#endif /* __PIC32_ETH_H__ */
