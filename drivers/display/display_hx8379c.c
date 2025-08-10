/*
 * Copyright 2023, NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT himax_hx8379c

#include <zephyr/drivers/display.h>
#include <zephyr/drivers/mipi_dsi.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <stm32u5xx_hal.h>
#include <stm32u5xx_hal_rcc_ex.h>

LOG_MODULE_REGISTER(hx8379c, LOG_LEVEL_INF);

/* HX8379C DCS Commands */
#define HX8379C_CMD_ID1 0xDA   /* Read ID1 */
#define HX8379C_CMD_ID2 0xDB   /* Read ID2 */
#define HX8379C_CMD_ID3 0xDC   /* Read ID3 */

struct hx8379c_config {
	const struct device *mipi_dsi;
	const struct gpio_dt_spec reset_gpio;
	const struct gpio_dt_spec backlight;
	uint16_t panel_width;
	uint16_t panel_height;
	uint16_t rotation;
	uint16_t hsync;
	uint16_t hbp;
	uint16_t hfp;
	uint16_t vfp;
	uint16_t vbp;
	uint16_t vsync;
	uint8_t num_of_lanes;
	uint8_t pixel_format;
	uint8_t channel;
};

static void hx8379c_get_capabilities(const struct device *dev,
				     struct display_capabilities *capabilities)
{
	const struct hx8379c_config *config = dev->config;

	memset(capabilities, 0, sizeof(struct display_capabilities));
	capabilities->x_resolution = config->panel_width;
	capabilities->y_resolution = config->panel_height;
	capabilities->supported_pixel_formats = config->pixel_format;
	capabilities->current_pixel_format = config->pixel_format;
	capabilities->current_orientation = DISPLAY_ORIENTATION_NORMAL;
}

static DEVICE_API(display, hx8379c_api) = {
	// .blanking_on = hx8379c_blanking_on,
	// .blanking_off = hx8379c_blanking_off,
	// .write = hx8379c_write,
	.get_capabilities = hx8379c_get_capabilities,
	// .set_pixel_format = hx8379c_set_pixel_format,
	// .set_orientation = hx8379c_set_orientation,
};

static int hx8379c_check_id(const struct device *dev)
{
	const struct hx8379c_config *config = dev->config;
	uint8_t id1 = 0, id2 = 0, id3 = 0;
	int ret;

	/* Read ID1 register (0xDA) */
	ret = mipi_dsi_dcs_read(config->mipi_dsi, config->channel, HX8379C_CMD_ID1, &id1, sizeof(id1));
	if (ret != sizeof(id1)) {
		LOG_ERR("Read panel ID1 failed! (%d)", ret);
		return -EIO;
	}

	/* Read ID2 register (0xDB) */
	ret = mipi_dsi_dcs_read(config->mipi_dsi, config->channel, HX8379C_CMD_ID2, &id2, sizeof(id2));
	if (ret != sizeof(id2)) {
		LOG_ERR("Read panel ID2 failed! (%d)", ret);
		return -EIO;
	}

	/* Read ID3 register (0xDC) */
	ret = mipi_dsi_dcs_read(config->mipi_dsi, config->channel, HX8379C_CMD_ID3, &id3, sizeof(id3));
	if (ret != sizeof(id3)) {
		LOG_ERR("Read panel ID3 failed! (%d)", ret);
		return -EIO;
	}

	LOG_INF("Panel IDs: ID1=0x%02X, ID2=0x%02X, ID3=0x%02X", id1, id2, id3);

	/* Note: According to HX8379C datasheet, ID values are customer-defined in OTP
	 * Default ID1=0x00, but actual values depend on the specific panel manufacturer
	 * For now, we'll log the values and only verify if they're non-zero (indicating valid read)
	 */

	/* Basic sanity check - at least one ID should be non-zero for a valid panel */
	if (id1 == 0x00 && id2 == 0x00 && id3 == 0x00) {
		LOG_WRN("All panel IDs are 0x00 - this might indicate communication issues or default values");
		/* Don't fail here as 0x00 might be valid for some panels */
	}

	/* TODO: Once actual panel ID values are known, uncomment and update these checks:
	 *
	 * if (id1 != HX8379C_ID1) {
	 *     LOG_ERR("ID1 mismatch: 0x%02X (expected 0x%02X)", id1, HX8379C_ID1);
	 *     return -EINVAL;
	 * }
	 *
	 * if (id2 != HX8379C_ID2) {
	 *     LOG_ERR("ID2 mismatch: 0x%02X (expected 0x%02X)", id2, HX8379C_ID2);
	 *     return -EINVAL;
	 * }
	 *
	 * if (id3 != HX8379C_ID3) {
	 *     LOG_ERR("ID3 mismatch: 0x%02X (expected 0x%02X)", id3, HX8379C_ID3);
	 *     return -EINVAL;
	 * }
	 */

	return 0;
}

static int hx8379c_configure(const struct device *dev)
{
	const struct hx8379c_config *config = dev->config;
	int ret;

	LOG_INF("Configuring HX8379C DSI panel...");

	/* CMD Mode */
	uint8_t InitParam1[3] = {0xFF, 0x83, 0x79};
	ret = mipi_dsi_dcs_write(config->mipi_dsi, config->channel, 0xB9, InitParam1, sizeof(InitParam1));
	if (ret < 0) {
		LOG_ERR("Panel init step 1 failed");
		return ret;
	}

	/* SETPOWER */
	uint8_t InitParam3[16] = {0x44,0x1C,0x1C,0x37,0x57,0x90,0xD0,0xE2,0x58,0x80,0x38,0x38,0xF8,0x33,0x34,0x42};
	if (mipi_dsi_dcs_write(config->mipi_dsi, config->channel, 0xB1, InitParam3, sizeof(InitParam3)) < 0) {
		LOG_ERR("Panel SETPOWER failed");
		return -1;
	}

	/* SETDISP */
	uint8_t InitParam4[9] = {0x80,0x14,0x0C,0x30,0x20,0x50,0x11,0x42,0x1D};
	ret = mipi_dsi_dcs_write(config->mipi_dsi, config->channel, 0xB2, InitParam4, sizeof(InitParam4));
	if (ret < 0) {
		LOG_ERR("Panel SETDISP failed");
		return ret;
	}

	/* Set display cycle timing */
	uint8_t InitParam5[10] = {0x01,0xAA,0x01,0xAF,0x01,0xAF,0x10,0xEA,0x1C,0xEA};
	ret = mipi_dsi_dcs_write(config->mipi_dsi, config->channel, 0xB4, InitParam5, sizeof(InitParam5));
	if (ret < 0) {
		LOG_ERR("Panel timing config failed");
		return ret;
	}

	/* SETVCOM */
	uint8_t InitParam60[4] = {0x00,0x00,0x00,0xC0};
	ret = mipi_dsi_dcs_write(config->mipi_dsi, config->channel, 0xC7, InitParam60, sizeof(InitParam60));
	if (ret < 0) {
		LOG_ERR("Panel SETVCOM failed");
		return ret;
	}

	/* Set Panel Related Registers */
	uint8_t InitParamCC[1] = {0x02};
	ret = mipi_dsi_dcs_write(config->mipi_dsi, config->channel, 0xCC, InitParamCC, sizeof(InitParamCC));
	if (ret < 0) {
		LOG_ERR("Panel register 0xCC failed");
		return ret;
	}

	uint8_t InitParamD2[1] = {0x77};
	ret = mipi_dsi_dcs_write(config->mipi_dsi, config->channel, 0xD2, InitParamD2, sizeof(InitParamD2));
	if (ret < 0) {
		LOG_ERR("Panel register 0xD2 failed");
		return ret;
	}

	/* Complex register configuration 0xD3 */
	uint8_t InitParam50[37] = {0x00,0x07,0x00,0x00,0x00,0x08,0x08,0x32,0x10,0x01,0x00,0x01,0x03,0x72,0x03,0x72,0x00,0x08,0x00,0x08,0x33,0x33,0x05,0x05,0x37,0x05,0x05,0x37,0x0A,0x00,0x00,0x00,0x0A,0x00,0x01,0x00,0x0E};
	ret = mipi_dsi_dcs_write(config->mipi_dsi, config->channel, 0xD3, InitParam50, sizeof(InitParam50));
	if (ret < 0) {
		LOG_ERR("Panel register 0xD3 failed");
		return ret;
	}

	/* Register 0xD5 configuration */
	uint8_t InitParam51[34] = {0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x19,0x19,0x18,0x18,0x18,0x18,0x19,0x19,0x01,0x00,0x03,0x02,0x05,0x04,0x07,0x06,0x23,0x22,0x21,0x20,0x18,0x18,0x18,0x18,0x00,0x00};
	ret = mipi_dsi_dcs_write(config->mipi_dsi, config->channel, 0xD5, InitParam51, sizeof(InitParam51));
	if (ret < 0) {
		LOG_ERR("Panel register 0xD5 failed");
		return ret;
	}

	/* Register 0xD6 configuration */
	uint8_t InitParam52[35] = {0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x19,0x19,0x18,0x18,0x19,0x19,0x18,0x18,0x06,0x07,0x04,0x05,0x02,0x03,0x00,0x01,0x20,0x21,0x22,0x23,0x18,0x18,0x18,0x18};
	ret = mipi_dsi_dcs_write(config->mipi_dsi, config->channel, 0xD6, InitParam52, sizeof(InitParam52));
	if (ret < 0) {
		LOG_ERR("Panel register 0xD6 failed");
		return ret;
	}

	/* SET GAMMA */
	uint8_t InitParam8[42] = {0x00,0x16,0x1B,0x30,0x36,0x3F,0x24,0x40,0x09,0x0D,0x0F,0x18,0x0E,0x11,0x12,0x11,0x14,0x07,0x12,0x13,0x18,0x00,0x17,0x1C,0x30,0x36,0x3F,0x24,0x40,0x09,0x0C,0x0F,0x18,0x0E,0x11,0x14,0x11,0x12,0x07,0x12,0x14,0x18};
	ret = mipi_dsi_dcs_write(config->mipi_dsi, config->channel, 0xE0, InitParam8, sizeof(InitParam8));
	if (ret < 0) {
		LOG_ERR("Panel gamma configuration failed");
		return ret;
	}

	/* Additional B6 register */
	uint8_t InitParam44[3] = {0x2C,0x2C,0x00};
	ret = mipi_dsi_dcs_write(config->mipi_dsi, config->channel, 0xB6, InitParam44, sizeof(InitParam44));
	if (ret < 0) {
		LOG_ERR("Panel register 0xB6 failed");
		return ret;
	}

	/* BD register setup */
	uint8_t InitParamBD[1] = {0x00};
	ret = mipi_dsi_dcs_write(config->mipi_dsi, config->channel, 0xBD, InitParamBD, sizeof(InitParamBD));
	if (ret < 0) {
		LOG_ERR("Panel register 0xBD(0) failed");
		return ret;
	}

	/* Color LUT configuration 1 */
	uint8_t InitParam14[43] = {0x01,0x00,0x07,0x0F,0x16,0x1F,0x27,0x30,0x38,0x40,0x47,0x4E,0x56,0x5D,0x65,0x6D,0x74,0x7D,0x84,0x8A,0x90,0x99,0xA1,0xA9,0xB0,0xB6,0xBD,0xC4,0xCD,0xD4,0xDD,0xE5,0xEC,0xF3,0x36,0x07,0x1C,0xC0,0x1B,0x01,0xF1,0x34,0x00};
	ret = mipi_dsi_dcs_write(config->mipi_dsi, config->channel, 0xC1, InitParam14, sizeof(InitParam14));
	if (ret < 0) {
		LOG_ERR("Panel color LUT 1 failed");
		return ret;
	}

	uint8_t InitParamBD1[1] = {0x01};
	ret = mipi_dsi_dcs_write(config->mipi_dsi, config->channel, 0xBD, InitParamBD1, sizeof(InitParamBD1));
	if (ret < 0) {
		LOG_ERR("Panel register 0xBD(1) failed");
		return ret;
	}

	/* Color LUT configuration 2 */
	uint8_t InitParam15[42] = {0x00,0x08,0x0F,0x16,0x1F,0x28,0x31,0x39,0x41,0x48,0x51,0x59,0x60,0x68,0x70,0x78,0x7F,0x87,0x8D,0x94,0x9C,0xA3,0xAB,0xB3,0xB9,0xC1,0xC8,0xD0,0xD8,0xE0,0xE8,0xEE,0xF5,0x3B,0x1A,0xB6,0xA0,0x07,0x45,0xC5,0x37,0x00};
	ret = mipi_dsi_dcs_write(config->mipi_dsi, config->channel, 0xC1, InitParam15, sizeof(InitParam15));
	if (ret < 0) {
		LOG_ERR("Panel color LUT 2 failed");
		return ret;
	}

	uint8_t InitParamBD2[1] = {0x02};
	ret = mipi_dsi_dcs_write(config->mipi_dsi, config->channel, 0xBD, InitParamBD2, sizeof(InitParamBD2));
	if (ret < 0) {
		LOG_ERR("Panel register 0xBD(2) failed");
		return ret;
	}

	/* Color LUT configuration 3 */
	uint8_t InitParam20[42] = {0x00,0x09,0x0F,0x18,0x21,0x2A,0x34,0x3C,0x45,0x4C,0x56,0x5E,0x66,0x6E,0x76,0x7E,0x87,0x8E,0x95,0x9D,0xA6,0xAF,0xB7,0xBD,0xC5,0xCE,0xD5,0xDF,0xE7,0xEE,0xF4,0xFA,0xFF,0x0C,0x31,0x83,0x3C,0x5B,0x56,0x1E,0x5A,0xFF};
	ret = mipi_dsi_dcs_write(config->mipi_dsi, config->channel, 0xC1, InitParam20, sizeof(InitParam20));
	if (ret < 0) {
		LOG_ERR("Panel color LUT 3 failed");
		return ret;
	}

	uint8_t InitParamBD0[1] = {0x00};
	ret = mipi_dsi_dcs_write(config->mipi_dsi, config->channel, 0xBD, InitParamBD0, sizeof(InitParamBD0));
	if (ret < 0) {
		LOG_ERR("Panel register 0xBD(0) failed");
		return ret;
	}

	/* Exit Sleep Mode */
	uint8_t InitParam11[1] = {0x00};
	ret = mipi_dsi_dcs_write(config->mipi_dsi, config->channel, 0x11, InitParam11, sizeof(InitParam11));
	if (ret < 0) {
		LOG_ERR("Exit sleep mode failed");
		return ret;
	}

	k_msleep(120);

	/* Display On */
	uint8_t InitParam29[1] = {0x00};
	ret = mipi_dsi_dcs_write(config->mipi_dsi, config->channel, 0x29, InitParam29, sizeof(InitParam29));
	if (ret < 0) {
		LOG_ERR("Display on failed");
		return ret;
	}

	k_msleep(120);

	LOG_INF("DSI panel configured successfully");
	return 0;
}

static int hx8379c_init(const struct device *dev)
{
	const struct hx8379c_config *config = dev->config;
	struct mipi_dsi_device mdev;
	int ret;

	LOG_INF("HX8379C config:");
	LOG_INF("\tMIPI DSI device: %p", config->mipi_dsi);
	LOG_INF("\tReset GPIO pin: %d", config->reset_gpio.pin);
	LOG_INF("\tBacklight GPIO pin: %d", config->backlight.pin);
	LOG_INF("\tNumber of lanes: %u", config->num_of_lanes);
	LOG_INF("\tPixel format: 0x%02X", config->pixel_format);
	LOG_INF("\tPanel width: %u", config->panel_width);
	LOG_INF("\tPanel height: %u", config->panel_height);
	LOG_INF("\tChannel: %u", config->channel);

	if (config->reset_gpio.port) {
		if (!gpio_is_ready_dt(&config->reset_gpio)) {
			LOG_ERR("Reset GPIO device is not ready!");
			return -ENODEV;
		}
		ret = gpio_pin_configure_dt(&config->reset_gpio, GPIO_OUTPUT_INACTIVE);
		if (ret < 0) {
			LOG_ERR("Reset display failed! (%d)", ret);
			return ret;
		}
		k_msleep(11);
		ret = gpio_pin_set_dt(&config->reset_gpio, 1);
		if (ret < 0) {
			LOG_ERR("Enable display failed! (%d)", ret);
			return ret;
		}
		k_msleep(150);
	}

	if (config->backlight.port) {
		if (!gpio_is_ready_dt(&config->backlight)) {
			LOG_ERR("Backlight GPIO device is not ready!");
			return -ENODEV;
		}
		ret = gpio_pin_configure_dt(&config->backlight, GPIO_OUTPUT_ACTIVE);
		if (ret < 0) {
			LOG_ERR("Backlight GPIO configuration failed! (%d)", ret);
			return ret;
		}
	}

	/* attach to MIPI-DSI host */
	mdev.data_lanes = config->num_of_lanes;
	mdev.pixfmt = config->pixel_format;
	mdev.mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST;
	// mdev.mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST | MIPI_DSI_MODE_LPM;

	#warning "Pixel clock changed from 62500 to 20833"
	mdev.timings.hactive = config->panel_width;
	mdev.timings.hsync = config->hsync; //2; //6; //24;
	mdev.timings.hbp = config->hbp; //1; //3; //12;
	mdev.timings.hfp = config->hfp; //1; //1323; //5292
	mdev.timings.vactive = config->panel_height;
	mdev.timings.vfp = config->vfp; //50;
	mdev.timings.vbp = config->vbp; //12;
	mdev.timings.vsync = config->vsync; //1;

	ret = mipi_dsi_attach(config->mipi_dsi, config->channel, &mdev);
	if (ret < 0) {
		LOG_ERR("MIPI-DSI attach failed! (%d)", ret);
		return ret;
	}

	// ret = hx8379c_check_id(dev);
	// if (ret) {
	// 	LOG_ERR("Panel ID check failed! (%d)", ret);
	// 	return ret;
	// }

	ret = hx8379c_configure(dev);
	if (ret) {
		LOG_ERR("DSI init sequence failed! (%d)", ret);
		return ret;
	}

	// ret = hx8379c_blanking_off(dev);
	// if (ret) {
	// 	LOG_ERR("Display blanking off failed! (%d)", ret);
	// 	return ret;
	// }

	return ret;
}

#define HX8379C_PANEL_DEVICE(id)							\
	static const struct hx8379c_config hx8379c_config_##id = {			\
		.mipi_dsi = DEVICE_DT_GET(DT_INST_BUS(id)),				\
		.reset_gpio = GPIO_DT_SPEC_INST_GET_OR(id, reset_gpios, {0}),		\
		.backlight = GPIO_DT_SPEC_INST_GET_OR(id, bl_gpios, {0}),		\
		.num_of_lanes = DT_INST_PROP_BY_IDX(id, data_lanes, 0),			\
		.pixel_format = DT_INST_PROP(id, pixel_format),				\
		.panel_width = DT_INST_PROP(id, width),					\
		.panel_height = DT_INST_PROP(id, height),				\
		.channel = DT_INST_REG_ADDR(id),					\
		.rotation = DT_INST_PROP(id, rotation),					\
		.hsync = DT_PROP(DT_INST_CHILD(id, display_timings), hsync_len),	\
		.hbp = DT_PROP(DT_INST_CHILD(id, display_timings), hback_porch),	\
		.hfp = DT_PROP(DT_INST_CHILD(id, display_timings), hfront_porch),	\
		.vsync = DT_PROP(DT_INST_CHILD(id, display_timings), vsync_len),	\
		.vbp = DT_PROP(DT_INST_CHILD(id, display_timings), vback_porch),	\
		.vfp = DT_PROP(DT_INST_CHILD(id, display_timings), vfront_porch),	\
	};										\
	DEVICE_DT_INST_DEFINE(id, &hx8379c_init, NULL,					\
			    NULL, &hx8379c_config_##id, POST_KERNEL,			\
			    CONFIG_DISPLAY_HX8379C_INIT_PRIORITY, &hx8379c_api);

DT_INST_FOREACH_STATUS_OKAY(HX8379C_PANEL_DEVICE)
