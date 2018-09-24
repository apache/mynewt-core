#include "hal/hal_i2c.h"
#include "i2cn/i2cn.h"

int
i2cn_master_read(uint8_t i2c_num, struct hal_i2c_master_data *pdata,
                 uint32_t timeout, uint8_t last_op, int retries)
{
    int rc = 0;
    int i;

    /* Ensure at least one try. */
    if (retries < 0) {
        retries = 0;
    }

    for (i = 0; i <= retries; i++) {
        rc = hal_i2c_master_read(i2c_num, pdata, timeout, last_op);
        if (rc == 0) {
            break;
        }
    }

    return rc;
}

int
i2cn_master_write(uint8_t i2c_num, struct hal_i2c_master_data *pdata,
                  uint32_t timeout, uint8_t last_op, int retries)
{
    int rc = 0;
    int i;

    /* Ensure at least one try. */
    if (retries < 0) {
        retries = 0;
    }

    for (i = 0; i <= retries; i++) {
        rc = hal_i2c_master_write(i2c_num, pdata, timeout, last_op);
        if (rc == 0) {
            break;
        }
    }

    return rc;
}
