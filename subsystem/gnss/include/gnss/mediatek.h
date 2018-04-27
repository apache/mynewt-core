#ifndef _GNSS_MEDIATEK_H_
#define _GNSS_MEDIATEK_H_

#define GNSS_MEDIATEK_DEFAULT_BAUD_RATE 115200

/**
 * Configuration of the MediaTek driver
 */
struct gnss_mediatek {
    int wakeup_pin;       /**< pin use for wakeup             */
    int reset_pin;        /**< pin use for reset              */
    int data_ready_pin;    /**< pin use to signal data ready   */
    uint16_t cmd_delay;   /**< delay required after a command */

    int standby_level;
    gnss_data_ready_callback_t data_ready_cb;
};

/**
 * Initialize the driver layer with MediaTek device.
 *
 * @param ctx		GNSS context 
 * @param uart		Configuration
 *
 * @return true on success
 */
int gnss_mediatek_init(gnss_t *ctx, struct gnss_mediatek *mtk);

#endif
