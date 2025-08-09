/*
 * Copyright (c) 2025 Charles Dias
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/display.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(dsi_minimal, LOG_LEVEL_INF);

/* Global display parameters - will be set from device capabilities */
static uint32_t lcd_width = 0;
static uint32_t lcd_height = 0;
static uint32_t *framebuffer = NULL;

/* Forward declarations */
static void demo_pattern(void);
static void fill_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color);

/* Get the display device using Zephyr device tree */
static const struct device *get_display_device(void)
{
	const struct device *dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

	if (!dev) {
		LOG_ERR("No display device found in device tree");
		return NULL;
	}

	if (!device_is_ready(dev)) {
		LOG_ERR("Display device not ready");
		return NULL;
	}

	LOG_INF("Found display device: %s", dev->name);
	return dev;
}

/* Fill a rectangular area with color */
static void fill_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color)
{
	if (!framebuffer) {
		LOG_ERR("Framebuffer not available");
		return;
	}

	for (uint32_t row = y; row < y + height && row < lcd_height; row++) {
		for (uint32_t col = x; col < x + width && col < lcd_width; col++) {
			framebuffer[row * lcd_width + col] = color;
		}
	}
}

/* Vertical color bar demo pattern */
static void demo_pattern(void)
{
	static uint32_t frame_count = 0;
	uint32_t colors[] = {
		0xFFFF0000,  /* Blue */
		0xFF00FF00,  /* Green */
		0xFF0000FF,  /* Red */
		0xFFFFFF00,  /* Cyan */
		0xFFFF00FF,  /* Magenta */
		0xFF00FFFF,  /* Yellow */
		0xFFFFFFFF,  /* White */
		0xFF808080   /* Gray */
	};
	uint32_t num_colors = sizeof(colors) / sizeof(colors[0]);
	uint32_t bar_width = lcd_width / num_colors;
	uint32_t i, start_x, end_x, color_index;

	if (!framebuffer || lcd_width == 0 || lcd_height == 0) {
		LOG_ERR("Display parameters not initialized");
		return;
	}

	/* Draw vertical color bars */
	for (i = 0; i < num_colors; i++) {
		start_x = i * bar_width;
		end_x = (i + 1) * bar_width;

		/* Handle the last bar to fill remaining pixels */
		if (i == num_colors - 1) {
			end_x = lcd_width;
		}

		color_index = (i + frame_count) % num_colors;
		fill_rect(start_x, 0, end_x - start_x, lcd_height, colors[color_index]);
	}

	frame_count++;
}

int main(void)
{
	const struct device *display_dev;
	struct display_capabilities caps;

	LOG_INF("STM32U5G9J-DK1 DSI Generic Driver Application Starting...");

	/* Get the display device */
	display_dev = get_display_device();
	if (!display_dev) {
		LOG_ERR("Display device not found");
		return -ENODEV;
	}

	/* Check if device is ready */
	if (!device_is_ready(display_dev)) {
		LOG_ERR("Display device is not ready");
		return -ENODEV;
	}

	/* Get display capabilities and set global parameters */
	display_get_capabilities(display_dev, &caps);
	lcd_width = caps.x_resolution;
	lcd_height = caps.y_resolution;

	/* Get framebuffer from display device */
	framebuffer = (uint32_t *)display_get_framebuffer(display_dev);

	LOG_INF("Display capabilities:");
	LOG_INF("  Resolution: %dx%d", caps.x_resolution, caps.y_resolution);
	LOG_INF("  Pixel format: 0x%08X", caps.current_pixel_format);
	LOG_INF("  Orientation: %d", caps.current_orientation);
	LOG_INF("  Framebuffer: 0x%08X", (uint32_t)framebuffer);

	if (!framebuffer) {
		LOG_ERR("Failed to get framebuffer from display device");
		return -ENODEV;
	}

	/* Turn off blanking (turn on display) */
	// if (display_blanking_off(display_dev) != 0) {
	// 	LOG_WRN("Failed to turn off display blanking");
	// }

	LOG_INF("Display subsystem initialized successfully!");
	LOG_INF("Running demo pattern...");

	/* Main application loop */
	while (1) {
		demo_pattern();

		/* Wait 3 seconds between pattern changes */
		k_sleep(K_SECONDS(3));
	}

	return 0;
}


