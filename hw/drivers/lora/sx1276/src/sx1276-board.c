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
#include "node/radio.h"
#include "sx1276.h"
#include "sx1276-board.h"

/*!
 * Flag used to set the RF switch control pins in low power mode when the radio is not active.
 */
static bool RadioIsActive = false;

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
    .SetPublicNetwork = SX1276SetPublicNetwork
};

void SX1276IoInit( void )
{
    struct hal_spi_settings spi_settings;
    int rc;

    rc = hal_gpio_init_out(SX1276_RXTX, 1);
    assert(rc == 0);

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

void SX1276IoIrqInit( DioIrqHandler **irqHandlers )
{
    int rc;

    rc = hal_gpio_irq_init(SX1276_DIO0, irqHandlers[0], NULL,
                           HAL_GPIO_TRIG_RISING, HAL_GPIO_PULL_NONE);
    assert(rc == 0);
    hal_gpio_irq_enable(SX1276_DIO0);

    rc = hal_gpio_irq_init(SX1276_DIO1, irqHandlers[1], NULL,
                           HAL_GPIO_TRIG_RISING, HAL_GPIO_PULL_NONE);
    assert(rc == 0);
    hal_gpio_irq_enable(SX1276_DIO1);

    rc = hal_gpio_irq_init(SX1276_DIO2, irqHandlers[2], NULL,
                           HAL_GPIO_TRIG_RISING, HAL_GPIO_PULL_NONE);
    assert(rc == 0);
    hal_gpio_irq_enable(SX1276_DIO2);

    rc = hal_gpio_irq_init(SX1276_DIO3, irqHandlers[3], NULL,
                           HAL_GPIO_TRIG_RISING, HAL_GPIO_PULL_NONE);
    assert(rc == 0);
    hal_gpio_irq_enable(SX1276_DIO3);

    rc = hal_gpio_irq_init(SX1276_DIO4, irqHandlers[4], NULL,
                           HAL_GPIO_TRIG_RISING, HAL_GPIO_PULL_NONE);
    assert(rc == 0);
    hal_gpio_irq_enable(SX1276_DIO4);

    rc = hal_gpio_irq_init(SX1276_DIO5, irqHandlers[5], NULL,
                           HAL_GPIO_TRIG_RISING, HAL_GPIO_PULL_NONE);
    assert(rc == 0);
    hal_gpio_irq_enable(SX1276_DIO5);
}

void SX1276IoDeInit( void )
{
    hal_gpio_irq_release(SX1276_DIO0);
    hal_gpio_irq_release(SX1276_DIO1);
    hal_gpio_irq_release(SX1276_DIO2);
    hal_gpio_irq_release(SX1276_DIO3);
    hal_gpio_irq_release(SX1276_DIO4);
    hal_gpio_irq_release(SX1276_DIO5);
}

uint8_t SX1276GetPaSelect( uint32_t channel )
{
    if( channel < RF_MID_BAND_THRESH )
    {
        return RF_PACONFIG_PASELECT_PABOOST;
    }
    else
    {
        return RF_PACONFIG_PASELECT_RFO;
    }
}

void SX1276SetAntSwLowPower( bool status )
{
    if( RadioIsActive != status )
    {
        RadioIsActive = status;

        if( status == false )
        {
            SX1276AntSwInit( );
        }
        else
        {
            SX1276AntSwDeInit( );
        }
    }
}

void SX1276AntSwInit( void )
{
    // Consider turning off GPIO pins for low power. They are always on right
    // now. GPIOTE library uses 0.5uA max when on, typical 0.1uA.
}

void SX1276AntSwDeInit( void )
{
    // Consider this for low power - ie turning off GPIO pins
}

void SX1276SetAntSw( uint8_t rxTx )
{
    if( rxTx != 0 ) // 1: TX, 0: RX
    {
        hal_gpio_write(SX1276_RXTX, 1);
    }
    else
    {
        hal_gpio_write(SX1276_RXTX, 0);
    }
}

bool SX1276CheckRfFrequency( uint32_t frequency )
{
    // Implement check. Currently all frequencies are supported
    return true;
}
