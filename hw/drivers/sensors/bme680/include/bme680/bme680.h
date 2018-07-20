#ifndef __BME860_H__
#define __BME860_H__

#include "os/mynewt.h"
#include "sensor/sensor.h"

#include "bme680/bme680_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

struct bme680 {
    struct os_dev dev;
    struct sensor sensor;
    struct bme680_cfg cfg;
};

int bme680_init(struct os_dev *dev, void *arg);
int bme680_config(struct bme680 *bme680, struct bme680_cfg *cfg);

#ifdef __cplusplus
}
#endif

#endif /* __BME860_H__ */
