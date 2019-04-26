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

/**
 * @file
 * @brief Public Monochrome Character Framebuffer API
 */

#ifndef __CFB_H__
#define __CFB_H__

#include "os/mynewt.h"
#include "display/display.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Display Drivers
 * @defgroup display_interfaces Display Drivers
 * @{
 * @}
 */

/**
 * @brief Public Monochrome Character Framebuffer API
 * @defgroup monochrome_character_framebuffer Monochrome Character Framebuffer
 * @ingroup display_interfaces
 * @{
 */

enum cfb_display_param {
	CFB_DISPLAY_HEIGH		= 0,
	CFB_DISPLAY_WIDTH,
	CFB_DISPLAY_PPT,
	CFB_DISPLAY_ROWS,
	CFB_DISPLAY_COLS,
};

enum cfb_font_caps {
	CFB_FONT_MONO_VPACKED		= (1UL << 0),
	CFB_FONT_MONO_HPACKED		= (1UL << 1),
};

struct cfb_font {
	const void *data;
	uint8_t width;
	uint8_t height;
	enum cfb_font_caps caps;
	uint8_t first_char;
	uint8_t last_char;
};

#define CFB_FONTS_COUNT 3

/**
 * @brief Macro for creating a font entry.
 *
 * @param _name   Name of the font entry.
 * @param _width  Width of the font in pixels
 * @param _height Heigth of the font in pixels.
 * @param _caps   Font capabilities.
 * @param _data   Raw data of the font.
 * @param _fc     Character mapped to first font element.
 * @param _lc     Character mapped to last font element.
 */
#define FONT_ENTRY_DEFINE(_name, _width, _height, _caps, _data, _fc, _lc)      \
	{								       \
		.width = _width,					       \
		.height = _height,					       \
		.caps = _caps,						       \
		.data = _data,						       \
		.first_char = _fc,					       \
		.last_char = _lc,					       \
	}

/**
 * @brief Print a string into the framebuffer.
 *
 * @param dev Pointer to device structure for driver instance
 * @param str String to print
 * @param x Position in X direction of the beginning of the string
 * @param y Position in Y direction of the beginning of the string
 *
 * @return 0 on success, negative value otherwise
 */
int cfb_print(struct os_dev *dev, char *str, uint16_t x, uint16_t y);

/**
 * @brief Clear framebuffer.
 *
 * @param dev Pointer to device structure for driver instance
 * @param clear_display Clear the display as well
 *
 * @return 0 on success, negative value otherwise
 */
int cfb_framebuffer_clear(struct os_dev *dev, bool clear_display);

/**
 * @brief Finalize framebuffer and write it to display RAM,
 * invert or reorder pixels if necessary.
 *
 * @param dev Pointer to device structure for driver instance
 *
 * @return 0 on success, negative value otherwise
 */
int cfb_framebuffer_finalize(struct os_dev *dev);

/**
 * @brief Get display parameter.
 *
 * @param dev Pointer to device structure for driver instance
 * @param cfb_display_param One of the display parameters
 *
 * @return Display parameter value
 */
int cfb_get_display_parameter(struct os_dev *dev, enum cfb_display_param);

/**
 * @brief Set font.
 *
 * @param dev Pointer to device structure for driver instance
 * @param idx Font index
 *
 * @return 0 on success, negative value otherwise
 */
int cfb_framebuffer_set_font(struct os_dev *dev, uint8_t idx);

/**
 * @brief Get font size.
 *
 * @param dev Pointer to device structure for driver instance
 * @param idx Font index
 * @param width Pointers to the variable where the font width will be stored.
 * @param height Pointers to the variable where the font height will be stored.
 *
 * @return 0 on success, negative value otherwise
 */
int cfb_get_font_size(struct os_dev *dev, uint8_t idx,
		      uint8_t *width, uint8_t *height);

/**
 * @brief Initialize Character Framebuffer.
 *
 * @param dev Pointer to device structure for driver instance
 *
 * @return 0 on success, negative value otherwise
 */
int cfb_framebuffer_init(struct os_dev *dev);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* __CFB_H__ */
