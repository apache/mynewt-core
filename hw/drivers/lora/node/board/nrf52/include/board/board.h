/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
    (C)2013 Semtech

Description: Target board general functions implementation

License: Revised BSD License, see LICENSE.TXT file include in the project

Maintainer: Miguel Luis and Gregory Cristian
*/
#ifndef __BOARD_H__
#define __BOARD_H__

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "bsp/bsp.h"
#include "node/utilities.h"

/*!
 * Board MCU pins definitions
 */

#define RADIO_RESET                 SX1276_NRESET

#define RADIO_SPI_IDX               MYNEWT_VAL(LORA_NODE_BOARD_SPI_IDX)

#if RADIO_SPI_IDX == 0
#define RADIO_NSS                   MYNEWT_VAL(SPI_0_MASTER_SS_PIN)
#else
#error Invalid LORA_NODE_BOARD_SPI_IDX value
#endif

#define RADIO_DIO_0                 SX1276_DIO0
#define RADIO_DIO_1                 SX1276_DIO1
#define RADIO_DIO_2                 SX1276_DIO2
#define RADIO_DIO_3                 SX1276_DIO3
#define RADIO_DIO_4                 SX1276_DIO4
#define RADIO_DIO_5                 SX1276_DIO5

#define RADIO_ANT_SWITCH_HF         SX1276_ANT_HF_CTRL

#define RF_RXTX                     SX1276_RXTX

/*!
 * Possible power sources
 */
enum BoardPowerSources
{
    USB_POWER = 0,
    BATTERY_POWER,
};

/*!
 * \brief Get the current battery level
 *
 * \retval value  battery level [  0: USB,
 *                                 1: Min level,
 *                                 x: level
 *                               254: fully charged,
 *                               255: Error]
 */
uint8_t BoardGetBatteryLevel( void );

/*!
 * Returns a pseudo random seed generated using the MCU Unique ID
 *
 * \retval seed Generated pseudo random seed
 */
uint32_t BoardGetRandomSeed( void );

/*!
 * \brief Gets the board 64 bits unique ID
 *
 * \param [IN] id Pointer to an array that will contain the Unique ID
 */
void BoardGetUniqueId( uint8_t *id );

/*!
 * \brief Get the board power source
 *
 * \retval value  power source [0: USB_POWER, 1: BATTERY_POWER]
 */
uint8_t GetBoardPowerSource( void );

#endif // __BOARD_H__
