#ifndef _GNSS_UBLOX_H_
#define _GNSS_UBLOX_H_

/**
 * Driver for u-blox chipset
 *
 * - based on M8N
 * - requires protocol 18
 */

#include <gnss/nmea.h>

#define GNSS_UBLOX_DEFAULT_BAUD_RATE 9600

/**
 * Configuration of the Ublox driver
 */
struct gnss_ublox {
    int wakeup_pin;       /**< pin use for wakeup             */
    int reset_pin;        /**< pin use for reset              */
    int data_ready_pin;   /**< pin use to signal data ready   */
    uint16_t cmd_delay;   /**< delay required after a command */

    int standby_level;
};

/**
 * Initialize the driver layer with Ublox device.
 *
 * @param ctx		GNSS context 
 * @param uart		Configuration
 *
 * @return true on success
 */
int gnss_ublox_init(gnss_t *ctx, struct gnss_ublox *mtk);


int
gnss_ublox_set_bauds(gnss_t *ctx, uint32_t bauds);

/**
 * Control the rate of NMEA sentence.
 *
 * @note No guarante is made on the final result.
 * @note Apply, to UART, I2C, and SPI.
 *
 * @param ctx		GNSS context 
 * @param limit		List of sentence/rate.
 *                      List is terminated using GNSS_NMEA_SENTENCE_NONE
 * @param size          Number of item in the list
 */
int gnss_ublox_nmea_rate(gnss_t *ctx, struct gnss_nmea_rate *rates) ;


int gnss_ublox_gnss(gnss_t *ctx, uint32_t gnss);

#endif
