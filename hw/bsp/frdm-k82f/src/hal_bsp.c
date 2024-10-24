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

#include <stddef.h>
#include <stdint.h>
#include <errno.h>
#include <sys/types.h>
#include <stdio.h>
#include "os/mynewt.h"
#include "mynewt_cm.h"
#include "bsp/bsp.h"
#include "hal/hal_bsp.h"
#include "hal/hal_flash_int.h"
#include "flash_map/flash_map.h"
#include "hal/hal_flash.h"
#if MYNEWT_VAL(TIMER_0) || MYNEWT_VAL(TIMER_1)
#include "hal/hal_timer.h"
#endif
#if MYNEWT_VAL(ENC_FLASH_DEV)
#include <ef_crypto/ef_crypto.h>
#endif
#if MYNEWT_VAL(HASH)
#include "hash/hash.h"
#include "hash_kinetis/hash_kinetis.h"
#endif
#if MYNEWT_VAL(TRNG)
#include "trng/trng.h"
#include "trng_kinetis/trng_kinetis.h"
#endif
#if MYNEWT_VAL(CRYPTO)
#include "crypto/crypto.h"
#include "crypto_kinetis/crypto_kinetis.h"
#endif
#if MYNEWT_VAL(UART_0) || MYNEWT_VAL(UART_1) || MYNEWT_VAL(UART_2) || \
    MYNEWT_VAL(UART_3) || MYNEWT_VAL(UART_4)
#include "uart/uart.h"
#include "uart_hal/uart_hal.h"
#include "hal/hal_uart.h"
#endif
#if MYNEWT_VAL(I2C_0) || MYNEWT_VAL(I2C_1) || MYNEWT_VAL(I2C_2) || MYNEWT_VAL(I2C_3)
#include "hal/hal_i2c.h"
#endif
#if MYNEWT_VAL(SPI_0_MASTER) || MYNEWT_VAL(SPI_1_MASTER) || \
    MYNEWT_VAL(SPI_2_MASTER) || MYNEWT_VAL(SPI_0_SLAVE) || \
    MYNEWT_VAL(SPI_1_SLAVE) || MYNEWT_VAL(SPI_2_SLAVE)
#include "hal/hal_spi.h"
#endif
#include "mcu/cmsis_nvic.h"
#include "mcu/frdm-k8xf_hal.h"
#include "fsl_device_registers.h"
#include "fsl_common.h"
#include "fsl_clock.h"
#include "fsl_port.h"
#include "clock_config.h"

#if MYNEWT_VAL(UART_0)
static struct uart_dev os_bsp_uart0;
#endif
#if MYNEWT_VAL(UART_1)
static struct uart_dev os_bsp_uart1;
#endif
#if MYNEWT_VAL(UART_2)
static struct uart_dev os_bsp_uart2;
#endif
#if MYNEWT_VAL(UART_3)
static struct uart_dev os_bsp_uart3;
#endif
#if MYNEWT_VAL(UART_4)
static struct uart_dev os_bsp_uart4;
#endif

#if MYNEWT_VAL(I2C_0)
static const struct nxp_hal_i2c_cfg hal_i2c0_cfg = {
    .pin_sda = MYNEWT_VAL(I2C_0_PIN_SDA),
    .pin_scl = MYNEWT_VAL(I2C_0_PIN_SCL),
    .frequency = MYNEWT_VAL(I2C_0_FREQ_KHZ),
};
#endif
#if MYNEWT_VAL(I2C_1)
static const struct nxp_hal_i2c_cfg hal_i2c1_cfg = {
    .pin_sda = MYNEWT_VAL(I2C_1_PIN_SDA),
    .pin_scl = MYNEWT_VAL(I2C_1_PIN_SCL),
    .frequency = MYNEWT_VAL(I2C_1_FREQ_KHZ),
};
#endif
#if MYNEWT_VAL(I2C_2)
static const struct nxp_hal_i2c_cfg hal_i2c2_cfg = {
    .pin_sda = MYNEWT_VAL(I2C_2_PIN_SDA),
    .pin_scl = MYNEWT_VAL(I2C_2_PIN_SCL),
    .frequency = MYNEWT_VAL(I2C_2_FREQ_KHZ),
};
#endif
#if MYNEWT_VAL(I2C_3)
static const struct nxp_hal_i2c_cfg hal_i2c3_cfg = {
    .pin_sda = MYNEWT_VAL(I2C_3_PIN_SDA),
    .pin_scl = MYNEWT_VAL(I2C_3_PIN_SCL),
    .frequency = MYNEWT_VAL(I2C_3_FREQ_KHZ),
};
#endif

#if MYNEWT_VAL(HASH)
static struct hash_dev os_bsp_hash;
#endif

#if MYNEWT_VAL(TRNG)
static struct trng_dev os_bsp_trng;
#endif

#if MYNEWT_VAL(CRYPTO)
static struct crypto_dev os_bsp_crypto;
#endif

/*
 * What memory to include in coredump.
 */
static const struct hal_bsp_mem_dump dump_cfg[] = {
    [0] = {
        .hbmd_start = &__DATA_ROM,
        .hbmd_size = RAM_SIZE
    }
};

static void
init_hardware(void)
{
    /* Disable the MPU otherwise USB cannot access the bus */
    SYSMPU->CESR = 0;

    /* Enable all the ports */
    SIM->SCGC5 |= (SIM_SCGC5_PORTA_MASK | SIM_SCGC5_PORTB_MASK | SIM_SCGC5_PORTC_MASK | SIM_SCGC5_PORTD_MASK |
                   SIM_SCGC5_PORTE_MASK);
}

extern void BOARD_BootClockRUN(void);

#if MYNEWT_VAL(ENC_FLASH_DEV)
static struct eflash_crypto_dev enc_flash_dev0 = {
    .ecd_dev = {
        .efd_hal = {
            .hf_itf = &enc_flash_funcs,
        },
        .efd_hwdev = &kinetis_flash_dev,
    }
};
#endif

const struct hal_flash *
hal_bsp_flash_dev(uint8_t id)
{
    if (id == 0) {
        return &kinetis_flash_dev;
    }
#if MYNEWT_VAL(QSPI_ENABLE)
    if (id == 1) {
        return &nxp_qspi_dev;
    }
#endif
#if MYNEWT_VAL(ENC_FLASH_DEV)
    if (id == 2) {
        return &enc_flash_dev0.ecd_dev.efd_hal;
    }
#endif
    return NULL;
}

const struct hal_bsp_mem_dump *
hal_bsp_core_dump(int *area_cnt)
{
    *area_cnt = sizeof(dump_cfg) / sizeof(dump_cfg[0]);
    return dump_cfg;
}

int
hal_bsp_power_state(int state)
{
    return (0);
}

/**
 * Returns the configured priority for the given interrupt. If no priority
 * configured, return the priority passed in
 *
 * @param irq_num
 * @param pri
 *
 * @return uint32_t
 */
uint32_t
hal_bsp_get_nvic_priority(int irq_num, uint32_t pri)
{
    /* Add any interrupt priorities configured by the bsp here */
    return pri;
}

void
hal_bsp_init(void)
{
    int rc = 0;

    (void)rc;

    /* Init pinmux and other hardware setup. */
    init_hardware();
    BOARD_BootClockRUN();

#if MYNEWT_VAL(TIMER_0)
    rc = hal_timer_init(0, NULL);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(TIMER_1)
    rc = hal_timer_init(1, NULL);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(HASH)
    rc = os_dev_create(&os_bsp_hash.dev, "hash", OS_DEV_INIT_KERNEL,
                       OS_DEV_INIT_PRIO_DEFAULT, kinetis_hash_dev_init, NULL);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(TRNG)
    rc = os_dev_create(&os_bsp_trng.dev, "trng",
                       OS_DEV_INIT_KERNEL, OS_DEV_INIT_PRIO_DEFAULT,
                       kinetis_trng_dev_init, NULL);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(CRYPTO)
    rc = os_dev_create(&os_bsp_crypto.dev, "crypto",
                       OS_DEV_INIT_KERNEL, OS_DEV_INIT_PRIO_DEFAULT,
                       kinetis_crypto_dev_init, NULL);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(UART_0)
    rc = os_dev_create((struct os_dev *) &os_bsp_uart0, "uart0",
                       OS_DEV_INIT_PRIMARY, 0, uart_hal_init, NULL);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(UART_1)
    rc = os_dev_create((struct os_dev *) &os_bsp_uart1, "uart1",
                       OS_DEV_INIT_PRIMARY, 0, uart_hal_init, NULL);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(UART_2)
    rc = os_dev_create((struct os_dev *) &os_bsp_uart2, "uart2",
                       OS_DEV_INIT_PRIMARY, 0, uart_hal_init, NULL);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(UART_3)
    rc = os_dev_create((struct os_dev *) &os_bsp_uart3, "uart3",
                       OS_DEV_INIT_PRIMARY, 0, uart_hal_init, NULL);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(UART_4)
    rc = os_dev_create((struct os_dev *) &os_bsp_uart4, "uart4",
                       OS_DEV_INIT_PRIMARY, 0, uart_hal_init, NULL);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(I2C_0)
    rc = hal_i2c_init(0, (void *)&hal_i2c0_cfg);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(I2C_1)
    rc = hal_i2c_init(1, (void *)&hal_i2c1_cfg);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(I2C_2)
    rc = hal_i2c_init(2, (void *)&hal_i2c2_cfg);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(I2C_3)
    rc = hal_i2c_init(3, (void *)&hal_i2c3_cfg);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(SPI_0_MASTER)
    rc = hal_spi_init(0, NULL, HAL_SPI_TYPE_MASTER);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(SPI_1_MASTER)
    rc = hal_spi_init(1, NULL, HAL_SPI_TYPE_MASTER);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(SPI_2_MASTER)
    rc = hal_spi_init(2, NULL, HAL_SPI_TYPE_MASTER);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(SPI_0_SLAVE)
    rc = hal_spi_init(0, NULL, HAL_SPI_TYPE_SLAVE);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(SPI_1_SLAVE)
    rc = hal_spi_init(1, NULL, HAL_SPI_TYPE_SLAVE);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(SPI_2_SLAVE)
    rc = hal_spi_init(2, NULL, HAL_SPI_TYPE_SLAVE);
    assert(rc == 0);
#endif

}

void
hal_bsp_deinit(void)
{
    Cortex_DisableAll();
}
