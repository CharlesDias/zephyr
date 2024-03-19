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

#include <zephyr/drivers/dma.h>
#include <zephyr/drivers/dma/dma_stm32.h>
#include <stm32_ll_dma.h>
// #include <stm32h7xx_hal_dma.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(video_stm32_dcmi);

struct dcmi_dma_stream {
	const struct device *dev;
	uint32_t channel;
	uint32_t channel_nb;
	DMA_TypeDef *reg;
	struct dma_config cfg;
};

struct video_stm32_dcmi_data {
	const struct device *dev;
	DMA_HandleTypeDef hdma;
	DCMI_HandleTypeDef hdcmi;
	struct video_format fmt;
	struct k_fifo fifo_in;
	struct k_fifo fifo_out;
	uint32_t pixel_format;
	uint32_t height;
	uint32_t width;
	uint32_t pitch;
	struct k_poll_signal *signal;
	struct dcmi_dma_stream dma;
	uint16_t pic[160][120];
};

struct video_stm32_dcmi_config {
	struct stm32_pclken pclken;
	const struct pinctrl_dev_config *pctrl;
	const struct device *sensor_dev;
};

// static int stm32_sdmmc_configure_dma(DMA_HandleTypeDef *handle, struct dcmi_dma_stream *dma)
// {
// 	int ret;

// 	if (!device_is_ready(dma->dev)) {
// 		LOG_ERR("Failed to get dma dev");
// 		return -ENODEV;
// 	}

// 	dma->cfg.user_data = handle;

// 	ret = dma_config(dma->dev, dma->channel, &dma->cfg);
// 	if (ret != 0) {
// 		LOG_ERR("Failed to conig");
// 		return ret;
// 	}

// 	handle->Instance                 = __LL_DMA_GET_STREAM_INSTANCE(dma->reg, dma->channel_nb);
// 	handle->Init.Channel             = dma->cfg.dma_slot * DMA_CHANNEL_1;
// 	handle->Init.PeriphInc           = DMA_PINC_DISABLE;
// 	handle->Init.MemInc              = DMA_MINC_ENABLE;
// 	handle->Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
// 	handle->Init.MemDataAlignment    = DMA_MDATAALIGN_WORD;
// 	handle->Init.Mode                = DMA_PFCTRL;
// 	handle->Init.Priority            = table_priority[dma->cfg.channel_priority],
// 	handle->Init.FIFOMode            = DMA_FIFOMODE_ENABLE;
// 	handle->Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_FULL;
// 	handle->Init.MemBurst            = DMA_MBURST_INC4;
// 	handle->Init.PeriphBurst         = DMA_PBURST_INC4;

// 	return ret;
// }

static int video_stm32_dcmi_dma_init(const struct device *dev)
{
	struct video_stm32_dcmi_data *data = dev->data;
	static DMA_HandleTypeDef hdma_handler;

	LOG_WRN("using dma");

	/* Enable DMA1 clock */
	__HAL_RCC_DMA1_CLK_ENABLE();

	/*** Configure the DMA ***/
	/* Set the parameters to be configured */
	hdma_handler.Instance = DMA1_Stream0;
	hdma_handler.Init.Request = DMA_REQUEST_DCMI;
	hdma_handler.Init.Direction = DMA_PERIPH_TO_MEMORY;
	hdma_handler.Init.PeriphInc = DMA_PINC_DISABLE;
	hdma_handler.Init.MemInc = DMA_MINC_ENABLE;
	hdma_handler.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
	hdma_handler.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
	hdma_handler.Init.Mode = DMA_CIRCULAR;
	hdma_handler.Init.Priority = DMA_PRIORITY_MEDIUM;
	hdma_handler.Init.FIFOMode = DMA_FIFOMODE_DISABLE;

	/* Associate the initialized DMA handle to the DCMI handle */
	__HAL_LINKDMA(&data->hdcmi, DMA_Handle, hdma_handler);

	/*** Configure the NVIC for DCMI and DMA ***/
	/* NVIC configuration for DCMI transfer complete interrupt */
	HAL_NVIC_SetPriority(DCMI_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(DCMI_IRQn);

	/* Configure the DMA stream */
	if(HAL_DMA_DeInit(&hdma_handler) != HAL_OK)
	{
		/* Initialization Error */
		LOG_ERR("DMA deinitialization failed");
		return -EIO;
	}

	if (HAL_DMA_Init(&hdma_handler) != HAL_OK)
	{
		/* Initialization Error */
		LOG_ERR("DMA initialization failed");
		return -EIO;
	}

	return 0;
}

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

	struct video_stm32_dcmi_data *data = dev->data;

	if (HAL_DCMI_Start_DMA(&data->hdcmi,DCMI_MODE_CONTINUOUS,
				(uint32_t)&data->pic, 160 * 120 * 2 / 4) != HAL_OK)
	// if (HAL_DCMIEx_Start_DMA_MDMA(&data->hdcmi, DCMI_MODE_CONTINUOUS,
	// 				&data->pic, 160 * 2, 120) != HAL_OK)
	{
		/* Initialization Error */
		LOG_ERR("DCMI DMA start failed");
		return -EIO;
	}

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
	// dma = {
	// 	// .dev = DEVICE_DT_GET(DT_INST_BUS(0)),
	// 	// .channel = DT_INST_PROP(0, dma_channel),
	// 	// .channel_nb = DT_INST_PROP(0, dma_channel_number),
	// 	// .reg = DT_INST_REG_ADDR_BY_NAME(0, dma),
	// 	// .cfg = {
	// 	// 	.dma_slot = DT_INST_PROP(0, dma_slot),
	// 	// 	.channel_priority = DT_INST_PROP(0, dma_priority),
	// 	// 	.dma_callback = video_stm32_dcmi_dma_cb,
	// 	// 	.linked_channels = STM32_DMA_HAL_OVERRIDE,
	// 	// },
	// 	.dev = DEVICE_DT_GET(DT_INST_BUS(0)),
	// 	.channel = DT_INST_PROP(0, dma_channel),
	// 	.channel_nb = DT_INST_PROP(0, dma_channel_number),
	// 	.reg = DT_INST_REG_ADDR_BY_NAME(0, dma),
	// 	.cfg = {
	// 		.dma_slot = DT_INST_PROP(0, dma_slot),
	// 		.channel_priority = DT_INST_PROP(0, dma_priority),
	// 		.dma_callback = video_stm32_dcmi_dma_cb,
	// 		.linked_channels = STM32_DMA_HAL_OVERRIDE,
	// 	},
	// },
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
	LOG_WRN("video_stm32_dcmi_init_0: Initializing %s", dev->name);

	int err;
	const struct video_stm32_dcmi_config *config = dev->config;
	struct video_stm32_dcmi_data *data = dev->data;


	/* Configure DT provided pins */
	err = pinctrl_apply_state(config->pctrl, PINCTRL_STATE_DEFAULT);
	if (err < 0) {
		LOG_ERR("DCMI pinctrl setup failed. \n" \
			"Error code %d: %s", err, strerror(-err));

		return err;
	}

	if (!device_is_ready(DEVICE_DT_GET(STM32_CLOCK_CONTROL_NODE))) {
		LOG_ERR("clock control device not ready");
		return -ENODEV;
	}

	data->dev = dev;

	/* Turn on DCMI peripheral clock */
	err = clock_control_on(DEVICE_DT_GET(STM32_CLOCK_CONTROL_NODE),
				(clock_control_subsys_t) &config->pclken);
	if (err < 0) {
		LOG_ERR("Could not enable DCMI peripheral clock.\n " \
			"Error code %d: %s", err, strerror(-err));
		return err;
	}

	// __HAL_RCC_DCMI_FORCE_RESET();
	// __HAL_RCC_DCMI_RELEASE_RESET();

	// Initialize DMA to transfer data from DCMI to memory
	err = video_stm32_dcmi_dma_init(dev);
	if (err < 0) {
		LOG_ERR("DCMI DMA initialization failed.\n " \
			"Error code %d: %s", err, strerror(-err));
		return err;
	}

	/* Initialise the DCMI peripheral */
	err = HAL_DCMI_Init(&data->hdcmi);
	if (err != HAL_OK) {
		LOG_ERR("DCMI initialization failed.\n " \
			"Error code %d: %s", err, strerror(-err));
		return err;
	}

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
