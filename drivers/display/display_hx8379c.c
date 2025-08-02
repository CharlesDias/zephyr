/*
 * Copyright (c) 2024 STMicroelectronics
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT himax_hx8379c

#include <zephyr/drivers/display.h>
#include <zephyr/drivers/mipi_dsi.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(hx8379c, CONFIG_DISPLAY_LOG_LEVEL);

/* HX8379C specific commands */
#define HX8379C_CMD_SETEXTC             0xB9
#define HX8379C_CMD_SETPOWER            0xB1
#define HX8379C_CMD_SETDISP             0xB2
#define HX8379C_CMD_SETCYC              0xB4
#define HX8379C_CMD_SETVCOM             0xB6
#define HX8379C_CMD_SETEQ               0xB7
#define HX8379C_CMD_SETPANEL            0xCC
#define HX8379C_CMD_SETGIP0             0xD3
#define HX8379C_CMD_SETGIP1             0xD5
#define HX8379C_CMD_SETGIP2             0xD6
#define HX8379C_CMD_SETGAMMA            0xE0
#define HX8379C_CMD_SETMIPI             0xC7
#define HX8379C_CMD_SETC1               0xC1
#define HX8379C_CMD_SETBD               0xBD

struct hx8379c_config {
	const struct device *mipi_dsi;
	const struct gpio_dt_spec reset_gpio;
	const struct gpio_dt_spec bl_gpio;
	uint8_t num_of_lanes;
	uint8_t pixel_format;
	uint16_t panel_width;
	uint16_t panel_height;
	uint8_t channel;
};

static int hx8379c_dcs_write(const struct device *dev, uint8_t cmd, uint8_t *buf,
				 uint8_t len)
{
	const struct hx8379c_config *config = dev->config;

	return mipi_dsi_dcs_write(config->mipi_dsi, config->channel, cmd, buf, len);
}

static int hx8379c_write(const struct device *dev, const uint16_t x,
			 const uint16_t y,
			 const struct display_buffer_descriptor *desc,
			 const void *buf)
{
	/* HX8379C is read-only in video mode, framebuffer is handled by LTDC */
	return -ENOTSUP;
}

static int hx8379c_blanking_off(const struct device *dev)
{
	const struct hx8379c_config *config = dev->config;

	if (config->bl_gpio.port != NULL) {
		return gpio_pin_set_dt(&config->bl_gpio, 1);
	} else {
		return -ENOTSUP;
	}
}

static int hx8379c_blanking_on(const struct device *dev)
{
	const struct hx8379c_config *config = dev->config;

	if (config->bl_gpio.port != NULL) {
		return gpio_pin_set_dt(&config->bl_gpio, 0);
	} else {
		return -ENOTSUP;
	}
}

static int hx8379c_set_pixel_format(const struct device *dev,
				    const enum display_pixel_format pixel_format)
{
	const struct hx8379c_config *config = dev->config;

	if (pixel_format == config->pixel_format) {
		return 0;
	}
	LOG_ERR("Pixel format change not implemented");
	return -ENOTSUP;
}

static int hx8379c_set_orientation(const struct device *dev,
				   const enum display_orientation orientation)
{
	if (orientation == DISPLAY_ORIENTATION_NORMAL) {
		return 0;
	}
	LOG_ERR("Changing display orientation not implemented");
	return -ENOTSUP;
}

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
	.blanking_on = hx8379c_blanking_on,
	.blanking_off = hx8379c_blanking_off,
	.write = hx8379c_write,
	.get_capabilities = hx8379c_get_capabilities,
	.set_pixel_format = hx8379c_set_pixel_format,
	.set_orientation = hx8379c_set_orientation,
};

static int hx8379c_panel_init(const struct device *dev)
{
	uint8_t param;
	int ret;

	/* CMD Mode - Set Extension Command */
	uint8_t setextc_param[3] = {0xFF, 0x83, 0x79};
	ret = hx8379c_dcs_write(dev, HX8379C_CMD_SETEXTC, setextc_param, 3);
	if (ret < 0) {
		LOG_ERR("Failed to write SETEXTC command");
		return ret;
	}

	/* SETPOWER */
	uint8_t setpower_param[16] = {
		0x44, 0x1C, 0x1C, 0x37, 0x57, 0x90, 0xD0, 0xE2,
		0x58, 0x80, 0x38, 0x38, 0xF8, 0x33, 0x34, 0x42
	};
	ret = hx8379c_dcs_write(dev, HX8379C_CMD_SETPOWER, setpower_param, 16);
	if (ret < 0) {
		LOG_ERR("Failed to write SETPOWER command");
		return ret;
	}

	/* SETDISP */
	uint8_t setdisp_param[9] = {0x80, 0x14, 0x0C, 0x30, 0x20, 0x50, 0x11, 0x42, 0x1D};
	ret = hx8379c_dcs_write(dev, HX8379C_CMD_SETDISP, setdisp_param, 9);
	if (ret < 0) {
		LOG_ERR("Failed to write SETDISP command");
		return ret;
	}

	/* Set display cycle timing */
	uint8_t setcyc_param[10] = {
		0x01, 0xAA, 0x01, 0xAF, 0x01, 0xAF, 0x10, 0xEA, 0x1C, 0xEA
	};
	ret = hx8379c_dcs_write(dev, HX8379C_CMD_SETCYC, setcyc_param, 10);
	if (ret < 0) {
		LOG_ERR("Failed to write SETCYC command");
		return ret;
	}

	/* SETVCOM */
	uint8_t setvcom_param[4] = {0x00, 0x00, 0x00, 0xC0};
	ret = hx8379c_dcs_write(dev, HX8379C_CMD_SETVCOM, setvcom_param, 4);
	if (ret < 0) {
		LOG_ERR("Failed to write SETVCOM command");
		return ret;
	}

	/* Set Panel Related Registers */
	param = 0x02;
	ret = hx8379c_dcs_write(dev, HX8379C_CMD_SETPANEL, &param, 1);
	if (ret < 0) {
		LOG_ERR("Failed to write SETPANEL command");
		return ret;
	}

	param = 0x77;
	ret = hx8379c_dcs_write(dev, 0xD2, &param, 1);
	if (ret < 0) {
		LOG_ERR("Failed to write D2 command");
		return ret;
	}

	/* SETGIP0 */
	uint8_t setgip0_param[37] = {
		0x00, 0x07, 0x00, 0x00, 0x00, 0x08, 0x08, 0x32, 0x10, 0x01, 0x00, 0x01, 0x03,
		0x72, 0x03, 0x72, 0x00, 0x08, 0x00, 0x08, 0x33, 0x33, 0x05, 0x05, 0x37, 0x05,
		0x05, 0x37, 0x0A, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x01, 0x00, 0x0E
	};
	ret = hx8379c_dcs_write(dev, HX8379C_CMD_SETGIP0, setgip0_param, 37);
	if (ret < 0) {
		LOG_ERR("Failed to write SETGIP0 command");
		return ret;
	}

	/* SETGIP1 */
	uint8_t setgip1_param[34] = {
		0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x19, 0x19, 0x18, 0x18, 0x18,
		0x18, 0x19, 0x19, 0x01, 0x00, 0x03, 0x02, 0x05, 0x04, 0x07, 0x06, 0x23, 0x22,
		0x21, 0x20, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00
	};
	ret = hx8379c_dcs_write(dev, HX8379C_CMD_SETGIP1, setgip1_param, 34);
	if (ret < 0) {
		LOG_ERR("Failed to write SETGIP1 command");
		return ret;
	}

	/* SETGIP2 */
	uint8_t setgip2_param[35] = {
		0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x19, 0x19, 0x18, 0x18, 0x19,
		0x19, 0x18, 0x18, 0x06, 0x07, 0x04, 0x05, 0x02, 0x03, 0x00, 0x01, 0x20, 0x21,
		0x22, 0x23, 0x18, 0x18, 0x18, 0x18
	};
	ret = hx8379c_dcs_write(dev, HX8379C_CMD_SETGIP2, setgip2_param, 35);
	if (ret < 0) {
		LOG_ERR("Failed to write SETGIP2 command");
		return ret;
	}

	/* SET GAMMA */
	uint8_t setgamma_param[42] = {
		0x00, 0x16, 0x1B, 0x30, 0x36, 0x3F, 0x24, 0x40, 0x09, 0x0D, 0x0F, 0x18, 0x0E,
		0x11, 0x12, 0x11, 0x14, 0x07, 0x12, 0x13, 0x18, 0x00, 0x17, 0x1C, 0x30, 0x36,
		0x3F, 0x24, 0x40, 0x09, 0x0C, 0x0F, 0x18, 0x0E, 0x11, 0x14, 0x11, 0x12, 0x07,
		0x12, 0x14, 0x18
	};
	ret = hx8379c_dcs_write(dev, HX8379C_CMD_SETGAMMA, setgamma_param, 42);
	if (ret < 0) {
		LOG_ERR("Failed to write SETGAMMA command");
		return ret;
	}

	/* SETVCOM again */
	uint8_t setvcom2_param[3] = {0x2C, 0x2C, 0x00};
	ret = hx8379c_dcs_write(dev, HX8379C_CMD_SETVCOM, setvcom2_param, 3);
	if (ret < 0) {
		LOG_ERR("Failed to write SETVCOM2 command");
		return ret;
	}

	/* Set BD to 0x00 */
	param = 0x00;
	ret = hx8379c_dcs_write(dev, HX8379C_CMD_SETBD, &param, 1);
	if (ret < 0) {
		LOG_ERR("Failed to write SETBD command");
		return ret;
	}

	/* Set C1 registers for color calibration - Bank 0 */
	uint8_t setc1_bank0[42] = {
		0x01, 0x00, 0x07, 0x0F, 0x16, 0x1F, 0x27, 0x30, 0x38, 0x40, 0x47, 0x4E, 0x56,
		0x5D, 0x65, 0x6D, 0x74, 0x7D, 0x84, 0x8A, 0x90, 0x99, 0xA1, 0xA9, 0xB0, 0xB6,
		0xBD, 0xC4, 0xCD, 0xD4, 0xDD, 0xE5, 0xEC, 0xF3, 0x36, 0x07, 0x1C, 0xC0, 0x1B,
		0x01, 0xF1, 0x34
	};
	ret = hx8379c_dcs_write(dev, HX8379C_CMD_SETC1, setc1_bank0, 42);
	if (ret < 0) {
		LOG_ERR("Failed to write SETC1 bank 0 command");
		return ret;
	}

	/* Set BD to 0x01 */
	param = 0x01;
	ret = hx8379c_dcs_write(dev, HX8379C_CMD_SETBD, &param, 1);
	if (ret < 0) {
		LOG_ERR("Failed to write SETBD 1 command");
		return ret;
	}

	/* Set C1 registers for color calibration - Bank 1 */
	uint8_t setc1_bank1[42] = {
		0x00, 0x08, 0x0F, 0x16, 0x1F, 0x28, 0x31, 0x39, 0x41, 0x48, 0x51, 0x59, 0x60,
		0x68, 0x70, 0x78, 0x7F, 0x87, 0x8D, 0x94, 0x9C, 0xA3, 0xAB, 0xB3, 0xB9, 0xC1,
		0xC8, 0xD0, 0xD8, 0xE0, 0xE8, 0xEE, 0xF5, 0x3B, 0x1A, 0xB6, 0xA0, 0x07, 0x45,
		0xC5, 0x37, 0x00
	};
	ret = hx8379c_dcs_write(dev, HX8379C_CMD_SETC1, setc1_bank1, 42);
	if (ret < 0) {
		LOG_ERR("Failed to write SETC1 bank 1 command");
		return ret;
	}

	/* Set BD to 0x02 */
	param = 0x02;
	ret = hx8379c_dcs_write(dev, HX8379C_CMD_SETBD, &param, 1);
	if (ret < 0) {
		LOG_ERR("Failed to write SETBD 2 command");
		return ret;
	}

	/* Set C1 registers for color calibration - Bank 2 */
	uint8_t setc1_bank2[42] = {
		0x00, 0x09, 0x0F, 0x18, 0x21, 0x2A, 0x34, 0x3C, 0x45, 0x4C, 0x56, 0x5E, 0x66,
		0x6E, 0x76, 0x7E, 0x87, 0x8E, 0x95, 0x9D, 0xA6, 0xAF, 0xB7, 0xBD, 0xC5, 0xCE,
		0xD5, 0xDF, 0xE7, 0xEE, 0xF4, 0xFA, 0xFF, 0x0C, 0x31, 0x83, 0x3C, 0x5B, 0x56,
		0x1E, 0x5A, 0xFF
	};
	ret = hx8379c_dcs_write(dev, HX8379C_CMD_SETC1, setc1_bank2, 42);
	if (ret < 0) {
		LOG_ERR("Failed to write SETC1 bank 2 command");
		return ret;
	}

	/* Set BD back to 0x00 */
	param = 0x00;
	ret = hx8379c_dcs_write(dev, HX8379C_CMD_SETBD, &param, 1);
	if (ret < 0) {
		LOG_ERR("Failed to write SETBD final command");
		return ret;
	}

	return 0;
}

static int hx8379c_init(const struct device *dev)
{
	const struct hx8379c_config *config = dev->config;
	int ret;
	struct mipi_dsi_device mdev;

	LOG_INF("HX8379C initialization starting...");
	LOG_INF("Panel config: %dx%d, %d lanes, channel %d", 
		config->panel_width, config->panel_height, 
		config->num_of_lanes, config->channel);

	/* attach to MIPI-DSI host */
	mdev.data_lanes = config->num_of_lanes;
	mdev.pixfmt = config->pixel_format;
	/* HX8379C runs in video mode */
	mdev.mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST;

	mdev.timings.hactive = config->panel_width;
	mdev.timings.hbp = 12;
	mdev.timings.hfp = 5292;
	mdev.timings.hsync = 24;

	mdev.timings.vactive = config->panel_height + 1;
	mdev.timings.vbp = 12;
	mdev.timings.vfp = 50;
	mdev.timings.vsync = 1;

	LOG_INF("MIPI DSI timings: h=%d+%d+%d+%d, v=%d+%d+%d+%d", 
		mdev.timings.hactive, mdev.timings.hbp, mdev.timings.hfp, mdev.timings.hsync,
		mdev.timings.vactive, mdev.timings.vbp, mdev.timings.vfp, mdev.timings.vsync);
	LOG_INF("Attaching to MIPI-DSI host...");

	ret = mipi_dsi_attach(config->mipi_dsi, config->channel, &mdev);
	if (ret < 0) {
		LOG_ERR("Could not attach to MIPI-DSI host");
		return ret;
	}
	LOG_INF("Successfully attached to MIPI-DSI host");

	if (config->reset_gpio.port) {
		if (!gpio_is_ready_dt(&config->reset_gpio)) {
			LOG_ERR("Reset GPIO device is not ready!");
			return -ENODEV;
		}
		LOG_INF("Configuring reset GPIO...");
		ret = gpio_pin_configure_dt(&config->reset_gpio, GPIO_OUTPUT_INACTIVE);
		if (ret < 0) {
			LOG_ERR("Could not configure reset GPIO (%d)", ret);
			return ret;
		}
		LOG_INF("Performing panel reset sequence...");
		k_msleep(1);
		gpio_pin_set_dt(&config->reset_gpio, 0);
		k_msleep(10);
		gpio_pin_set_dt(&config->reset_gpio, 1);
		k_msleep(150);
		LOG_INF("Panel reset completed");
	}

	/* Initialize panel with configuration sequence */
	LOG_INF("Starting panel initialization sequence...");
	ret = hx8379c_panel_init(dev);
	if (ret < 0) {
		LOG_ERR("Panel initialization failed");
		return ret;
	}
	LOG_INF("Panel initialization sequence completed");

	/* Exit Sleep Mode */
	LOG_INF("Exiting sleep mode...");
	ret = hx8379c_dcs_write(dev, MIPI_DCS_EXIT_SLEEP_MODE, NULL, 0);
	if (ret < 0) {
		LOG_ERR("Failed to exit sleep mode");
		return ret;
	}

	k_sleep(K_MSEC(120));

	/* Display On */
	LOG_INF("Turning on display...");
	ret = hx8379c_dcs_write(dev, MIPI_DCS_SET_DISPLAY_ON, NULL, 0);
	if (ret < 0) {
		LOG_ERR("Failed to turn on display");
		return ret;
	}

	k_sleep(K_MSEC(120));

	if (config->bl_gpio.port != NULL) {
		LOG_INF("Configuring backlight GPIO...");
		ret = gpio_pin_configure_dt(&config->bl_gpio, GPIO_OUTPUT_ACTIVE);
		if (ret < 0) {
			LOG_ERR("Could not configure bl GPIO (%d)", ret);
			return ret;
		}
		LOG_INF("Backlight enabled");
	}

	LOG_INF("HX8379C panel initialized successfully");

	return 0;
}

#define HX8379C_PANEL(id)							\
	static const struct hx8379c_config hx8379c_config_##id = {		\
		.mipi_dsi = DEVICE_DT_GET(DT_INST_BUS(id)),			\
		.reset_gpio = GPIO_DT_SPEC_INST_GET_OR(id, reset_gpios, {0}),	\
		.bl_gpio = GPIO_DT_SPEC_INST_GET_OR(id, bl_gpios, {0}),		\
		.num_of_lanes = DT_INST_PROP_BY_IDX(id, data_lanes, 0),		\
		.pixel_format = DT_INST_PROP(id, pixel_format),			\
		.panel_width = DT_INST_PROP(id, width),				\
		.panel_height = DT_INST_PROP(id, height),			\
		.channel = DT_INST_REG_ADDR(id),				\
	};									\
	DEVICE_DT_INST_DEFINE(id,						\
			    &hx8379c_init,					\
			    NULL,						\
			    NULL,						\
			    &hx8379c_config_##id,				\
			    POST_KERNEL,					\
			    87,							\
			    &hx8379c_api);

DT_INST_FOREACH_STATUS_OKAY(HX8379C_PANEL)
