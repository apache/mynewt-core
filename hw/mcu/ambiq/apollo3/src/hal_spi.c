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
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <stdbool.h>
#include "os/mynewt.h"
#include "hal/hal_spi.h"
#include "mcu/hal_apollo3.h"
#include "mcu/cmsis_nvic.h"

#include "am_mcu_apollo.h"

/* Prevent CMSIS from breaking apollo3 macros. */
#undef GPIO
#undef IOSLAVE
#undef CLKGEN

#define SPI_MASTER_ANY_ENABLED ( \
    MYNEWT_VAL(SPI_0_MASTER) ||  \
    MYNEWT_VAL(SPI_1_MASTER) ||  \
    MYNEWT_VAL(SPI_2_MASTER) ||  \
    MYNEWT_VAL(SPI_3_MASTER) ||  \
    MYNEWT_VAL(SPI_4_MASTER) ||  \
    MYNEWT_VAL(SPI_5_MASTER))

#define SPI_SLAVE_ANY_ENABLED ( \
    MYNEWT_VAL(SPI_0_SLAVE))

#if SPI_MASTER_ANY_ENABLED || SPI_SLAVE_ANY_ENABLED

struct apollo3_spi {
    uint8_t spi_num;
    uint8_t spi_type;
    int8_t  ss_pin[4];
    void *spi_handle;

    int8_t cur_ss_pin;
    bool  cont_tx;

    hal_spi_txrx_cb txrx_cb_func;
    void *txrx_cb_arg;
};

#if MYNEWT_VAL(SPI_0_MASTER)
static struct apollo3_spi apollo3_spi0_master;
#endif
#if MYNEWT_VAL(SPI_1_MASTER)
static struct apollo3_spi apollo3_spi1_master;
#endif
#if MYNEWT_VAL(SPI_2_MASTER)
static struct apollo3_spi apollo3_spi2_master;
#endif
#if MYNEWT_VAL(SPI_3_MASTER)
static struct apollo3_spi apollo3_spi3_master;
#endif
#if MYNEWT_VAL(SPI_4_MASTER)
static struct apollo3_spi apollo3_spi4_master;
#endif
#if MYNEWT_VAL(SPI_5_MASTER)
static struct apollo3_spi apollo3_spi5_master;
#endif

static am_hal_iom_config_t g_iom_spi_config =
{
    .eInterfaceMode = AM_HAL_IOM_SPI_MODE,
    .ui32ClockFreq = AM_HAL_IOM_4MHZ,
    .eSpiMode = AM_HAL_IOM_SPI_MODE_0, /* CPOL = 0; CPHA = 0 */
};

#define AM_IOS_TX_BUFSIZE_MAX   1023
uint8_t g_tx_fifo_buffer[AM_IOS_TX_BUFSIZE_MAX];
static am_hal_ios_config_t g_ios_spi_config =
{
    /* Configure the IOS in SPI mode. */
    .ui32InterfaceSelect = AM_HAL_IOS_USE_SPI,

    /* Eliminate the "read-only" section, so an external host can use the entire "direct write" section. */
    .ui32ROBase = 0x78,

    /* Making the "FIFO" section as big as possible. */
    .ui32FIFOBase = 0x80,

    /* We don't need any RAM space, so extend the FIFO all the way to the end of the LRAM. */
    .ui32RAMBase = 0x100,

    /* FIFO Threshold - set to half the size */
    .ui32FIFOThreshold = 0x20,

    .pui8SRAMBuffer = g_tx_fifo_buffer,
    .ui32SRAMBufferCap = AM_IOS_TX_BUFSIZE_MAX,
};

static struct apollo3_spi *
apollo3_spi_resolve(int spi_num)
{
    switch (spi_num) {
#if MYNEWT_VAL(SPI_0_MASTER)
    case 0:
        return &apollo3_spi0_master;
#endif
#if MYNEWT_VAL(SPI_1_MASTER)
    case 1:
        return &apollo3_spi1_master;
#endif
#if MYNEWT_VAL(SPI_2_MASTER)
    case 2:
        return &apollo3_spi2_master;
#endif
#if MYNEWT_VAL(SPI_3_MASTER)
    case 3:
        return &apollo3_spi3_master;
#endif
#if MYNEWT_VAL(SPI_4_MASTER)
    case 4:
        return &apollo3_spi4_master;
#endif
#if MYNEWT_VAL(SPI_5_MASTER)
    case 5:
        return &apollo3_spi5_master;
#endif
    default:
        return NULL;
    }
}

static uint32_t
apollo3_spi_data_mode(int spi_mode)
{
    switch (spi_mode) {
    case HAL_SPI_MODE0:     return AM_HAL_IOM_SPI_MODE_0;
    case HAL_SPI_MODE1:     return AM_HAL_IOM_SPI_MODE_1;
    case HAL_SPI_MODE2:     return AM_HAL_IOM_SPI_MODE_2;
    case HAL_SPI_MODE3:     return AM_HAL_IOM_SPI_MODE_3;
    default:                return -1;
    }
}

static int
hal_spi_config_master(int spi_num, const struct hal_spi_settings *settings)
{
    am_hal_iom_config_t sdk_config;
    struct apollo3_spi *spi;

    spi = apollo3_spi_resolve(spi_num);
    if (spi == NULL) {
        return SYS_EINVAL;
    }

    sdk_config.eInterfaceMode = AM_HAL_IOM_SPI_MODE;
    sdk_config.ui32ClockFreq = settings->baudrate;
    sdk_config.eSpiMode = (am_hal_iom_spi_mode_e)apollo3_spi_data_mode(settings->data_mode);
    am_hal_iom_configure(spi->spi_handle, &sdk_config);

    return 0;
}

static int
hal_spi_config_slave(int spi_num, const struct hal_spi_settings *settings)
{
    struct apollo3_spi *spi;

    spi = apollo3_spi_resolve(spi_num);
    if (spi == NULL) {
        return SYS_EINVAL;
    }

    am_hal_ios_configure(spi->spi_handle, &g_ios_spi_config);

    return 0;
}

/*  | spi:cfg   | sck   | miso  | mosi  |
 *  |-----------+-------+-------+-------|
 *  | 0:1       | 5     | 6     | 7     |
 *  | 1:1       | 8     | 9     | 10    |
 *  | 2:5       | 27    | 28    | 25    |
 *  | 3:5       | 42    | 43    | 38    |
 *  | 4:5       | 39    | 40    | 44    |
 *  | 5:5       | 48    | 49    | 47    |
 */
static int
hal_spi_pin_config_master(int spi_num, const struct apollo3_spi_cfg *pins)
{
#if SPI_MASTER_ANY_ENABLED
    const int8_t miso = pins->miso_pin;
    const int8_t mosi = pins->mosi_pin;
    const int8_t sck = pins->sck_pin;
#endif

    switch (spi_num) {
#if MYNEWT_VAL(SPI_0_MASTER)
    case 0:
        if (sck == 5 && miso == 6 && mosi == 7) {
            return 1;
        } else {
            return -1;
        }
#endif
#if MYNEWT_VAL(SPI_1_MASTER)
    case 1:
        if (sck == 8 && miso == 9 && mosi == 10) {
            return 1;
        } else {
            return -1;
        }
#endif
#if MYNEWT_VAL(SPI_2_MASTER)
    case 2:
        if (sck == 27 && miso == 25 && mosi == 28) {
            return 5;
        } else {
            return -1;
        }
#endif
#if MYNEWT_VAL(SPI_3_MASTER)
    case 3:
        if (sck == 42 && miso == 43 && mosi == 38) {
            return 5;
        } else {
            return -1;
        }
#endif
#if MYNEWT_VAL(SPI_4_MASTER)
    case 4:
        if (sck == 39 && miso == 40 && mosi == 44) {
            return 5;
        } else {
            return -1;
        }
#endif
#if MYNEWT_VAL(SPI_5_MASTER)
    case 5:
        if (sck == 48 && miso == 49 && mosi == 47) {
            return 5;
        } else {
            return -1;
        }
#endif
    default:
        return -1;
    }
}

static int
hal_spi_pin_config_slave(int spi_num, const struct apollo3_spi_cfg *pins)
{
#if SPI_SLAVE_ANY_ENABLED
    const int8_t miso = pins->miso_pin;
    const int8_t mosi = pins->mosi_pin;
    const int8_t sck = pins->sck_pin;
#endif

    switch (spi_num) {
#if MYNEWT_VAL(SPI_0_SLAVE)
    case 0:
        if (sck == 0 && miso == 2 && mosi == 1) {
            return 1;
        } else {
            return -1;
        }
#endif
    default:
        return -1;
    }
}

static int
hal_spi_ss_pin_config_master(int spi_num, int8_t ss_pin)
{
    switch (ss_pin) {
    case 7:
    case 41:
      return 0;
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
    case 16:
    case 17:
    case 18:
    case 19:
    case 20:
    case 21:
    case 22:
    case 23:
    case 24:
    case 25:
    case 26:
    case 27:
    case 28:
    case 29:
    case 30:
    case 31:
    case 32:
    case 33:
    case 34:
    case 35:
    case 36:
    case 37:
    case 38:
    case 42:
    case 43:
    case 44:
    case 45:
    case 46:
    case 47:
    case 48:
    case 49:
      return 1;
    case 3:
    case 4:
    case 8:
    case 9:
    case 10:
      return 2;
    case 0:
    case 1:
    case 2:
      return 7;
    default:
      return -1;
    }
}

static int
hal_spi_ss_pin_config_slave(int spi_num, int8_t ss_pin)
{
    switch (spi_num) {
#if MYNEWT_VAL(SPI_0_SLAVE)
    case 0:
        switch (ss_pin) {
          case 3:
            return 1;
          default:
            return -1;
        }
#endif
    default:
      return -1;
    }
}

static uint32_t
hal_spi_get_uNCE(int spi_num, int ss_pin) {
    switch(spi_num) {
#if MYNEWT_VAL(SPI_0_MASTER)
    case 0:
        switch (ss_pin) {
        case 8:
        case 11:
        case 16:
        case 23:
        case 31:
        case 35:
        case 42:
          return 0;
        case 13:
        case 17:
        case 24:
        case 32:
        case 41:
        case 43:
        case 47:
          return 1;
        case 1:
        case 14:
        case 18:
        case 25:
        case 33:
        case 37:
        case 44:
        case 48:
          return 2;
        case 15:
        case 19:
        case 22:
        case 30:
        case 34:
        case 38:
        case 49:
          return 3;
        default:
          return -1;
        }
#endif
#if MYNEWT_VAL(SPI_1_MASTER)
    case 1:
        switch (ss_pin) {
        case 11:
        case 16:
        case 23:
        case 27:
        case 31:
        case 35:
        case 42:
          return 0;
        case 4:
        case 17:
        case 24:
        case 32:
        case 41:
        case 43:
        case 47:
          return 1;
        case 1:
        case 14:
        case 18:
        case 29:
        case 33:
        case 44:
        case 48:
          return 2;
        case 0:
        case 15:
        case 19:
        case 26:
        case 34:
        case 38:
        case 45:
        case 49:
          return 3;
        default:
          return -1;
        }
#endif
#if MYNEWT_VAL(SPI_2_MASTER)
    case 2:
        switch (ss_pin) {
        case 3:
        case 11:
        case 16:
        case 23:
        case 31:
        case 35:
        case 42:
          return 0;
        case 2:
        case 17:
        case 20:
        case 24:
        case 32:
        case 41:
        case 43:
        case 47:
          return 1;
        case 1:
        case 14:
        case 18:
        case 21:
        case 33:
        case 44:
        case 48:
          return 2;
        case 9:
        case 15:
        case 34:
        case 38:
        case 49:
          return 3;
        default:
          return -1;
        }
#endif
#if MYNEWT_VAL(SPI_3_MASTER)
    case 3:
    switch (ss_pin) {
        case 3:
        case 8:
        case 11:
        case 12:
        case 27:
        case 35:
        case 46:
          return 0;
        case 4:
        case 7:
        case 13:
        case 20:
        case 28:
        case 36:
        case 47:
          return 1;
        case 0:
        case 10:
        case 18:
        case 21:
        case 25:
        case 29:
        case 37:
        case 48:
          return 2;
        case 2:
        case 9:
        case 19:
        case 22:
        case 26:
        case 30:
        case 34:
        case 45:
          return 3;
        default:
          return -1;
    }
#endif
#if MYNEWT_VAL(SPI_4_MASTER)
    case 4:
    switch (ss_pin) {
    case 3:
    case 8:
    case 12:
    case 23:
    case 27:
    case 31:
    case 46:
      return 0;
    case 4:
    case 7:
    case 13:
    case 17:
    case 20:
    case 28:
    case 36:
      return 1;
    case 0:
    case 10:
    case 14:
    case 21:
    case 25:
    case 29:
    case 37:
      return 2;
    case 2:
    case 9:
    case 22:
    case 26:
    case 30:
    case 45:
    case 49:
      return 3;
    default:
      return -1;
    }
#endif
#if MYNEWT_VAL(SPI_5_MASTER)
    case 5:
    switch (ss_pin) {
    case 3:
    case 8:
    case 12:
    case 16:
    case 27:
    case 42:
    case 46:
      return 0;
    case 4:
    case 7:
    case 13:
    case 20:
    case 24:
    case 28:
    case 36:
      return 1;
    case 0:
    case 10:
    case 21:
    case 25:
    case 29:
    case 33:
    case 37:
    case 44:
      return 2;
    case 2:
    case 9:
    case 22:
    case 26:
    case 30:
    case 38:
    case 45:
      return 3;
    default:
      return -1;
    }
#endif
    default:
        return -1;
    }
}

static int
hal_spi_pin_config(int spi_num, int master, const struct apollo3_spi_cfg *pins)
{
    if (master) {
        return hal_spi_pin_config_master(spi_num, pins);
    } else {
        return hal_spi_pin_config_slave(spi_num, pins);
    }
}

static int
hal_spi_ss_pin_config(int spi_num, int master, int8_t ss_pin)
{
    if (master) {
        return hal_spi_ss_pin_config_master(spi_num, ss_pin);
    } else {
        return hal_spi_ss_pin_config_slave(spi_num, ss_pin);
    }
}

static int
hal_spi_ss_pin_init(struct apollo3_spi *spi, int8_t ss_index, int8_t ss_pin)
{
    am_hal_gpio_pincfg_t spi_ss_cfg;
    int ss_pin_func_sel;

    if (spi->ss_pin[ss_index] != ss_pin) {
        ss_pin_func_sel = hal_spi_ss_pin_config(spi->spi_num, 1, ss_pin);
        if (ss_pin_func_sel != -1 && ss_index != -1) {
            memset(&spi_ss_cfg, 0x0, sizeof(am_hal_gpio_pincfg_t));
            spi_ss_cfg.uFuncSel = ss_pin_func_sel;
            spi_ss_cfg.eDriveStrength = AM_HAL_GPIO_PIN_DRIVESTRENGTH_12MA;
            spi_ss_cfg.eGPOutcfg = AM_HAL_GPIO_PIN_OUTCFG_PUSHPULL;
            spi_ss_cfg.eGPInput = AM_HAL_GPIO_PIN_INPUT_NONE;
            spi_ss_cfg.eIntDir = AM_HAL_GPIO_PIN_INTDIR_LO2HI;
            spi_ss_cfg.uIOMnum = spi->spi_num;
            spi_ss_cfg.uNCE = ss_index;
            spi_ss_cfg.eCEpol = AM_HAL_GPIO_PIN_CEPOL_ACTIVELOW;
            if (am_hal_gpio_pinconfig(ss_pin, spi_ss_cfg) != AM_HAL_STATUS_SUCCESS) {
                return SYS_EINVAL;
            }
            spi->ss_pin[ss_index] = ss_pin;
        } else {
            return SYS_EINVAL;
        }
    }

    return 0;
}

static int
hal_spi_init_master(int spi_num, const struct apollo3_spi_cfg *cfg)
{
    struct apollo3_spi *spi;
    int spi_pin_func_sel, ss_pin_func_sel, i;
    am_hal_gpio_pincfg_t spi_sck_cfg, spi_miso_cfg, spi_mosi_cfg, spi_ss_cfg;

    spi = apollo3_spi_resolve(spi_num);
    if (spi == NULL) {
        return SYS_EINVAL;
    }
    memset(spi, 0, sizeof *spi);

    /* Initialize the IOM. */
    if (am_hal_iom_initialize(spi_num, &(spi->spi_handle)) != AM_HAL_STATUS_SUCCESS) {
        return SYS_EINVAL;
    }
    
    if (am_hal_iom_power_ctrl(spi->spi_handle, AM_HAL_SYSCTRL_WAKE, false) != AM_HAL_STATUS_SUCCESS) {
        return SYS_EINVAL;
    }

    /* Set the required configuration settings for the IOM. */
    if (am_hal_iom_configure(spi->spi_handle, &g_iom_spi_config) != AM_HAL_STATUS_SUCCESS) {
        return SYS_EINVAL;
    }

    /* Configure the IOM pins. */
    spi_pin_func_sel = hal_spi_pin_config(spi_num, 1, cfg);
    if (spi_pin_func_sel == -1) {
        return SYS_EINVAL;
    }

    for (i = 0; i < 4; i++) {
        ss_pin_func_sel = hal_spi_ss_pin_config(spi_num, 1, cfg->ss_pin[i]);
        if (ss_pin_func_sel != -1) {
            memset(&spi_ss_cfg, 0x0, sizeof(am_hal_gpio_pincfg_t));
            spi_ss_cfg.uFuncSel = ss_pin_func_sel;
            spi_ss_cfg.eDriveStrength = AM_HAL_GPIO_PIN_DRIVESTRENGTH_12MA;
            spi_ss_cfg.eGPOutcfg = AM_HAL_GPIO_PIN_OUTCFG_PUSHPULL;
            spi_ss_cfg.eGPInput = AM_HAL_GPIO_PIN_INPUT_NONE;
            spi_ss_cfg.eIntDir = AM_HAL_GPIO_PIN_INTDIR_LO2HI;
            spi_ss_cfg.uIOMnum = spi_num;
            spi_ss_cfg.uNCE = hal_spi_get_uNCE(spi_num, cfg->ss_pin[i]);
            spi_ss_cfg.eCEpol = AM_HAL_GPIO_PIN_CEPOL_ACTIVELOW;
            if (am_hal_gpio_pinconfig(cfg->ss_pin[i], spi_ss_cfg) != AM_HAL_STATUS_SUCCESS) {
                return SYS_EINVAL;
            }
            spi->ss_pin[i] = cfg->ss_pin[i];
            spi->cur_ss_pin = cfg->ss_pin[i];
        } else {
            spi->ss_pin[i] = -1;
        }
    }

    memset(&spi_sck_cfg, 0x0, sizeof(am_hal_gpio_pincfg_t));
    spi_sck_cfg.uFuncSel = spi_pin_func_sel;
    spi_sck_cfg.eDriveStrength = AM_HAL_GPIO_PIN_DRIVESTRENGTH_12MA;
    spi_sck_cfg.uIOMnum = spi_num;
    if (am_hal_gpio_pinconfig(cfg->sck_pin, spi_sck_cfg) != AM_HAL_STATUS_SUCCESS){
        return SYS_EINVAL;
    }

    memset(&spi_miso_cfg, 0x0, sizeof(am_hal_gpio_pincfg_t));
    spi_miso_cfg.uFuncSel = spi_pin_func_sel;
    spi_miso_cfg.uIOMnum = spi_num;
    if (am_hal_gpio_pinconfig(cfg->miso_pin, spi_miso_cfg) != AM_HAL_STATUS_SUCCESS) {
        return SYS_EINVAL;
    }

    memset(&spi_mosi_cfg, 0x0, sizeof(am_hal_gpio_pincfg_t));
    spi_mosi_cfg.uFuncSel = spi_pin_func_sel;
    spi_mosi_cfg.eDriveStrength = AM_HAL_GPIO_PIN_DRIVESTRENGTH_12MA;
    spi_mosi_cfg.uIOMnum = spi_num;
    if (am_hal_gpio_pinconfig(cfg->mosi_pin, spi_mosi_cfg) != AM_HAL_STATUS_SUCCESS) {
        return SYS_EINVAL;
    }

    /* Enable the IOM. */
    hal_spi_enable(spi_num);

    spi->spi_num = spi_num;
    spi->cont_tx = false;
    spi->spi_type = HAL_SPI_TYPE_MASTER;

    return 0;
}

static int
hal_spi_init_slave(int spi_num, struct apollo3_spi_cfg *cfg)
{
    return SYS_ERANGE;
}

int apollo3_spi_set_ss_pin(int spi_num, int8_t ss_pin)
{
  struct apollo3_spi *spi;

  spi = apollo3_spi_resolve(spi_num);
  if (spi == NULL) {
      return SYS_EINVAL;
  }

  spi->cur_ss_pin = ss_pin;

  return 0;
}

int apollo3_spi_set_continuation(int spi_num, bool cont)
{
  struct apollo3_spi *spi;

  spi = apollo3_spi_resolve(spi_num);
  if (spi == NULL) {
      return SYS_EINVAL;
  }

  spi->cont_tx = cont;

  return 0;
}

/**
 * Initialize the SPI, given by spi_num.
 *
 * @param spi_num The number of the SPI to initialize
 * @param cfg HW/MCU specific configuration,
 *            passed to the underlying implementation, providing extra
 *            configuration.
 * @param spi_type SPI type (master or slave)
 *
 * @return int 0 on success, non-zero error code on failure.
 */
int
hal_spi_init(int spi_num, void *cfg, uint8_t spi_type)
{
    int rc;

    if (cfg == NULL) {
        return SYS_EINVAL;
    }

    switch (spi_type) {
    case HAL_SPI_TYPE_MASTER:
        rc = hal_spi_init_master(spi_num, cfg);
        if (rc != 0) {
            return rc;
        }
        break;

    case HAL_SPI_TYPE_SLAVE:
        rc = hal_spi_init_slave(spi_num, cfg);
        if (rc != 0) {
            return rc;
        }
        break;

    default:
        return SYS_EINVAL;
    }

    return 0;
}

int
hal_spi_init_hw(uint8_t spi_num, uint8_t spi_type,
                const struct hal_spi_hw_settings *cfg)
{
    int i;
    struct apollo3_spi_cfg hal_cfg;

    hal_cfg.sck_pin = cfg->pin_sck;
    hal_cfg.mosi_pin = cfg->pin_mosi;
    hal_cfg.miso_pin = cfg->pin_miso;
    for (i = 0; i < 4; i++) {
        hal_cfg.ss_pin[i] = -1;
    }
    if (spi_type == HAL_SPI_TYPE_MASTER) {
        int spi_index = hal_spi_get_uNCE(spi_num, cfg->pin_ss);
        if (spi_index != -1) {
            hal_cfg.ss_pin[spi_index] = cfg->pin_ss;
        }
    } else {
        hal_cfg.ss_pin[0] = cfg->pin_ss;
    }

    return hal_spi_init(spi_num, &hal_cfg, spi_type);
}

/**
 * Configure the spi. Must be called after the spi is initialized (after
 * hal_spi_init is called) and when the spi is disabled (user must call
 * hal_spi_disable if the spi has been enabled through hal_spi_enable prior
 * to calling this function). Can also be used to reconfigure an initialized
 * SPI (assuming it is disabled as described previously).
 *
 * @param spi_num The number of the SPI to configure.
 * @param psettings The settings to configure this SPI with
 *
 * @return int 0 on success, non-zero error code on failure.
 */
int
hal_spi_config(int spi_num, struct hal_spi_settings *settings)
{
    const struct apollo3_spi *spi;
    int rc;

    spi = apollo3_spi_resolve(spi_num);
    if (spi == NULL) {
        return SYS_EINVAL;
    }

    if (spi->spi_type == HAL_SPI_TYPE_MASTER) {
        rc = hal_spi_config_master(spi_num, settings);
    } else {
        rc = hal_spi_config_slave(spi_num, settings);
    }

    return rc;
}

/**
 * Enables the SPI. This does not start a transmit or receive operation;
 * it is used for power mgmt. Cannot be called when a SPI transfer is in
 * progress.
 *
 * @param spi_num
 *
 * @return int 0 on success, non-zero error code on failure.
 */
int
hal_spi_enable(int spi_num)
{
    struct apollo3_spi *spi;

    spi = apollo3_spi_resolve(spi_num);
    if (spi == NULL) {
        return SYS_EINVAL;
    }
    am_hal_iom_enable(spi->spi_handle);

    return 0;
}

/**
 * Disables the SPI. Used for power mgmt. It will halt any current SPI transfers
 * in progress.
 *
 * @param spi_num
 *
 * @return int 0 on success, non-zero error code on failure.
 */
int
hal_spi_disable(int spi_num)
{
    struct apollo3_spi *spi;

    spi = apollo3_spi_resolve(spi_num);
    if (spi == NULL) {
        return SYS_EINVAL;
    }
    am_hal_iom_disable(spi->spi_handle);

    return 0;
}

/**
 * Blocking call to send a value on the SPI. Returns the value received from
 * the SPI slave.
 *
 * MASTER: Sends the value and returns the received value from the slave.
 * SLAVE: Invalid API. Returns 0xFFFF
 *
 * @param spi_num   Spi interface to use
 * @param val       Value to send
 *
 * @return uint16_t Value received on SPI interface from slave. Returns 0xFFFF
 * if called when the SPI is configured to be a slave
 */
uint16_t
hal_spi_tx_val(int spi_num, uint16_t val)
{
    am_hal_iom_transfer_t transaction;
    struct apollo3_spi *spi;
    uint32_t tx_buf = val;
    uint32_t rx_buf = 0xffff;
    int ss_pin_index;

    spi = apollo3_spi_resolve(spi_num);
    if (spi == NULL) {
        return SYS_EINVAL;
    }

    ss_pin_index = hal_spi_get_uNCE(spi_num, spi->cur_ss_pin);
    if (hal_spi_ss_pin_init(spi, ss_pin_index, spi->cur_ss_pin) != 0) {
        return SYS_EINVAL;
    }

    memset(&transaction, 0x0, sizeof(am_hal_iom_transfer_t ));
    transaction.eDirection      = AM_HAL_IOM_FULLDUPLEX;
    transaction.ui32NumBytes    = sizeof(val);
    transaction.pui32TxBuffer   = &tx_buf;
    transaction.pui32RxBuffer   = &rx_buf;
    transaction.bContinue       = spi->cont_tx;

    transaction.uPeerInfo.ui32SpiChipSelect = ss_pin_index;
    
    if (am_hal_iom_spi_blocking_fullduplex(spi->spi_handle, &transaction) != AM_HAL_STATUS_SUCCESS) {
        return 0xffff;
    }

    return rx_buf;
}

/**
 * Sets the txrx callback (executed at interrupt context) when the
 * buffer is transferred by the master or the slave using the non-blocking API.
 * Cannot be called when the spi is enabled. This callback will also be called
 * when chip select is de-asserted on the slave.
 *
 * NOTE: This callback is only used for the non-blocking interface and must
 * be called prior to using the non-blocking API.
 *
 * @param spi_num   SPI interface on which to set callback
 * @param txrx      Callback function
 * @param arg       Argument to be passed to callback function
 *
 * @return int 0 on success, non-zero error code on failure.
 */
int
hal_spi_set_txrx_cb(int spi_num, hal_spi_txrx_cb txrx_cb, void *arg)
{
    /* Not implemented */
    return SYS_ERANGE;
}

/**
 * Blocking interface to send a buffer and store the received values from the
 * slave. The transmit and receive buffers are either arrays of 8-bit (uint8_t)
 * values or 16-bit values depending on whether the spi is configured for 8 bit
 * data or more than 8 bits per value. The 'cnt' parameter is the number of
 * 8-bit or 16-bit values. Thus, if 'cnt' is 10, txbuf/rxbuf would point to an
 * array of size 10 (in bytes) if the SPI is using 8-bit data; otherwise
 * txbuf/rxbuf would point to an array of size 20 bytes (ten, uint16_t values).
 *
 * NOTE: these buffers are in the native endian-ness of the platform.
 *
 *     MASTER: master sends all the values in the buffer and stores the
 *             stores the values in the receive buffer if rxbuf is not NULL.
 *             The txbuf parameter cannot be NULL.
 *     SLAVE: cannot be called for a slave; returns -1
 *
 * @param spi_num   SPI interface to use
 * @param txbuf     Pointer to buffer where values to transmit are stored.
 * @param rxbuf     Pointer to buffer to store values received from peer.
 * @param cnt       Number of 8-bit or 16-bit values to be transferred.
 *
 * @return int 0 on success, non-zero error code on failure.
 */
int
hal_spi_txrx(int spi_num, void *txbuf, void *rxbuf, int num_bytes)
{
    am_hal_iom_transfer_t transaction;
    struct apollo3_spi *spi;
    int ss_pin_index;

    /* Cannot operate if both tx and rx buf are NULL */
    if(txbuf == NULL && rxbuf == NULL) {
        return SYS_EINVAL;
    }

    spi = apollo3_spi_resolve(spi_num);
    if (spi == NULL) {
        return SYS_EINVAL;
    }

    ss_pin_index = hal_spi_get_uNCE(spi_num, spi->cur_ss_pin);
    if (hal_spi_ss_pin_init(spi, ss_pin_index, spi->cur_ss_pin) != 0) {
        return SYS_EINVAL;
    }

    memset(&transaction, 0x0, sizeof(am_hal_iom_transfer_t ));
    transaction.ui32NumBytes    = num_bytes;
    transaction.bContinue       = spi->cont_tx;
    transaction.uPeerInfo.ui32SpiChipSelect = ss_pin_index;

    if (rxbuf == NULL) {
        /* Perform half duplex tx */
        transaction.eDirection      = AM_HAL_IOM_TX;
        transaction.pui32TxBuffer   = txbuf;

        return am_hal_iom_blocking_transfer(spi->spi_handle, &transaction);
    }
    else if (txbuf == NULL) {
        /* Perform half duplex rx */
        transaction.eDirection      = AM_HAL_IOM_RX;
        transaction.pui32RxBuffer   = rxbuf;

        return am_hal_iom_blocking_transfer(spi->spi_handle, &transaction);
    }
    else {
        /* Perform full duplex txrx */
        transaction.eDirection      = AM_HAL_IOM_FULLDUPLEX;
        transaction.pui32TxBuffer   = txbuf;
        transaction.pui32RxBuffer   = rxbuf;

        return am_hal_iom_spi_blocking_fullduplex(spi->spi_handle, &transaction);
    }
}

int
hal_spi_txrx_noblock(int spi_num, void *txbuf, void *rxbuf, int num_bytes)
{
    /* Not implemented */
    return SYS_ERANGE;
}

/**
 * Sets the default value transferred by the slave. Not valid for master
 *
 * @param spi_num SPI interface to use
 *
 * @return int 0 on success, non-zero error code on failure.
 */
int
hal_spi_slave_set_def_tx_val(int spi_num, uint16_t val)
{
    return SYS_ERANGE;
}

/**
 * This aborts the current transfer but keeps the spi enabled.
 *
 * @param spi_num   SPI interface on which transfer should be aborted.
 *
 * @return int 0 on success, non-zero error code on failure.
 *
 * NOTE: does not return an error if no transfer was in progress.
 */
int
hal_spi_abort(int spi_num)
{
    return SYS_ERANGE;
}

#endif /* SPI_MASTER_ANY_ENABLED || SPI_SLAVE_ANY_ENABLED */
