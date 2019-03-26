/*
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
 *
 * Copyright (c) 2018 PHYTEC Messtechnik GmbH
 *
 */

#include <stdint.h>

#include "os/mynewt.h"
#include "hal/hal_gpio.h"
#include "hal/hal_spi.h"
#include "modlog/modlog.h"

#include "display/display.h"
#include "display/cfb.h"

#include "ssd1673_regs.h"

#define EPD_PANEL_WIDTH			250
#define EPD_PANEL_HEIGHT		120
#define EPD_PANEL_NUMOF_COLUMS		250
#define EPD_PANEL_NUMOF_ROWS_PER_PAGE	8
#define EPD_PANEL_NUMOF_PAGES		(EPD_PANEL_HEIGHT / \
					 EPD_PANEL_NUMOF_ROWS_PER_PAGE)

#define SSD1673_PANEL_FIRST_PAGE	0
#define SSD1673_PANEL_LAST_PAGE		(EPD_PANEL_NUMOF_PAGES - 1)
#define SSD1673_PANEL_FIRST_GATE	0
#define SSD1673_PANEL_LAST_GATE		249

struct ssd1673_data {
	struct display_driver_api driver_api;
	struct hal_spi_settings spi_config;
	uint8_t contrast;
	uint8_t scan_mode;
	uint8_t last_lut;
	uint8_t numof_part_cycles;
};

static struct ssd1673_data ssd1673_driver;
static struct os_dev ssd1673;

#define SSD1673_LAST_LUT_INITIAL		0
#define SSD1673_LAST_LUT_DEFAULT		255
#define SSD1673_LUT_SIZE			29

static uint8_t ssd1673_lut_initial[SSD1673_LUT_SIZE] = {
	0x22, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x11,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E,
	0x01, 0x00, 0x00, 0x00, 0x00
};

static uint8_t ssd1673_lut_default[SSD1673_LUT_SIZE] = {
	0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x0F, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00
};

#define SSD1673_BUSY_DELAY_TICKS os_time_ms_to_ticks32(SSD1673_BUSY_DELAY)
#define SSD1673_RESET_DELAY_TICKS os_time_ms_to_ticks32(SSD1673_RESET_DELAY)

#define CONFIG_SSD1673_OS_DEV_NAME	MYNEWT_VAL(SSD1673_OS_DEV_NAME)
#define CONFIG_SSD1673_BUSY_PIN		MYNEWT_VAL(SSD1673_BUSY_PIN)
#define CONFIG_SSD1673_RESET_PIN	MYNEWT_VAL(SSD1673_RESET_PIN)
#define CONFIG_SSD1673_DC_PIN		MYNEWT_VAL(SSD1673_DC_PIN)
#define CONFIG_SSD1673_CS_PIN		MYNEWT_VAL(SSD1673_CS_PIN)
#define CONFIG_SSD1673_SPI_FREQ		MYNEWT_VAL(SSD1673_SPI_FREQ)
#define CONFIG_SSD1673_SPI_DEV		MYNEWT_VAL(SSD1673_SPI_DEV)

static inline int ssd1673_write_cmd(struct ssd1673_data *driver,
				    uint8_t cmd, uint8_t *data, size_t len)
{
	hal_gpio_write(CONFIG_SSD1673_DC_PIN, 0);
	hal_gpio_write(CONFIG_SSD1673_CS_PIN, 0);

	if (hal_spi_txrx(CONFIG_SSD1673_SPI_DEV,
			 &cmd, NULL, sizeof(cmd))) {
		hal_gpio_write(CONFIG_SSD1673_CS_PIN, 1);
		return -1;
	}

	if (data != NULL) {
		hal_gpio_write(CONFIG_SSD1673_DC_PIN, 1);
		if (hal_spi_txrx(CONFIG_SSD1673_SPI_DEV,
				 data, NULL, len)) {
			hal_gpio_write(CONFIG_SSD1673_CS_PIN, 1);
			return -1;
		}
	}

	hal_gpio_write(CONFIG_SSD1673_CS_PIN, 1);
	return 0;
}

static inline void ssd1673_busy_wait(struct ssd1673_data *driver)
{
	int val = 0;

	val = hal_gpio_read(CONFIG_SSD1673_BUSY_PIN);
	while (val) {
		os_cputime_delay_ticks(SSD1673_BUSY_DELAY_TICKS);
		val = hal_gpio_read(CONFIG_SSD1673_BUSY_PIN);
	};
}

static inline int ssd1673_set_ram_param(struct ssd1673_data *driver,
					uint8_t sx, uint8_t ex,
					uint8_t sy, uint8_t ey)
{
	uint8_t tmp[2];

	tmp[0] = sx; tmp[1] = ex;
	if (ssd1673_write_cmd(driver, SSD1673_CMD_RAM_XPOS_CTRL,
			      tmp, sizeof(tmp))) {
		return -1;
	}

	tmp[0] = sy; tmp[1] = ey;
	if (ssd1673_write_cmd(driver, SSD1673_CMD_RAM_YPOS_CTRL,
			      tmp, sizeof(tmp))) {
		return -1;
	}

	return 0;
}

static inline int ssd1673_set_ram_ptr(struct ssd1673_data *driver,
				       uint8_t x, uint8_t y)
{
	if (ssd1673_write_cmd(driver, SSD1673_CMD_RAM_XPOS_CNTR,
			      &x, sizeof(x))) {
		return -1;
	}

	if (ssd1673_write_cmd(driver, SSD1673_CMD_RAM_YPOS_CNTR,
			      &y, sizeof(y))) {
		return -1;
	}

	return 0;
}

static inline void ssd1673_set_orientation(struct ssd1673_data *driver)

{
#if CONFIG_SSD1673_ORIENTATION_FLIPPED == 1
	driver->scan_mode = SSD1673_DATA_ENTRY_XIYDY;
#else
	driver->scan_mode = SSD1673_DATA_ENTRY_XDYIY;
#endif
}

int ssd1673_resume(const struct os_dev *dev)
{
	struct ssd1673_data *driver = dev->od_init_arg;
	uint8_t tmp;

	/*
	 * Uncomment for voltage measurement
	 * tmp = SSD1673_CTRL2_ENABLE_CLK;
	 * ssd1673_write_cmd(SSD1673_CMD_UPDATE_CTRL2, &tmp, sizeof(tmp));
	 * ssd1673_write_cmd(SSD1673_CMD_MASTER_ACTIVATION, NULL, 0);
	 */

	tmp = SSD1673_SLEEP_MODE_PON;
	return ssd1673_write_cmd(driver, SSD1673_CMD_SLEEP_MODE,
				 &tmp, sizeof(tmp));
}

static int ssd1673_suspend(const struct os_dev *dev)
{
	struct ssd1673_data *driver = dev->od_init_arg;
	uint8_t tmp = SSD1673_SLEEP_MODE_DSM;

	return ssd1673_write_cmd(driver, SSD1673_CMD_SLEEP_MODE,
				 &tmp, sizeof(tmp));
}

static int ssd1673_update_display(const struct os_dev *dev, bool initial)
{
	struct ssd1673_data *driver = dev->od_init_arg;
	uint8_t tmp;

	tmp = SSD1673_CTRL1_INITIAL_UPDATE_LH;
	if (ssd1673_write_cmd(driver, SSD1673_CMD_UPDATE_CTRL1,
			      &tmp, sizeof(tmp))) {
		return -1;
	}

	if (initial) {
		driver->numof_part_cycles = 0;
		driver->last_lut = SSD1673_LAST_LUT_INITIAL;
		if (ssd1673_write_cmd(driver, SSD1673_CMD_UPDATE_LUT,
				      ssd1673_lut_initial,
				      sizeof(ssd1673_lut_initial))) {
			return -1;
		}

	} else {
		driver->numof_part_cycles++;
		if (driver->last_lut != SSD1673_LAST_LUT_DEFAULT) {
			driver->last_lut = SSD1673_LAST_LUT_DEFAULT;
			if (ssd1673_write_cmd(driver, SSD1673_CMD_UPDATE_LUT,
					      ssd1673_lut_default,
					      sizeof(ssd1673_lut_default))) {
				return -1;
			}
		}
	}

	tmp = (SSD1673_CTRL2_ENABLE_CLK |
	       SSD1673_CTRL2_ENABLE_ANALOG |
	       SSD1673_CTRL2_TO_PATTERN |
	       SSD1673_CTRL2_DISABLE_ANALOG |
	       SSD1673_CTRL2_DISABLE_CLK);
	if (ssd1673_write_cmd(driver, SSD1673_CMD_UPDATE_CTRL2,
			      &tmp, sizeof(tmp))) {
		return -1;
	}

	if (ssd1673_write_cmd(driver, SSD1673_CMD_MASTER_ACTIVATION,
			      NULL, 0)) {
		return -1;
	}

	return 0;
}

static int ssd1673_write(const struct os_dev *dev, const uint16_t x,
			 const uint16_t y,
			 const struct display_buffer_descriptor *desc,
			 const void *buf)
{
	struct ssd1673_data *driver = dev->od_init_arg;
	uint8_t cmd = SSD1673_CMD_WRITE_RAM;
	uint8_t dummy_page[SSD1673_RAM_YRES];
	bool update = true;

	if (desc->pitch < desc->width) {
		MODLOG_DFLT(ERROR, "Pitch is smaller then width");
		return -1;
	}

	if (buf == NULL || desc->buf_size == 0) {
		MODLOG_DFLT(ERROR, "Display buffer is not available");
		return -1;
	}

	if (desc->pitch > desc->width) {
		MODLOG_DFLT(ERROR, "Unsupported mode");
		return -1;
	}

	if (x != 0 && y != 0) {
		MODLOG_DFLT(ERROR, "Unsupported origin");
		return -1;
	}


	ssd1673_busy_wait(driver);
	memset(dummy_page, 0xff, sizeof(dummy_page));

	switch (driver->scan_mode) {
	case SSD1673_DATA_ENTRY_XIYDY:
		if (ssd1673_set_ram_param(driver,
					  SSD1673_PANEL_FIRST_PAGE,
					  SSD1673_PANEL_LAST_PAGE + 1,
					  SSD1673_PANEL_LAST_GATE,
					  SSD1673_PANEL_FIRST_GATE)) {
			return -1;
		}

		if (ssd1673_set_ram_ptr(driver,
					SSD1673_PANEL_FIRST_PAGE,
					SSD1673_PANEL_LAST_GATE)) {
			return -1;
		}

		break;

	case SSD1673_DATA_ENTRY_XDYIY:
		if (ssd1673_set_ram_param(driver,
					  SSD1673_PANEL_LAST_PAGE + 1,
					  SSD1673_PANEL_FIRST_PAGE,
					  SSD1673_PANEL_FIRST_GATE,
					  SSD1673_PANEL_LAST_GATE)) {
			return -1;
		}

		if (ssd1673_set_ram_ptr(driver,
					SSD1673_PANEL_LAST_PAGE + 1,
					SSD1673_PANEL_FIRST_GATE)) {
			return -1;
		}

		break;
	default:
		return -1;
	}

	if (ssd1673_write_cmd(driver, SSD1673_CMD_ENTRY_MODE,
			      &driver->scan_mode, sizeof(driver->scan_mode))) {
		return -1;
	}

	hal_gpio_write(CONFIG_SSD1673_DC_PIN, 0);
	hal_gpio_write(CONFIG_SSD1673_CS_PIN, 0);

	if (hal_spi_txrx(CONFIG_SSD1673_SPI_DEV, &cmd, NULL, sizeof(cmd))) {
		hal_gpio_write(CONFIG_SSD1673_CS_PIN, 1);
		return -1;
	}

	hal_gpio_write(CONFIG_SSD1673_DC_PIN, 1);
	/* clear unusable page */
	if (driver->scan_mode == SSD1673_DATA_ENTRY_XDYIY) {
		if (hal_spi_txrx(CONFIG_SSD1673_SPI_DEV, dummy_page,
				 NULL, sizeof(dummy_page))) {
			hal_gpio_write(CONFIG_SSD1673_CS_PIN, 1);
			return -1;
		}
	}

	if (hal_spi_txrx(CONFIG_SSD1673_SPI_DEV, (uint8_t *)buf,
			 NULL, desc->buf_size)) {
		hal_gpio_write(CONFIG_SSD1673_CS_PIN, 1);
		return -1;
	}

	/* clear unusable page */
	if (driver->scan_mode == SSD1673_DATA_ENTRY_XIYDY) {
		if (hal_spi_txrx(CONFIG_SSD1673_SPI_DEV, dummy_page,
				 NULL, sizeof(dummy_page))) {
			hal_gpio_write(CONFIG_SSD1673_CS_PIN, 1);
			return -1;
		}
	}

	hal_gpio_write(CONFIG_SSD1673_CS_PIN, 1);

	if (update) {
		if (driver->contrast) {
			return ssd1673_update_display(dev, true);
		}
		return ssd1673_update_display(dev, false);
	}

	return 0;
}

static int ssd1673_read(const struct os_dev *dev, const uint16_t x,
			const uint16_t y,
			const struct display_buffer_descriptor *desc,
			void *buf)
{
	MODLOG_DFLT(ERROR, "not supported");
	return -1;
}

static void *ssd1673_get_framebuffer(const struct os_dev *dev)
{
	MODLOG_DFLT(ERROR, "not supported");
	return NULL;
}

static int ssd1673_set_brightness(const struct os_dev *dev,
				  const uint8_t brightness)
{
	MODLOG_DFLT(WARN, "not supported");
	return -1;
}

static int ssd1673_set_contrast(const struct os_dev *dev, uint8_t contrast)
{
	struct ssd1673_data *driver = dev->od_init_arg;

	driver->contrast = contrast;

	return 0;
}

static void ssd1673_get_capabilities(const struct os_dev *dev,
				     struct display_capabilities *caps)
{
	memset(caps, 0, sizeof(struct display_capabilities));
	caps->x_resolution = EPD_PANEL_WIDTH;
	caps->y_resolution = EPD_PANEL_HEIGHT;
	caps->supported_pixel_formats = PIXEL_FORMAT_MONO10;
	caps->current_pixel_format = PIXEL_FORMAT_MONO10;
	caps->screen_info = SCREEN_INFO_MONO_VTILED |
			    SCREEN_INFO_MONO_MSB_FIRST |
			    SCREEN_INFO_EPD;
}

static int ssd1673_set_pixel_format(const struct os_dev *dev,
				    const enum display_pixel_format pf)
{
	MODLOG_DFLT(ERROR, "not supported");
	return -1;
}

static int ssd1673_controller_init(struct os_dev *dev)
{
	struct ssd1673_data *driver = dev->od_init_arg;
	uint8_t tmp[3];

	MODLOG_DFLT(DEBUG, "");

	hal_gpio_write(CONFIG_SSD1673_RESET_PIN, 0);
	os_time_delay(SSD1673_RESET_DELAY_TICKS);
	hal_gpio_write(CONFIG_SSD1673_RESET_PIN, 1);
	os_time_delay(SSD1673_RESET_DELAY_TICKS);
	ssd1673_busy_wait(driver);

	if (ssd1673_write_cmd(driver, SSD1673_CMD_SW_RESET, NULL, 0)) {
		return -1;
	}
	ssd1673_busy_wait(driver);

	tmp[0] = (SSD1673_RAM_YRES - 1);
	tmp[1] = 0;
	if (ssd1673_write_cmd(driver, SSD1673_CMD_GDO_CTRL, tmp, 2)) {
		return -1;
	}

	tmp[0] = SSD1673_VAL_GDV_CTRL_A;
	tmp[1] = SSD1673_VAL_GDV_CTRL_B;
	if (ssd1673_write_cmd(driver, SSD1673_CMD_GDV_CTRL, tmp, 2)) {
		return -1;
	}

	tmp[0] = SSD1673_VAL_SDV_CTRL;
	if (ssd1673_write_cmd(driver, SSD1673_CMD_SDV_CTRL, tmp, 1)) {
		return -1;
	}

	tmp[0] = SSD1673_VAL_VCOM_VOLTAGE;
	if (ssd1673_write_cmd(driver, SSD1673_CMD_VCOM_VOLTAGE, tmp, 1)) {
		return -1;
	}

	tmp[0] = SSD1673_VAL_DUMMY_LINE;
	if (ssd1673_write_cmd(driver, SSD1673_CMD_DUMMY_LINE, tmp, 1)) {
		return -1;
	}

	tmp[0] = SSD1673_VAL_GATE_LWIDTH;
	if (ssd1673_write_cmd(driver, SSD1673_CMD_GATE_LINE_WIDTH, tmp, 1)) {
		return -1;
	}

	ssd1673_set_orientation(driver);
	driver->numof_part_cycles = 0;
	driver->last_lut = SSD1673_LAST_LUT_INITIAL;
	driver->contrast = 0;

	return 0;
}

static int ssd1673_init(struct os_dev *dev, void *arg)
{
	struct ssd1673_data *driver = dev->od_init_arg;
	int rc;

	MODLOG_DFLT(DEBUG, "");

	driver->spi_config.baudrate = CONFIG_SSD1673_SPI_FREQ;
	driver->spi_config.data_mode = HAL_SPI_MODE0;
	driver->spi_config.data_order = HAL_SPI_MSB_FIRST;
	driver->spi_config.word_size = HAL_SPI_WORD_SIZE_8BIT;
	rc = hal_spi_config(0, &driver->spi_config);
	assert(rc == 0);

	driver->driver_api.blanking_on = ssd1673_resume,
	driver->driver_api.blanking_off = ssd1673_suspend,
	driver->driver_api.write = ssd1673_write,
	driver->driver_api.read = ssd1673_read,
	driver->driver_api.get_framebuffer = ssd1673_get_framebuffer,
	driver->driver_api.set_brightness = ssd1673_set_brightness,
	driver->driver_api.set_contrast = ssd1673_set_contrast,
	driver->driver_api.get_capabilities = ssd1673_get_capabilities,
	driver->driver_api.set_pixel_format = ssd1673_set_pixel_format,
	driver->driver_api.set_orientation = NULL,

	rc = hal_gpio_init_out(CONFIG_SSD1673_RESET_PIN, 1);
	assert(rc == 0);
	rc = hal_gpio_init_out(CONFIG_SSD1673_DC_PIN, 1);
	assert(rc == 0);
	rc = hal_gpio_init_out(CONFIG_SSD1673_CS_PIN, 1);
	assert(rc == 0);
	rc = hal_gpio_init_in(CONFIG_SSD1673_BUSY_PIN, HAL_GPIO_PULL_NONE);
	assert(rc == 0);

	ssd1673_controller_init(dev);

	return 0;
}

int ssd1673_pkg_init(void)
{
	int rc;

	/* Ensure this function only gets called by sysinit. */
	SYSINIT_ASSERT_ACTIVE();

	rc = os_dev_create(&ssd1673, CONFIG_SSD1673_OS_DEV_NAME,
			   OS_DEV_INIT_SECONDARY, OS_DEV_INIT_PRIO_DEFAULT,
			   ssd1673_init, &ssd1673_driver);
	SYSINIT_PANIC_ASSERT(rc == 0);

	return 0;
}
