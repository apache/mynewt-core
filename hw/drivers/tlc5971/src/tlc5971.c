/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
#include <string.h>
#include "os/mynewt.h"
#include "tlc5971/tlc5971.h"
#include "hal/hal_spi.h"

/**
 * tlc5972 open
 *
 * Device open funtion.
 *
 * @param odev Pointer to OS device structure
 * @param wait The amount of time to wait to open (if needed).
 * @param arg device open arg (not used)
 *
 * @return int 0:success; error code otherwise
 */
static int
tlc5971_open(struct os_dev *odev, uint32_t wait, void *arg)
{
    int rc;
    int spi_num;
    struct tlc5971_dev *dev;
    struct hal_spi_settings spi_cfg;

    dev = (struct tlc5971_dev *)odev;

    /* Configure the spi and enable it */
    spi_cfg.baudrate = dev->tlc_itf.tpi_spi_freq;
    spi_cfg.data_mode = HAL_SPI_MODE0;
    spi_cfg.data_order = HAL_SPI_MSB_FIRST;
    spi_cfg.word_size = HAL_SPI_WORD_SIZE_8BIT;

    spi_num = dev->tlc_itf.tpi_spi_num;
    hal_spi_disable(spi_num);
    rc = hal_spi_config(spi_num, &spi_cfg);
    if (rc) {
        return rc;
    }
    hal_spi_enable(spi_num);

    dev->is_enabled = true;
    return 0;
}

/**
 * tlc5971 close
 *
 * os device close function
 *
 * @param odev Pointer to os device to close
 *
 * @return int 0: success (does not fail).
 */
static int
tlc5971_close(struct os_dev *odev)
{
    struct tlc5971_dev *dev;

    dev = (struct tlc5971_dev *)odev;

    /* Disable the SPI */
    hal_spi_disable(dev->tlc_itf.tpi_spi_num);

    /* Device is no longer enabled */
    dev->is_enabled = false;

    return 0;
}

/**
 * tlc5971 is enabled
 *
 * Returns device enabled flag
 *
 *
 * @param dev Pointer to tlc5971 device
 *
 * @return uint8_t 0:not enabled, 1:enabled
 */
int
tlc5971_is_enabled(struct tlc5971_dev *dev)
{
    return dev->is_enabled;
}

/**
 * tlc5971 construct packet
 *
 * This routine constructs the 224-bit buffer to send to the device. Bit 224
 * should get sent first. Since packet buffer byte 0 gets sent first to the
 * spi, we construct the data packet in reverse order. On other words, byte 27
 * gets placed into packet location 0, byte in location 1, etc.
 *
 * @param dev Pointer to tlc5971 device
 */
static void
tlc5971_construct_packet(struct tlc5971_dev *dev)
{
    int i;
    uint8_t *dptr;

    dptr = dev->data_packet;

    /* Place command, control and global brightness control into buffer */
    dptr[0] = TLC5971_DATA_BYTE27(dev->control_data);
    dptr[1] = TLC5971_DATA_BYTE26(dev->control_data, dev->bc.bc_blue);
    dptr[2] = TLC5971_DATA_BYTE25(dev->bc.bc_blue, dev->bc.bc_green);
    dptr[3] = TLC5971_DATA_BYTE24(dev->bc.bc_green, dev->bc.bc_red);
    dptr += 4;

    /* Now place 16-bit values into buffer */
    i = 3;
    while (i >= 0) {
        dptr[0] = (uint8_t)(dev->gs[i].gs_blue >> 8);
        dptr[1] = (uint8_t)(dev->gs[i].gs_blue);
        dptr[2] = (uint8_t)(dev->gs[i].gs_green >> 8);
        dptr[3] = (uint8_t)(dev->gs[i].gs_green);
        dptr[4] = (uint8_t)(dev->gs[i].gs_red >> 8);
        dptr[5] = (uint8_t)(dev->gs[i].gs_red);
        dptr += 6;
        --i;
    }
}

/**
 * tlc5971 write
 *
 * Send the 224 bits to the device. Note that the device must be opened
 * prior to calling this function
 *
 * @param dev   Pointer to tlc5971 device
 *
 * @return int  0: success; -1 error
 */
int
tlc5971_write(struct tlc5971_dev *dev)
{
    int rc;
    os_sr_t sr;

    if (!dev->is_enabled) {
        return -1;
    }

    /*
     * XXX: for now, disable interrupts around write as it is possible that
     * too long a gap will cause mis-program of device.
     */
    tlc5971_construct_packet(dev);

    OS_ENTER_CRITICAL(sr);
    rc = hal_spi_txrx(dev->tlc_itf.tpi_spi_num, dev->data_packet, NULL,
                      TLC5971_PACKET_LENGTH);
    OS_EXIT_CRITICAL(sr);

    return rc;
}

/**
 * tlc5971 set global brightness
 *
 * Sets the three global brightness controls. This allows for current control
 * of each LED grouping.
 *
 * @param dev pointer to tlc5971 device
 * @param bc_channel RED, GREEN, BLUE or ALL.
 * @param brightness A brightness value from 0 - 127
 */
void
tlc5971_set_global_brightness(struct tlc5971_dev *dev,
                              tlc5971_bc_channel_t bc_channel,
                              uint8_t brightness)
{
    switch (bc_channel) {
    case TLC5971_BC_CHANNEL_RED:
        dev->bc.bc_red = brightness;
        break;
    case TLC5971_BC_CHANNEL_GREEN:
        dev->bc.bc_green = brightness;
        break;
    case TLC5971_BC_CHANNEL_BLUE:
        dev->bc.bc_blue = brightness;
        break;
    /* NOTE: fall-through intentional. Default case should never occur */
    case TLC5971_BC_CHANNEL_ALL:
    default:
        dev->bc.bc_red = brightness;
        dev->bc.bc_green = brightness;
        dev->bc.bc_blue = brightness;
        break;
    }
}

/**
 * tlc5971 set channel rgb
 *
 * Sets the RBG 16-bit grayscale values for a LED channel (or all channels)
 *
 * @param dev Pointer to tlc5971 device
 * @param channel The RGB channel to set (or all).
 * @param red The 16-bit PWM grayscale value (0 - 65535) for the red led
 * @param green The 16-bit PWM grayscale value (0 - 65535) for the green led
 * @param blue The 16-bit PWM grayscale value (0 - 65535) for the blue led
 */
void
tlc5971_set_channel_rgb(struct tlc5971_dev *dev, tlc5971_channel_t channel,
                        uint16_t red, uint16_t green, uint16_t blue)
{
    uint8_t i;

    if (channel == TLC5971_ALL_CHANNELS) {
        for (i = 0; i < TLC5971_NUM_LED_CHANNELS; i++) {
            dev->gs[i].gs_red = red;
            dev->gs[i].gs_green = green;
            dev->gs[i].gs_blue = blue;
        }
    } else {
        dev->gs[channel].gs_red = red;
        dev->gs[channel].gs_green = green;
        dev->gs[channel].gs_blue = blue;
    }
}

/**
 * tlc5971 set cfg
 *
 * Sets the device configuration
 *
 * @param dev Pointer to tlc5971 device
 * @param cfg Pointer to config structure
 */
void
tlc5971_set_cfg(struct tlc5971_dev *dev, struct tlc5971_cfg *cfg)
{
    dev->control_data = cfg->tlc_ctrl_data;
}

/**
 * tlc get cfg
 *
 * Returns the current config structure.
 *
 * @param dev Pointer to tlc5971 device
 * @param cfg Pointer to config structure
 */
void
tlc5971_get_cfg(struct tlc5971_dev *dev, struct tlc5971_cfg *cfg)
{
    cfg->tlc_ctrl_data = dev->control_data;
}

/**
 * tlc5971 init
 *
 * @param odev
 * @param arg
 *
 * @return int
 */
int
tlc5971_init(struct os_dev *odev, void *arg)
{
    struct tlc5971_dev *dev;
    struct tlc5971_periph_itf *itf;

    dev = (struct tlc5971_dev *)odev;
    itf = (struct tlc5971_periph_itf *)arg;

    /* Copy the interface */
    memcpy(&dev->tlc_itf, itf, sizeof(struct tlc5971_periph_itf));

    /*
     * Set up a default config. This is the following:
     *  - Unblanks the leds
     *  - Enable auto repeat
     *  - Enable timing reset
     *  - Use internal clock for PWM
     *  - Set GS reference clock edge to rising
     */
    dev->control_data = TLC5971_DSPRPT_MASK | TLC5971_TMGRST_MASK |
                        TLC5971_OUTTMG_MASK;

     /* Set the device open and close handlers */
    OS_DEV_SETHANDLERS(odev, tlc5971_open, tlc5971_close);

    return 0;
}
