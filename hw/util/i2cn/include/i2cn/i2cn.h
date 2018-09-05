#ifndef H_I2CN_
#define H_I2CN_

#include <inttypes.h>
#include "os/mynewt.h"
struct hal_i2c_master_data;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Reads from an I2C slave, retrying the specified number of times on
 * failure.
 *
 * @param i2c_num               The index of the I2C interface to read from.
 * @param pdata                 Additional parameters describing the read
 *                                  operation.
 * @param timeout               The time, in OS ticks, to wait for the MCU to
 *                                  indicate completion of each clocked byte.
 * @param last_op               1 if this is the final message in the
 *                                  transaction.
 *
 * @return                      0 on success;
 *                              HAL_I2C_ERR_[...] code on failure.
 */
int i2cn_master_read(uint8_t i2c_num, struct hal_i2c_master_data *pdata,
                     uint32_t timeout, uint8_t last_op, int retries);

/**
 * @brief Writes to an I2C slave, retrying the specified number of times on
 * failure.
 *
 * @param i2c_num               The index of the I2C interface to write to.
 * @param pdata                 Additional parameters describing the write
 *                                  operation.
 * @param timeout               The time, in OS ticks, to wait for the MCU to
 *                                  indicate completion of each clocked byte.
 * @param last_op               1 if this is the final message in the
 *                                  transaction.
 *
 * @return                      0 on success;
 *                              HAL_I2C_ERR_[...] code on failure.
 */
int i2cn_master_write(uint8_t i2c_num, struct hal_i2c_master_data *pdata,
                      uint32_t timeout, uint8_t last_op, int retries);

#ifdef __cplusplus
}
#endif

#endif
