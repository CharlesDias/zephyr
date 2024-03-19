/*
 * Copyright (c) 2024 Charles Dias <charlesdias.cd@outlook.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT st_stm32_dcmi

#include <zephyr/kernel.h>

#include <zephyr/drivers/video.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/irq.h>

#include <string.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <stm32_ll_rcc.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/drivers/clock_control/stm32_clock_control.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/pm/device.h>
#include <zephyr/sys/barrier.h>
#include <zephyr/cache.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(video_stm32_dcmi);

struct video_stm32_dcmi_data {
	const struct device *dev;
	DCMI_HandleTypeDef hdcmi;
	struct video_format fmt;
	// csi_config_t csi_config;
	// csi_handle_t csi_handle;
	struct k_fifo fifo_in;
	struct k_fifo fifo_out;
	uint32_t pixel_format;
	uint32_t height;
	uint32_t width;
	uint32_t pitch;
	struct k_poll_signal *signal;
};

struct video_stm32_dcmi_config {
	struct stm32_pclken pclken;
	const struct pinctrl_dev_config *pctrl;
	const struct device *sensor_dev;
};

static int video_stm32_dcmi_set_fmt(const struct device *dev,
				  enum video_endpoint_id ep,
				  struct video_format *fmt)
{
	const struct video_stm32_dcmi_config *config = dev->config;
	struct video_stm32_dcmi_data *data = dev->data;

	if (ep != VIDEO_EP_OUT) {
		return -EINVAL;
	}

	data->pixel_format = fmt->pixelformat;
	data->pitch = fmt->pitch;
	data->height = fmt->height;
	data->width = fmt->width;

	if (config->sensor_dev && video_set_format(config->sensor_dev, ep, fmt)) {
		return -EIO;
	}

	return 0;
}

static int video_stm32_dcmi_get_fmt(const struct device *dev,
				  enum video_endpoint_id ep,
				  struct video_format *fmt)
{
	struct video_stm32_dcmi_data *data = dev->data;
	const struct video_stm32_dcmi_config *config = dev->config;

	if (fmt == NULL || ep != VIDEO_EP_OUT) {
		return -EINVAL;
	}

	if (config->sensor_dev && !video_get_format(config->sensor_dev, ep, fmt)) {
		/* align DCMI with sensor fmt */
		return video_stm32_dcmi_set_fmt(dev, ep, fmt);
	}

	fmt->pixelformat = data->pixel_format;
	fmt->height = data->height;
	fmt->width = data->width;
	fmt->pitch = data->pitch;

	return 0;
}

static int video_stm32_dcmi_stream_start(const struct device *dev)
{
	LOG_WRN("stream_start not implemented");

	return 0;
}

static int video_stm32_dcmi_stream_stop(const struct device *dev)
{
	LOG_WRN("stream_stop not implemented");

	return -EPERM;
}

static int video_stm32_dcmi_enqueue(const struct device *dev,
				  enum video_endpoint_id ep,
				  struct video_buffer *vbuf)
{
	LOG_WRN("enqueue not implemented");

	return -EPERM;
}

static int video_stm32_dcmi_dequeue(const struct device *dev,
				  enum video_endpoint_id ep,
				  struct video_buffer **vbuf,
				  k_timeout_t timeout)
{
	LOG_WRN("dequeue not implemented");

	return -EPERM;
}

static int video_stm32_dcmi_get_caps(const struct device *dev,
				   enum video_endpoint_id ep,
				   struct video_caps *caps)
{
	const struct video_stm32_dcmi_config *config = dev->config;
	int err = -ENODEV;

	if (ep != VIDEO_EP_OUT) {
		return -EINVAL;
	}

	/* Forward the message to the sensor device */
	if (config->sensor_dev) {
		err = video_get_caps(config->sensor_dev, ep, caps);
	}

	/* NXP MCUX CSI request at least 2 buffer before starting */
	// FIXME: This value has to be checked
	caps->min_vbuf_count = 2;

	return err;
}

static const struct video_driver_api video_stm32_dcmi_driver_api = {
	.set_format = video_stm32_dcmi_set_fmt,
	.get_format = video_stm32_dcmi_get_fmt,
	.stream_start = video_stm32_dcmi_stream_start,
	.stream_stop = video_stm32_dcmi_stream_stop,
	.enqueue = video_stm32_dcmi_enqueue,
	.dequeue = video_stm32_dcmi_dequeue,
	.get_caps = video_stm32_dcmi_get_caps,
};


#if 1 /* Unique Instance */
PINCTRL_DT_INST_DEFINE(0);

static struct video_stm32_dcmi_data video_stm32_dcmi_data_0 = {
	.hdcmi = {
		.Instance = (DCMI_TypeDef *) DT_INST_REG_ADDR(0),
		.Init = {
				.SynchroMode = DCMI_SYNCHRO_HARDWARE,
				.PCKPolarity = DCMI_PCKPOLARITY_RISING,
				.VSPolarity = DCMI_VSPOLARITY_LOW,
				.HSPolarity = DCMI_HSPOLARITY_LOW,
				.CaptureRate = DCMI_CR_ALL_FRAME,
				.ExtendedDataMode = DCMI_EXTEND_DATA_8B,
				.JPEGMode = DCMI_JPEG_DISABLE,
				.ByteSelectMode = DCMI_BSM_ALL,
				.ByteSelectStart = DCMI_OEBS_ODD,
				.LineSelectMode = DCMI_LSM_ALL,
				.LineSelectStart = DCMI_OELS_ODD,
		},
	},
};

static const struct video_stm32_dcmi_config video_stm32_dcmi_config_0 = {
	.pclken = {
		.enr = DT_INST_CLOCKS_CELL(0, bits),
		.bus = DT_INST_CLOCKS_CELL(0, bus)
	},
	.pctrl = PINCTRL_DT_INST_DEV_CONFIG_GET(0),
	.sensor_dev = DEVICE_DT_GET(DT_INST_PHANDLE(0, sensor)),
};

static int video_stm32_dcmi_init_0(const struct device *dev)
{
	LOG_WRN("init_0 not implemented");

	int err;
	const struct video_stm32_dcmi_config *config = dev->config;
	struct video_stm32_dcmi_data *data = dev->data;


	/* Configure DT provided pins */
	err = pinctrl_apply_state(config->pctrl, PINCTRL_STATE_DEFAULT);
	if (err < 0) {
		LOG_ERR("DCMI pinctrl setup failed");
		return err;
	}

	if (!device_is_ready(DEVICE_DT_GET(STM32_CLOCK_CONTROL_NODE))) {
		LOG_ERR("clock control device not ready");
		return -ENODEV;
	}

	data->dev = dev;
	// data->dev.api->enqueue = video_stm32_dcmi_enqueue;
	// data->dev.api->dequeue = video_stm32_dcmi_dequeue;
	// data->dev->api.enqueue = video_stm32_dcmi_enqueue;

	/* Não inicializa se descomentar essas linhas /*

	// /* Turn on DCMI peripheral clock */
	// err = clock_control_on(DEVICE_DT_GET(STM32_CLOCK_CONTROL_NODE),
	// 			(clock_control_subsys_t) &config->pclken);
	// if (err < 0) {
	// 	LOG_ERR("Could not enable DCMI peripheral clock");
	// 	return err;
	// }

	// __HAL_RCC_DCMI_FORCE_RESET();
	// __HAL_RCC_DCMI_RELEASE_RESET();

	// /* Initialise the LTDC peripheral */
	// err = HAL_DCMI_Init(&data->hdcmi);
	// if (err != HAL_OK) {
	// 	LOG_ERR("DCMI initialization failed");
	// 	return err;
	// }

	return 0;
}

/* CONFIG_KERNEL_INIT_PRIORITY_DEVICE is used to make sure the
 * CSI peripheral is initialized before the camera, which is
 * necessary since the clock to the camera is provided by the
 * CSI peripheral.
 */
DEVICE_DT_INST_DEFINE(0, &video_stm32_dcmi_init_0,
		    NULL, &video_stm32_dcmi_data_0,
		    &video_stm32_dcmi_config_0,
		    POST_KERNEL, CONFIG_VIDEO_INIT_PRIORITY,
		    &video_stm32_dcmi_driver_api);
#endif
