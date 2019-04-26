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

#include "os/mynewt.h"
#include "modlog/modlog.h"
#include "display/cfb.h"

extern const struct cfb_font font_array[CFB_FONTS_COUNT];

struct char_framebuffer {
	/** Pointer to a buffer in RAM */
	uint8_t *buf;

	/** Size of the framebuffer */
	uint32_t size;

	/** Pointer to the font entry array */
	const struct cfb_font *fonts;

	/** Display pixel format */
	enum display_pixel_format pixel_format;

	/** Display screen info */
	enum display_screen_info screen_info;

	/** Resolution of a framebuffer in pixels in X direction */
	uint8_t x_res;

	/** Resolution of a framebuffer in pixels in Y direction */
	uint8_t y_res;

	/** Number of pixels per tile, typically 8 */
	uint8_t ppt;

	/** Number of available fonts */
	uint8_t numof_fonts;

	/** Current font index */
	uint8_t font_idx;

	/** Font kerning */
	int8_t kerning;

	/** Inverted */
	bool inverted;
};

static struct char_framebuffer char_fb;

static inline uint8_t *get_glyph_ptr(const struct cfb_font *fptr, char c)
{
	if (fptr->caps & CFB_FONT_MONO_VPACKED) {
		return (uint8_t *)fptr->data +
		       (c - fptr->first_char) *
		       (fptr->width * fptr->height / 8);
	}

	return NULL;
}

/*
 * Draw the monochrome character in the monochrome tiled framebuffer,
 * a byte is interpreted as 8 pixels ordered vertically among each other.
 */
static uint8_t draw_char_vtmono(const struct char_framebuffer *fb,
			     char c, uint16_t x, uint16_t y)
{
	const struct cfb_font *fptr = &(fb->fonts[fb->font_idx]);
	uint8_t *glyph_ptr;

	if (c < fptr->first_char || c > fptr->last_char) {
		c = ' ';
	}

	glyph_ptr = get_glyph_ptr(fptr, c);
	if (!glyph_ptr) {
		return 0;
	}

	for (size_t g_x = 0; g_x < fptr->width; g_x++) {
		uint32_t y_segment = y / 8;

		for (size_t g_y = 0; g_y < fptr->height / 8; g_y++) {
			uint32_t fb_y = (y_segment + g_y) * fb->x_res;

			if ((fb_y + x + g_x) >= fb->size) {
				return 0;
			}
			fb->buf[fb_y + x + g_x] =
				glyph_ptr[g_x * (fptr->height / 8) + g_y];
		}

	}

	return fptr->width;
}

int cfb_print(struct os_dev *dev, char *str, uint16_t x, uint16_t y)
{
	const struct char_framebuffer *fb = &char_fb;
	const struct cfb_font *fptr = &(fb->fonts[fb->font_idx]);

	if (!fb->fonts || !fb->buf) {
		return -1;
	}

	if (fptr->height % 8) {
		MODLOG_DFLT(ERROR, "Wrong font size");
		return -1;
	}

	if ((fb->screen_info & SCREEN_INFO_MONO_VTILED) && !(y % 8)) {
		for (size_t i = 0; i < strlen(str); i++) {
			if (x + fptr->width > fb->x_res) {
				x = 0;
				y += fptr->height;
			}
			x += fb->kerning + draw_char_vtmono(fb, str[i], x, y);
		}
		return 0;
	}

	MODLOG_DFLT(ERROR, "Unsupported framebuffer configuration");
	return -1;
}

static int cfb_reverse_bytes(const struct char_framebuffer *fb)
{
	if (!(fb->screen_info & SCREEN_INFO_MONO_VTILED)) {
		MODLOG_DFLT(ERROR, "Unsupported framebuffer configuration");
		return -1;
	}

	for (size_t i = 0; i < fb->x_res * fb->y_res / 8; i++) {
		fb->buf[i] = (fb->buf[i] & 0xf0) >> 4 |
			      (fb->buf[i] & 0x0f) << 4;
		fb->buf[i] = (fb->buf[i] & 0xcc) >> 2 |
			      (fb->buf[i] & 0x33) << 2;
		fb->buf[i] = (fb->buf[i] & 0xaa) >> 1 |
			      (fb->buf[i] & 0x55) << 1;
	}

	return 0;
}

static int cfb_invert(const struct char_framebuffer *fb)
{
	for (size_t i = 0; i < fb->x_res * fb->y_res / 8; i++) {
		fb->buf[i] = ~fb->buf[i];
	}

	return 0;
}

int cfb_framebuffer_clear(struct os_dev *dev, bool clear_display)
{
	const struct display_driver_api *api = dev->od_init_arg;
	const struct char_framebuffer *fb = &char_fb;
	struct display_buffer_descriptor desc;
	int rc;

	if (!fb || !fb->buf) {
		return -1;
	}

	desc.buf_size = fb->size;
	desc.width = 0;
	desc.height = 0;
	desc.pitch = 0;
	memset(fb->buf, 0, fb->size);

	if (clear_display && (fb->screen_info & SCREEN_INFO_EPD)) {
		rc = api->set_contrast(dev, 1);
		if (rc) {
			return rc;
		}

		rc = api->write(dev, 0, 0, &desc, fb->buf);
		if (rc) {
			return rc;
		}

		rc = api->set_contrast(dev, 0);
		if (rc) {
			return rc;
		}
	}

	return 0;
}

int cfb_framebuffer_finalize(struct os_dev *dev)
{
	const struct display_driver_api *api = dev->od_init_arg;
	const struct char_framebuffer *fb = &char_fb;
	struct display_buffer_descriptor desc;
	int rc;

	if (!fb || !fb->buf) {
		return -1;
	}

	desc.buf_size = fb->size;
	desc.width = 0;
	desc.height = 0;
	desc.pitch = 0;

	if (!(fb->pixel_format & PIXEL_FORMAT_MONO10) != !(fb->inverted)) {
		cfb_invert(fb);
	}

	if (fb->screen_info & SCREEN_INFO_MONO_MSB_FIRST) {
		cfb_reverse_bytes(fb);
	}

	rc = api->write(dev, 0, 0, &desc, fb->buf);
	return rc;
}

int cfb_get_display_parameter(struct os_dev *dev,
			       enum cfb_display_param param)
{
	const struct char_framebuffer *fb = &char_fb;

	switch (param) {
	case CFB_DISPLAY_HEIGH:
		return fb->y_res;
	case CFB_DISPLAY_WIDTH:
		return fb->x_res;
	case CFB_DISPLAY_PPT:
		return fb->ppt;
	case CFB_DISPLAY_ROWS:
		if (fb->screen_info & SCREEN_INFO_MONO_VTILED) {
			return fb->y_res / fb->ppt;
		}
		return fb->y_res;
	case CFB_DISPLAY_COLS:
		if (fb->screen_info & SCREEN_INFO_MONO_VTILED) {
			return fb->x_res;
		}
		return fb->x_res / fb->ppt;
	}
	return 0;
}

int cfb_framebuffer_set_font(struct os_dev *dev, uint8_t idx)
{
	struct char_framebuffer *fb = &char_fb;

	if (idx >= fb->numof_fonts) {
		return -1;
	}

	fb->font_idx = idx;

	return 0;
}

int cfb_get_font_size(struct os_dev *dev, uint8_t idx,
		      uint8_t *width, uint8_t *height)
{
	const struct char_framebuffer *fb = &char_fb;

	if (idx >= fb->numof_fonts) {
		return -1;
	}

	if (width) {
		*width = font_array[idx].width;
	}

	if (height) {
		*height = font_array[idx].height;
	}

	return 0;
}

int cfb_framebuffer_init(struct os_dev *dev)
{
	const struct display_driver_api *api = dev->od_init_arg;
	struct char_framebuffer *fb = &char_fb;
	struct display_capabilities cfg;

	api->get_capabilities(dev, &cfg);


	fb->numof_fonts = (sizeof(font_array) / sizeof((font_array)[0]));
	MODLOG_DFLT(DEBUG, "number of fonts %d", fb->numof_fonts);
	if (!fb->numof_fonts) {
		return -1;
	}

	fb->x_res = cfg.x_resolution;
	fb->y_res = cfg.y_resolution;
	fb->ppt = 8;
	fb->pixel_format = cfg.current_pixel_format;
	fb->screen_info = cfg.screen_info;
	fb->buf = NULL;
	fb->font_idx = 0;
	fb->kerning = 0;
	fb->inverted = false;

	fb->fonts = font_array;
	fb->font_idx = 0;

	fb->size = fb->x_res * fb->y_res / fb->ppt;
	fb->buf = malloc(fb->size);
	if (!fb->buf) {
		return -1;
	}

	memset(fb->buf, 0, fb->size);

	return 0;
}
