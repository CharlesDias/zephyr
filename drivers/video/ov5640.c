/*
 * Copyright 2024 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT ovti_ov5640

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/video.h>
#include <zephyr/kernel.h>

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(ov5640);

#include <zephyr/sys/byteorder.h>
#include "ov5640_reg.h"

// #define STM32

#define CHIP_ID_REG 0x300a
#define CHIP_ID_VAL 0x5640

#define SYS_CTRL0_REG     0x3008
#define SYS_CTRL0_SW_PWDN 0x42
#define SYS_CTRL0_SW_PWUP 0x02
#define SYS_CTRL0_SW_RST  0x82

#define SYS_RESET00_REG      0x3000
#define SYS_RESET02_REG      0x3002
#define SYS_CLK_ENABLE00_REG 0x3004
#define SYS_CLK_ENABLE02_REG 0x3006
#define IO_MIPI_CTRL00_REG   0x300e
#define SYSTEM_CONTROL1_REG  0x302e
#define SCCB_SYS_CTRL1_REG   0x3103
#define TIMING_TC_REG20_REG  0x3820
#define TIMING_TC_REG21_REG  0x3821
#define HZ5060_CTRL01_REG    0x3c01
#define ISP_CTRL01_REG       0x5001

#define SC_PLL_CTRL0_REG 0x3034
#define SC_PLL_CTRL1_REG 0x3035
#define SC_PLL_CTRL2_REG 0x3036
#define SC_PLL_CTRL3_REG 0x3037
#define SYS_ROOT_DIV_REG 0x3108
#define PCLK_PERIOD_REG  0x4837

#define AEC_CTRL00_REG 0x3a00
#define AEC_CTRL0F_REG 0x3a0f
#define AEC_CTRL10_REG 0x3a10
#define AEC_CTRL11_REG 0x3a11
#define AEC_CTRL1B_REG 0x3a1b
#define AEC_CTRL1E_REG 0x3a1e
#define AEC_CTRL1F_REG 0x3a1f

#define BLC_CTRL01_REG 0x4001
#define BLC_CTRL04_REG 0x4004
#define BLC_CTRL05_REG 0x4005

#define AWB_CTRL00_REG 0x5180
#define AWB_CTRL01_REG 0x5181
#define AWB_CTRL02_REG 0x5182
#define AWB_CTRL03_REG 0x5183
#define AWB_CTRL04_REG 0x5184
#define AWB_CTRL05_REG 0x5185
#define AWB_CTRL17_REG 0x5191
#define AWB_CTRL18_REG 0x5192
#define AWB_CTRL19_REG 0x5193
#define AWB_CTRL20_REG 0x5194
#define AWB_CTRL21_REG 0x5195
#define AWB_CTRL22_REG 0x5196
#define AWB_CTRL23_REG 0x5197
#define AWB_CTRL30_REG 0x519e

#define SDE_CTRL0_REG  0x5580
#define SDE_CTRL3_REG  0x5583
#define SDE_CTRL4_REG  0x5584
#define SDE_CTRL9_REG  0x5589
#define SDE_CTRL10_REG 0x558a
#define SDE_CTRL11_REG 0x558b

#define DEFAULT_MIPI_CHANNEL 0

#define OV5640_RESOLUTION_PARAM_NUM 24

struct ov5640_config {
	struct i2c_dt_spec i2c;
	struct gpio_dt_spec reset_gpio;
	struct gpio_dt_spec powerdown_gpio;
};

struct ov5640_data {
	struct video_format fmt;
};

struct ov5640_reg {
	uint16_t addr;
	uint8_t val;
};

struct ov5640_mipi_clock_config {
	uint8_t pllCtrl1;
	uint8_t pllCtrl2;
};

struct ov5640_resolution_config {
	uint16_t width;
	uint16_t height;
	const struct ov5640_reg *res_params;
	const struct ov5640_mipi_clock_config mipi_pclk;
};

// static const struct ov5640_reg ov5640InitParams[] = {
// 	/* Power down */
// 	{SYS_CTRL0_REG, SYS_CTRL0_SW_PWDN},

// 	/* System setting. */
// 	{SCCB_SYS_CTRL1_REG, 0x13},
// 	{SCCB_SYS_CTRL1_REG, 0x03},
// 	{SYS_RESET00_REG, 0x00},
// 	{SYS_CLK_ENABLE00_REG, 0xff},
// 	{SYS_RESET02_REG, 0x1c},
// 	{SYS_CLK_ENABLE02_REG, 0xc3},
// 	{SYSTEM_CONTROL1_REG, 0x08},
// 	{0x3618, 0x00},
// 	{0x3612, 0x29},
// 	{0x3708, 0x64},
// 	{0x3709, 0x52},
// 	{0x370c, 0x03},
// 	{TIMING_TC_REG20_REG, 0x41},
// 	{TIMING_TC_REG21_REG, 0x07},
// 	{0x3630, 0x36},
// 	{0x3631, 0x0e},
// 	{0x3632, 0xe2},
// 	{0x3633, 0x12},
// 	{0x3621, 0xe0},
// 	{0x3704, 0xa0},
// 	{0x3703, 0x5a},
// 	{0x3715, 0x78},
// 	{0x3717, 0x01},
// 	{0x370b, 0x60},
// 	{0x3705, 0x1a},
// 	{0x3905, 0x02},
// 	{0x3906, 0x10},
// 	{0x3901, 0x0a},
// 	{0x3731, 0x12},
// 	{0x3600, 0x08},
// 	{0x3601, 0x33},
// 	{0x302d, 0x60},
// 	{0x3620, 0x52},
// 	{0x371b, 0x20},
// 	{0x471c, 0x50},
// 	{0x3a13, 0x43},
// 	{0x3a18, 0x00},
// 	{0x3a19, 0x7c},
// 	{0x3635, 0x13},
// 	{0x3636, 0x03},
// 	{0x3634, 0x40},
// 	{0x3622, 0x01},
// 	{HZ5060_CTRL01_REG, 0x00},
// 	{AEC_CTRL00_REG, 0x58},
// 	{BLC_CTRL01_REG, 0x02},
// 	{BLC_CTRL04_REG, 0x02},
// 	{BLC_CTRL05_REG, 0x1a},
// 	{ISP_CTRL01_REG, 0xa3},

// 	/* AEC */
// 	{AEC_CTRL0F_REG, 0x30},
// 	{AEC_CTRL10_REG, 0x28},
// 	{AEC_CTRL1B_REG, 0x30},
// 	{AEC_CTRL1E_REG, 0x26},
// 	{AEC_CTRL11_REG, 0x60},
// 	{AEC_CTRL1F_REG, 0x14},

// 	/* AWB */
// 	{AWB_CTRL00_REG, 0xff},
// 	{AWB_CTRL01_REG, 0xf2},
// 	{AWB_CTRL02_REG, 0x00},
// 	{AWB_CTRL03_REG, 0x14},
// 	{AWB_CTRL04_REG, 0x25},
// 	{AWB_CTRL05_REG, 0x24},
// 	{0x5186, 0x09},
// 	{0x5187, 0x09},
// 	{0x5188, 0x09},
// 	{0x5189, 0x88},
// 	{0x518a, 0x54},
// 	{0x518b, 0xee},
// 	{0x518c, 0xb2},
// 	{0x518d, 0x50},
// 	{0x518e, 0x34},
// 	{0x518f, 0x6b},
// 	{0x5190, 0x46},
// 	{AWB_CTRL17_REG, 0xf8},
// 	{AWB_CTRL18_REG, 0x04},
// 	{AWB_CTRL19_REG, 0x70},
// 	{AWB_CTRL20_REG, 0xf0},
// 	{AWB_CTRL21_REG, 0xf0},
// 	{AWB_CTRL22_REG, 0x03},
// 	{AWB_CTRL23_REG, 0x01},
// 	{0x5198, 0x04},
// 	{0x5199, 0x6c},
// 	{0x519a, 0x04},
// 	{0x519b, 0x00},
// 	{0x519c, 0x09},
// 	{0x519d, 0x2b},
// 	{AWB_CTRL30_REG, 0x38},

// 	/* Color Matrix */
// 	{0x5381, 0x1e},
// 	{0x5382, 0x5b},
// 	{0x5383, 0x08},
// 	{0x5384, 0x0a},
// 	{0x5385, 0x7e},
// 	{0x5386, 0x88},
// 	{0x5387, 0x7c},
// 	{0x5388, 0x6c},
// 	{0x5389, 0x10},
// 	{0x538a, 0x01},
// 	{0x538b, 0x98},

// 	/* Sharp */
// 	{0x5300, 0x08},
// 	{0x5301, 0x30},
// 	{0x5302, 0x10},
// 	{0x5303, 0x00},
// 	{0x5304, 0x08},
// 	{0x5305, 0x30},
// 	{0x5306, 0x08},
// 	{0x5307, 0x16},
// 	{0x5309, 0x08},
// 	{0x530a, 0x30},
// 	{0x530b, 0x04},
// 	{0x530c, 0x06},

// 	/* Gamma */
// 	{0x5480, 0x01},
// 	{0x5481, 0x08},
// 	{0x5482, 0x14},
// 	{0x5483, 0x28},
// 	{0x5484, 0x51},
// 	{0x5485, 0x65},
// 	{0x5486, 0x71},
// 	{0x5487, 0x7d},
// 	{0x5488, 0x87},
// 	{0x5489, 0x91},
// 	{0x548a, 0x9a},
// 	{0x548b, 0xaa},
// 	{0x548c, 0xb8},
// 	{0x548d, 0xcd},
// 	{0x548e, 0xdd},
// 	{0x548f, 0xea},
// 	{0x5490, 0x1d},

// 	/* UV adjust. */
// 	{SDE_CTRL0_REG, 0x02},
// 	{SDE_CTRL3_REG, 0x40},
// 	{SDE_CTRL4_REG, 0x10},
// 	{SDE_CTRL9_REG, 0x10},
// 	{SDE_CTRL10_REG, 0x00},
// 	{SDE_CTRL11_REG, 0xf8},

// 	/* Lens correction. */
// 	{0x5800, 0x23},
// 	{0x5801, 0x14},
// 	{0x5802, 0x0f},
// 	{0x5803, 0x0f},
// 	{0x5804, 0x12},
// 	{0x5805, 0x26},
// 	{0x5806, 0x0c},
// 	{0x5807, 0x08},
// 	{0x5808, 0x05},
// 	{0x5809, 0x05},
// 	{0x580a, 0x08},
// 	{0x580b, 0x0d},
// 	{0x580c, 0x08},
// 	{0x580d, 0x03},
// 	{0x580e, 0x00},
// 	{0x580f, 0x00},
// 	{0x5810, 0x03},
// 	{0x5811, 0x09},
// 	{0x5812, 0x07},
// 	{0x5813, 0x03},
// 	{0x5814, 0x00},
// 	{0x5815, 0x01},
// 	{0x5816, 0x03},
// 	{0x5817, 0x08},
// 	{0x5818, 0x0d},
// 	{0x5819, 0x08},
// 	{0x581a, 0x05},
// 	{0x581b, 0x06},
// 	{0x581c, 0x08},
// 	{0x581d, 0x0e},
// 	{0x581e, 0x29},
// 	{0x581f, 0x17},
// 	{0x5820, 0x11},
// 	{0x5821, 0x11},
// 	{0x5822, 0x15},
// 	{0x5823, 0x28},
// 	{0x5824, 0x46},
// 	{0x5825, 0x26},
// 	{0x5826, 0x08},
// 	{0x5827, 0x26},
// 	{0x5828, 0x64},
// 	{0x5829, 0x26},
// 	{0x582a, 0x24},
// 	{0x582b, 0x22},
// 	{0x582c, 0x24},
// 	{0x582d, 0x24},
// 	{0x582e, 0x06},
// 	{0x582f, 0x22},
// 	{0x5830, 0x40},
// 	{0x5831, 0x42},
// 	{0x5832, 0x24},
// 	{0x5833, 0x26},
// 	{0x5834, 0x24},
// 	{0x5835, 0x22},
// 	{0x5836, 0x22},
// 	{0x5837, 0x26},
// 	{0x5838, 0x44},
// 	{0x5839, 0x24},
// 	{0x583a, 0x26},
// 	{0x583b, 0x28},
// 	{0x583c, 0x42},
// 	{0x583d, 0xce},
// 	{0x5000, 0xa7},
// };

static const struct ov5640_reg ov5640InitParams_ST[] = {
    {OV5640_SCCB_SYSTEM_CTRL1, 0x11},
    {OV5640_SYSTEM_CTROL0, 0x82},
    {OV5640_SCCB_SYSTEM_CTRL1, 0x03},
    {OV5640_PAD_OUTPUT_ENABLE01, 0xFF},
    {OV5640_PAD_OUTPUT_ENABLE02, 0xf3},
    {OV5640_SC_PLL_CONTRL0, 0x18},
    {OV5640_SYSTEM_CTROL0, 0x02},
    {OV5640_SC_PLL_CONTRL1, 0x41},
    {OV5640_SC_PLL_CONTRL2, 0x30},
    {OV5640_SC_PLL_CONTRL3, 0x13},
    {OV5640_SYSTEM_ROOT_DIVIDER, 0x01},
    {0x3630, 0x36},
    {0x3631, 0x0e},
    {0x3632, 0xe2},
    {0x3633, 0x12},
    {0x3621, 0xe0},
    {0x3704, 0xa0},
    {0x3703, 0x5a},
    {0x3715, 0x78},
    {0x3717, 0x01},
    {0x370b, 0x60},
    {0x3705, 0x1a},
    {0x3905, 0x02},
    {0x3906, 0x10},
    {0x3901, 0x0a},
    {0x3731, 0x12},
    {0x3600, 0x08},
    {0x3601, 0x33},
    {0x302d, 0x60},
    {0x3620, 0x52},
    {0x371b, 0x20},
    {0x471c, 0x50},
    {OV5640_AEC_CTRL13, 0x43},
    {OV5640_AEC_GAIN_CEILING_HIGH, 0x00},
    {OV5640_AEC_GAIN_CEILING_LOW, 0xf8},
    {0x3635, 0x13},
    {0x3636, 0x03},
    {0x3634, 0x40},
    {0x3622, 0x01},
    {OV5640_5060HZ_CTRL01, 0x34},
    {OV5640_5060HZ_CTRL04, 0x28},
    {OV5640_5060HZ_CTRL05, 0x98},
    {OV5640_LIGHTMETER1_TH_HIGH, 0x00},
    {OV5640_LIGHTMETER1_TH_LOW, 0x00},
    {OV5640_LIGHTMETER2_TH_HIGH, 0x01},
    {OV5640_LIGHTMETER2_TH_LOW, 0x2c},
    {OV5640_SAMPLE_NUMBER_HIGH, 0x9c},
    {OV5640_SAMPLE_NUMBER_LOW, 0x40},
    {OV5640_TIMING_TC_REG20, 0x06},
    {OV5640_TIMING_TC_REG21, 0x00},
    {OV5640_TIMING_X_INC, 0x31},
    {OV5640_TIMING_Y_INC, 0x31},
    {OV5640_TIMING_HS_HIGH, 0x00},
    {OV5640_TIMING_HS_LOW, 0x00},
    {OV5640_TIMING_VS_HIGH, 0x00},
    {OV5640_TIMING_VS_LOW, 0x04},
    {OV5640_TIMING_HW_HIGH, 0x0a},
    {OV5640_TIMING_HW_LOW, 0x3f},
    {OV5640_TIMING_VH_HIGH, 0x07},
    {OV5640_TIMING_VH_LOW, 0x9b},
    {OV5640_TIMING_DVPHO_HIGH, 0x03},
    {OV5640_TIMING_DVPHO_LOW, 0x20},
    {OV5640_TIMING_DVPVO_HIGH, 0x02},
    {OV5640_TIMING_DVPVO_LOW, 0x58},
    {OV5640_TIMING_HTS_HIGH, 0x06},
    {OV5640_TIMING_HTS_LOW, 0x40},
    {OV5640_TIMING_VTS_HIGH, 0x03},
    {OV5640_TIMING_VTS_LOW, 0xe8},
    {OV5640_TIMING_HOFFSET_HIGH, 0x00},
    {OV5640_TIMING_HOFFSET_LOW, 0x10},
    {OV5640_TIMING_VOFFSET_HIGH, 0x00},
    {OV5640_TIMING_VOFFSET_LOW, 0x06},
    {0x3618, 0x00},
    {0x3612, 0x29},
    {0x3708, 0x64},
    {0x3709, 0x52},
    {0x370c, 0x03},
    {OV5640_AEC_CTRL02, 0x03},
    {OV5640_AEC_CTRL03, 0xd8},
    {OV5640_AEC_B50_STEP_HIGH, 0x01},
    {OV5640_AEC_B50_STEP_LOW, 0x27},
    {OV5640_AEC_B60_STEP_HIGH, 0x00},
    {OV5640_AEC_B60_STEP_LOW, 0xf6},
    {OV5640_AEC_CTRL0E, 0x03},
    {OV5640_AEC_CTRL0D, 0x04},
    {OV5640_AEC_MAX_EXPO_HIGH, 0x03},
    {OV5640_AEC_MAX_EXPO_LOW, 0xd8},
    {OV5640_BLC_CTRL01, 0x02},
    {OV5640_BLC_CTRL04, 0x02},
    {OV5640_SYSREM_RESET00, 0x00},
    {OV5640_SYSREM_RESET02, 0x1c},
    {OV5640_CLOCK_ENABLE00, 0xff},
    {OV5640_CLOCK_ENABLE02, 0xc3},
    {OV5640_MIPI_CONTROL00, 0x58},
    {0x302e, 0x00},
    {OV5640_POLARITY_CTRL, 0x22},
    {OV5640_FORMAT_CTRL00, 0x6F},
    {OV5640_FORMAT_MUX_CTRL, 0x01},
    {OV5640_JPG_MODE_SELECT, 0x03},
    {OV5640_JPEG_CTRL07, 0x04},
    {0x440e, 0x00},
    {0x460b, 0x35},
    {0x460c, 0x23},
    {OV5640_PCLK_PERIOD, 0x22},
    {0x3824, 0x02},
    {OV5640_ISP_CONTROL00, 0xa7},
    {OV5640_ISP_CONTROL01, 0xa3},
    {OV5640_AWB_CTRL00, 0xff},
    {OV5640_AWB_CTRL01, 0xf2},
    {OV5640_AWB_CTRL02, 0x00},
    {OV5640_AWB_CTRL03, 0x14},
    {OV5640_AWB_CTRL04, 0x25},
    {OV5640_AWB_CTRL05, 0x24},
    {OV5640_AWB_CTRL06, 0x09},
    {OV5640_AWB_CTRL07, 0x09},
    {OV5640_AWB_CTRL08, 0x09},
    {OV5640_AWB_CTRL09, 0x75},
    {OV5640_AWB_CTRL10, 0x54},
    {OV5640_AWB_CTRL11, 0xe0},
    {OV5640_AWB_CTRL12, 0xb2},
    {OV5640_AWB_CTRL13, 0x42},
    {OV5640_AWB_CTRL14, 0x3d},
    {OV5640_AWB_CTRL15, 0x56},
    {OV5640_AWB_CTRL16, 0x46},
    {OV5640_AWB_CTRL17, 0xf8},
    {OV5640_AWB_CTRL18, 0x04},
    {OV5640_AWB_CTRL19, 0x70},
    {OV5640_AWB_CTRL20, 0xf0},
    {OV5640_AWB_CTRL21, 0xf0},
    {OV5640_AWB_CTRL22, 0x03},
    {OV5640_AWB_CTRL23, 0x01},
    {OV5640_AWB_CTRL24, 0x04},
    {OV5640_AWB_CTRL25, 0x12},
    {OV5640_AWB_CTRL26, 0x04},
    {OV5640_AWB_CTRL27, 0x00},
    {OV5640_AWB_CTRL28, 0x06},
    {OV5640_AWB_CTRL29, 0x82},
    {OV5640_AWB_CTRL30, 0x38},
    {OV5640_CMX1, 0x1e},
    {OV5640_CMX2, 0x5b},
    {OV5640_CMX3, 0x08},
    {OV5640_CMX4, 0x0a},
    {OV5640_CMX5, 0x7e},
    {OV5640_CMX6, 0x88},
    {OV5640_CMX7, 0x7c},
    {OV5640_CMX8, 0x6c},
    {OV5640_CMX9, 0x10},
    {OV5640_CMXSIGN_HIGH, 0x01},
    {OV5640_CMXSIGN_LOW, 0x98},
    {OV5640_CIP_SHARPENMT_TH1, 0x08},
    {OV5640_CIP_SHARPENMT_TH2, 0x30},
    {OV5640_CIP_SHARPENMT_OFFSET1, 0x10},
    {OV5640_CIP_SHARPENMT_OFFSET2, 0x00},
    {OV5640_CIP_DNS_TH1, 0x08},
    {OV5640_CIP_DNS_TH2, 0x30},
    {OV5640_CIP_DNS_OFFSET1, 0x08},
    {OV5640_CIP_DNS_OFFSET2, 0x16},
    {OV5640_CIP_CTRL, 0x08},
    {OV5640_CIP_SHARPENTH_TH1, 0x30},
    {OV5640_CIP_SHARPENTH_TH2, 0x04},
    {OV5640_CIP_SHARPENTH_OFFSET1, 0x06},
    {OV5640_GAMMA_CTRL00, 0x01},
    {OV5640_GAMMA_YST00, 0x08},
    {OV5640_GAMMA_YST01, 0x14},
    {OV5640_GAMMA_YST02, 0x28},
    {OV5640_GAMMA_YST03, 0x51},
    {OV5640_GAMMA_YST04, 0x65},
    {OV5640_GAMMA_YST05, 0x71},
    {OV5640_GAMMA_YST06, 0x7d},
    {OV5640_GAMMA_YST07, 0x87},
    {OV5640_GAMMA_YST08, 0x91},
    {OV5640_GAMMA_YST09, 0x9a},
    {OV5640_GAMMA_YST0A, 0xaa},
    {OV5640_GAMMA_YST0B, 0xb8},
    {OV5640_GAMMA_YST0C, 0xcd},
    {OV5640_GAMMA_YST0D, 0xdd},
    {OV5640_GAMMA_YST0E, 0xea},
    {OV5640_GAMMA_YST0F, 0x1d},
    {OV5640_SDE_CTRL0, 0x02},
    {OV5640_SDE_CTRL3, 0x40},
    {OV5640_SDE_CTRL4, 0x10},
    {OV5640_SDE_CTRL9, 0x10},
    {OV5640_SDE_CTRL10, 0x00},
    {OV5640_SDE_CTRL11, 0xf8},
    {OV5640_GMTRX00, 0x23},
    {OV5640_GMTRX01, 0x14},
    {OV5640_GMTRX02, 0x0f},
    {OV5640_GMTRX03, 0x0f},
    {OV5640_GMTRX04, 0x12},
    {OV5640_GMTRX05, 0x26},
    {OV5640_GMTRX10, 0x0c},
    {OV5640_GMTRX11, 0x08},
    {OV5640_GMTRX12, 0x05},
    {OV5640_GMTRX13, 0x05},
    {OV5640_GMTRX14, 0x08},
    {OV5640_GMTRX15, 0x0d},
    {OV5640_GMTRX20, 0x08},
    {OV5640_GMTRX21, 0x03},
    {OV5640_GMTRX22, 0x00},
    {OV5640_GMTRX23, 0x00},
    {OV5640_GMTRX24, 0x03},
    {OV5640_GMTRX25, 0x09},
    {OV5640_GMTRX30, 0x07},
    {OV5640_GMTRX31, 0x03},
    {OV5640_GMTRX32, 0x00},
    {OV5640_GMTRX33, 0x01},
    {OV5640_GMTRX34, 0x03},
    {OV5640_GMTRX35, 0x08},
    {OV5640_GMTRX40, 0x0d},
    {OV5640_GMTRX41, 0x08},
    {OV5640_GMTRX42, 0x05},
    {OV5640_GMTRX43, 0x06},
    {OV5640_GMTRX44, 0x08},
    {OV5640_GMTRX45, 0x0e},
    {OV5640_GMTRX50, 0x29},
    {OV5640_GMTRX51, 0x17},
    {OV5640_GMTRX52, 0x11},
    {OV5640_GMTRX53, 0x11},
    {OV5640_GMTRX54, 0x15},
    {OV5640_GMTRX55, 0x28},
    {OV5640_BRMATRX00, 0x46},
    {OV5640_BRMATRX01, 0x26},
    {OV5640_BRMATRX02, 0x08},
    {OV5640_BRMATRX03, 0x26},
    {OV5640_BRMATRX04, 0x64},
    {OV5640_BRMATRX05, 0x26},
    {OV5640_BRMATRX06, 0x24},
    {OV5640_BRMATRX07, 0x22},
    {OV5640_BRMATRX08, 0x24},
    {OV5640_BRMATRX09, 0x24},
    {OV5640_BRMATRX20, 0x06},
    {OV5640_BRMATRX21, 0x22},
    {OV5640_BRMATRX22, 0x40},
    {OV5640_BRMATRX23, 0x42},
    {OV5640_BRMATRX24, 0x24},
    {OV5640_BRMATRX30, 0x26},
    {OV5640_BRMATRX31, 0x24},
    {OV5640_BRMATRX32, 0x22},
    {OV5640_BRMATRX33, 0x22},
    {OV5640_BRMATRX34, 0x26},
    {OV5640_BRMATRX40, 0x44},
    {OV5640_BRMATRX41, 0x24},
    {OV5640_BRMATRX42, 0x26},
    {OV5640_BRMATRX43, 0x28},
    {OV5640_BRMATRX44, 0x42},
    {OV5640_LENC_BR_OFFSET, 0xce},
    {0x5025, 0x00},
    {OV5640_AEC_CTRL0F, 0x30},
    {OV5640_AEC_CTRL10, 0x28},
    {OV5640_AEC_CTRL1B, 0x30},
    {OV5640_AEC_CTRL1E, 0x26},
    {OV5640_AEC_CTRL11, 0x60},
    {OV5640_AEC_CTRL1F, 0x14},
    {OV5640_SYSTEM_CTROL0, 0x02},
};

static const struct ov5640_reg ov5640InitParams_Weact[] = {
	{0x4740, 0x21}, // { 0x47, 0x40, 0x20 },
	{0x4050, 0x6e},
	{0x4051, 0x8f},
	{0x3008, 0x42},
	{0x3103, 0x03},
	{0x3017, 0xff}, // { 0x30, 0x17, 0x7f },
	{0x3018, 0xff},
	{0x302c, 0x02},
	{0x3108, 0x01},
	{0x3630, 0x2e},
	{0x3632, 0xe2},
	{0x3633, 0x23},
	{0x3621, 0xe0},
	{0x3704, 0xa0},
	{0x3703, 0x5a},
	{0x3715, 0x78},
	{0x3717, 0x01},
	{0x370b, 0x60},
	{0x3705, 0x1a},
	{0x3905, 0x02},
	{0x3906, 0x10},
	{0x3901, 0x0a},
	{0x3731, 0x12},
	{0x3600, 0x08},
	{0x3601, 0x33},
	{0x302d, 0x60},
	{0x3620, 0x52},
	{0x371b, 0x20},
	{0x471c, 0x50},
	{0x3a18, 0x00},
	{0x3a19, 0xf8},
	{0x3635, 0x1c},
	{0x3634, 0x40},
	{0x3622, 0x01},
	{0x3c04, 0x28},
	{0x3c05, 0x98},
	{0x3c06, 0x00},
	{0x3c07, 0x08},
	{0x3c08, 0x00},
	{0x3c09, 0x1c},
	{0x3c0a, 0x9c},
	{0x3c0b, 0x40},
	{0x3820, 0x47}, // { 0x38, 0x20, 0x41 },
	{0x3821, 0x01},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x04},
	{0x3804, 0x0a},
	{0x3805, 0x3f},
	{0x3806, 0x07},
	{0x3807, 0x9b},
	{0x3808, 0x05},
	{0x3809, 0x00},
	{0x380a, 0x03},
	{0x380b, 0xc0},
	{0x3810, 0x00},
	{0x3811, 0x10},
	{0x3812, 0x00},
	{0x3813, 0x06},
	{0x3814, 0x31},
	{0x3815, 0x31},
	{0x3034, 0x1a},
	{0x3035, 0x11}, // { 0x30, 0x35, 0x21 },
	{0x3036, 0x64}, // { 0x30, 0x36, 0x46 },
	{0x3037, 0x13},
	{0x3038, 0x00},
	{0x3039, 0x00},
	{0x380c, 0x07},
	{0x380d, 0x68},
	{0x380e, 0x03},
	{0x380f, 0xd8},
	{0x3c01, 0xb4},
	{0x3c00, 0x04},
	{0x3a08, 0x00},
	{0x3a09, 0x93},
	{0x3a0e, 0x06},
	{0x3a0a, 0x00},
	{0x3a0b, 0x7b},
	{0x3a0d, 0x08},
	{0x3a00, 0x38}, // { 0x3a, 0x00, 0x3c },
	{0x3a02, 0x05},
	{0x3a03, 0xc4},
	{0x3a14, 0x05},
	{0x3a15, 0xc4},
	{0x3618, 0x00},
	{0x3612, 0x29},
	{0x3708, 0x64},
	{0x3709, 0x52},
	{0x370c, 0x03},
	{0x4001, 0x02},
	{0x4004, 0x02},
	{0x3000, 0x00},
	{0x3002, 0x1c},
	{0x3004, 0xff},
	{0x3006, 0xc3},
	{0x300e, 0x58},
	{0x302e, 0x00},
	{0x4300, 0x30},
	{0x501f, 0x00},
	{0x4713, 0x04}, // { 0x47, 0x13, 0x03 },
	{0x4407, 0x04},
	{0x460b, 0x35},
	{0x460c, 0x22},
	{0x3824, 0x02}, // { 0x38, 0x24, 0x01 },
	{0x5001, 0xa3},
	{0x3406, 0x01},
	{0x3400, 0x06},
	{0x3401, 0x80},
	{0x3402, 0x04},
	{0x3403, 0x00},
	{0x3404, 0x06},
	{0x3405, 0x00},
	{0x5180, 0xff},
	{0x5181, 0xf2},
	{0x5182, 0x00},
	{0x5183, 0x14},
	{0x5184, 0x25},
	{0x5185, 0x24},
	{0x5186, 0x16},
	{0x5187, 0x16},
	{0x5188, 0x16},
	{0x5189, 0x62},
	{0x518a, 0x62},
	{0x518b, 0xf0},
	{0x518c, 0xb2},
	{0x518d, 0x50},
	{0x518e, 0x30},
	{0x518f, 0x30},
	{0x5190, 0x50},
	{0x5191, 0xf8},
	{0x5192, 0x04},
	{0x5193, 0x70},
	{0x5194, 0xf0},
	{0x5195, 0xf0},
	{0x5196, 0x03},
	{0x5197, 0x01},
	{0x5198, 0x04},
	{0x5199, 0x12},
	{0x519a, 0x04},
	{0x519b, 0x00},
	{0x519c, 0x06},
	{0x519d, 0x82},
	{0x519e, 0x38},
	{0x5381, 0x1e},
	{0x5382, 0x5b},
	{0x5383, 0x14},
	{0x5384, 0x06},
	{0x5385, 0x82},
	{0x5386, 0x88},
	{0x5387, 0x7c},
	{0x5388, 0x60},
	{0x5389, 0x1c},
	{0x538a, 0x01},
	{0x538b, 0x98},
	{0x5300, 0x08},
	{0x5301, 0x30},
	{0x5302, 0x3f},
	{0x5303, 0x10},
	{0x5304, 0x08},
	{0x5305, 0x30},
	{0x5306, 0x18},
	{0x5307, 0x28},
	{0x5309, 0x08},
	{0x530a, 0x30},
	{0x530b, 0x04},
	{0x530c, 0x06},
	{0x5480, 0x01},
	{0x5481, 0x06},
	{0x5482, 0x12},
	{0x5483, 0x24},
	{0x5484, 0x4a},
	{0x5485, 0x58},
	{0x5486, 0x65},
	{0x5487, 0x72},
	{0x5488, 0x7d},
	{0x5489, 0x88},
	{0x548a, 0x92},
	{0x548b, 0xa3},
	{0x548c, 0xb2},
	{0x548d, 0xc8},
	{0x548e, 0xdd},
	{0x548f, 0xf0},
	{0x5490, 0x15},
	{0x5580, 0x06},
	{0x5583, 0x40},
	{0x5584, 0x20},
	{0x5589, 0x10},
	{0x558a, 0x00},
	{0x558b, 0xf8},
	{0x5000, 0xa7},
	{0x5800, 0x20},
	{0x5801, 0x19},
	{0x5802, 0x17},
	{0x5803, 0x16},
	{0x5804, 0x18},
	{0x5805, 0x21},
	{0x5806, 0x0F},
	{0x5807, 0x0A},
	{0x5808, 0x07},
	{0x5809, 0x07},
	{0x580a, 0x0A},
	{0x580b, 0x0C},
	{0x580c, 0x0A},
	{0x580d, 0x03},
	{0x580e, 0x01},
	{0x580f, 0x01},
	{0x5810, 0x03},
	{0x5811, 0x09},
	{0x5812, 0x0A},
	{0x5813, 0x03},
	{0x5814, 0x01},
	{0x5815, 0x01},
	{0x5816, 0x03},
	{0x5817, 0x08},
	{0x5818, 0x10},
	{0x5819, 0x0A},
	{0x581a, 0x06},
	{0x581b, 0x06},
	{0x581c, 0x08},
	{0x581d, 0x0E},
	{0x581e, 0x22},
	{0x581f, 0x18},
	{0x5820, 0x13},
	{0x5821, 0x12},
	{0x5822, 0x16},
	{0x5823, 0x1E},
	{0x5824, 0x64},
	{0x5825, 0x2A},
	{0x5826, 0x2C},
	{0x5827, 0x2A},
	{0x5828, 0x46},
	{0x5829, 0x2A},
	{0x582a, 0x26},
	{0x582b, 0x24},
	{0x582c, 0x26},
	{0x582d, 0x2A},
	{0x582e, 0x28},
	{0x582f, 0x42},
	{0x5830, 0x40},
	{0x5831, 0x42},
	{0x5832, 0x08},
	{0x5833, 0x28},
	{0x5834, 0x26},
	{0x5835, 0x24},
	{0x5836, 0x26},
	{0x5837, 0x2A},
	{0x5838, 0x44},
	{0x5839, 0x4A},
	{0x583a, 0x2C},
	{0x583b, 0x2a},
	{0x583c, 0x46},
	{0x583d, 0xCE},
	{0x5688, 0x22},
	{0x5689, 0x22},
	{0x568a, 0x42},
	{0x568b, 0x24},
	{0x568c, 0x42},
	{0x568d, 0x24},
	{0x568e, 0x22},
	{0x568f, 0x22},
	{0x5025, 0x00},
	{0x3a0f, 0x30},
	{0x3a10, 0x28},
	{0x3a1b, 0x30},
	{0x3a1e, 0x28},
	{0x3a11, 0x61},
	{0x3a1f, 0x10},
	{0x4005, 0x1a},
	{0x3406, 0x00},
	{0x3503, 0x00},
	{0x3008, 0x02},
	// OpenMV Custom.
	{0x3a02, 0x07},
	{0x3a03, 0xae},
	{0x3a08, 0x01},
	{0x3a09, 0x27},
	{0x3a0a, 0x00},
	{0x3a0b, 0xf6},
	{0x3a0e, 0x06},
	{0x3a0d, 0x08},
	{0x3a14, 0x07},
	{0x3a15, 0xae},
	{0x4401, 0x0d}, // | Read SRAM enable when blanking | Read SRAM at first blanking
	{0x4723, 0x03}, // DVP JPEG Mode456 Skip Line Number
};
static const struct ov5640_reg ov5640_320_240_res_params[] = {
	{0x3800, 0x00}, {0x3801, 0x00}, {0x3802, 0x00}, {0x3803, 0x04}, {0x3804, 0x0a},
	{0x3805, 0x3f}, {0x3806, 0x07}, {0x3807, 0x9b}, {0x3808, 0x01}, {0x3809, 0x40},
	{0x380a, 0x00},	{0x380b, 0xf0}, {0x380c, 0x07}, {0x380d, 0x68}, {0x380e, 0x03},
	{0x380f, 0xd8}, {0x3810, 0x00}, {0x3811, 0x10}, {0x3812, 0x00}, {0x3813, 0x06},
	{0x3814, 0x31}, {0x3815, 0x31}, {0x3824, 0x02}, {0x460c, 0x22}};

static const struct ov5640_reg ov5640_low_res_params[] = {
	{0x3800, 0x00}, {0x3801, 0x00}, {0x3802, 0x00}, {0x3803, 0x04}, {0x3804, 0x0a},
	{0x3805, 0x3f}, {0x3806, 0x07}, {0x3807, 0x9b}, {0x3808, 0x02}, {0x3809, 0x80},
	{0x380a, 0x01}, {0x380b, 0xe0}, {0x380c, 0x07}, {0x380d, 0x68}, {0x380e, 0x03},
	{0x380f, 0xd8}, {0x3810, 0x00}, {0x3811, 0x10}, {0x3812, 0x00}, {0x3813, 0x06},
	{0x3814, 0x31}, {0x3815, 0x31}, {0x3824, 0x02}, {0x460c, 0x22}};

static const struct ov5640_reg ov5640_720p_res_params[] = {
	{0x3800, 0x00}, {0x3801, 0x00}, {0x3802, 0x00}, {0x3803, 0xfa}, {0x3804, 0x0a},
	{0x3805, 0x3f}, {0x3806, 0x06}, {0x3807, 0xa9}, {0x3808, 0x05}, {0x3809, 0x00},
	{0x380a, 0x02}, {0x380b, 0xd0}, {0x380c, 0x07}, {0x380d, 0x64}, {0x380e, 0x02},
	{0x380f, 0xe4}, {0x3810, 0x00}, {0x3811, 0x10}, {0x3812, 0x00}, {0x3813, 0x04},
	{0x3814, 0x31}, {0x3815, 0x31}, {0x3824, 0x04}, {0x460c, 0x20}};

static const struct ov5640_resolution_config resolutionParams[] = {
	{.width = 160,
	 .height = 120,
	 .res_params = ov5640_320_240_res_params,
	 .mipi_pclk = {
			.pllCtrl1 = 0x41,
			.pllCtrl2 = 0x30,
		 }},
	{.width = 320,
	 .height = 240,
	 .res_params = ov5640_320_240_res_params,
	 .mipi_pclk = {
			.pllCtrl1 = 0x41,
			.pllCtrl2 = 0x30,
		 }},
	{.width = 480,
	 .height = 272,
	 .res_params = ov5640_320_240_res_params,
	 .mipi_pclk = {
			.pllCtrl1 = 0x41,
			.pllCtrl2 = 0x30,
		 }},
	{.width = 640,
	 .height = 480,
	 .res_params = ov5640_low_res_params,
	 .mipi_pclk = {
			 .pllCtrl1 = 0x14,
			 .pllCtrl2 = 0x38,
		 }},
	{.width = 1280,
	 .height = 720,
	 .res_params = ov5640_720p_res_params,
	 .mipi_pclk = {
			 .pllCtrl1 = 0x21,
			 .pllCtrl2 = 0x54,
		 }},
};

#define OV5640_VIDEO_FORMAT_CAP(width, height, format)                                             \
	{                                                                                          \
		.pixelformat = (format), .width_min = (width), .width_max = (width),               \
		.height_min = (height), .height_max = (height), .width_step = 0, .height_step = 0  \
	}

static const struct video_format_cap fmts[] = {
	OV5640_VIDEO_FORMAT_CAP(1280, 720, VIDEO_PIX_FMT_RGB565),
	OV5640_VIDEO_FORMAT_CAP(1280, 720, VIDEO_PIX_FMT_YUYV),
	OV5640_VIDEO_FORMAT_CAP(640, 480, VIDEO_PIX_FMT_RGB565),
	OV5640_VIDEO_FORMAT_CAP(640, 480, VIDEO_PIX_FMT_YUYV),
	OV5640_VIDEO_FORMAT_CAP(480, 272, VIDEO_PIX_FMT_RGB565),
	OV5640_VIDEO_FORMAT_CAP(320, 240, VIDEO_PIX_FMT_RGB565),
	OV5640_VIDEO_FORMAT_CAP(160, 120, VIDEO_PIX_FMT_RGB565),
	{0}};

static int ov5640_read_reg(const struct i2c_dt_spec *spec, const uint16_t addr, void *val,
			   const uint8_t val_size)
{
	int ret;
	struct i2c_msg msg[2];
	uint8_t addr_buf[2];

	if (val_size > 4) {
		return -ENOTSUP;
	}

	addr_buf[1] = addr & 0xFF;
	addr_buf[0] = addr >> 8;
	msg[0].buf = addr_buf;
	msg[0].len = 2U;
	msg[0].flags = I2C_MSG_WRITE;

	msg[1].buf = (uint8_t *)val;
	msg[1].len = val_size;
	msg[1].flags = I2C_MSG_READ | I2C_MSG_STOP | I2C_MSG_RESTART;

	ret = i2c_transfer_dt(spec, msg, 2);
	if (ret) {
		return ret;
	}

	switch (val_size) {
	case 4:
		*(uint32_t *)val = sys_be32_to_cpu(*(uint32_t *)val);
		break;
	case 2:
		*(uint16_t *)val = sys_be16_to_cpu(*(uint16_t *)val);
		break;
	case 1:
		break;
	default:
		return -ENOTSUP;
	}

	return 0;
}

static int ov5640_write_reg(const struct i2c_dt_spec *spec, const uint16_t addr, const uint8_t val)
{
	uint8_t addr_buf[2];
	struct i2c_msg msg[2];

	addr_buf[1] = addr & 0xFF;
	addr_buf[0] = addr >> 8;
	msg[0].buf = addr_buf;
	msg[0].len = 2U;
	msg[0].flags = I2C_MSG_WRITE;

	msg[1].buf = (uint8_t *)&val;
	msg[1].len = 1;
	msg[1].flags = I2C_MSG_WRITE | I2C_MSG_STOP;

	return i2c_transfer_dt(spec, msg, 2);
}

static int ov5640_modify_reg(const struct i2c_dt_spec *spec, const uint16_t addr,
			     const uint8_t mask, const uint8_t val)
{
	uint8_t regVal = 0;
	int ret = ov5640_read_reg(spec, addr, &regVal, sizeof(regVal));

	if (ret) {
		return ret;
	}

	return ov5640_write_reg(spec, addr, (regVal & ~mask) | (val & mask));
}

static int ov5640_write_multi_regs(const struct i2c_dt_spec *spec, const struct ov5640_reg *regs,
				   const uint32_t num_regs)
{
	int ret;

	for (int i = 0; i < num_regs; i++) {
		ret = ov5640_write_reg(spec, regs[i].addr, regs[i].val);
		if (ret) {
			return ret;
		}
	}

	return 0;
}

// Need GNU extensions
#define IM_MAX(a, b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a > _b ? _a : _b; })
#define IM_MIN(a, b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a < _b ? _a : _b; })

#define BLANK_LINES 8
#define DUMMY_LINES 6

#define BLANK_COLUMNS 0
#define DUMMY_COLUMNS 16

#define SENSOR_WIDTH 2624
#define SENSOR_HEIGHT 1964

#define ACTIVE_SENSOR_WIDTH (SENSOR_WIDTH - BLANK_COLUMNS - (2 * DUMMY_COLUMNS))
#define ACTIVE_SENSOR_HEIGHT (SENSOR_HEIGHT - BLANK_LINES - (2 * DUMMY_LINES))

#define DUMMY_WIDTH_BUFFER 16
#define DUMMY_HEIGHT_BUFFER 8

#define HSYNC_TIME 252
#define VYSNC_TIME 24

static uint16_t hts_target = 0;

static int16_t readout_x = 0;
static int16_t readout_y = 0;

static uint16_t readout_w = ACTIVE_SENSOR_WIDTH;
static uint16_t readout_h = ACTIVE_SENSOR_HEIGHT;

// HTS (Horizontal Time) is the readout width plus the HSYNC_TIME time. However, if this value gets
// too low the OV5640 will crash. The minimum was determined empirically with testing...
// Additionally, when the image width gets too large we need to slow down the line transfer rate by
// increasing HTS so that DCMI_DMAConvCpltUser() can keep up with the data rate.
//
// WARNING! IF YOU CHANGE ANYTHING HERE RETEST WITH **ALL** RESOLUTIONS FOR THE AFFECTED MODE!
static int calculate_hts(uint16_t width)
{
	uint16_t hts = hts_target;

	if (width > 640)
		hts = IM_MAX((width * 2) + 8, hts_target);

   	return IM_MAX(hts + HSYNC_TIME, (SENSOR_WIDTH + HSYNC_TIME) / 2); // Fix to prevent crashing.
}

// VTS (Vertical Time) is the readout height plus the VYSNC_TIME time. However, if this value gets
// too low the OV5640 will crash. The minimum was determined empirically with testing...
//
// WARNING! IF YOU CHANGE ANYTHING HERE RETEST WITH **ALL** RESOLUTIONS FOR THE AFFECTED MODE!
static int calculate_vts(uint16_t readout_height)
{
    	return IM_MAX(readout_height + VYSNC_TIME, (SENSOR_HEIGHT + VYSNC_TIME) / 8); // Fix to prevent crashing.
}

static int ov5640_set_framesize(const struct ov5640_config *cfg, uint32_t width, uint32_t height)
{
    uint8_t reg;
    int ret = 0;
    uint16_t w = width;
    uint16_t h = height;

    // Step 0: Clamp readout settings.

    readout_w = IM_MAX(readout_w, w);
    readout_h = IM_MAX(readout_h, h);

    int readout_x_max = (ACTIVE_SENSOR_WIDTH - readout_w) / 2;
    int readout_y_max = (ACTIVE_SENSOR_HEIGHT - readout_h) / 2;
    readout_x = IM_MAX(IM_MIN(readout_x, readout_x_max), -readout_x_max);
    readout_y = IM_MAX(IM_MIN(readout_y, readout_y_max), -readout_y_max);

    // Step 1: Determine readout area and subsampling amount.

    uint16_t sensor_div = 0;

    if ((w > (readout_w / 2)) || (h > (readout_h / 2)))
    {
        sensor_div = 1;
    }
    else
    {
        sensor_div = 2;
    }

    // Step 2: Determine horizontal and vertical start and end points.

    uint16_t sensor_w = readout_w + DUMMY_WIDTH_BUFFER;  // camera hardware needs dummy pixels to sync
    uint16_t sensor_h = readout_h + DUMMY_HEIGHT_BUFFER; // camera hardware needs dummy lines to sync

    uint16_t sensor_ws = IM_MAX(IM_MIN((((ACTIVE_SENSOR_WIDTH - sensor_w) / 4) + (readout_x / 2)) * 2, ACTIVE_SENSOR_WIDTH - sensor_w), -(DUMMY_WIDTH_BUFFER / 2)) + DUMMY_COLUMNS; // must be multiple of 2
    uint16_t sensor_we = sensor_ws + sensor_w - 1;

    uint16_t sensor_hs = IM_MAX(IM_MIN((((ACTIVE_SENSOR_HEIGHT - sensor_h) / 4) - (readout_y / 2)) * 2, ACTIVE_SENSOR_HEIGHT - sensor_h), -(DUMMY_HEIGHT_BUFFER / 2)) + DUMMY_LINES; // must be multiple of 2
    uint16_t sensor_he = sensor_hs + sensor_h - 1;

    // Step 3: Determine scaling window offset.

    float ratio = IM_MIN((readout_w / sensor_div) / ((float)w), (readout_h / sensor_div) / ((float)h));

    uint16_t w_mul = w * ratio;
    uint16_t h_mul = h * ratio;
    uint16_t x_off = ((sensor_w / sensor_div) - w_mul) / 2;
    uint16_t y_off = ((sensor_h / sensor_div) - h_mul) / 2;

    // Step 4: Compute total frame time.

    hts_target = sensor_w / sensor_div;

    uint16_t sensor_hts = calculate_hts(w);
    uint16_t sensor_vts = calculate_vts(sensor_h / sensor_div);

    uint16_t sensor_x_inc = (((sensor_div * 2) - 1) << 4) | (1 << 0); // odd[7:4]/even[3:0] pixel inc on the bayer pattern
    uint16_t sensor_y_inc = (((sensor_div * 2) - 1) << 4) | (1 << 0); // odd[7:4]/even[3:0] pixel inc on the bayer pattern

    // Step 5: Write regs.
    ret |= ov5640_write_reg(&cfg->i2c, TIMING_HS_H, sensor_ws >> 8);
    ret |= ov5640_write_reg(&cfg->i2c, TIMING_HS_L, sensor_ws);

    ret |= ov5640_write_reg(&cfg->i2c, TIMING_VS_H, sensor_hs >> 8);
    ret |= ov5640_write_reg(&cfg->i2c, TIMING_VS_L, sensor_hs);

    ret |= ov5640_write_reg(&cfg->i2c, TIMING_HW_H, sensor_we >> 8);
    ret |= ov5640_write_reg(&cfg->i2c, TIMING_HW_L, sensor_we);

    ret |= ov5640_write_reg(&cfg->i2c, TIMING_VH_H, sensor_he >> 8);
    ret |= ov5640_write_reg(&cfg->i2c, TIMING_VH_L, sensor_he);

    ret |= ov5640_write_reg(&cfg->i2c, TIMING_DVPHO_H, w >> 8);
    ret |= ov5640_write_reg(&cfg->i2c, TIMING_DVPHO_L, w);

    ret |= ov5640_write_reg(&cfg->i2c, TIMING_DVPVO_H, h >> 8);
    ret |= ov5640_write_reg(&cfg->i2c, TIMING_DVPVO_L, h);

    ret |= ov5640_write_reg(&cfg->i2c, TIMING_HTS_H, sensor_hts >> 8);
    ret |= ov5640_write_reg(&cfg->i2c, TIMING_HTS_L, sensor_hts);

    ret |= ov5640_write_reg(&cfg->i2c, TIMING_VTS_H, sensor_vts >> 8);
    ret |= ov5640_write_reg(&cfg->i2c, TIMING_VTS_L, sensor_vts);

    ret |= ov5640_write_reg(&cfg->i2c, TIMING_HOFFSET_H, x_off >> 8);
    ret |= ov5640_write_reg(&cfg->i2c, TIMING_HOFFSET_L, x_off);

    ret |= ov5640_write_reg(&cfg->i2c, TIMING_VOFFSET_H, y_off >> 8);
    ret |= ov5640_write_reg(&cfg->i2c, TIMING_VOFFSET_L, y_off);

    ret |= ov5640_write_reg(&cfg->i2c, TIMING_X_INC, sensor_x_inc);
    ret |= ov5640_write_reg(&cfg->i2c, TIMING_Y_INC, sensor_y_inc);

    ret |= ov5640_read_reg(&cfg->i2c, TIMING_TC_REG_20, &reg, sizeof(reg));
    ret |= ov5640_write_reg(&cfg->i2c, TIMING_TC_REG_20, (reg & 0xFE) | (sensor_div > 1));

    ret |= ov5640_read_reg(&cfg->i2c, TIMING_TC_REG_21, &reg, sizeof(reg));
    ret |= ov5640_write_reg(&cfg->i2c, TIMING_TC_REG_21, (reg & 0xFE) | (sensor_div > 1));

    ret |= ov5640_write_reg(&cfg->i2c, VFIFO_HSIZE_H, w >> 8);
    ret |= ov5640_write_reg(&cfg->i2c, VFIFO_HSIZE_L, w);

    ret |= ov5640_write_reg(&cfg->i2c, VFIFO_VSIZE_H, h >> 8);
    ret |= ov5640_write_reg(&cfg->i2c, VFIFO_VSIZE_L, h);

    return ret;
}

static int ov5640_set_hmirror(const struct ov5640_config *cfg, int enable)
{
    uint8_t reg;
    int ret = ov5640_read_reg(&cfg->i2c, TIMING_TC_REG_21, &reg, sizeof(reg));
    if (enable)
    {
        ret |= ov5640_write_reg(&cfg->i2c, TIMING_TC_REG_21, reg | 0x06);
    }
    else
    {
        ret |= ov5640_write_reg(&cfg->i2c, TIMING_TC_REG_21, reg & 0xF9);
    }
    return ret;
}

static int ov5640_set_vflip(const struct ov5640_config *cfg, int enable)
{
    uint8_t reg;
    int ret = ov5640_read_reg(&cfg->i2c, TIMING_TC_REG_20, &reg, sizeof(reg));
    if (!enable)
    {
        ret |= ov5640_write_reg(&cfg->i2c, TIMING_TC_REG_20, reg | 0x06);
    }
    else
    {
        ret |= ov5640_write_reg(&cfg->i2c, TIMING_TC_REG_20, reg & 0xF9);
    }
    return ret;
}

static int ov5640_set_fmt(const struct device *dev, enum video_endpoint_id ep,
			  struct video_format *fmt)
{
	struct ov5640_data *drv_data = dev->data;
	const struct ov5640_config *cfg = dev->config;
	int ret;
	int i;

	for (i = 0; i < ARRAY_SIZE(fmts); ++i) {
		if (fmt->pixelformat == fmts[i].pixelformat && fmt->width >= fmts[i].width_min &&
		    fmt->width <= fmts[i].width_max && fmt->height >= fmts[i].height_min &&
		    fmt->height <= fmts[i].height_max) {
			break;
		}
	}

	if (i == ARRAY_SIZE(fmts)) {
		LOG_ERR("Unsupported pixel format or resolution");
		return -ENOTSUP;
	}

	if (!memcmp(&drv_data->fmt, fmt, sizeof(drv_data->fmt))) {
		return 0;
	}

	drv_data->fmt = *fmt;

#ifdef STM32
	/* Set pixel format, default to VIDEO_PIX_FMT_RGB565 */
	struct ov5640_reg fmt_params[2] = {
		{0x4300, 0x6f},
		{0x501f, 0x01},
	};

	ret = ov5640_write_multi_regs(&cfg->i2c, fmt_params, ARRAY_SIZE(fmt_params));
	if (ret) {
		LOG_ERR("Unable to set pixel format");
		return ret;
	}

	/* Initialization sequence for QQVGA resolution (160x120) */
	struct ov5640_reg OV5640_QQVGA[4] =
	{
		{OV5640_TIMING_DVPHO_HIGH, 0x00},
		{OV5640_TIMING_DVPHO_LOW, 0xA0},
		{OV5640_TIMING_DVPVO_HIGH, 0x00},
		{OV5640_TIMING_DVPVO_LOW, 0x78},
	};

	/* Initialization sequence for QVGA resolution (320x240) */
	struct ov5640_reg OV5640_QVGA[4] =
	{
		{OV5640_TIMING_DVPHO_HIGH, 0x01},
		{OV5640_TIMING_DVPHO_LOW, 0x40},
		{OV5640_TIMING_DVPVO_HIGH, 0x00},
		{OV5640_TIMING_DVPVO_LOW, 0xF0},
	};

	/* Set resolution parameters */
	for (i = 0; i < ARRAY_SIZE(resolutionParams); i++) {
		if (fmt->width == resolutionParams[i].width &&
		    fmt->height == resolutionParams[i].height) {
			ret = ov5640_write_multi_regs(&cfg->i2c, OV5640_QVGA, ARRAY_SIZE(OV5640_QVGA));
			if (ret) {
				LOG_ERR("Unable to set resolution parameters");
				return ret;
			}
			break;
		}
	}

	// Set polarities
	uint8_t tmp = (uint8_t)(0x01 << 5U) | (0x01 << 1U) | 0x00;
	ret = ov5640_write_reg(&cfg->i2c, OV5640_POLARITY_CTRL, tmp);
	if (ret) {
		LOG_ERR("Unable to set polarities");
		return ret;
	}

	tmp = 0xC0; /* Bits[7:0]: PLL multiplier */
	ret = ov5640_write_reg(&cfg->i2c, OV5640_SC_PLL_CONTRL2, tmp);
	if (ret) {
		LOG_ERR("Unable to set PLL multiplier");
		return ret;
	}

	k_sleep(K_MSEC(100));
#else
	uint8_t reg;

	ret |= ov5640_write_reg(&cfg->i2c, FORMAT_CONTROL, 0x6f);
        ret |= ov5640_write_reg(&cfg->i2c, FORMAT_CONTROL_MUX, 0x01);

	ret |= ov5640_read_reg(&cfg->i2c, TIMING_TC_REG_21, &reg, sizeof(reg));
	ret |= ov5640_write_reg(&cfg->i2c, TIMING_TC_REG_21, (reg & 0xDF) | 0x00);

	ret |= ov5640_read_reg(&cfg->i2c, SYSTEM_RESET_02, &reg, sizeof(reg));
	ret |= ov5640_write_reg(&cfg->i2c, SYSTEM_RESET_02, (reg & 0xE3) | 0x1C);

	ret |= ov5640_read_reg(&cfg->i2c, CLOCK_ENABLE_02, &reg, sizeof(reg));
	ret |= ov5640_write_reg(&cfg->i2c, CLOCK_ENABLE_02, (reg & 0xD7) | 0x00);

	ov5640_set_framesize(cfg, fmt->width, fmt->height);
	ov5640_set_hmirror(cfg, 0);
    	ov5640_set_vflip(cfg, 0);
#endif
	// /* Configure MIPI pixel clock */
	// ret |= ov5640_modify_reg(&cfg->i2c, SC_PLL_CTRL0_REG, 0x1f, 0x18);
	// ret |= ov5640_modify_reg(&cfg->i2c, SC_PLL_CTRL1_REG, 0xff,
	// 			 resolutionParams[i].mipi_pclk.pllCtrl1);
	// ret |= ov5640_modify_reg(&cfg->i2c, SC_PLL_CTRL2_REG, 0xff,
	// 			 resolutionParams[i].mipi_pclk.pllCtrl2);
	// ret |= ov5640_modify_reg(&cfg->i2c, SC_PLL_CTRL3_REG, 0x1f, 0x13);
	// ret |= ov5640_modify_reg(&cfg->i2c, SYS_ROOT_DIV_REG, 0x3f, 0x01);
	// ret |= ov5640_write_reg(&cfg->i2c, PCLK_PERIOD_REG, 0x22);
	// if (ret) {
	// 	LOG_ERR("Unable to configure MIPI pixel clock");
	// 	return ret;
	// }

	return 0;
}

static int ov5640_get_fmt(const struct device *dev, enum video_endpoint_id ep,
			  struct video_format *fmt)
{
	struct ov5640_data *drv_data = dev->data;

	*fmt = drv_data->fmt;

	return 0;
}

static int ov5640_get_caps(const struct device *dev, enum video_endpoint_id ep,
			   struct video_caps *caps)
{
	caps->format_caps = fmts;
	return 0;
}

static int ov5640_stream_start(const struct device *dev)
{
	// const struct ov5640_config *cfg = dev->config;
	// /* Power up MIPI PHY HS Tx & LP Rx in 2 data lanes mode */
	// int ret = ov5640_write_reg(&cfg->i2c, IO_MIPI_CTRL00_REG, 0x45);

	// if (ret) {
	// 	LOG_ERR("Unable to power up MIPI PHY");
	// 	return ret;
	// }
	// return ov5640_write_reg(&cfg->i2c, SYS_CTRL0_REG, SYS_CTRL0_SW_PWUP);
	return 0;
}

static int ov5640_stream_stop(const struct device *dev)
{
	// const struct ov5640_config *cfg = dev->config;
	// /* Power down MIPI PHY HS Tx & LP Rx */
	// int ret = ov5640_write_reg(&cfg->i2c, IO_MIPI_CTRL00_REG, 0x40);

	// if (ret) {
	// 	LOG_ERR("Unable to power down MIPI PHY");
	// 	return ret;
	// }
	// return ov5640_write_reg(&cfg->i2c, SYS_CTRL0_REG, SYS_CTRL0_SW_PWDN);
	return 0;
}

static const struct video_driver_api ov5640_driver_api = {
	.set_format = ov5640_set_fmt,
	.get_format = ov5640_get_fmt,
	.get_caps = ov5640_get_caps,
	.stream_start = ov5640_stream_start,
	.stream_stop = ov5640_stream_stop,
};

static int ov5640_init(const struct device *dev)
{
	const struct ov5640_config *cfg = dev->config;
	struct video_format fmt;
	uint16_t chip_id;
	int ret;

	if (!device_is_ready(cfg->i2c.bus)) {
		LOG_ERR("Bus device is not ready");
		return -ENODEV;
	}

	if (!gpio_is_ready_dt(&cfg->reset_gpio)) {
		LOG_ERR("%s: device %s is not ready", dev->name, cfg->reset_gpio.port->name);
		return -ENODEV;
	}

	if (!gpio_is_ready_dt(&cfg->powerdown_gpio)) {
		LOG_ERR("%s: device %s is not ready", dev->name, cfg->powerdown_gpio.port->name);
		return -ENODEV;
	}

	/* Power up sequence */
	if (cfg->powerdown_gpio.port != NULL) {
		ret = gpio_pin_configure_dt(&cfg->powerdown_gpio, GPIO_OUTPUT_ACTIVE);
		if (ret) {
			return ret;
		}
	}

	if (cfg->reset_gpio.port != NULL) {
		ret = gpio_pin_configure_dt(&cfg->reset_gpio, GPIO_OUTPUT_ACTIVE);
		if (ret) {
			return ret;
		}
	}

	k_sleep(K_MSEC(5));

	if (cfg->powerdown_gpio.port != NULL) {
		gpio_pin_set_dt(&cfg->powerdown_gpio, 0);
	}

	k_sleep(K_MSEC(1));

	if (cfg->reset_gpio.port != NULL) {
		gpio_pin_set_dt(&cfg->reset_gpio, 0);
	}

	k_sleep(K_MSEC(20));

	/* Reset all registers */
	ret = ov5640_write_reg(&cfg->i2c, SCCB_SYSTEM_CTRL_1, 0x11);
	if (ret) {
		LOG_ERR("Unable to write to SCCB_SYSTEM_CTRL_1");
		return -EIO;
	}

	/* Software reset */
	ret = ov5640_write_reg(&cfg->i2c, SYS_CTRL0_REG, SYS_CTRL0_SW_RST);
	if (ret) {
		LOG_ERR("Unable to perform software reset");
		return -EIO;
	}

	k_sleep(K_MSEC(5));

	/* Initialize register values */
#ifdef STM32
	ret = ov5640_write_multi_regs(&cfg->i2c, ov5640InitParams_ST, ARRAY_SIZE(ov5640InitParams_ST));
#else
	ret = ov5640_write_multi_regs(&cfg->i2c, ov5640InitParams_Weact, ARRAY_SIZE(ov5640InitParams_Weact));
#endif
	if (ret) {
		LOG_ERR("Unable to initialize the sensor");
		return -EIO;
	}

	k_sleep(K_MSEC(300));

	// /* Set virtual channel */
	// ret = ov5640_modify_reg(&cfg->i2c, 0x4814, 3U << 6, (uint8_t)(DEFAULT_MIPI_CHANNEL) << 6);
	// if (ret) {
	// 	LOG_ERR("Unable to set virtual channel");
	// 	return -EIO;
	// }

	/* Check sensor chip id */
	ret = ov5640_read_reg(&cfg->i2c, CHIP_ID_REG, &chip_id, sizeof(chip_id));
	if (ret) {
		LOG_ERR("Unable to read sensor chip ID, ret = %d", ret);
		return -ENODEV;
	}

	if (chip_id != CHIP_ID_VAL) {
		LOG_ERR("Wrong chip ID: %04x (expected %04x)", chip_id, CHIP_ID_VAL);
		return -ENODEV;
	}

	/* Set default format to 720p RGB565 */
	fmt.pixelformat = VIDEO_PIX_FMT_RGB565;
#ifdef STM32
	fmt.width = 320;
	fmt.height = 240;
#else
	// fmt.width = 160;
	// fmt.height = 120;
	// fmt.width = 320;
	// fmt.height = 240;
	fmt.width = 480;
	fmt.height = 272;
#endif
	fmt.pitch = fmt.width * 2;
	ret = ov5640_set_fmt(dev, VIDEO_EP_OUT, &fmt);
	if (ret) {
		LOG_ERR("Unable to configure default format");
		return -EIO;
	}

	return 0;
}

#define OV5640_INIT(n)                                                                             \
	static struct ov5640_data ov5640_data_##n;                                                 \
                                                                                                   \
	static const struct ov5640_config ov5640_cfg_##n = {                                       \
		.i2c = I2C_DT_SPEC_INST_GET(n),                                                    \
		.reset_gpio = GPIO_DT_SPEC_INST_GET_OR(n, reset_gpios, {0}),                       \
		.powerdown_gpio = GPIO_DT_SPEC_INST_GET_OR(n, powerdown_gpios, {0}),               \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(n, &ov5640_init, NULL, &ov5640_data_##n, &ov5640_cfg_##n,            \
			      POST_KERNEL, CONFIG_VIDEO_INIT_PRIORITY, &ov5640_driver_api);

DT_INST_FOREACH_STATUS_OKAY(OV5640_INIT)
