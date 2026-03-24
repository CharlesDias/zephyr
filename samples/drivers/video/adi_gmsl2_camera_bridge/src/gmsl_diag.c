/*
 * Copyright (c) 2026 Charles Dias
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "gmsl_diag.h"

#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(gmsl_diag, CONFIG_APP_LOG_LEVEL);

/* Device I2C addresses (7-bit) */
#define MAX96724_I2C_ADDR           DT_REG_ADDR(DT_NODELABEL(max96724))
#define MAX96717_I2C_ADDR           DT_REG_ADDR(DT_NODELABEL(max96717))
#define IMX219_I2C_ADDR             DT_REG_ADDR(DT_NODELABEL(imx219))

/* Chip ID register addresses (16-bit) and expected values */
#define MAX96724_REG_DEVID          0x000D
#define MAX96717_REG_DEVID          0x000D
#define IMX219_REG_DEVID            0x0000
#define IMX219_DEVID_EXPECT         0x0219

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

static const char *max96717_variant_model(uint8_t id)
{
	switch (id) {
	case 0xBF:
		return "MAX96717";
	default:
		return "unknown";
	}
}

void i2c_scan_devices(const struct device *i2c_dev)
{
	struct i2c_msg msg;
	uint8_t buffer;
	int found = 0;

	msg.buf = &buffer;
	msg.len = 0U;
	msg.flags = I2C_MSG_WRITE | I2C_MSG_STOP;

	LOG_DBG("--- Scanning I2C bus %s ---", i2c_dev->name);

	for (uint8_t addr = 0x04; addr <= 0x77; addr++) {
		if (i2c_transfer(i2c_dev, &msg, 1, addr) == 0) {
			LOG_DBG("  Found device at 0x%02X", addr);
			found++;
		}
	}

	LOG_DBG("Scan complete: %d device(s) found", found);
}

static int check_device_id(const struct device *i2c_dev, const char *name, uint8_t addr,
			   uint16_t reg, uint8_t *id, uint8_t len)
{
	uint8_t reg_buf[2] = {reg >> 8, reg & 0xFF};
	int ret;

	ret = i2c_write_read(i2c_dev, addr, reg_buf, sizeof(reg_buf), id, len);
	if (ret < 0) {
		LOG_ERR("%s (0x%02x): not reachable (err %d)", name, addr, ret);
		return ret;
	}

	if (len == 1) {
		LOG_DBG("%s (0x%02X): ID = 0x%02X", name, addr, id[0]);
	} else {
		LOG_DBG("%s (0x%02X): ID = 0x%02X 0x%02X", name, addr, id[0], id[1]);
	}

	return 0;
}

void verify_device_ids(const struct device *i2c_dev)
{
	uint8_t id[2];

	LOG_DBG("--- Device ID check ---");

	check_device_id(i2c_dev, "MAX96724", MAX96724_I2C_ADDR, MAX96724_REG_DEVID, id, 1);
	LOG_DBG("  Model: %s (ID=0x%02X)", max96724_variant_model(id[0]), id[0]);

	check_device_id(i2c_dev, "MAX96717", MAX96717_I2C_ADDR, MAX96717_REG_DEVID, id, 1);
	LOG_DBG("  Model: %s (ID=0x%02X)", max96717_variant_model(id[0]), id[0]);

	check_device_id(i2c_dev, "IMX219", IMX219_I2C_ADDR, IMX219_REG_DEVID, id, 2);
	if (((uint16_t)id[0] << 8 | id[1]) != IMX219_DEVID_EXPECT) {
		LOG_WRN("  Expected 0x%04X, got 0x%02X%02X", IMX219_DEVID_EXPECT, id[0], id[1]);
	}
}
