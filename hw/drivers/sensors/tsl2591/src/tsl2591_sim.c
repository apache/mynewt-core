/**************************************************************************/
/*!
    @file     tsl2591_sim.c
    @author   Kevin Townsend
    @section LICENSE
    Software License Agreement (BSD License)
    Copyright (c) 2018, Kevin Townsend
    All rights reserved.
    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
    1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    3. Neither the name of the copyright holders nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.
    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
    THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/**************************************************************************/

#if ARCH_sim

#include <string.h>
#include <errno.h>
#include "os/mynewt.h"
#include "tsl2591/tsl2591.h"
#include "tsl2591_priv.h"
#include "mcu/mcu_sim_i2c.h"

/* Default register values for this sensor */
static uint8_t g_tsl2591_sim_regs[TSL2591_REGISTER_CHAN1_HIGH] = {
  /* 0x00 = TSL2591_REGISTER_ENABLE */
  0x00,
  /* 0x01 = TSL2591_REGISTER_CONTROL */
  0x00,
  /* 0x02 = RESERVED */
  0x00,
  /* 0x03 = RESERVED */
  0x00,
  /* 0x04 = TSL2591_REGISTER_THRESHOLD_AILTL */
  0x00,
  /* 0x05 = TSL2591_REGISTER_THRESHOLD_AILTH */
  0x00,
  /* 0x06 = TSL2591_REGISTER_THRESHOLD_AIHTL */
  0x00,
  /* 0x07 = TSL2591_REGISTER_THRESHOLD_AIHTH */
  0x00,
  /* 0x08 = TSL2591_REGISTER_THRESHOLD_NPAILTL */
  0x00,
  /* 0x09 = TSL2591_REGISTER_THRESHOLD_NPAILTH */
  0x00,
  /* 0x0A = TSL2591_REGISTER_THRESHOLD_NPAIHTL */
  0x00,
  /* 0x0B = TSL2591_REGISTER_THRESHOLD_NPAIHTH */
  0x00,
  /* 0x0C = TSL2591_REGISTER_PERSIST_FILTER */
  0x00,
  /* 0x11 = TSL2591_REGISTER_PACKAGE_PID */
  0x00,
  /* 0x12 = TSL2591_REGISTER_DEVICE_ID */
  0x50,
  /* 0x13 = TSL2591_REGISTER_DEVICE_STATUS */
  0x00,
  /* 0x14 = TSL2591_REGISTER_CHAN0_LOW */
  0x00,
  /* 0x15 = TSL2591_REGISTER_CHAN0_HIGH */
  0x00,
  /* 0x16 = TSL2591_REGISTER_CHAN1_LOW */
  0x00,
  /* 0x17 = TSL2591_REGISTER_CHAN1_HIGH */
  0x00
};

int
tsl2591_sensor_sim_write(uint8_t i2c_num, struct hal_i2c_master_data *pdata,
                         uint32_t timeout, uint8_t last_op)
{
    printf("TSL2591 wrote %d byte(s):", pdata->len);
    for (uint16_t i=0; i<pdata->len; i++) {
        printf(" 0x%02X", pdata->buffer[i]);
    }
    printf("\n");
    fflush(stdout);

    return 0;
}

int
tsl2591_sensor_sim_read(uint8_t i2c_num, struct hal_i2c_master_data *pdata,
                        uint32_t timeout, uint8_t last_op)
{
    printf("TSL2591  read %d byte(s):", pdata->len);
    for (uint16_t i=0; i<pdata->len; i++) {
        printf(" 0x%02X", pdata->buffer[i]);
    }
    printf("\n");
    fflush(stdout);

    return 0;
}

static const struct hal_i2c_sim_driver g_tsl2591_sensor_sim_driver = {
    .sd_write = tsl2591_sensor_sim_write,
    .sd_read = tsl2591_sensor_sim_read,
    .addr = MYNEWT_VAL(TSL2591_SHELL_ITF_ADDR)
};

int
tsl2591_sim_init(void)
{
    printf("Registering TSL2591 sim driver\n");
    fflush(stdout);

    (void)g_tsl2591_sim_regs;

    /* Register this sim driver with the hal_i2c simulator */
    hal_i2c_sim_register((struct hal_i2c_sim_driver *)
                         &g_tsl2591_sensor_sim_driver);

    return 0;
}

#endif /* ARCH_sim */
