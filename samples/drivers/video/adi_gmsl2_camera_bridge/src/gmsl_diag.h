/*
 * Copyright (c) 2026 Charles Dias
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef GMSL_DIAG_H
#define GMSL_DIAG_H

#include <zephyr/device.h>

void i2c_scan_devices(const struct device *i2c_dev);
void verify_device_ids(const struct device *i2c_dev);

#endif /* GMSL_DIAG_H */
