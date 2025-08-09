/*
 * Copyright (c) 2025 Charles Dias
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT st_stm32_ltdc_custom

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/logging/log.h>
#include <string.h>

#include <stm32u5xx_hal.h>
#include <stm32u5xx_hal_rcc_ex.h>
#include <stm32u5xx_hal_ltdc.h>

LOG_MODULE_REGISTER(stm32_ltdc_custom, LOG_LEVEL_INF);

/* Horizontal synchronization pulse polarity */
#define LTDC_HSPOL_ACTIVE_LOW     0x00000000
#define LTDC_HSPOL_ACTIVE_HIGH    0x80000000

/* Vertical synchronization pulse polarity */
#define LTDC_VSPOL_ACTIVE_LOW     0x00000000
#define LTDC_VSPOL_ACTIVE_HIGH    0x40000000

/* Data enable pulse polarity */
#define LTDC_DEPOL_ACTIVE_LOW     0x00000000
#define LTDC_DEPOL_ACTIVE_HIGH    0x20000000

/* Pixel clock polarity */
#define LTDC_PCPOL_ACTIVE_LOW     0x00000000
#define LTDC_PCPOL_ACTIVE_HIGH    0x10000000

#if CONFIG_STM32_LTDC_ARGB8888
#warning "Using CONFIG_STM32_LTDC_ARGB8888 pixel format"
#define STM32_LTDC_INIT_PIXEL_SIZE	4u
#define STM32_LTDC_INIT_PIXEL_FORMAT	LTDC_PIXEL_FORMAT_ARGB8888
#define DISPLAY_INIT_PIXEL_FORMAT	PIXEL_FORMAT_ARGB_8888
#elif CONFIG_STM32_LTDC_RGB888
#warning "Using CONFIG_STM32_LTDC_RGB888 pixel format"
#define STM32_LTDC_INIT_PIXEL_SIZE	3u
#define STM32_LTDC_INIT_PIXEL_FORMAT	LTDC_PIXEL_FORMAT_RGB888
#define DISPLAY_INIT_PIXEL_FORMAT	PIXEL_FORMAT_RGB_888
#elif CONFIG_STM32_LTDC_RGB565
#warning "Using CONFIG_STM32_LTDC_RGB565 pixel format"
#define STM32_LTDC_INIT_PIXEL_SIZE	2u
#define STM32_LTDC_INIT_PIXEL_FORMAT	LTDC_PIXEL_FORMAT_RGB565
#define DISPLAY_INIT_PIXEL_FORMAT	PIXEL_FORMAT_BGR_565
#else
#error "Invalid LTDC pixel format chosen"
#endif

/* Driver configuration */
struct stm32_ltdc_custom_config {
	uint32_t width;
	uint32_t height;
};

struct stm32_ltdc_custom_data {
	LTDC_HandleTypeDef hltdc;
	enum display_pixel_format current_pixel_format;
	uint8_t current_pixel_size;
	bool initialized;
};

/* Error Handler */
static void error_handler(void)
{
	LOG_ERR("STM32 DSI Display Error Handler called");
	__disable_irq();
	while (1) {
		/* Stay here */
	}
}

/* System Power Configuration */
void stm32_ltdc_custom_power_config(void)
{
	/* Disable the internal Pull-Up in Dead Battery pins of UCPD peripheral */
	HAL_PWREx_DisableUCPDDeadBattery();

	/* Switch to SMPS regulator instead of LDO */
	if (HAL_PWREx_ConfigSupply(PWR_SMPS_SUPPLY) != HAL_OK) {
		error_handler();
	}
}

/* System Clock Configuration */
void stm32_ltdc_custom_system_clock_config(void)
{
	RCC_OscInitTypeDef RCC_OscInitStruct = {0};
	RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

	/* Configure the main internal regulator output voltage */
	if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK) {
		error_handler();
	}

	/* Initializes the CPU, AHB and APB buses clocks */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_MSI;
	RCC_OscInitStruct.HSEState = RCC_HSE_ON;
	RCC_OscInitStruct.MSIState = RCC_MSI_ON;
	RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
	RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_4;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
	RCC_OscInitStruct.PLL.PLLMBOOST = RCC_PLLMBOOST_DIV1;
	RCC_OscInitStruct.PLL.PLLM = 1;
	RCC_OscInitStruct.PLL.PLLN = 80;
	RCC_OscInitStruct.PLL.PLLP = 2;
	RCC_OscInitStruct.PLL.PLLQ = 2;
	RCC_OscInitStruct.PLL.PLLR = 2;
	RCC_OscInitStruct.PLL.PLLRGE = RCC_PLLVCIRANGE_0;
	RCC_OscInitStruct.PLL.PLLFRACN = 0;

	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		error_handler();
	}

	/* Initializes the CPU, AHB and APB buses clocks */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
				      RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 |
				      RCC_CLOCKTYPE_PCLK3;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
	RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK) {
		error_handler();
	}
}

/* Peripherals Common Clock Configuration */
void stm32_ltdc_custom_periph_clock_config(void)
{
	RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

	/* Initializes the common periph clock */
	PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_LTDC | RCC_PERIPHCLK_DSI;
	PeriphClkInit.DsiClockSelection = RCC_DSICLKSOURCE_PLL3;
	PeriphClkInit.LtdcClockSelection = RCC_LTDCCLKSOURCE_PLL3;
	PeriphClkInit.PLL3.PLL3Source = RCC_PLLSOURCE_HSE;
	PeriphClkInit.PLL3.PLL3M = 4;
	PeriphClkInit.PLL3.PLL3N = 125;
	PeriphClkInit.PLL3.PLL3P = 8;
	PeriphClkInit.PLL3.PLL3Q = 2;
	PeriphClkInit.PLL3.PLL3R = 24;
	PeriphClkInit.PLL3.PLL3RGE = RCC_PLLVCIRANGE_0;
	PeriphClkInit.PLL3.PLL3FRACN = 0;
	PeriphClkInit.PLL3.PLL3ClockOut = RCC_PLL3_DIVP | RCC_PLL3_DIVR;

	if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
		error_handler();
	}
}

/* LCD Set Default Clock function - configures DSI PHY clock and LCD reset */
void stm32_ltdc_custom_lcd_set_default_clock(void)
{
	RCC_PeriphCLKInitTypeDef DSIPHYInitPeriph;

	/* Switch to DSI PHY PLL clock */
	DSIPHYInitPeriph.PeriphClockSelection = RCC_PERIPHCLK_DSI;
	DSIPHYInitPeriph.DsiClockSelection = RCC_DSICLKSOURCE_DSIPHY;

	if (HAL_RCCEx_PeriphCLKConfig(&DSIPHYInitPeriph) != HAL_OK) {
		LOG_ERR("Failed to configure DSI PHY clock");
		error_handler();
	}

	/* LCD Reset sequence */
	HAL_Delay(11);
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_5, GPIO_PIN_SET);
	HAL_Delay(150);

	LOG_INF("DSI PHY clock configured and LCD reset completed");
}

/* LTDC Initialization */
int stm32_ltdc_custom_ltdc_init(struct stm32_ltdc_custom_data *data,
				const struct stm32_ltdc_custom_config *config)
{
	/* Enable LTDC clock */
	__HAL_RCC_LTDC_CLK_ENABLE();

	if (HAL_LTDC_Init(&data->hltdc) != HAL_OK) {
		LOG_ERR("Failed to initialize LTDC");
		return -1;
	}

	if (HAL_LTDC_ConfigLayer(&data->hltdc, &data->hltdc.LayerCfg[0], LTDC_LAYER_1) != HAL_OK) {
		LOG_ERR("Failed to configure LTDC layer");
		return -1;
	}

	LOG_INF("STM32 LTDC Display Driver initialized");

	/* Log LTDC initialization parameters */
	LOG_INF("LTDC Init Parameters:");
	LOG_INF("  Instance: 0x%08X", (uint32_t)data->hltdc.Instance);
	LOG_INF("  HSPolarity: 0x%08X", data->hltdc.Init.HSPolarity);
	LOG_INF("  VSPolarity: 0x%08X", data->hltdc.Init.VSPolarity);
	LOG_INF("  DEPolarity: 0x%08X", data->hltdc.Init.DEPolarity);
	LOG_INF("  PCPolarity: 0x%08X", data->hltdc.Init.PCPolarity);
	LOG_INF("  HorizontalSync: %d", data->hltdc.Init.HorizontalSync);
	LOG_INF("  VerticalSync: %d", data->hltdc.Init.VerticalSync);
	LOG_INF("  AccumulatedHBP: %d", data->hltdc.Init.AccumulatedHBP);
	LOG_INF("  AccumulatedVBP: %d", data->hltdc.Init.AccumulatedVBP);
	LOG_INF("  AccumulatedActiveW: %d", data->hltdc.Init.AccumulatedActiveW);
	LOG_INF("  AccumulatedActiveH: %d", data->hltdc.Init.AccumulatedActiveH);
	LOG_INF("  TotalWidth: %d", data->hltdc.Init.TotalWidth);
	LOG_INF("  TotalHeight: %d", data->hltdc.Init.TotalHeigh);
	LOG_INF("  Backcolor RGB: (%d, %d, %d)", data->hltdc.Init.Backcolor.Red,
		data->hltdc.Init.Backcolor.Green, data->hltdc.Init.Backcolor.Blue);

	/* Log Layer configuration parameters */
	LOG_INF("LTDC Layer 0 Configuration:");
	LOG_INF("  Window X0: %d, X1: %d", data->hltdc.LayerCfg[0].WindowX0, data->hltdc.LayerCfg[0].WindowX1);
	LOG_INF("  Window Y0: %d, Y1: %d", data->hltdc.LayerCfg[0].WindowY0, data->hltdc.LayerCfg[0].WindowY1);
	LOG_INF("  PixelFormat: 0x%08X", data->hltdc.LayerCfg[0].PixelFormat);
	LOG_INF("  Alpha: %d", data->hltdc.LayerCfg[0].Alpha);
	LOG_INF("  Alpha0: %d", data->hltdc.LayerCfg[0].Alpha0);
	LOG_INF("  BlendingFactor1: 0x%08X", data->hltdc.LayerCfg[0].BlendingFactor1);
	LOG_INF("  BlendingFactor2: 0x%08X", data->hltdc.LayerCfg[0].BlendingFactor2);
	LOG_INF("  FBStartAddress: 0x%08X", data->hltdc.LayerCfg[0].FBStartAdress);
	LOG_INF("  ImageWidth: %d", data->hltdc.LayerCfg[0].ImageWidth);
	LOG_INF("  ImageHeight: %d", data->hltdc.LayerCfg[0].ImageHeight);
	LOG_INF("  Layer Backcolor RGB: (%d, %d, %d)", data->hltdc.LayerCfg[0].Backcolor.Red,
		data->hltdc.LayerCfg[0].Backcolor.Green, data->hltdc.LayerCfg[0].Backcolor.Blue);

	// /* Additional timing verification */
	// uint32_t horizontal_total = data->hltdc.Init.TotalWidth + 1;
	// uint32_t vertical_total = data->hltdc.Init.TotalHeigh + 1;
	// uint32_t horizontal_active = data->hltdc.Init.AccumulatedActiveW - data->hltdc.Init.AccumulatedHBP;
	// uint32_t vertical_active = data->hltdc.Init.AccumulatedActiveH - data->hltdc.Init.AccumulatedVBP;

	// LOG_INF("LTDC Timing Verification:");
	// LOG_INF("  Horizontal Total: %d pixels", horizontal_total);
	// LOG_INF("  Vertical Total: %d lines", vertical_total);
	// LOG_INF("  Horizontal Active: %d pixels", horizontal_active);
	// LOG_INF("  Vertical Active: %d lines", vertical_active);
	// LOG_INF("  Horizontal Front Porch: %d pixels", horizontal_total - data->hltdc.Init.AccumulatedActiveW - 1);
	// LOG_INF("  Vertical Front Porch: %d lines", vertical_total - data->hltdc.Init.AccumulatedActiveH - 1);
	// LOG_INF("  Horizontal Back Porch: %d pixels", data->hltdc.Init.AccumulatedHBP - data->hltdc.Init.HorizontalSync - 1);
	// LOG_INF("  Vertical Back Porch: %d lines", data->hltdc.Init.AccumulatedVBP - data->hltdc.Init.VerticalSync - 1);

	return 0;
}

/* Display driver API implementation */
int stm32_ltdc_custom_init(const struct device *dev)
{
	struct stm32_ltdc_custom_data *data = dev->data;
	const struct stm32_ltdc_custom_config *config = dev->config;
	int ret;

	LOG_INF("Initializing STM32 LTDC Display Driver");

	/* Initialize HAL */
	HAL_Init();

	/* Configure system power */
	stm32_ltdc_custom_power_config();

	/* Configure system clocks */
	stm32_ltdc_custom_system_clock_config();
	stm32_ltdc_custom_periph_clock_config();

	data->current_pixel_format = DISPLAY_INIT_PIXEL_FORMAT;
	data->current_pixel_size = STM32_LTDC_INIT_PIXEL_SIZE;

	ret = stm32_ltdc_custom_ltdc_init(data, config);
	if (ret < 0) {
		LOG_ERR("LTDC initialization failed");
		return ret;
	}

	data->initialized = true;

	return 0;
}

void stm32_ltdc_custom_get_capabilities(const struct device *dev,
					struct display_capabilities *capabilities)
{
	struct stm32_ltdc_custom_data *data = dev->data;

	memset(capabilities, 0, sizeof(struct display_capabilities));

	capabilities->x_resolution = data->hltdc.LayerCfg[0].WindowX1 -
				     data->hltdc.LayerCfg[0].WindowX0;
	capabilities->y_resolution = data->hltdc.LayerCfg[0].WindowY1 -
				     data->hltdc.LayerCfg[0].WindowY0;
	capabilities->supported_pixel_formats = PIXEL_FORMAT_ARGB_8888 |
					PIXEL_FORMAT_RGB_888 |
					PIXEL_FORMAT_BGR_565;
	capabilities->current_pixel_format = data->current_pixel_format;
	capabilities->current_orientation = DISPLAY_ORIENTATION_NORMAL;
}

void *stm32_ltdc_custom_get_framebuffer(const struct device *dev)
{
	struct stm32_ltdc_custom_data *data = dev->data;
	return (void *)data->hltdc.LayerCfg[0].FBStartAdress;
}

/* Display driver API */
static const struct display_driver_api stm32_ltdc_custom_api = {
	.get_framebuffer = stm32_ltdc_custom_get_framebuffer,
	.get_capabilities = stm32_ltdc_custom_get_capabilities,
};

#if DT_INST_NODE_HAS_PROP(0, ext_sdram)

#if DT_SAME_NODE(DT_INST_PHANDLE(0, ext_sdram), DT_NODELABEL(sdram1))
#define FRAME_BUFFER_SECTION __stm32_sdram1_section
#elif DT_SAME_NODE(DT_INST_PHANDLE(0, ext_sdram), DT_NODELABEL(sdram2))
#define FRAME_BUFFER_SECTION __stm32_sdram2_section
#elif DT_SAME_NODE(DT_INST_PHANDLE(0, ext_sdram), DT_NODELABEL(psram))
#define FRAME_BUFFER_SECTION __stm32_psram_section
#else
#error "LTDC ext-sdram property in device tree does not reference SDRAM1 or SDRAM2 node or PSRAM "\
	"node"
#define FRAME_BUFFER_SECTION
#endif /* DT_SAME_NODE(DT_INST_PHANDLE(0, ext_sdram), DT_NODELABEL(sdram1)) */

#else
#define FRAME_BUFFER_SECTION
#endif /* DT_INST_NODE_HAS_PROP(0, ext_sdram) */

#define STM32_LTDC_FRAME_BUFFER_LEN(inst)							\
	(STM32_LTDC_INIT_PIXEL_SIZE * DT_INST_PROP(inst, height) * DT_INST_PROP(inst, width))	\

#if defined(CONFIG_STM32_LTDC_FB_USE_SHARED_MULTI_HEAP)
#define STM32_LTDC_FRAME_BUFFER_ADDR(inst)  (NULL)
#define STM32_LTDC_FRAME_BUFFER_DEFINE(inst)
#else
#define STM32_LTDC_FRAME_BUFFER_ADDR(inst)  frame_buffer_##inst
#define STM32_LTDC_FRAME_BUFFER_DEFINE(inst)    \
	/* frame buffer aligned to cache line width for optimal cache flushing */                  \
	FRAME_BUFFER_SECTION static uint8_t __aligned(32)                                          \
		frame_buffer_##inst[CONFIG_STM32_LTDC_FB_NUM * STM32_LTDC_FRAME_BUFFER_LEN(inst)];
#endif

/* Device definition using device tree */
#define STM32_LTDC_CUSTOM_DEVICE(inst)								\
	STM32_LTDC_FRAME_BUFFER_DEFINE(inst);							\
	static struct stm32_ltdc_custom_data stm32_ltdc_custom_data_##inst = {			\
		.hltdc = {									\
			.Instance = (LTDC_TypeDef *) DT_INST_REG_ADDR(inst),			\
			.Init = {								\
				.HSPolarity = (DT_PROP(DT_INST_CHILD(inst, display_timings),	\
						hsync_active) ?					\
						LTDC_HSPOL_ACTIVE_HIGH : LTDC_HSPOL_ACTIVE_LOW),\
				.VSPolarity = (DT_PROP(DT_INST_CHILD(inst,			\
						display_timings), vsync_active) ?		\
						LTDC_VSPOL_ACTIVE_HIGH : LTDC_VSPOL_ACTIVE_LOW),\
				.DEPolarity = (DT_PROP(DT_INST_CHILD(inst,			\
						display_timings), de_active) ?			\
						LTDC_DEPOL_ACTIVE_HIGH : LTDC_DEPOL_ACTIVE_LOW),\
				.PCPolarity = (DT_PROP(DT_INST_CHILD(inst,			\
						display_timings), pixelclk_active) ?		\
						LTDC_PCPOL_ACTIVE_HIGH : LTDC_PCPOL_ACTIVE_LOW),\
				.HorizontalSync = DT_PROP(DT_INST_CHILD(inst,			\
							display_timings), hsync_len) - 1,	\
				.VerticalSync = DT_PROP(DT_INST_CHILD(inst,			\
							display_timings), vsync_len) - 1,	\
				.AccumulatedHBP = DT_PROP(DT_INST_CHILD(inst,			\
							display_timings), hback_porch) +	\
							DT_PROP(DT_INST_CHILD(inst,		\
							display_timings), hsync_len) - 1,	\
				.AccumulatedVBP = DT_PROP(DT_INST_CHILD(inst,			\
							display_timings), vback_porch) +	\
							DT_PROP(DT_INST_CHILD(inst,		\
							display_timings), vsync_len) - 1,	\
				.AccumulatedActiveW = DT_PROP(DT_INST_CHILD(inst,		\
							display_timings), hback_porch) +	\
							DT_PROP(DT_INST_CHILD(inst,		\
							display_timings), hsync_len) +		\
							DT_INST_PROP(inst, width) - 1,		\
				.AccumulatedActiveH = DT_PROP(DT_INST_CHILD(inst,		\
							display_timings), vback_porch) +	\
							DT_PROP(DT_INST_CHILD(inst,		\
							display_timings), vsync_len) +		\
							DT_INST_PROP(inst, height) - 1,		\
				.TotalWidth = DT_PROP(DT_INST_CHILD(inst,			\
						display_timings), hback_porch) +		\
						DT_PROP(DT_INST_CHILD(inst,			\
						display_timings), hsync_len) +			\
						DT_INST_PROP(inst, width) +			\
						DT_PROP(DT_INST_CHILD(inst,			\
						display_timings), hfront_porch) - 1,		\
				.TotalHeigh = DT_PROP(DT_INST_CHILD(inst,			\
						display_timings), vback_porch) +		\
						DT_PROP(DT_INST_CHILD(inst,			\
						display_timings), vsync_len) +			\
						DT_INST_PROP(inst, height) +			\
						DT_PROP(DT_INST_CHILD(inst,			\
						display_timings), vfront_porch) - 1,		\
				.Backcolor.Red =						\
					DT_INST_PROP_OR(inst, def_back_color_red, 0xFF),	\
				.Backcolor.Green =						\
					DT_INST_PROP_OR(inst, def_back_color_green, 0xFF),	\
				.Backcolor.Blue =						\
					DT_INST_PROP_OR(inst, def_back_color_blue, 0xFF),	\
			},									\
			.LayerCfg[0] = {							\
				.WindowX0 = DT_INST_PROP_OR(inst, window0_x0, 0),		\
				.WindowX1 = DT_INST_PROP_OR(inst, window0_x1,			\
								DT_INST_PROP(inst, width)),	\
				.WindowY0 = DT_INST_PROP_OR(inst, window0_y0, 0),		\
				.WindowY1 = DT_INST_PROP_OR(inst, window0_y1,			\
								DT_INST_PROP(inst, height)),	\
				.PixelFormat = STM32_LTDC_INIT_PIXEL_FORMAT,			\
				.Alpha = 255,							\
				.Alpha0 = 0,							\
				.BlendingFactor1 = LTDC_BLENDING_FACTOR1_CA,			\
				.BlendingFactor2 = LTDC_BLENDING_FACTOR2_CA,			\
				.FBStartAdress = (uint32_t) STM32_LTDC_FRAME_BUFFER_ADDR(inst), \
				.ImageWidth = DT_INST_PROP(inst, width),			\
				.ImageHeight = DT_INST_PROP(inst, height),			\
				.Backcolor.Red =						\
					DT_INST_PROP_OR(inst, def_back_color_red, 0xFF),	\
				.Backcolor.Green =						\
					DT_INST_PROP_OR(inst, def_back_color_green, 0xFF),	\
				.Backcolor.Blue =						\
					DT_INST_PROP_OR(inst, def_back_color_blue, 0xFF),	\
			},									\
		},										\
	};											\
	static const struct stm32_ltdc_custom_config stm32_ltdc_custom_config_##inst = {      \
		.width = DT_INST_PROP(inst, width),                                           \
		.height = DT_INST_PROP(inst, height),                                         \
	};                                                                                     \
	DEVICE_DT_INST_DEFINE(inst, &stm32_ltdc_custom_init, NULL,                             \
			       &stm32_ltdc_custom_data_##inst,                                \
			       &stm32_ltdc_custom_config_##inst,                              \
			       POST_KERNEL, CONFIG_DISPLAY_INIT_PRIORITY,                     \
			       &stm32_ltdc_custom_api);

DT_INST_FOREACH_STATUS_OKAY(STM32_LTDC_CUSTOM_DEVICE)
