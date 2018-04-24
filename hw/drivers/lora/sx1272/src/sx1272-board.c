/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
    (C)2013 Semtech

Description: SX1272 driver specific target board functions implementation

License: Revised BSD License, see LICENSE.TXT file include in the project

Maintainer: Miguel Luis and Gregory Cristian
*/

#include <assert.h>
#include "os/mynewt.h"
#include "hal/hal_spi.h"
#include "hal/hal_gpio.h"
#include "bsp/bsp.h"
#include "radio/radio.h"
#include "sx1272.h"
#include "sx1272-board.h"

#if (MYNEWT_VAL(SX1272_HAS_ANT_SW) && MYNEWT_VAL(SX1272_HAS_COMP_ANT_SW))
#error "Cannot have both SX1272_HAS_ANT_SW and SX1272_HAS_COMP_ANT_SW set true"
#endif

/*!
 * Flag used to set the RF switch control pins in low power mode when the radio is not active.
 */
#if (MYNEWT_VAL(SX1272_HAS_ANT_SW) || MYNEWT_VAL(SX1272_HAS_COMP_ANT_SW))
static bool g_radio_is_active = false;
#endif

extern DioIrqHandler *DioIrq[];

/*!
 * Radio driver structure initialization
 */
const struct Radio_s Radio =
{
    SX1272Init,
    SX1272GetStatus,
    SX1272SetModem,
    SX1272SetChannel,
    SX1272IsChannelFree,
    SX1272Random,
    SX1272SetRxConfig,
    SX1272SetTxConfig,
    SX1272CheckRfFrequency,
    SX1272GetTimeOnAir,
    SX1272Send,
    SX1272SetSleep,
    SX1272SetStby,
    SX1272SetRx,
    SX1272StartCad,
    SX1272SetTxContinuousWave,
    SX1272ReadRssi,
    SX1272Write,
    SX1272Read,
    SX1272WriteBuffer,
    SX1272ReadBuffer,
    SX1272SetMaxPayloadLength,
    SX1272SetPublicNetwork,
    SX1272GetWakeupTime
};

void
SX1272IoInit(void)
{
    struct hal_spi_settings spi_settings;
    int rc;

#if MYNEWT_VAL(SX1272_HAS_ANT_SW)
    rc = hal_gpio_init_out(SX1272_RXTX, 0);
    assert(rc == 0);
#endif

    /*
     * XXX: This should really be in the BSP! On the schematic the pins are
     * labeled LORA_FEM_CTX and nLORA_FEM_CTX but here they are RXTX and N_RXTX
     *
     * RXTX high, N_RXTX low = RX
     * RXTX low, N_RXTX high = TX
     *
     * By default, put in RX
     */
#if MYNEWT_VAL(SX1272_HAS_COMP_ANT_SW)
    rc = hal_gpio_init_out(SX1272_RXTX, 1);
    assert(rc == 0);
    rc = hal_gpio_init_out(SX1272_N_RXTX, 0);
    assert(rc == 0);
#endif

    rc = hal_gpio_init_out(RADIO_NSS, 1);
    assert(rc == 0);

    hal_spi_disable(RADIO_SPI_IDX);

    spi_settings.data_order = HAL_SPI_MSB_FIRST;
    spi_settings.data_mode = HAL_SPI_MODE0;
    spi_settings.baudrate = MYNEWT_VAL(SX1272_SPI_BAUDRATE);
    spi_settings.word_size = HAL_SPI_WORD_SIZE_8BIT;

    rc = hal_spi_config(RADIO_SPI_IDX, &spi_settings);
    assert(rc == 0);

    rc = hal_spi_enable(RADIO_SPI_IDX);
    assert(rc == 0);
}

void
SX1272IoIrqInit(DioIrqHandler **irqHandlers)
{
    int rc;

    if (irqHandlers[0] != NULL) {
        rc = hal_gpio_irq_init(SX1272_DIO0, irqHandlers[0], NULL,
                               HAL_GPIO_TRIG_RISING, HAL_GPIO_PULL_DOWN);
        assert(rc == 0);
        hal_gpio_irq_enable(SX1272_DIO0);
    }

    if (irqHandlers[1] != NULL) {
        rc = hal_gpio_irq_init(SX1272_DIO1, irqHandlers[1], NULL,
                               HAL_GPIO_TRIG_RISING, HAL_GPIO_PULL_DOWN);
        assert(rc == 0);
        hal_gpio_irq_enable(SX1272_DIO1);
    }

    if (irqHandlers[2] != NULL) {
        rc = hal_gpio_irq_init(SX1272_DIO2, irqHandlers[2], NULL,
                               HAL_GPIO_TRIG_RISING, HAL_GPIO_PULL_DOWN);
        assert(rc == 0);
        hal_gpio_irq_enable(SX1272_DIO2);
    }

    if (irqHandlers[3] != NULL) {
        rc = hal_gpio_irq_init(SX1272_DIO3, irqHandlers[3], NULL,
                               HAL_GPIO_TRIG_RISING, HAL_GPIO_PULL_DOWN);
        assert(rc == 0);
        hal_gpio_irq_enable(SX1272_DIO3);
    }

    if (irqHandlers[4] != NULL) {
        rc = hal_gpio_irq_init(SX1272_DIO4, irqHandlers[4], NULL,
                               HAL_GPIO_TRIG_RISING, HAL_GPIO_PULL_DOWN);
        assert(rc == 0);
        hal_gpio_irq_enable(SX1272_DIO4);
    }

    if (irqHandlers[5] != NULL) {
        rc = hal_gpio_irq_init(SX1272_DIO5, irqHandlers[5], NULL,
                               HAL_GPIO_TRIG_RISING, HAL_GPIO_PULL_DOWN);
        assert(rc == 0);
        hal_gpio_irq_enable(SX1272_DIO5);
    }
}

void
SX1272IoDeInit(void)
{
    if (DioIrq[0] != NULL) {
        hal_gpio_irq_release(SX1272_DIO0);
    }
    if (DioIrq[1] != NULL) {
        hal_gpio_irq_release(SX1272_DIO1);
    }
    if (DioIrq[2] != NULL) {
        hal_gpio_irq_release(SX1272_DIO2);
    }
    if (DioIrq[3] != NULL) {
        hal_gpio_irq_release(SX1272_DIO3);
    }
    if (DioIrq[4] != NULL) {
        hal_gpio_irq_release(SX1272_DIO4);
    }
    if (DioIrq[5] != NULL) {
        hal_gpio_irq_release(SX1272_DIO5);
    }
}

void
SX1272SetRfTxPower(int8_t power)
{
    uint8_t paconfig;
    uint8_t padac;

    paconfig = SX1272Read(REG_PACONFIG);
    padac = SX1272Read(REG_PADAC);

    paconfig = (paconfig & RF_PACONFIG_PASELECT_MASK) | SX1272GetPaSelect(SX1272.Settings.Channel);

    if ((paconfig & RF_PACONFIG_PASELECT_PABOOST) == RF_PACONFIG_PASELECT_PABOOST) {
        if (power > 17) {
            padac = (padac & RF_PADAC_20DBM_MASK) | RF_PADAC_20DBM_ON;
        } else {
            padac = (padac & RF_PADAC_20DBM_MASK) | RF_PADAC_20DBM_OFF;
        }
        if ((padac & RF_PADAC_20DBM_ON) == RF_PADAC_20DBM_ON) {
            if (power < 5) {
                power = 5;
            }

            if (power > 20) {
                power = 20;
            }
            paconfig = (paconfig & RFLR_PACONFIG_OUTPUTPOWER_MASK) | (uint8_t)((uint16_t)(power - 5) & 0x0F);
        } else {
            if (power < 2) {
                power = 2;
            }

            if (power > 17) {
                power = 17;
            }
            paconfig = (paconfig & RFLR_PACONFIG_OUTPUTPOWER_MASK) | (uint8_t)((uint16_t)(power - 2) & 0x0F);
        }
    } else {
        if (power < -1) {
            power = -1;
        }

        if (power > 14) {
            power = 14;
        }
        paconfig = (paconfig & RFLR_PACONFIG_OUTPUTPOWER_MASK) | (uint8_t)((uint16_t)(power + 1) & 0x0F);
    }
    SX1272Write(REG_PACONFIG, paconfig);
    SX1272Write(REG_PADAC, padac);
}

uint8_t
SX1272GetPaSelect(uint32_t channel)
{
#if (MYNEWT_VAL(SX1272_USE_PA_BOOST) == 1)
    return RF_PACONFIG_PASELECT_PABOOST;
#else
    return RF_PACONFIG_PASELECT_RFO;
#endif
}

#if (MYNEWT_VAL(SX1272_HAS_ANT_SW) || MYNEWT_VAL(SX1272_HAS_COMP_ANT_SW))
void
SX1272SetAntSwLowPower(bool status)
{
    if (g_radio_is_active != status) {
        g_radio_is_active = status;
        if (status == false) {
            SX1272AntSwInit();
        } else {
            SX1272AntSwDeInit();
        }
    }
}

void
SX1272AntSwInit(void)
{
    /*
     * XXX: consider doing this to save power. Currently the gpio are
     * set to rx mode automatically in the IO init function.
     */
}

void
SX1272AntSwDeInit(void)
{
    /*
     * XXX: consider doing this to save power. Currently the gpio are
     * set to rx mode automatically in the IO init function.
     */
}

void
SX1272SetAntSw(uint8_t opMode)
{
    os_sr_t sr;

    OS_ENTER_CRITICAL(sr);
    switch(opMode) {
    case RFLR_OPMODE_TRANSMITTER:
#if MYNEWT_VAL(SX1272_HAS_COMP_ANT_SW)
        hal_gpio_write(SX1272_RXTX, 0);
        hal_gpio_write(SX1272_N_RXTX, 1);
#endif
#if MYNEWT_VAL(SX1272_HAS_ANT_SW)
        hal_gpio_write(SX1272_RXTX, 1);
#endif
        break;
    case RFLR_OPMODE_RECEIVER:
    case RFLR_OPMODE_RECEIVER_SINGLE:
    case RFLR_OPMODE_CAD:
    default:
#if MYNEWT_VAL(SX1272_HAS_COMP_ANT_SW)
        hal_gpio_write(SX1272_RXTX, 1);
        hal_gpio_write(SX1272_N_RXTX, 0);
#endif
#if MYNEWT_VAL(SX1272_HAS_ANT_SW)
        hal_gpio_write(SX1272_RXTX, 0);
#endif
        break;
    }
    OS_EXIT_CRITICAL(sr);
}
#else
void
SX1272SetAntSwLowPower(bool status)
{
    (void)status;
}

void
SX1272AntSwInit(void)
{
    /*
     * XXX: consider doing this to save power. Currently the gpio are
     * set to rx mode automatically in the IO init function.
     */
}

void
SX1272AntSwDeInit(void)
{
    /*
     * XXX: consider doing this to save power. Currently the gpio are
     * set to rx mode automatically in the IO init function.
     */
}

void
SX1272SetAntSw(uint8_t opMode)
{
    (void)opMode;
}
#endif

bool
SX1272CheckRfFrequency(uint32_t frequency)
{
    // Implement check. Currently all frequencies are supported
    return true;
}

uint32_t
SX1272GetBoardTcxoWakeupTime(void)
{
    return 0;
}

