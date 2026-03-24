/*
 * Copyright (c) 2026 Charles Dias
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/drivers/video.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "gmsl_diag.h"

LOG_MODULE_REGISTER(main, CONFIG_APP_LOG_LEVEL);

#if !DT_HAS_CHOSEN(zephyr_camera)
#error No camera chosen in devicetree.
#endif

#define CAMERA_DEV DEVICE_DT_GET(DT_CHOSEN(zephyr_camera))
#define I2C_BUS    DEVICE_DT_GET(DT_NODELABEL(csi_i2c))

int main(void)
{
	const struct device *camera_dev = CAMERA_DEV;
	const struct device *i2c_dev = I2C_BUS;

	LOG_INF("GMSL tunnel validation start");

	LOG_INF("Camera device: %s", camera_dev->name);

	if (!device_is_ready(i2c_dev)) {
		LOG_ERR("I2C bus %s is not ready", i2c_dev->name);
		return -ENODEV;
	}

#if DT_NODE_EXISTS(DT_NODELABEL(max96724))
	if (!device_is_ready(DEVICE_DT_GET(DT_NODELABEL(max96724)))) {
		LOG_ERR("MAX96724 bridge device is not ready");
		return -ENODEV;
	}
#endif

#if DT_NODE_EXISTS(DT_NODELABEL(max96717))
	if (!device_is_ready(DEVICE_DT_GET(DT_NODELABEL(max96717)))) {
		LOG_ERR("MAX96717 bridge device is not ready");
		return -ENODEV;
	}
#endif

	/* Scan I2C bus and print found devices */
	i2c_scan_devices(i2c_dev);

	/* Verify device IDs and print model information */
	verify_device_ids(i2c_dev);

	LOG_INF("GMSL tunnel validation succeeded");

	return 0;
}
