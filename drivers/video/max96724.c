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

#define MAX96724_POLL_RETRIES  10
#define MAX96724_POLL_DELAY_MS 100

#define MAX96724_REG8(addr) ((addr) | VIDEO_REG_ADDR16_DATA8)

/* */
#define MAX96724_REG_REG0 MAX96724_REG8(0x0000)

#define MAX96724_REG_REG13 MAX96724_REG8(0x000D)

#define MAX96724_REG_PWR1       MAX96724_REG8(0x0013)
#define MAX96724_PWR1_RESET_ALL BIT(6)

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
	const struct device *source_dev;
	uint8_t csi_tx_num_lanes;
	uint8_t csi_tx_phy_index;
	int64_t csi_tx_link_freq;
};

struct max96724_data {
};

static int max96724_poll_device(const struct i2c_dt_spec *i2c)
{
	uint32_t val;
	int ret;

	for (int i = 0; i < MAX96724_POLL_RETRIES; i++) {
		ret = video_read_cci_reg(i2c, MAX96724_REG_REG0, &val);
		if (ret == 0 && val != 0) {
			LOG_DBG("MAX96724 device address: 0x%02x", val);
			return 0;
		}

		LOG_DBG("Waiting for device to become active (%d/%d)", i + 1,
			MAX96724_POLL_RETRIES);
		k_msleep(MAX96724_POLL_DELAY_MS);
	}

	return -ENODEV;
}

static int max96724_device_reset(const struct i2c_dt_spec *i2c)
{
	int ret;

	ret = max96724_poll_device(i2c);
	if (ret < 0) {
		return ret;
	}

	/* This is equivalent to toggling the PWDNB pin. The bit is cleared when written.*/
	ret = video_modify_cci_reg(i2c, MAX96724_REG_PWR1, MAX96724_PWR1_RESET_ALL,
				   MAX96724_PWR1_RESET_ALL);
	if (ret < 0) {
		LOG_ERR("Failed to reset device (err %d)", ret);
		return ret;
	}

	ret = max96724_poll_device(i2c);
	if (ret < 0) {
		return ret;
	}

	return 0;
}

static int max96724_log_device_id(const struct i2c_dt_spec *i2c)
{
	uint32_t dev_id;
	int ret;

	ret = video_read_cci_reg(i2c, MAX96724_REG_REG13, &dev_id);
	if (ret < 0) {
		LOG_ERR("Failed to read device ID (err %d)", ret);
		return ret;
	}

	LOG_DBG("Found %s (ID=0x%02X) on %s@0x%02x", max96724_variant_model(dev_id), dev_id,
		i2c->bus->name, i2c->addr);

	return 0;
}

static int max96724_check_link_lock(const struct i2c_dt_spec *i2c)
{
	uint32_t val;
	int ret;

	/* Poll LOCK_PIN — reflects all enabled GMSL links locked */
	for (int i = 0; i < MAX96724_POLL_RETRIES; i++) {
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
			MAX96724_POLL_RETRIES, val);
		k_msleep(MAX96724_POLL_DELAY_MS);
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

static int max96724_get_caps(const struct device *dev, struct video_caps *caps)
{

	return -ENOSYS;
}

static int max96724_get_format(const struct device *dev, struct video_format *fmt)
{

	return -ENOSYS;
}

static int max96724_set_format(const struct device *dev, struct video_format *fmt)
{

	return -ENOSYS;
}

static int max96724_set_stream(const struct device *dev, bool enable, enum video_buf_type type)
{

	return -ENOSYS;
}

static int max96724_set_frmival(const struct device *dev, struct video_frmival *frmival)
{
	const struct max96724_config *cfg = dev->config;

	if (cfg->source_dev == NULL) {
		return -ENOSYS;
	}

	return video_set_frmival(cfg->source_dev, frmival);
}

static int max96724_get_frmival(const struct device *dev, struct video_frmival *frmival)
{
	const struct max96724_config *cfg = dev->config;

	if (cfg->source_dev == NULL) {
		return -ENOSYS;
	}

	return video_get_frmival(cfg->source_dev, frmival);
}

static int max96724_enum_frmival(const struct device *dev, struct video_frmival_enum *fie)
{
	const struct max96724_config *cfg = dev->config;

	if (cfg->source_dev == NULL) {
		return -ENOSYS;
	}

	return video_enum_frmival(cfg->source_dev, fie);
}

static DEVICE_API(video, max96724_api) = {
	.get_caps = max96724_get_caps,
	.get_format = max96724_get_format,
	.set_format = max96724_set_format,
	.set_stream = max96724_set_stream,
	.set_frmival = max96724_set_frmival,
	.get_frmival = max96724_get_frmival,
	.enum_frmival = max96724_enum_frmival,
};

static int max96724_init(const struct device *dev)
{
	const struct max96724_config *cfg = dev->config;
	int ret;

	if (!i2c_is_ready_dt(&cfg->i2c)) {
		LOG_ERR("I2C bus not ready");
		return -ENODEV;
	}

	ret = max96724_device_reset(&cfg->i2c);
	if (ret < 0) {
		return ret;
	}

	ret = max96724_log_device_id(&cfg->i2c);
	if (ret < 0) {
		return ret;
	}

	return 0;
}

#define SOURCE_DEV(inst)                                                                           \
	COND_CODE_1(                                                                               \
		DT_NODE_HAS_STATUS_OKAY(                                                           \
			DT_NODE_REMOTE_DEVICE(DT_INST_ENDPOINT_BY_ID(inst, 0, 0))),                \
		(DEVICE_DT_GET(DT_NODE_REMOTE_DEVICE(DT_INST_ENDPOINT_BY_ID(inst, 0, 0)))),        \
		(NULL))

#define MAX96724_CSI_OUT_PORT DT_NODELABEL(des_csi_port)
#define MAX96724_CSI_OUT_EP   DT_NODELABEL(des_csi_out)

/* CSI-2 TX PHY index from the port's phy-id property. */
#define CSI_TX_PHY_INDEX(inst) DT_PROP(MAX96724_CSI_OUT_PORT, phy_id)

/* Number of CSI-2 TX data lanes from the CSI output endpoint's data-lanes property. */
#define CSI_TX_NUM_LANES(inst) DT_PROP_LEN(MAX96724_CSI_OUT_EP, data_lanes)

/* CSI-2 TX link frequency (Hz) from the CSI output endpoint's link-frequencies property. */
#define CSI_TX_LINK_FREQ(inst) DT_PROP_BY_IDX(MAX96724_CSI_OUT_EP, link_frequencies, 0)

#define MAX96724_INIT(inst)                                                                        \
	static const struct max96724_config max96724_config_##inst = {                             \
		.i2c = I2C_DT_SPEC_INST_GET(inst),                                                 \
		.source_dev = SOURCE_DEV(inst),                                                    \
		.csi_tx_num_lanes = CSI_TX_NUM_LANES(inst),                                        \
		.csi_tx_phy_index = CSI_TX_PHY_INDEX(inst),                                        \
		.csi_tx_link_freq = CSI_TX_LINK_FREQ(inst),                                        \
	};                                                                                         \
	static struct max96724_data max96724_data_##inst;                                          \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(inst, max96724_init, NULL, &max96724_data_##inst,                    \
			      &max96724_config_##inst, POST_KERNEL,                                \
			      CONFIG_VIDEO_MAX96724_INIT_PRIORITY, &max96724_api);                 \
                                                                                                   \
	VIDEO_DEVICE_DEFINE(max96724_##inst, DEVICE_DT_INST_GET(inst), SOURCE_DEV(inst));

DT_INST_FOREACH_STATUS_OKAY(MAX96724_INIT)
