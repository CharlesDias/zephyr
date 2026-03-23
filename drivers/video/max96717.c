/*
 * Copyright (c) 2026 Charles Dias
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#define DT_DRV_COMPAT adi_max96717

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/video.h>
#include <zephyr/logging/log.h>

#include "video_common.h"
#include "video_device.h"

LOG_MODULE_REGISTER(video_max96717, CONFIG_VIDEO_LOG_LEVEL);

#define MAX96717_REG8(addr)       ((addr) | VIDEO_REG_ADDR16_DATA8)
#define MAX96717_REG_DEVID        MAX96717_REG8(0x000D)
#define MAX96717_REG_CTRL3        MAX96717_REG8(0x0013)
#define MAX96717_CTRL3_LOCKED     BIT(3)
#define MAX96717_CTRL3_ERROR      BIT(2)
#define MAX96717_CTRL3_CMU_LOCKED BIT(1)

#define MAX96717_LINK_LOCK_RETRIES  50
#define MAX96717_LINK_LOCK_DELAY_MS 20

static const char *max96717_variant_model(uint8_t id)
{
	switch (id) {
	case 0xBF:
		return "MAX96717";
	default:
		return "unknown";
	}
}

struct max96717_config {
	struct i2c_dt_spec i2c;
};

struct max96717_data {
};

static int max96717_check_link_lock(const struct i2c_dt_spec *i2c)
{
	uint32_t val;
	int ret;

	for (int i = 0; i < MAX96717_LINK_LOCK_RETRIES; i++) {
		ret = video_read_cci_reg(i2c, MAX96717_REG_CTRL3, &val);
		if (ret < 0) {
			LOG_ERR("Failed to read CTRL3 (err %d)", ret);
			return ret;
		}

		if (val & MAX96717_CTRL3_LOCKED) {
			LOG_INF("GMSL2 link locked (CTRL3=0x%02X)", val);
			break;
		}

		LOG_DBG("Waiting for GMSL2 link lock (%d/%d, CTRL3=0x%02X)", i + 1,
			MAX96717_LINK_LOCK_RETRIES, val);
		k_msleep(MAX96717_LINK_LOCK_DELAY_MS);
	}

	if (!(val & MAX96717_CTRL3_LOCKED)) {
		LOG_WRN("GMSL2 link not locked (CTRL3=0x%02X)", val);
	}

	if (!(val & MAX96717_CTRL3_CMU_LOCKED)) {
		LOG_WRN("CMU not locked");
	}

	if (val & MAX96717_CTRL3_ERROR) {
		LOG_WRN("ERRB asserted — check GMSL link integrity");
	}

	return 0;
}

static int max96717_init(const struct device *dev)
{
	const struct max96717_config *cfg = dev->config;
	uint32_t chip_id;
	int ret;

	if (!i2c_is_ready_dt(&cfg->i2c)) {
		LOG_ERR("I2C bus not ready");
		return -ENODEV;
	}

	ret = video_read_cci_reg(&cfg->i2c, MAX96717_REG_DEVID, &chip_id);
	if (ret < 0) {
		LOG_ERR("Failed to read device ID (err %d)", ret);
		return ret;
	}

	LOG_INF("Found %s (ID=0x%02X) on %s@0x%02x", max96717_variant_model(chip_id), chip_id,
		cfg->i2c.bus->name, cfg->i2c.addr);

	ret = max96717_check_link_lock(&cfg->i2c);
	if (ret < 0) {
		return ret;
	}

	return 0;
}

static DEVICE_API(video, max96717_api) = {};

#define MAX96717_INIT(inst)                                                                        \
	static const struct max96717_config max96717_config_##inst = {                             \
		.i2c = I2C_DT_SPEC_INST_GET(inst),                                                 \
	};                                                                                         \
	static struct max96717_data max96717_data_##inst;                                          \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(inst, max96717_init, NULL, &max96717_data_##inst,                    \
			      &max96717_config_##inst, POST_KERNEL,                                \
			      CONFIG_VIDEO_MAX96717_INIT_PRIORITY, &max96717_api);                 \
                                                                                                   \
	VIDEO_DEVICE_DEFINE(max96717_##inst, DEVICE_DT_INST_GET(inst), NULL);

DT_INST_FOREACH_STATUS_OKAY(MAX96717_INIT)
