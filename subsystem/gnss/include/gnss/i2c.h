#ifndef _GNSS_I2C_H_
#define _GNSS_I2C_H_

#include <stdint.h>
#include <hal/hal_i2c.h>

/**
 * GNSS i2c configuration
 */
struct gnss_i2c {
    int     dev;          /**< I2C device */
    uint8_t addr;         /**< I2C address */
    uint8_t refill_delay; /**< Delay before fetching a new buffer */
    
    struct os_callout polling;
};

/**
 * Initialize the transport layer with i2c.
 *
 * @param ctx		GNSS context 
 * @param i2c		Configuration
 *
 * @return true on success
 */
int gnss_i2c_init(gnss_t *ctx, struct gnss_i2c *i2c);


int gnss_i2c_mediatek_process_buffer(gnss_t *ctx);

#endif

