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

// #include <string.h>
// #include <errno.h>
// #include <assert.h>
// #include <os/mynewt.h>
// #include <mcu/cmsis_nvic.h>
// #include <hal/hal_spi.h>
// #include <mcu/nrf54h20_rad_hal.h>
// #include <nrf.h>
// #include <nrfx_common.h>

// #if MYNEWT_VAL(SPI_0_MASTER) || MYNEWT_VAL(SPI_0_SLAVE)

// #define SPIM_TXD_MAXCNT_MAX 0xffff

// /* Used to disable all interrupts */
// #define NRF_SPI_IRQ_DISABLE_ALL 0xffffffff

// /*
//  *  Slave states
//  *
//  *  IDLE: Slave not ready to be used. If master attempts to access
//  *        slave it will receive the default character
//  *  ACQ_SEM: Slave is attempting to acquire semaphore.
//  *  READY: Slave is ready for master to send it data
//  *
//  */
// #define HAL_SPI_SLAVE_STATE_IDLE        (0)
// #define HAL_SPI_SLAVE_STATE_ACQ_SEM     (1)
// #define HAL_SPI_SLAVE_STATE_READY       (2)

// #if MYNEWT_VAL(SPI_0_MASTER)
// struct nrf54h20_rad_hal_spi
// {
//     uint8_t spi_xfr_flag;
//     uint8_t dummy_rx;
//     uint16_t nhs_buflen;
//     uint16_t nhs_bytes_txd;
//     struct hal_spi_settings spi_cfg;

//     /* Pointers to tx/rx buffers */
//     uint8_t *nhs_txbuf;
//     uint8_t *nhs_rxbuf;

//     /* Callback and arguments */
//     hal_spi_txrx_cb txrx_cb_func;
//     void            *txrx_cb_arg;
// };
// #endif

// #if MYNEWT_VAL(SPI_0_SLAVE)
// struct nrf54h20_rad_hal_spi
// {
//     uint8_t slave_state;
//     uint16_t nhs_buflen;
//     uint16_t nhs_bytes_txd;
//     struct hal_spi_settings spi_cfg;

//     /* Pointers to tx/rx buffers */
//     uint8_t *nhs_txbuf;
//     uint8_t *nhs_rxbuf;

//     /* Callback and arguments */
//     hal_spi_txrx_cb txrx_cb_func;
//     void            *txrx_cb_arg;
// };
// #endif

// static struct nrf54h20_rad_hal_spi nrf54h20_rad_hal_spi0;

// static void
// nrf54h20_rad_spi0_irq_handler(void)
// {
//     struct nrf54h20_rad_hal_spi *spi = &nrf54h20_rad_hal_spi0;
//     uint16_t xfr_bytes;
// #if MYNEWT_VAL(SPI_0_MASTER)
//     uint16_t len;
// #endif

//     os_trace_isr_enter();

// #if MYNEWT_VAL(SPI_0_MASTER)
//     if (NRF_SPIM0_NS->EVENTS_END) {
//         NRF_SPIM0_NS->EVENTS_END = 0;

//         /* Should not occur but if no transfer just leave  */
//         if (spi->spi_xfr_flag != 0) {
//             /* Are there more bytes to send? */
//             xfr_bytes = NRF_SPIM0_NS->TXD.AMOUNT;
//             spi->nhs_bytes_txd += xfr_bytes;
//             if (spi->nhs_bytes_txd < spi->nhs_buflen) {
//                 spi->nhs_txbuf += xfr_bytes;
//                 len = spi->nhs_buflen - spi->nhs_bytes_txd;
//                 len = min(SPIM_TXD_MAXCNT_MAX, len);
//                 NRF_SPIM0_NS->TXD.PTR = (uint32_t)spi->nhs_txbuf;
//                 NRF_SPIM0_NS->TXD.MAXCNT = len;

//                 /* If no rxbuf, we need to set rxbuf and maxcnt to 1 */
//                 if (spi->nhs_rxbuf) {
//                     spi->nhs_rxbuf += xfr_bytes;
//                     NRF_SPIM0_NS->RXD.PTR    = (uint32_t)spi->nhs_rxbuf;
//                     NRF_SPIM0_NS->RXD.MAXCNT = len;
//                 }
//                 NRF_SPIM0_NS->TASKS_START = 1;
//             } else {
//                 spi->spi_xfr_flag = 0;
//                 NRF_SPIM0_NS->INTENCLR = SPIM_INTENSET_END_Msk;
//                 if (spi->txrx_cb_func) {
//                     spi->txrx_cb_func(spi->txrx_cb_arg, spi->nhs_buflen);
//                 }
//             }
//         }
//     }
// #endif

// #if MYNEWT_VAL(SPI_0_SLAVE)
//     /* Semaphore acquired event */
//     if (NRF_SPIS0_NS->EVENTS_ACQUIRED) {
//         NRF_SPIS0_NS->EVENTS_ACQUIRED = 0;

//         if (spi->slave_state == HAL_SPI_SLAVE_STATE_ACQ_SEM) {
//             if (spi->nhs_txbuf == NULL) {
//                 NRF_SPIS0_NS->TXD.PTR = 0;
//                 NRF_SPIS0_NS->TXD.MAXCNT = 0;
//             } else {
//                 NRF_SPIS0_NS->TXD.PTR = (uint32_t)spi->nhs_txbuf;
//                 NRF_SPIS0_NS->TXD.MAXCNT = spi->nhs_buflen;
//             }

//             if (spi->nhs_rxbuf == NULL) {
//                 NRF_SPIS0_NS->RXD.PTR = 0;
//                 NRF_SPIS0_NS->RXD.MAXCNT = 0;
//             } else {
//                 NRF_SPIS0_NS->RXD.PTR = (uint32_t)spi->nhs_rxbuf;
//                 NRF_SPIS0_NS->RXD.MAXCNT = spi->nhs_buflen;
//             }
//             NRF_SPIS0_NS->TASKS_RELEASE = 1;
//             spi->slave_state = HAL_SPI_SLAVE_STATE_READY;
//         }
//     }

//     /* SPI transaction complete */
//     if (NRF_SPIS0_NS->EVENTS_END) {
//         NRF_SPIS0_NS->EVENTS_END = 0;
//         if (spi->slave_state == HAL_SPI_SLAVE_STATE_READY) {
//             if (spi->txrx_cb_func) {
//                 /* Get transfer length */
//                 if (spi->nhs_txbuf == NULL) {
//                     xfr_bytes = NRF_SPIS0_NS->RXD.AMOUNT;
//                 } else {
//                     xfr_bytes = NRF_SPIS0_NS->TXD.AMOUNT;
//                 }
//                 spi->txrx_cb_func(spi->txrx_cb_arg, xfr_bytes);
//             }
//             spi->slave_state = HAL_SPI_SLAVE_STATE_IDLE;
//         }
//     }
// #endif

//     os_trace_isr_exit();
// }

// #if MYNEWT_VAL(SPI_0_MASTER)
// static void
// hal_spi_master_stop_transfer(void)
// {
//     NRF_SPIM0_NS->TASKS_STOP = 1;
//     while (!NRF_SPIM0_NS->EVENTS_STOPPED) {
//     };
//     NRF_SPIM0_NS->EVENTS_STOPPED = 0;
// }

// static int
// hal_spi_config_master(struct nrf54h20_rad_hal_spi *spi,
//                       struct hal_spi_settings *settings)
// {
//     int rc;
//     uint32_t nrf_config;
//     uint32_t frequency;
//     NRF_GPIO_Type *port;
//     uint32_t pin;

//     memcpy(&spi->spi_cfg, settings, sizeof(*settings));

//     /*
//      * Configure SCK. NOTE: this is done here in the config API as the data
//      * mode is not set at init time so we do it here when we configure the SPI.
//      */
//     pin = NRF_SPIM0_NS->PSEL.SCK & SPIM_PSEL_SCK_PIN_Msk;
//     if (NRF_SPIM0_NS->PSEL.SCK & SPIM_PSEL_SCK_PORT_Msk) {
//         port = NRF_P1_NS;
//     } else {
//         port = NRF_P0_NS;
//     }

//     if (settings->data_mode <= HAL_SPI_MODE1) {
//         port->OUTCLR = (1UL << pin);
//     } else {
//         port->OUTSET = (1UL << pin);
//     }
//     port->PIN_CNF[pin] =
//         (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) |
//         (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos);

//     /* Only 8-bit word sizes supported. */
//     rc = 0;
//     switch (settings->word_size) {
//         case HAL_SPI_WORD_SIZE_8BIT:
//             break;
//         default:
//             rc = EINVAL;
//             break;
//     }

//     switch (settings->data_mode) {
//         case HAL_SPI_MODE0:
//             nrf_config = (SPIM_CONFIG_CPOL_ActiveHigh << SPIM_CONFIG_CPOL_Pos) |
//                          (SPIM_CONFIG_CPHA_Leading << SPIM_CONFIG_CPHA_Pos);
//             break;
//         case HAL_SPI_MODE1:
//             nrf_config = (SPIM_CONFIG_CPOL_ActiveHigh << SPIM_CONFIG_CPOL_Pos) |
//                          (SPIM_CONFIG_CPHA_Trailing << SPIM_CONFIG_CPHA_Pos);
//             break;
//         case HAL_SPI_MODE2:
//             nrf_config = (SPIM_CONFIG_CPOL_ActiveLow << SPIM_CONFIG_CPOL_Pos) |
//                          (SPIM_CONFIG_CPHA_Leading << SPIM_CONFIG_CPHA_Pos);
//             break;
//         case HAL_SPI_MODE3:
//             nrf_config = (SPIM_CONFIG_CPOL_ActiveLow << SPIM_CONFIG_CPOL_Pos) |
//                          (SPIM_CONFIG_CPHA_Trailing << SPIM_CONFIG_CPHA_Pos);
//             break;
//         default:
//             nrf_config = 0;
//             rc = EINVAL;
//             break;
//     }

//     /* NOTE: msb first is 0 so no check done */
//     if (settings->data_order == HAL_SPI_LSB_FIRST) {
//         nrf_config |= SPIM_CONFIG_ORDER_LsbFirst;
//     }
//     NRF_SPIM0_NS->CONFIG = nrf_config;

//     switch (settings->baudrate) {
//         case 125:
//             frequency = SPIM_FREQUENCY_FREQUENCY_K125;
//             break;
//         case 250:
//             frequency = SPIM_FREQUENCY_FREQUENCY_K250;
//             break;
//         case 500:
//             frequency = SPIM_FREQUENCY_FREQUENCY_K500;
//             break;
//         case 1000:
//             frequency = SPIM_FREQUENCY_FREQUENCY_M1;
//             break;
//         case 2000:
//             frequency = SPIM_FREQUENCY_FREQUENCY_M2;
//             break;
//         case 4000:
//             frequency = SPIM_FREQUENCY_FREQUENCY_M4;
//             break;
//         case 8000:
//             frequency = SPIM_FREQUENCY_FREQUENCY_M8;
//             break;
//         default:
//             frequency = 0;
//             rc = EINVAL;
//             break;
//     }
//     NRF_SPIM0_NS->FREQUENCY = frequency;

//     return rc;
// }
// #endif

// #if MYNEWT_VAL(SPI_0_SLAVE)
// static int
// hal_spi_config_slave(struct nrf54h20_rad_hal_spi *spi,
//                      struct hal_spi_settings *settings)
// {
//     int rc;
//     uint32_t nrf_config;

//     rc = 0;
//     switch (settings->data_mode) {
//         case HAL_SPI_MODE0:
//             nrf_config = (SPIS_CONFIG_CPOL_ActiveHigh << SPIS_CONFIG_CPOL_Pos) |
//                          (SPIS_CONFIG_CPHA_Leading << SPIS_CONFIG_CPHA_Pos);
//             break;
//         case HAL_SPI_MODE1:
//             nrf_config = (SPIS_CONFIG_CPOL_ActiveHigh << SPIS_CONFIG_CPOL_Pos) |
//                          (SPIS_CONFIG_CPHA_Trailing << SPIS_CONFIG_CPHA_Pos);
//             break;
//         case HAL_SPI_MODE2:
//             nrf_config = (SPIS_CONFIG_CPOL_ActiveLow << SPIS_CONFIG_CPOL_Pos) |
//                          (SPIS_CONFIG_CPHA_Leading << SPIS_CONFIG_CPHA_Pos);
//             break;
//         case HAL_SPI_MODE3:
//             nrf_config = (SPIS_CONFIG_CPOL_ActiveLow << SPIS_CONFIG_CPOL_Pos) |
//                          (SPIS_CONFIG_CPHA_Trailing << SPIS_CONFIG_CPHA_Pos);
//             break;
//         default:
//             nrf_config = 0;
//             rc = EINVAL;
//             break;
//     }

//     if (settings->data_order == HAL_SPI_LSB_FIRST) {
//         nrf_config |= SPIS_CONFIG_ORDER_LsbFirst;
//     }
//     NRF_SPIS0_NS->CONFIG = nrf_config;

//     /* Only 8-bit word sizes supported. */
//     switch (settings->word_size) {
//         case HAL_SPI_WORD_SIZE_8BIT:
//             break;
//         default:
//             rc = EINVAL;
//             break;
//     }

//     return rc;
// }
// #endif

// #if MYNEWT_VAL(SPI_0_MASTER)
// static int
// hal_spi_init_master(struct nrf54h20_rad_hal_spi *spi,
//                     struct nrf54h20_rad_hal_spi_cfg *cfg)
// {
//     NRF_GPIO_Type *port;
//     uint32_t pin;

//     /*  Configure MOSI */
//     port = HAL_GPIO_PORT(cfg->mosi_pin);
//     pin = HAL_GPIO_INDEX(cfg->mosi_pin);
//     port->OUTCLR = (1UL << pin);
//     port->PIN_CNF[pin] =
//         ((uint32_t)GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) |
//         ((uint32_t)GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos);

//     /* Configure MISO */
//     port = HAL_GPIO_PORT(cfg->miso_pin);
//     pin = HAL_GPIO_INDEX(cfg->miso_pin);
//     port->PIN_CNF[pin] =
//         ((uint32_t)GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
//         ((uint32_t)GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos);

//     NRF_SPIM0_NS->PSEL.SCK = cfg->sck_pin;
//     NRF_SPIM0_NS->PSEL.MOSI = cfg->mosi_pin;
//     NRF_SPIM0_NS->PSEL.MISO = cfg->miso_pin;

//     NRF_SPIM0_NS->INTENCLR = NRF_SPI_IRQ_DISABLE_ALL;
//     NVIC_SetVector(SPIM0_SPIS0_TWIM0_TWIS0_UARTE0_IRQn,
//                    (uint32_t)nrf54h20_rad_spi0_irq_handler);
//     NVIC_SetPriority(SPIM0_SPIS0_TWIM0_TWIS0_UARTE0_IRQn,
//                      (1 << __NVIC_PRIO_BITS) - 1);
//     NVIC_ClearPendingIRQ(SPIM0_SPIS0_TWIM0_TWIS0_UARTE0_IRQn);
//     NVIC_EnableIRQ(SPIM0_SPIS0_TWIM0_TWIS0_UARTE0_IRQn);

//     return 0;
// }
// #endif

// #if MYNEWT_VAL(SPI_0_SLAVE)
// static int
// hal_spi_init_slave(struct nrf54h20_rad_hal_spi *spi,
//                    struct nrf54h20_rad_hal_spi_cfg *cfg)
// {
//     NRF_GPIO_Type *port;
//     uint32_t pin;

//     /* NOTE: making this pin an input is correct! See datasheet */
//     port = HAL_GPIO_PORT(cfg->miso_pin);
//     pin = HAL_GPIO_INDEX(cfg->miso_pin);
//     port->PIN_CNF[pin] =
//         ((uint32_t)GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
//         ((uint32_t)GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos);

//     port = HAL_GPIO_PORT(cfg->mosi_pin);
//     pin = HAL_GPIO_INDEX(cfg->mosi_pin);
//     port->PIN_CNF[pin] =
//         ((uint32_t)GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
//         ((uint32_t)GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos);

//     port = HAL_GPIO_PORT(cfg->ss_pin);
//     pin = HAL_GPIO_INDEX(cfg->ss_pin);
//     port->PIN_CNF[pin] =
//         ((uint32_t)GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
//         ((uint32_t)GPIO_PIN_CNF_PULL_Pullup  << GPIO_PIN_CNF_PULL_Pos) |
//         ((uint32_t)GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos);

//     port = HAL_GPIO_PORT(cfg->sck_pin);
//     pin = HAL_GPIO_INDEX(cfg->sck_pin);
//     port->PIN_CNF[pin] =
//         ((uint32_t)GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) |
//         ((uint32_t)GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos);

//     NRF_SPIS0_NS->PSEL.SCK  = cfg->sck_pin;
//     NRF_SPIS0_NS->PSEL.MOSI = cfg->mosi_pin;
//     NRF_SPIS0_NS->PSEL.MISO = cfg->miso_pin;
//     NRF_SPIS0_NS->PSEL.CSN  = cfg->ss_pin;

//     /* Disable interrupt and clear any interrupt events */
//     NRF_SPIS0_NS->INTENCLR = SPIS_INTENSET_ACQUIRED_Msk | SPIS_INTENSET_END_Msk;
//     NRF_SPIS0_NS->EVENTS_END = 0;
//     NRF_SPIS0_NS->EVENTS_ACQUIRED = 0;

//     /* Enable END_ACQUIRE shortcut. */
//     NRF_SPIS0_NS->SHORTS = SPIS_SHORTS_END_ACQUIRE_Msk;

//     /* Set interrupt vector and enable IRQ */
//     NVIC_SetVector(SPIM0_SPIS0_TWIM0_TWIS0_UARTE0_IRQn,
//                    (uint32_t)nrf54h20_rad_spi0_irq_handler);
//     NVIC_SetPriority(SPIM0_SPIS0_TWIM0_TWIS0_UARTE0_IRQn,
//                      (1 << __NVIC_PRIO_BITS) - 1);
//     NVIC_ClearPendingIRQ(SPIM0_SPIS0_TWIM0_TWIS0_UARTE0_IRQn);
//     NVIC_EnableIRQ(SPIM0_SPIS0_TWIM0_TWIS0_UARTE0_IRQn);

//     return 0;
// }
// #endif

// /**
//  * Initialize the SPI, given by spi_num.
//  *
//  * @param spi_num The number of the SPI to initialize
//  * @param cfg HW/MCU specific configuration,
//  *            passed to the underlying implementation, providing extra
//  *            configuration.
//  * @param spi_type SPI type (master or slave)
//  *
//  * @return int 0 on success, non-zero error code on failure.
//  */
// int
// hal_spi_init(int spi_num, void *cfg, uint8_t spi_type)
// {
//     struct nrf54h20_rad_hal_spi *spi = &nrf54h20_rad_hal_spi0;

//     /* Check for valid arguments */
//     if (spi_num != 0 || cfg == NULL) {
//        return EINVAL;
//     }

// #if MYNEWT_VAL(SPI_0_MASTER)
//     if (spi_type != HAL_SPI_TYPE_MASTER) {
//         return EINVAL;
//     }

//     return hal_spi_init_master(spi, (struct nrf54h20_rad_hal_spi_cfg *)cfg);
// #endif

// #if MYNEWT_VAL(SPI_0_SLAVE)
//     if (spi_type != HAL_SPI_TYPE_SLAVE) {
//         return EINVAL;
//     }

//     return hal_spi_init_slave(spi, (struct nrf54h20_rad_hal_spi_cfg *)cfg);
// #endif
// }

// int
// hal_spi_init_hw(uint8_t spi_num, uint8_t spi_type,
//                 const struct hal_spi_hw_settings *cfg)
// {
//     struct nrf54h20_rad_hal_spi_cfg hal_cfg;

//     hal_cfg.sck_pin = cfg->pin_sck;
//     hal_cfg.mosi_pin = cfg->pin_mosi;
//     hal_cfg.miso_pin = cfg->pin_miso;
//     hal_cfg.ss_pin = cfg->pin_ss;

//     return hal_spi_init(spi_num, &hal_cfg, spi_type);
// }

// /**
//  * Configure the spi. Must be called after the spi is initialized (after
//  * hal_spi_init is called) and when the spi is disabled (user must call
//  * hal_spi_disable if the spi has been enabled through hal_spi_enable prior
//  * to calling this function). Can also be used to reconfigure an initialized
//  * SPI (assuming it is disabled as described previously).
//  *
//  * @param spi_num The number of the SPI to configure.
//  * @param psettings The settings to configure this SPI with
//  *
//  * @return int 0 on success, non-zero error code on failure.
//  */
// int
// hal_spi_config(int spi_num, struct hal_spi_settings *settings)
// {
//     struct nrf54h20_rad_hal_spi *spi = &nrf54h20_rad_hal_spi0;

//     if (spi_num != 0) {
//         return EINVAL;
//     }

// #if MYNEWT_VAL(SPI_0_MASTER)
//     if (NRF_SPIM0_NS->ENABLE != 0) {
//         return EINVAL;
//     }

//     return hal_spi_config_master(spi, settings);
// #endif

// #if MYNEWT_VAL(SPI_0_SLAVE)
//     if (NRF_SPIS0_NS->ENABLE != 0) {
//         return EINVAL;
//     }

//     return hal_spi_config_slave(spi, settings);
// #endif
// }

// /**
//  * Enables the SPI. This does not start a transmit or receive operation;
//  * it is used for power mgmt. Cannot be called when a SPI transfer is in
//  * progress.
//  *
//  * @param spi_num
//  *
//  * @return int 0 on success, non-zero error code on failure.
//  */
// int
// hal_spi_enable(int spi_num)
// {
//     struct nrf54h20_rad_hal_spi *spi = &nrf54h20_rad_hal_spi0;

//     if (spi_num != 0 || spi->txrx_cb_func == NULL) {
//         return EINVAL;
//     }

// #if MYNEWT_VAL(SPI_0_MASTER)
//     NRF_SPIM0_NS->EVENTS_END = 0;
//     NRF_SPIM0_NS->ENABLE = (SPIM_ENABLE_ENABLE_Enabled << SPIM_ENABLE_ENABLE_Pos);
// #endif

// #if MYNEWT_VAL(SPI_0_SLAVE)
//     NRF_SPIS0_NS->EVENTS_END = 0;
//     NRF_SPIS0_NS->EVENTS_ACQUIRED = 0;
//     NRF_SPIS0_NS->INTENSET = SPIS_INTENSET_END_Msk | SPIS_INTENSET_ACQUIRED_Msk;
//     NRF_SPIS0_NS->ENABLE = (SPIS_ENABLE_ENABLE_Enabled << SPIS_ENABLE_ENABLE_Pos);
// #endif

//     return 0;
// }

// /**
//  * Disables the SPI. Used for power mgmt. It will halt any current SPI transfers
//  * in progress.
//  *
//  * @param spi_num
//  *
//  * @return int 0 on success, non-zero error code on failure.
//  */
// int
// hal_spi_disable(int spi_num)
// {
//     struct nrf54h20_rad_hal_spi *spi = &nrf54h20_rad_hal_spi0;

//     if (spi_num != 0) {
//         return -EINVAL;
//     }

// #if MYNEWT_VAL(SPI_0_MASTER)
//     NRF_SPIM0_NS->INTENCLR = NRF_SPI_IRQ_DISABLE_ALL;

//     if (spi->spi_xfr_flag) {
//         hal_spi_master_stop_transfer();
//         spi->spi_xfr_flag = 0;
//     }
//     NRF_SPIM0_NS->ENABLE = 0;
// #endif

// #if MYNEWT_VAL(SPI_0_SLAVE)
//     NRF_SPIS0_NS->INTENCLR = NRF_SPI_IRQ_DISABLE_ALL;
//     NRF_SPIS0_NS->EVENTS_END = 0;
//     NRF_SPIS0_NS->EVENTS_ACQUIRED = 0;
//     NRF_SPIS0_NS->ENABLE = 0;
//     spi->slave_state = HAL_SPI_SLAVE_STATE_IDLE;
// #endif

//     spi->nhs_txbuf = NULL;
//     spi->nhs_rxbuf = NULL;
//     spi->nhs_buflen = 0;
//     spi->nhs_bytes_txd = 0;

//     return 0;
// }

// /**
//  * Sets the txrx callback (executed at interrupt context) when the
//  * buffer is transferred by the master or the slave using the non-blocking API.
//  * Cannot be called when the spi is enabled. This callback will also be called
//  * when chip select is de-asserted on the slave.
//  *
//  * NOTE: This callback is only used for the non-blocking interface and must
//  * be called prior to using the non-blocking API.
//  *
//  * @param spi_num   SPI interface on which to set callback
//  * @param txrx      Callback function
//  * @param arg       Argument to be passed to callback function
//  *
//  * @return int 0 on success, non-zero error code on failure.
//  */
// int
// hal_spi_set_txrx_cb(int spi_num, hal_spi_txrx_cb txrx_cb, void *arg)
// {
//     struct nrf54h20_rad_hal_spi *spi = &nrf54h20_rad_hal_spi0;

//     if (spi_num != 0) {
//         return EINVAL;
//     }

// #if MYNEWT_VAL(SPI_0_MASTER)
//     if (NRF_SPIM0_NS->ENABLE != 0) {
//         return EINVAL;
//     }
// #endif

// #if MYNEWT_VAL(SPI_0_SLAVE)
//     if (NRF_SPIS0_NS->ENABLE != 0) {
//         return EINVAL;
//     }
// #endif

//     spi->txrx_cb_func = txrx_cb;
//     spi->txrx_cb_arg = arg;

//     return 0;
// }

// /**
//  * Non-blocking interface to send a buffer and store received values. Can be
//  * used for both master and slave SPI types. The user must configure the
//  * callback (using hal_spi_set_txrx_cb); the txrx callback is executed at
//  * interrupt context when the buffer is sent.
//  *
//  * The transmit and receive buffers are either arrays of 8-bit (uint8_t)
//  * values or 16-bit values depending on whether the spi is configured for 8 bit
//  * data or more than 8 bits per value. The 'cnt' parameter is the number of
//  * 8-bit or 16-bit values. Thus, if 'cnt' is 10, txbuf/rxbuf would point to an
//  * array of size 10 (in bytes) if the SPI is using 8-bit data; otherwise
//  * txbuf/rxbuf would point to an array of size 20 bytes (ten, uint16_t values).
//  *
//  * NOTE: these buffers are in the native endian-ness of the platform.
//  *
//  *     MASTER: master sends all the values in the buffer and stores the
//  *             stores the values in the receive buffer if rxbuf is not NULL.
//  *             The txbuf parameter cannot be NULL
//  *     SLAVE: Slave "preloads" the data to be sent to the master (values
//  *            stored in txbuf) and places received data from master in rxbuf
//  *            (if not NULL). The txrx callback occurs when len values are
//  *            transferred or master de-asserts chip select. If txbuf is NULL,
//  *            the slave transfers its default byte. Both rxbuf and txbuf cannot
//  *            be NULL.
//  *
//  * @param spi_num   SPI interface to use
//  * @param txbuf     Pointer to buffer where values to transmit are stored.
//  * @param rxbuf     Pointer to buffer to store values received from peer.
//  * @param cnt       Number of 8-bit or 16-bit values to be transferred.
//  *
//  * @return int 0 on success, non-zero error code on failure.
//  */
// int
// hal_spi_txrx_noblock(int spi_num, void *txbuf, void *rxbuf, int len)
// {
//     struct nrf54h20_rad_hal_spi *spi = &nrf54h20_rad_hal_spi0;

//     if (spi_num != 0 || (spi->txrx_cb_func == NULL) || (len == 0) || !nrfx_is_in_ram(txbuf)) {
//         return EINVAL;
//     }

//     if (rxbuf != NULL && !nrfx_is_in_ram(rxbuf)) {
//         return EINVAL;
//     }

// #if MYNEWT_VAL(SPI_0_MASTER)
//     /* Must have a txbuf for master! */
//     if (txbuf == NULL) {
//         return EINVAL;
//     }

//     /* Not allowed if transfer in progress */
//     if (spi->spi_xfr_flag) {
//         return EBUSY;
//     }

//     NRF_SPIM0_NS->INTENCLR = SPIM_INTENCLR_END_Msk;
//     spi->spi_xfr_flag = 1;

//     /* Set internal data structure information */
//     spi->nhs_bytes_txd = 0;
//     spi->nhs_buflen = len;
//     spi->nhs_txbuf = txbuf;

//     len = min(SPIM_TXD_MAXCNT_MAX, len);

//     /* Set chip registers */
//     NRF_SPIM0_NS->TXD.PTR = (uint32_t)txbuf;
//     NRF_SPIM0_NS->TXD.MAXCNT = len;

//     /* If no rxbuf, we need to set rxbuf and maxcnt to 1 */
//     spi->nhs_rxbuf = rxbuf;
//     if (rxbuf == NULL) {
//         NRF_SPIM0_NS->RXD.PTR = (uint32_t)&spi->dummy_rx;
//         NRF_SPIM0_NS->RXD.MAXCNT = 1;
//     } else {
//         NRF_SPIM0_NS->RXD.PTR = (uint32_t)rxbuf;
//         NRF_SPIM0_NS->RXD.MAXCNT = len;
//     }

//     NRF_SPIM0_NS->EVENTS_END = 0;
//     NRF_SPIM0_NS->EVENTS_STOPPED = 0;
//     NRF_SPIM0_NS->TASKS_START = 1;
//     NRF_SPIM0_NS->INTENSET = SPIM_INTENSET_END_Msk;
// #endif

// #if MYNEWT_VAL(SPI_0_SLAVE)
//     /* Must have txbuf or rxbuf */
//     if ((txbuf == NULL) && (rxbuf == NULL)) {
//         return EINVAL;
//     }

//     /* XXX: what to do here? */
//     if (len > 255) {
//         return EINVAL;
//     }

//     /*
//      * Ready the slave for a transfer. Do not allow this to be called
//      * if the slave has already been readied or is requesting the
//      * semaphore
//      */
//     if (spi->slave_state != HAL_SPI_SLAVE_STATE_IDLE) {
//         return EBUSY;
//     }

//     spi->nhs_rxbuf = rxbuf;
//     spi->nhs_txbuf = txbuf;
//     spi->nhs_buflen = len;
//     spi->slave_state = HAL_SPI_SLAVE_STATE_ACQ_SEM;
//     NRF_SPIS0_NS->TASKS_ACQUIRE = 1;
// #endif

//     return 0;
// }

// /**
//  * Sets the default value transferred by the slave. Not valid for master
//  *
//  * @param spi_num SPI interface to use
//  *
//  * @return int 0 on success, non-zero error code on failure.
//  */
// int
// hal_spi_slave_set_def_tx_val(int spi_num, uint16_t val)
// {
// #if MYNEWT_VAL(SPI_0_SLAVE)
//     if (spi_num != 0){
//         return EINVAL;
//     }

//     NRF_SPIS0_NS->DEF = (uint8_t)val;
//     NRF_SPIS0_NS->ORC = (uint8_t)val;

//     return 0;
// #else
//     return EINVAL;
// #endif
// }

// /**
//  * This aborts the current transfer but keeps the spi enabled.
//  *
//  * @param spi_num   SPI interface on which transfer should be aborted.
//  *
//  * @return int 0 on success, non-zero error code on failure.
//  *
//  * NOTE: does not return an error if no transfer was in progress.
//  */
// int
// hal_spi_abort(int spi_num)
// {
//     if (spi_num != 0) {
//         return EINVAL;
//     }

// #if MYNEWT_VAL(SPI_0_MASTER)
//     if (nrf54h20_rad_hal_spi0.spi_xfr_flag) {
//         NRF_SPIM0_NS->INTENCLR = NRF_SPI_IRQ_DISABLE_ALL;
//         hal_spi_master_stop_transfer();
//         nrf54h20_rad_hal_spi0.spi_xfr_flag = 0;
//         NRF_SPIM0_NS->INTENSET = SPIM_INTENSET_END_Msk;
//     }
// #endif

// #if MYNEWT_VAL(SPI_0_SLAVE)
//     /* Only way I can see doing this is to disable, then re-enable */
//     rc = hal_spi_disable(spi_num);
//     if (rc) {
//         return rc;
//     }
//     rc = hal_spi_enable(spi_num);
//     if (rc) {
//         return rc;
//     }
// #endif

//     return 0;
// }

// #endif
