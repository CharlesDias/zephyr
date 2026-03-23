/*
 * Copyright (c) 2026 Charles Dias
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#define DT_DRV_COMPAT adi_max96724

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/video.h>
#include <zephyr/logging/log.h>

#include "video_common.h"
#include "video_device.h"

LOG_MODULE_REGISTER(video_max96724, CONFIG_VIDEO_LOG_LEVEL);

#define MAX96724_REG8(addr)       ((addr) | VIDEO_REG_ADDR16_DATA8)
#define MAX96724_REG_DEVID        MAX96724_REG8(0x000D)
#define MAX96724_REG_CTRL12       MAX96724_REG8(0x000A)
#define MAX96724_REG_CTRL13       MAX96724_REG8(0x000B)
#define MAX96724_REG_CTRL14       MAX96724_REG8(0x000C)
#define MAX96724_REG_CTRL3        MAX96724_REG8(0x001A)
#define MAX96724_CTRL3_LOCKED_A   BIT(3)
#define MAX96724_CTRL3_ERROR      BIT(2)
#define MAX96724_CTRL3_CMU_LOCKED BIT(1)
#define MAX96724_CTRL3_LOCK_PIN   BIT(0)
#define MAX96724_CTRL_LOCKED      BIT(3)

#define MAX96724_LINK_LOCK_RETRIES  50
#define MAX96724_LINK_LOCK_DELAY_MS 20

static const char *max96724_variant_model(uint8_t id)
{
	switch (id) {
	case 0xA2:
		return "MAX96724";
	case 0xA3:
		return "MAX96724F";
	case 0xA4:
		return "MAX96724R";
	default:
		return "unknown";
	}
}

struct max96724_config {
	struct i2c_dt_spec i2c;
};

struct max96724_data {
};

static int max96724_check_link_lock(const struct i2c_dt_spec *i2c)
{
	uint32_t val;
	int ret;

	/* Poll LOCK_PIN — reflects all enabled GMSL links locked */
	for (int i = 0; i < MAX96724_LINK_LOCK_RETRIES; i++) {
		ret = video_read_cci_reg(i2c, MAX96724_REG_CTRL3, &val);
		if (ret < 0) {
			LOG_ERR("Failed to read CTRL3 (err %d)", ret);
			return ret;
		}

		if (val & MAX96724_CTRL3_LOCK_PIN) {
			LOG_INF("All enabled GMSL2 links locked (CTRL3=0x%02X)", val);
			break;
		}

		LOG_DBG("Waiting for GMSL2 link lock (%d/%d, CTRL3=0x%02X)", i + 1,
			MAX96724_LINK_LOCK_RETRIES, val);
		k_msleep(MAX96724_LINK_LOCK_DELAY_MS);
	}

	if (!(val & MAX96724_CTRL3_LOCK_PIN)) {
		/* LOCK_PIN still 0 — check each link individually */
		static const struct {
			uint32_t reg;
			const char *name;
		} links[] = {
			{MAX96724_REG_CTRL3, "A"},
			{MAX96724_REG_CTRL12, "B"},
			{MAX96724_REG_CTRL13, "C"},
			{MAX96724_REG_CTRL14, "D"},
		};

		LOG_WRN("Not all enabled GMSL2 links locked — checking individually:");

		for (int l = 0; l < ARRAY_SIZE(links); l++) {
			ret = video_read_cci_reg(i2c, links[l].reg, &val);
			if (ret < 0) {
				LOG_ERR("Failed to read link %s status (err %d)", links[l].name,
					ret);
				return ret;
			}

			if (val & MAX96724_CTRL_LOCKED) {
				LOG_INF("  Link %s: locked", links[l].name);
			} else {
				LOG_WRN("  Link %s: not locked", links[l].name);
			}
		}
	}

	if (!(val & MAX96724_CTRL3_CMU_LOCKED)) {
		LOG_WRN("CMU not locked");
	}

	if (val & MAX96724_CTRL3_ERROR) {
		LOG_WRN("ERRB asserted — check GMSL link integrity");
	}

	return 0;
}

static int max96724_init(const struct device *dev)
{
	const struct max96724_config *cfg = dev->config;
	uint32_t chip_id;
	int ret;

	if (!i2c_is_ready_dt(&cfg->i2c)) {
		LOG_ERR("I2C bus not ready");
		return -ENODEV;
	}

	ret = video_read_cci_reg(&cfg->i2c, MAX96724_REG_DEVID, &chip_id);
	if (ret < 0) {
		LOG_ERR("Failed to read device ID (err %d)", ret);
		return ret;
	}

	LOG_INF("Found %s (ID=0x%02X) on %s@0x%02x", max96724_variant_model(chip_id), chip_id,
		cfg->i2c.bus->name, cfg->i2c.addr);

	ret = max96724_check_link_lock(&cfg->i2c);
	if (ret < 0) {
		return ret;
	}

	return 0;
}

static DEVICE_API(video, max96724_api) = {};

#define MAX96724_INIT(inst)                                                                        \
	static const struct max96724_config max96724_config_##inst = {                             \
		.i2c = I2C_DT_SPEC_INST_GET(inst),                                                 \
	};                                                                                         \
	static struct max96724_data max96724_data_##inst;                                          \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(inst, max96724_init, NULL, &max96724_data_##inst,                    \
			      &max96724_config_##inst, POST_KERNEL,                                \
			      CONFIG_VIDEO_MAX96724_INIT_PRIORITY, &max96724_api);                 \
                                                                                                   \
	VIDEO_DEVICE_DEFINE(max96724_##inst, DEVICE_DT_INST_GET(inst), NULL);

DT_INST_FOREACH_STATUS_OKAY(MAX96724_INIT)
