/**
 * Copyright (c) 2015 Runtime Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <assert.h>
#include <errno.h>
#include "syscfg/syscfg.h"
#include "compiler.h"
#include "port.h"
#include "i2c_master.h"

#include "hal/hal_i2c.h"
#include "mcu/hal_i2c.h"
#include "mcu/samd21.h"
#include "samd21_priv.h"

/*
 * XXX Timeout value parameter in functions is not used, because Atmel's
 * SDK for i2c internally times out much faster than one OS tick takes.
 */
struct samd21_i2c_state {
    struct i2c_master_module module;
    const struct samd21_i2c_config *pconfig;
    Sercom *hw;
};

#define HAL_SAMD21_I2C_MAX (6)

#if MYNEWT_VAL(I2C_0)
static struct samd21_i2c_state samd21_i2c0;
#endif
#if MYNEWT_VAL(I2C_1)
static struct samd21_i2c_state samd21_i2c1;
#endif
#if MYNEWT_VAL(I2C_2)
static struct samd21_i2c_state samd21_i2c2;
#endif
#if MYNEWT_VAL(I2C_3)
static struct samd21_i2c_state samd21_i2c3;
#endif
#if MYNEWT_VAL(I2C_4)
static struct samd21_i2c_state samd21_i2c4;
#endif
#if MYNEWT_VAL(I2C_5)
static struct samd21_i2c_state samd21_i2c5;
#endif

struct samd21_i2c_state *samd21_hal_i2cs[HAL_SAMD21_I2C_MAX] = {
#if MYNEWT_VAL(I2C_0)
    &samd21_i2c0,
#else
    NULL,
#endif
#if MYNEWT_VAL(I2C_1)
    &samd21_i2c1,
#else
    NULL,
#endif
#if MYNEWT_VAL(I2C_2)
    &samd21_i2c2,
#else
    NULL,
#endif
#if MYNEWT_VAL(I2C_3)
    &samd21_i2c3,
#else
    NULL,
#endif
#if MYNEWT_VAL(I2C_4)
    &samd21_i2c4,
#else
    NULL,
#endif
#if MYNEWT_VAL(I2C_5)
    &samd21_i2c5,
#else
    NULL,
#endif
};

#define SAMD21_I2C_RESOLVE(__n, __v)                                    \
    if ((__n) >= HAL_SAMD21_I2C_MAX || !samd21_hal_i2cs[(__n)]) {       \
        rc = EINVAL;                                                    \
        goto err;                                                       \
    }                                                                   \
    (__v) = samd21_hal_i2cs[(__n)];


int
hal_i2c_init(uint8_t i2c_num, void *usercfg)
{
    struct samd21_i2c_state *i2c;
    enum status_code status;
    struct i2c_master_config cfg;
    int rc;

    SAMD21_I2C_RESOLVE(i2c_num, i2c);

    assert(usercfg != NULL);

    i2c->hw = samd21_sercom(i2c_num);
    if (i2c->hw == NULL) {
        rc = EINVAL;
        goto err;
    }

    i2c->pconfig = (struct samd21_i2c_config *)usercfg;

    i2c_master_get_config_defaults(&cfg);
    cfg.pinmux_pad0 = i2c->pconfig->pad0_pinmux;
    cfg.pinmux_pad1 = i2c->pconfig->pad1_pinmux;

    status = i2c_master_init(&i2c->module, i2c->hw, &cfg);
    if (status != STATUS_OK) {
        rc = (int)status;
        goto err;
    }

    i2c_master_enable(&i2c->module);

    return (0);
err:
    return (rc);
}

int
hal_i2c_master_write(uint8_t i2c_num, struct hal_i2c_master_data *ppkt,
                     uint32_t os_ticks, uint8_t last_op)
{
    struct samd21_i2c_state *i2c;
    struct i2c_master_packet pkt;
    enum status_code status;
    int rc;

    SAMD21_I2C_RESOLVE(i2c_num, i2c);

    memset(&pkt, 0, sizeof(pkt));
    pkt.address = ppkt->address;
    pkt.data_length = ppkt->len;
    pkt.data = ppkt->buffer;

    if (last_op) {
        status = i2c_master_write_packet_wait(&i2c->module, &pkt);
    } else {
        status = i2c_master_write_packet_wait_no_stop(&i2c->module, &pkt);
    }
    if (status != STATUS_OK) {
        rc = (int) status;
        goto err;
    }

    return (0);
err:
    return (rc);
}

int
hal_i2c_master_read(uint8_t i2c_num, struct hal_i2c_master_data *ppkt,
                    uint32_t os_ticks, uint8_t last_op)
{
    struct samd21_i2c_state *i2c;
    struct i2c_master_packet pkt;
    enum status_code status;
    int rc;

    SAMD21_I2C_RESOLVE(i2c_num, i2c);

    memset(&pkt, 0, sizeof(pkt));
    pkt.address = ppkt->address;
    pkt.data_length = ppkt->len;
    pkt.data = ppkt->buffer;

    if (last_op) {
        status = i2c_master_read_packet_wait(&i2c->module, &pkt);
    } else {
        status = i2c_master_read_packet_wait_no_stop(&i2c->module, &pkt);
    }
    if (status != STATUS_OK) {
        rc = (int)status;
        goto err;
    }
    return (0);
err:
    return (rc);
}

int
hal_i2c_master_probe(uint8_t i2c_num, uint8_t address, uint32_t os_ticks)
{
    struct samd21_i2c_state *i2c;
    struct i2c_master_packet pkt;
    enum status_code status;
    int rc;
    uint8_t buf;

    SAMD21_I2C_RESOLVE(i2c_num, i2c);

    memset(&pkt, 0, sizeof(pkt));
    pkt.address = address;
    pkt.data_length = 0;
    pkt.data = &buf;

    status = i2c_master_read_packet_wait(&i2c->module, &pkt);
    if (status != STATUS_OK) {
        rc = (int)status;
        goto err;
    }

    return (0);
err:
    return (rc);
}
