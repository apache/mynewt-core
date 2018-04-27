#ifndef _GNSS_UBLOX_H_
#define _GNSS_UBLOX_H_

#define GNSS_UBLOX_DEFAULT_BAUD_RATE 9600

/**
 * Configuration of the Ublox driver
 */
struct gnss_ublox {
    int wakeup_pin;       /**< pin use for wakeup             */
    int reset_pin;        /**< pin use for reset              */
    int data_ready_pin;    /**< pin use to signal data ready   */
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

#endif
