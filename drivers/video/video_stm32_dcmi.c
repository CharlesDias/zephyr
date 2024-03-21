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

// Define the image dimensions
#define WIDTH 160
#define HEIGHT 120

// Define the color bar
#define RED   0xF800
#define GREEN 0x07E0
#define BLUE  0x001F
#define YELLOW (RED | GREEN)
#define CYAN (GREEN | BLUE)
#define MAGENTA (RED | BLUE)
#define WHITE (RED | GREEN | BLUE)
#define BLACK 0x0000

uint16_t color_bar[8] = {RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA, WHITE, BLACK};

void generate_color_bar_image(uint16_t *image_data) {
    // Check if the memory allocation was successful
    if (image_data == NULL) {
        printf("Failed to allocate memory for image data\n");
        return;
    }

    // Fill the array with the color bar
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            int color_index = (x * 8) / WIDTH;
            image_data[y * WIDTH + x] = color_bar[color_index];
        }
    }
}

static void stm32_dcmi_isr(const struct device *dev)
{
	LOG_INF("DCMI ISR");

	struct video_stm32_dcmi_data *data = dev->data;
	struct video_buffer *vbuf;

	vbuf = k_fifo_get(&data->fifo_in, K_NO_WAIT);
	if (vbuf == NULL) {
		// LOG_ERR("No buffer available");
		goto out;
	}

	// Generate the color bar image to test the fifo and send it to the fifo_out
	generate_color_bar_image((uint16_t*)&data->pic);

	vbuf->buffer = (uint8_t*)&data->pic;
	vbuf->timestamp = k_uptime_get_32();

	k_fifo_put(&data->fifo_out, vbuf);

out:
	HAL_DCMI_IRQHandler(&data->hdcmi);
	HAL_DCMI_Suspend(&data->hdcmi);
}

static int video_stm32_dcmi_dma_init(const struct device *dev)
{
	struct video_stm32_dcmi_data *data = dev->data;
	static DMA_HandleTypeDef hdma_handler;
	HAL_StatusTypeDef ret;

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

	/* Delay of 200ms to allow the camera to start */
	k_sleep(K_MSEC(200));

	/* Start the DCMI capture */
	// ret = HAL_DCMIEx_Start_DMA_MDMA(&data->hdcmi, DCMI_MODE_CONTINUOUS,
	// 				&data->pic, 160 * 2, 120);

	// ret = HAL_DCMI_Start_DMA(&data->hdcmi,DCMI_MODE_CONTINUOUS,
	// 			(uint32_t)&data->pic, 160 * 120 * 2 / 4);
	// if (ret != HAL_OK)
	// {
	// 	/* Initialization Error */
	// 	LOG_ERR("DCMI DMA start failed");
	// 	return -EIO;
	// }

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
	LOG_WRN("Start stream capture");

	const struct video_stm32_dcmi_config *config = dev->config;
	struct video_stm32_dcmi_data *data = dev->data;
	HAL_StatusTypeDef ret;

	ret = HAL_DCMI_Start_DMA(&data->hdcmi,DCMI_MODE_CONTINUOUS,
				(uint32_t)&data->pic, 160 * 120 * 2 / 4);
	if (ret != HAL_OK)
	{
		/* Initialization Error */
		LOG_ERR("DCMI DMA start failed");
		return -EIO;
	}

	// ret = HAL_DCMI_Resume(&data->hdcmi);
	// if (ret != HAL_OK)
	// {
	// 	/* Initialization Error */
	// 	LOG_ERR("DCMI resume failed");
	// 	return -EIO;
	// }

	if (config->sensor_dev && video_stream_start(config->sensor_dev)) {
		return -EIO;
	}

	return 0;
}

static int video_stm32_dcmi_stream_stop(const struct device *dev)
{
	LOG_WRN("Stop stream capture");

	const struct video_stm32_dcmi_config *config = dev->config;
	struct video_stm32_dcmi_data *data = dev->data;
	HAL_StatusTypeDef ret;

	ret = HAL_DCMI_Stop(&data->hdcmi);
	if (ret != HAL_OK)
	{
		/* Initialization Error */
		LOG_ERR("DCMI stop failed");
		return -EIO;
	}

	if (config->sensor_dev && video_stream_stop(config->sensor_dev)) {
		return -EIO;
	}

	return 0;
}

static int video_stm32_dcmi_enqueue(const struct device *dev,
				  enum video_endpoint_id ep,
				  struct video_buffer *vbuf)
{
	LOG_INF("Enqueue buffer");
	// const struct video_stm32_dcmi_config *config = dev->config;
	struct video_stm32_dcmi_data *data = dev->data;
	unsigned int to_read;
	HAL_StatusTypeDef ret;

	if (ep != VIDEO_EP_OUT) {
		return -EINVAL;
	}

	ret = HAL_DCMI_Resume(&data->hdcmi);
	if (ret != HAL_OK)
	{
		/* Initialization Error */
		LOG_ERR("DCMI resume failed");
		return -EIO;
	}

	memset(&data->pic, 0, sizeof(data->pic));

	to_read = data->pitch * data->height;
	vbuf->bytesused = to_read;
	vbuf->buffer = (uint8_t*)&data->pic;

	k_fifo_put(&data->fifo_in, vbuf);

	return 0;
}

static int video_stm32_dcmi_dequeue(const struct device *dev,
				  enum video_endpoint_id ep,
				  struct video_buffer **vbuf,
				  k_timeout_t timeout)
{
	LOG_INF("Dequeue buffer");
	struct video_stm32_dcmi_data *data = dev->data;

	if (ep != VIDEO_EP_OUT) {
		return -EINVAL;
	}

	*vbuf = k_fifo_get(&data->fifo_out, timeout);
	if (*vbuf == NULL) {
		return -EAGAIN;
	}

	return 0;
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

static struct video_stm32_dcmi_data video_stm32_dcmi_data = {
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

static const struct video_stm32_dcmi_config video_stm32_dcmi_config = {
	.pclken = {
		.enr = DT_INST_CLOCKS_CELL(0, bits),
		.bus = DT_INST_CLOCKS_CELL(0, bus)
	},
	.pctrl = PINCTRL_DT_INST_DEV_CONFIG_GET(0),
	.sensor_dev = DEVICE_DT_GET(DT_INST_PHANDLE(0, sensor)),
};

static int video_stm32_dcmi_init(const struct device *dev)
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
	memset(&data->pic, 0, sizeof(data->pic));

	k_fifo_init(&data->fifo_in);
	k_fifo_init(&data->fifo_out);

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

	/* Connect the DCMI interrupt */
	IRQ_CONNECT(DT_INST_IRQN(0), DT_INST_IRQ(0, priority),
		stm32_dcmi_isr, DEVICE_DT_INST_GET(0), 0);
	irq_enable(DT_INST_IRQN(0));

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
DEVICE_DT_INST_DEFINE(0, &video_stm32_dcmi_init,
		    NULL, &video_stm32_dcmi_data,
		    &video_stm32_dcmi_config,
		    POST_KERNEL, CONFIG_VIDEO_INIT_PRIORITY,
		    &video_stm32_dcmi_driver_api);
#endif
