/*
 * Copyright 2025, STMicroelectronics
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(test_display, LOG_LEVEL_DBG);

/* Color definitions for RGB888 format */
#define COLOR_RED     0x00FF0000
#define COLOR_GREEN   0x0000FF00
#define COLOR_BLUE    0x000000FF
#define COLOR_WHITE   0x00FFFFFF
#define COLOR_BLACK   0x00000000
#define COLOR_YELLOW  0x00FFFF00
#define COLOR_CYAN    0x0000FFFF
#define COLOR_MAGENTA 0x00FF00FF

static void draw_test_pattern(const struct device *display_dev, 
                             uint16_t width, uint16_t height)
{
	struct display_buffer_descriptor desc;
	uint32_t *buffer;
	uint32_t colors[] = {COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_WHITE,
	                     COLOR_YELLOW, COLOR_CYAN, COLOR_MAGENTA, COLOR_BLACK};
	
	/* Try to allocate buffer for full display first */
	uint16_t test_height = height; /* Try full display height */
	size_t buffer_size = width * test_height * sizeof(uint32_t);
	int ret;
	
	/* Allocate frame buffer */
	buffer = k_malloc(buffer_size);
	if (!buffer) {
		LOG_ERR("Failed to allocate full frame buffer (size: %zu bytes)", buffer_size);
		
		/* Try half screen */
		test_height = height / 2;
		buffer_size = width * test_height * sizeof(uint32_t);
		buffer = k_malloc(buffer_size);
		if (!buffer) {
			LOG_ERR("Failed to allocate half frame buffer (size: %zu bytes)", buffer_size);
			
			/* Fall back to 64 lines */
			test_height = 64;
			buffer_size = width * test_height * sizeof(uint32_t);
			buffer = k_malloc(buffer_size);
			if (!buffer) {
				LOG_ERR("Failed to allocate small test buffer (size: %zu bytes)", buffer_size);
				return;
			}
		}
	}

	LOG_INF("Drawing test pattern (%dx%d)...", width, test_height);

	/* Fill buffer with a solid bright color first to test basic functionality */
	uint32_t test_color = COLOR_WHITE; /* Start with white - most visible */
	
	if (test_height >= height) {
		/* Full screen - use color bars */
		uint16_t bar_height = test_height / 8;
		if (bar_height == 0) bar_height = 1;
		
		for (int y = 0; y < test_height; y++) {
			uint32_t color = colors[(y / bar_height) % 8];
			for (int x = 0; x < width; x++) {
				buffer[y * width + x] = color;
			}
		}
	} else {
		/* Partial screen - use solid color for maximum visibility */
		for (int y = 0; y < test_height; y++) {
			for (int x = 0; x < width; x++) {
				buffer[y * width + x] = test_color;
			}
		}
	}

	/* Setup buffer descriptor */
	desc.buf_size = buffer_size;
	desc.width = width;
	desc.height = test_height;
	desc.pitch = width;

	/* Write to display */
	ret = display_write(display_dev, 0, 0, &desc, buffer);
	if (ret != 0) {
		LOG_WRN("Failed to write to display, error: %d", ret);
	} else {
		LOG_INF("Test pattern displayed successfully (%d lines)", test_height);
	}

	k_free(buffer);
}

int main(void)
{
	const struct device *display_dev;
	struct display_capabilities capabilities;
	uint32_t frame_counter = 0;

	LOG_INF("STM32U5G9J-DK1 HX8379-C Display Test");

	display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
	if (!device_is_ready(display_dev)) {
		LOG_ERR("Display device not ready");
		return -ENODEV;
	}

	LOG_INF("Display device ready");

	display_get_capabilities(display_dev, &capabilities);
	LOG_INF("Display capabilities:");
	LOG_INF("  Resolution: %dx%d", capabilities.x_resolution, capabilities.y_resolution);
	LOG_INF("  Pixel format: 0x%x", capabilities.current_pixel_format);
	LOG_INF("  Orientation: %d", capabilities.current_orientation);

	/* Turn on display */
	int ret = display_blanking_off(display_dev);
	if (ret != 0) {
		LOG_WRN("Failed to turn on display, error: %d", ret);
		/* Continue anyway - backlight might still work */
	} else {
		LOG_INF("Display turned on successfully");
	}

	/* Draw initial test pattern */
	draw_test_pattern(display_dev, capabilities.x_resolution, capabilities.y_resolution);

	LOG_INF("HX8379C test completed. Backlight should be on.");
	LOG_INF("If you see backlight but no image, the DSI video mode configuration needs adjustment.");

	while (1) {
		frame_counter++;
		LOG_INF("Frame %d - Display active", frame_counter);
		
		/* Redraw test pattern every 10 seconds to test refresh */
		if (frame_counter % 10 == 0) {
			LOG_INF("Refreshing display...");
			draw_test_pattern(display_dev, capabilities.x_resolution, capabilities.y_resolution);
		}
		
		k_sleep(K_SECONDS(1));
	}

	return 0;
}
