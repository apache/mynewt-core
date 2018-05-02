/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
    (C)2013 Semtech

Description: SX1276 driver specific target board functions implementation

License: Revised BSD License, see LICENSE.TXT file include in the project

Maintainer: Miguel Luis and Gregory Cristian
*/
#include <assert.h>
#include "hal/hal_spi.h"
#include "bsp/bsp.h"
#include "radio/radio.h"
#include "sx1276.h"
#include "sx1276-board.h"

extern DioIrqHandler *DioIrq[];

#if MYNEWT_VAL(SX1276_HAS_ANT_SW)
/*!
 * Flag used to set the RF switch control pins in low power mode when the radio is not active.
 */
static bool RadioIsActive = false;
#endif

/*!
 * Radio driver structure initialization
 */
const struct Radio_s Radio =
{
    .Init = SX1276Init,
    .GetStatus = SX1276GetStatus,
    .SetModem = SX1276SetModem,
    .SetChannel = SX1276SetChannel,
    .IsChannelFree = SX1276IsChannelFree,
    .Random = SX1276Random,
    .SetRxConfig = SX1276SetRxConfig,
    .SetTxConfig = SX1276SetTxConfig,
    .CheckRfFrequency = SX1276CheckRfFrequency,
    .TimeOnAir = SX1276GetTimeOnAir,
    .Send = SX1276Send,
    .Sleep = SX1276SetSleep,
    .Standby = SX1276SetStby,
    .Rx = SX1276SetRx,
    .StartCad = SX1276StartCad,
    .Rssi = SX1276ReadRssi,
    .Write = SX1276Write,
    .Read = SX1276Read,
    .WriteBuffer = SX1276WriteBuffer,
    .ReadBuffer = SX1276ReadBuffer,
    .SetMaxPayloadLength = SX1276SetMaxPayloadLength,
    .SetPublicNetwork = SX1276SetPublicNetwork,
    .GetWakeupTime = SX1276GetWakeupTime
};

void
SX1276IoInit(void)
{
    struct hal_spi_settings spi_settings;
    int rc;

#if MYNEWT_VAL(SX1276_HAS_ANT_SW)
    rc = hal_gpio_init_out(SX1276_RXTX, 0);
    assert(rc == 0);
#endif

    rc = hal_gpio_init_out(RADIO_NSS, 1);
    assert(rc == 0);

    hal_spi_disable(RADIO_SPI_IDX);

    spi_settings.data_order = HAL_SPI_MSB_FIRST;
    spi_settings.data_mode = HAL_SPI_MODE0;
    spi_settings.baudrate = MYNEWT_VAL(SX1276_SPI_BAUDRATE);
    spi_settings.word_size = HAL_SPI_WORD_SIZE_8BIT;

    rc = hal_spi_config(RADIO_SPI_IDX, &spi_settings);
    assert(rc == 0);

    rc = hal_spi_enable(RADIO_SPI_IDX);
    assert(rc == 0);
}

void
SX1276IoIrqInit(DioIrqHandler **irqHandlers)
{
    int rc;

    if (irqHandlers[0] != NULL) {
        rc = hal_gpio_irq_init(SX1276_DIO0, irqHandlers[0], NULL,
                               HAL_GPIO_TRIG_RISING, HAL_GPIO_PULL_NONE);
        assert(rc == 0);
        hal_gpio_irq_enable(SX1276_DIO0);
    }

    if (irqHandlers[1] != NULL) {
        rc = hal_gpio_irq_init(SX1276_DIO1, irqHandlers[1], NULL,
                               HAL_GPIO_TRIG_RISING, HAL_GPIO_PULL_NONE);
        assert(rc == 0);
        hal_gpio_irq_enable(SX1276_DIO1);
    }

    if (irqHandlers[2] != NULL) {
        rc = hal_gpio_irq_init(SX1276_DIO2, irqHandlers[2], NULL,
                               HAL_GPIO_TRIG_RISING, HAL_GPIO_PULL_NONE);
        assert(rc == 0);
        hal_gpio_irq_enable(SX1276_DIO2);
    }

    if (irqHandlers[3] != NULL) {
        rc = hal_gpio_irq_init(SX1276_DIO3, irqHandlers[3], NULL,
                               HAL_GPIO_TRIG_RISING, HAL_GPIO_PULL_NONE);
        assert(rc == 0);
        hal_gpio_irq_enable(SX1276_DIO3);
    }

    if (irqHandlers[4] != NULL) {
        rc = hal_gpio_irq_init(SX1276_DIO4, irqHandlers[4], NULL,
                               HAL_GPIO_TRIG_RISING, HAL_GPIO_PULL_NONE);
        assert(rc == 0);
        hal_gpio_irq_enable(SX1276_DIO4);
    }

    if (irqHandlers[5] != NULL) {
        rc = hal_gpio_irq_init(SX1276_DIO5, irqHandlers[5], NULL,
                               HAL_GPIO_TRIG_RISING, HAL_GPIO_PULL_NONE);
        assert(rc == 0);
        hal_gpio_irq_enable(SX1276_DIO5);
    }
}

void
SX1276IoDeInit( void )
{
    if (DioIrq[0] != NULL) {
        hal_gpio_irq_release(SX1276_DIO0);
    }
    if (DioIrq[1] != NULL) {
        hal_gpio_irq_release(SX1276_DIO1);
    }
    if (DioIrq[2] != NULL) {
        hal_gpio_irq_release(SX1276_DIO2);
    }
    if (DioIrq[3] != NULL) {
        hal_gpio_irq_release(SX1276_DIO3);
    }
    if (DioIrq[4] != NULL) {
        hal_gpio_irq_release(SX1276_DIO4);
    }
    if (DioIrq[5] != NULL) {
        hal_gpio_irq_release(SX1276_DIO5);
    }
}

uint8_t
SX1276GetPaSelect(uint32_t channel)
{
    uint8_t pacfg;

    if (channel < RF_MID_BAND_THRESH) {
#if (MYNEWT_VAL(SX1276_LF_USE_PA_BOOST) == 1)
        pacfg = RF_PACONFIG_PASELECT_PABOOST;
#else
        pacfg = RF_PACONFIG_PASELECT_RFO;
#endif
    } else {
#if (MYNEWT_VAL(SX1276_HF_USE_PA_BOOST) == 1)
        pacfg = RF_PACONFIG_PASELECT_PABOOST;
#else
        pacfg = RF_PACONFIG_PASELECT_RFO;
#endif
    }

    return pacfg;
}

#if MYNEWT_VAL(SX1276_HAS_ANT_SW)
void SX1276SetAntSwLowPower( bool status )
{
    if (RadioIsActive != status) {
        RadioIsActive = status;

        if (status == false) {
            SX1276AntSwInit( );
        } else {
            SX1276AntSwDeInit( );
        }
    }
}

void
SX1276AntSwInit(void)
{
    // Consider turning off GPIO pins for low power. They are always on right
    // now. GPIOTE library uses 0.5uA max when on, typical 0.1uA.
}

void
SX1276AntSwDeInit(void)
{
    // Consider this for low power - ie turning off GPIO pins
}

void
SX1276SetAntSw(uint8_t rxTx)
{
    // 1: TX, 0: RX
    if (rxTx != 0) {
        hal_gpio_write(SX1276_RXTX, 1);
    } else {
        hal_gpio_write(SX1276_RXTX, 0);
    }
}
#endif

bool
SX1276CheckRfFrequency(uint32_t frequency)
{
    // Implement check. Currently all frequencies are supported
    return true;
}

uint32_t
SX1276GetBoardTcxoWakeupTime(void)
{
    return 0;
}

