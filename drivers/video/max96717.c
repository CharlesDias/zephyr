/*
 * Copyright (c) 2026 Charles Dias
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#define DT_DRV_COMPAT adi_max96717

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/video.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include "video_common.h"
#include "video_device.h"

LOG_MODULE_REGISTER(video_max96717, CONFIG_VIDEO_LOG_LEVEL);

#define MAX96717_REG8(addr)       ((addr) | VIDEO_REG_ADDR16_DATA8)
#define MAX96717_REG_DEVID        MAX96717_REG8(0x000D)
#define MAX96717_REG_CTRL3        MAX96717_REG8(0x0013)
#define MAX96717_CTRL3_LOCKED     BIT(3)
#define MAX96717_CTRL3_ERROR      BIT(2)
#define MAX96717_CTRL3_CMU_LOCKED BIT(1)

/*
 * The MAX96717 has a single video pipe at hardware index 2 (not 0).
 * All per-pipe registers must be addressed with this index.
 * See Linux driver: max96717_info.pipe_hw_ids = { 2 }.
 */
#define MAX96717_PIPE_HW_ID  2

/* Per-pipe video TX enable (REG2) */
#define MAX96717_REG_REG2                 MAX96717_REG8(0x0002)
#define MAX96717_REG2_VID_TX_EN_P(p)      BIT(4 + (p))

/* Per-pipe GMSL stream ID selector (TX3) */
#define MAX96717_REG_TX3(p)               MAX96717_REG8(0x0053 + (p) * 0x4)
#define MAX96717_TX3_TX_STR_SEL           GENMASK(1, 0)

/* Per-pipe video TX configuration */
#define MAX96717_REG_VIDEO_TX0(p)         MAX96717_REG8(0x0100 + (p) * 0x8)
#define MAX96717_VIDEO_TX0_AUTO_BPP       BIT(3)

#define MAX96717_REG_VIDEO_TX1(p)         MAX96717_REG8(0x0101 + (p) * 0x8)
#define MAX96717_VIDEO_TX1_BPP_MASK       GENMASK(5, 0)

#define MAX96717_REG_VIDEO_TX2(p)         MAX96717_REG8(0x0102 + (p) * 0x8)
#define MAX96717_VIDEO_TX2_DRIFT_DET_EN   BIT(1)

/* FRONTTOP — pipe start/clock selection */
#define MAX96717_REG_FRONTTOP_0           MAX96717_REG8(0x0308)
#define MAX96717_FRONTTOP_0_START_PORT(p) BIT((p) + 4)

/* FRONTTOP_9 — pipe-to-PHY routing. Each pipe can output to PHY 0 or PHY 1. */
#define MAX96717_REG_FRONTTOP_9           MAX96717_REG8(0x0311)
#define MAX96717_FRONTTOP_9_START_PORT(p, phy) BIT((p) + (phy) * 4)

/* FRONTTOP_12 — per-pipe CSI-2 data type (MEM_DT). Two slots per pipe. */
#define MAX96717_REG_FRONTTOP_12(p, x)    MAX96717_REG8(0x0314 + (p) * 0x2 + (x))
#define MAX96717_FRONTTOP_12_MEM_DT_SEL   GENMASK(5, 0)
#define MAX96717_FRONTTOP_12_MEM_DT_EN    BIT(6)

/* FRONTTOP_20 — per-pipe soft BPP override (used when AUTO_BPP is off) */
#define MAX96717_REG_FRONTTOP_20(p)       MAX96717_REG8(0x031C + (p))
#define MAX96717_FRONTTOP_20_SOFT_BPP     GENMASK(4, 0)
#define MAX96717_FRONTTOP_20_SOFT_BPP_EN  BIT(5)

/* MIPI CSI-2 RX PHY configuration */
#define MAX96717_REG_MIPI_RX0            MAX96717_REG8(0x0330)
#define MAX96717_MIPI_RX0_NONCONTCLK_EN  BIT(6)

#define MAX96717_REG_MIPI_RX1            MAX96717_REG8(0x0331)
#define MAX96717_MIPI_RX1_CTRL_NUM_LANES GENMASK(5, 4)

/* EXT11 — tunnel mode control */
#define MAX96717_REG_EXT11                MAX96717_REG8(0x0383)
#define MAX96717_EXT11_TUN_MODE           BIT(7)

#define MIPI_CSI2_DT_RGB888 0x24
#define MIPI_CSI2_DT_RAW8   0x2A
#define MIPI_CSI2_DT_RAW10  0x2B
#define MIPI_CSI2_DT_RAW12  0x2C

/* CMU2 mandatory programming (per datasheet) */
#define MAX96717_REG_CMU2                 MAX96717_REG8(0x0302)
#define MAX96717_CMU2_PFDDIV_RSHORT_MASK  GENMASK(6, 4)
#define MAX96717_CMU2_PFDDIV_RSHORT_1_1V  0x1 /* 0b001 = 1.1V */

/*
 * Per-pipe PATGEN block (base 0x1C8, stride 0x43 per pipe).
 * The MAX96717 single pipe is at hardware index MAX96717_PIPE_HW_ID (2).
 */
#define MAX96717_REG_VTX(p, off)          MAX96717_REG8(0x01C8 + (p) * 0x43 + (off))
#define MAX96717_REG_VTX0(p)              MAX96717_REG8(0x01C8 + (p) * 0x43)
#define MAX96717_VTX0_VTG_MODE_MASK       GENMASK(1, 0)
#define MAX96717_VTX0_VTG_FREE_RUNNING    0x3
#define MAX96717_VTX0_DE_INV              BIT(2)
#define MAX96717_VTX0_HS_INV              BIT(3)
#define MAX96717_VTX0_VS_INV              BIT(4)
#define MAX96717_VTX0_GEN_DE              BIT(5)
#define MAX96717_VTX0_GEN_HS              BIT(6)
#define MAX96717_VTX0_GEN_VS              BIT(7)

#define MAX96717_REG_VTX1(p)              MAX96717_REG8(0x01C9 + (p) * 0x43)
#define MAX96717_VTX1_PATGEN_CLK_SRC_MASK GENMASK(3, 1)
#define MAX96717_VTX1_PATGEN_CLK_25MHZ    0x4 /* 0b100 */

/* VTX2..VTX27 timing field addresses (pipe 0) */
#define MAX96717_REG_VTX2_VS_DLY(p)       MAX96717_REG8(0x01CA + (p) * 0x43)
#define MAX96717_REG_VTX5_VS_HIGH(p)      MAX96717_REG8(0x01CD + (p) * 0x43)
#define MAX96717_REG_VTX8_VS_LOW(p)       MAX96717_REG8(0x01D0 + (p) * 0x43)
#define MAX96717_REG_VTX11_V2H(p)         MAX96717_REG8(0x01D3 + (p) * 0x43)
#define MAX96717_REG_VTX14_HS_HIGH(p)     MAX96717_REG8(0x01D6 + (p) * 0x43)
#define MAX96717_REG_VTX16_HS_LOW(p)      MAX96717_REG8(0x01D8 + (p) * 0x43)
#define MAX96717_REG_VTX18_HS_CNT(p)      MAX96717_REG8(0x01DA + (p) * 0x43)
#define MAX96717_REG_VTX20_V2D(p)         MAX96717_REG8(0x01DC + (p) * 0x43)
#define MAX96717_REG_VTX23_DE_HIGH(p)     MAX96717_REG8(0x01DF + (p) * 0x43)
#define MAX96717_REG_VTX25_DE_LOW(p)      MAX96717_REG8(0x01E1 + (p) * 0x43)
#define MAX96717_REG_VTX27_DE_CNT(p)      MAX96717_REG8(0x01E3 + (p) * 0x43)

/* VTX29 — PATGEN mode select */
#define MAX96717_REG_VTX29(p)             MAX96717_REG8(0x01E5 + (p) * 0x43)
#define MAX96717_VTX29_PATGEN_MODE_MASK   GENMASK(1, 0)
#define MAX96717_VTX29_PATGEN_DISABLED    0x0
#define MAX96717_VTX29_PATGEN_CHECKER     0x1
#define MAX96717_VTX29_PATGEN_GRADIENT    0x2

/* VTX30..VTX39 — gradient increment and checkerboard colour/repeat */
#define MAX96717_REG_VTX30_GRAD_INCR(p)   MAX96717_REG8(0x01E6 + (p) * 0x43)
#define MAX96717_REG_VTX31_CHKR_A_L(p)   MAX96717_REG8(0x01E7 + (p) * 0x43)
#define MAX96717_REG_VTX32_CHKR_A_M(p)   MAX96717_REG8(0x01E8 + (p) * 0x43)
#define MAX96717_REG_VTX33_CHKR_A_H(p)   MAX96717_REG8(0x01E9 + (p) * 0x43)
#define MAX96717_REG_VTX34_CHKR_B_L(p)   MAX96717_REG8(0x01EA + (p) * 0x43)
#define MAX96717_REG_VTX35_CHKR_B_M(p)   MAX96717_REG8(0x01EB + (p) * 0x43)
#define MAX96717_REG_VTX36_CHKR_B_H(p)   MAX96717_REG8(0x01EC + (p) * 0x43)
#define MAX96717_REG_VTX37_CHKR_RPT_A(p) MAX96717_REG8(0x01ED + (p) * 0x43)
#define MAX96717_REG_VTX38_CHKR_RPT_B(p) MAX96717_REG8(0x01EE + (p) * 0x43)
#define MAX96717_REG_VTX39_CHKR_ALT(p)   MAX96717_REG8(0x01EF + (p) * 0x43)

/*
 * 800x480 @ ~50 Hz, 25 MHz pixel clock — same constants as the MAX96724
 * VPG (see max96724.c MAX96724_TPG_*). Both chips share VTG register
 * semantics, so the computed byte values apply directly.
 *
 * TODO: refactor into a shared header if a third Maxim chip needs these.
 */
#define MAX96717_TPG_HACTIVE 800U
#define MAX96717_TPG_HFP     10U
#define MAX96717_TPG_HSYNC   96U
#define MAX96717_TPG_HBP     40U
#define MAX96717_TPG_HTOTAL  (MAX96717_TPG_HACTIVE + MAX96717_TPG_HFP + \
			      MAX96717_TPG_HSYNC + MAX96717_TPG_HBP)

#define MAX96717_TPG_VACTIVE 480U
#define MAX96717_TPG_VFP     2U
#define MAX96717_TPG_VSYNC   24U
#define MAX96717_TPG_VBP     24U
#define MAX96717_TPG_VTOTAL  (MAX96717_TPG_VACTIVE + MAX96717_TPG_VFP + \
			      MAX96717_TPG_VSYNC + MAX96717_TPG_VBP)

#define MAX96717_TPG_VS_HIGH (MAX96717_TPG_VSYNC * MAX96717_TPG_HTOTAL)
#define MAX96717_TPG_VS_LOW  ((MAX96717_TPG_VACTIVE + MAX96717_TPG_VFP + \
			       MAX96717_TPG_VBP) * MAX96717_TPG_HTOTAL)
#define MAX96717_TPG_HS_HIGH MAX96717_TPG_HSYNC
#define MAX96717_TPG_HS_LOW  (MAX96717_TPG_HACTIVE + MAX96717_TPG_HFP + \
			      MAX96717_TPG_HBP)
#define MAX96717_TPG_HS_CNT  (MAX96717_TPG_VACTIVE + MAX96717_TPG_VFP + \
			      MAX96717_TPG_VBP + MAX96717_TPG_VSYNC)
#define MAX96717_TPG_V2D     (MAX96717_TPG_HTOTAL * (MAX96717_TPG_VSYNC + \
							MAX96717_TPG_VBP) + \
			      (MAX96717_TPG_HSYNC + MAX96717_TPG_HBP))
#define MAX96717_TPG_DE_HIGH MAX96717_TPG_HACTIVE
#define MAX96717_TPG_DE_LOW  (MAX96717_TPG_HFP + MAX96717_TPG_HSYNC + \
			      MAX96717_TPG_HBP)
#define MAX96717_TPG_DE_CNT  MAX96717_TPG_VACTIVE

/* Checkerboard tile geometry — mirrors max96724.c values */
#define MAX96717_TPG_CHKR_RPT_A 80U
#define MAX96717_TPG_CHKR_RPT_B 80U
#define MAX96717_TPG_CHKR_ALT   48U
#define MAX96717_TPG_CHKR_A_R   0x00U
#define MAX96717_TPG_CHKR_A_G   0x00U
#define MAX96717_TPG_CHKR_A_B   0xFFU
#define MAX96717_TPG_CHKR_B_R   0xFFU
#define MAX96717_TPG_CHKR_B_G   0x00U
#define MAX96717_TPG_CHKR_B_B   0x00U

#define MAX96717_LINK_LOCK_RETRIES  50
#define MAX96717_LINK_LOCK_DELAY_MS 20

static const char *max96717_variant_model(uint8_t id)
{
	switch (id) {
	case 0xBF:
		return "MAX96717";
	default:
		return "unknown";
	}
}

struct max96717_config {
	struct i2c_dt_spec i2c;
	const struct device *source_dev;
	uint8_t csi_rx_num_lanes;
};

struct max96717_data {
	struct video_format fmt;
};

static int max96717_check_link_lock(const struct i2c_dt_spec *i2c)
{
	uint32_t val;
	int ret;

	for (int i = 0; i < MAX96717_LINK_LOCK_RETRIES; i++) {
		ret = video_read_cci_reg(i2c, MAX96717_REG_CTRL3, &val);
		if (ret < 0) {
			LOG_ERR("Failed to read CTRL3 (err %d)", ret);
			return ret;
		}

		if (val & MAX96717_CTRL3_LOCKED) {
			LOG_INF("GMSL2 link locked (CTRL3=0x%02X)", val);
			break;
		}

		LOG_DBG("Waiting for GMSL2 link lock (%d/%d, CTRL3=0x%02X)", i + 1,
			MAX96717_LINK_LOCK_RETRIES, val);
		k_msleep(MAX96717_LINK_LOCK_DELAY_MS);
	}

	if (!(val & MAX96717_CTRL3_LOCKED)) {
		LOG_WRN("GMSL2 link not locked (CTRL3=0x%02X)", val);
	}

	if (!(val & MAX96717_CTRL3_CMU_LOCKED)) {
		LOG_WRN("CMU not locked");
	}

	if (val & MAX96717_CTRL3_ERROR) {
		LOG_WRN("ERRB asserted — check GMSL link integrity");
	}

	return 0;
}

#if IS_ENABLED(CONFIG_VIDEO_MAX96717_PATGEN_ENABLE)

static int max96717_write_pattern_timings(const struct i2c_dt_spec *i2c)
{
	/*
	 * 800x480 @ ~50 Hz, 25 MHz PCLK — same VTG numbers as max96724.c
	 * so the LTDC panel is filled exactly. See max96724.c MAX96724_TPG_*.
	 *
	 * All VTX registers are addressed via MAX96717_PIPE_HW_ID (= 2),
	 * the only hardware pipe available on the MAX96717.
	 */
	const int p = MAX96717_PIPE_HW_ID;
	const struct video_reg regs[] = {
		/* 24-bit VS_DLY (zero) */
		{MAX96717_REG_VTX(p, 2),                    0},
		{MAX96717_REG_VTX(p, 3),                    0},
		{MAX96717_REG_VTX(p, 4),                    0},

		/* 24-bit VS_HIGH */
		{MAX96717_REG_VTX(p, 5),  (MAX96717_TPG_VS_HIGH >> 16) & 0xFF},
		{MAX96717_REG_VTX(p, 6),  (MAX96717_TPG_VS_HIGH >> 8)  & 0xFF},
		{MAX96717_REG_VTX(p, 7),  (MAX96717_TPG_VS_HIGH >> 0)  & 0xFF},

		/* 24-bit VS_LOW */
		{MAX96717_REG_VTX(p, 8),  (MAX96717_TPG_VS_LOW >> 16) & 0xFF},
		{MAX96717_REG_VTX(p, 9),  (MAX96717_TPG_VS_LOW >> 8)  & 0xFF},
		{MAX96717_REG_VTX(p, 10), (MAX96717_TPG_VS_LOW >> 0)  & 0xFF},

		/* 24-bit V2H (zero) */
		{MAX96717_REG_VTX(p, 11),                   0},
		{MAX96717_REG_VTX(p, 12),                   0},
		{MAX96717_REG_VTX(p, 13),                   0},

		/* 16-bit HS_HIGH */
		{MAX96717_REG_VTX(p, 14), (MAX96717_TPG_HS_HIGH >> 8) & 0xFF},
		{MAX96717_REG_VTX(p, 15), (MAX96717_TPG_HS_HIGH >> 0) & 0xFF},

		/* 16-bit HS_LOW */
		{MAX96717_REG_VTX(p, 16), (MAX96717_TPG_HS_LOW >> 8) & 0xFF},
		{MAX96717_REG_VTX(p, 17), (MAX96717_TPG_HS_LOW >> 0) & 0xFF},

		/* 16-bit HS_CNT */
		{MAX96717_REG_VTX(p, 18), (MAX96717_TPG_HS_CNT >> 8) & 0xFF},
		{MAX96717_REG_VTX(p, 19), (MAX96717_TPG_HS_CNT >> 0) & 0xFF},

		/* 24-bit V2D */
		{MAX96717_REG_VTX(p, 20), (MAX96717_TPG_V2D >> 16) & 0xFF},
		{MAX96717_REG_VTX(p, 21), (MAX96717_TPG_V2D >> 8)  & 0xFF},
		{MAX96717_REG_VTX(p, 22), (MAX96717_TPG_V2D >> 0)  & 0xFF},

		/* 16-bit DE_HIGH */
		{MAX96717_REG_VTX(p, 23), (MAX96717_TPG_DE_HIGH >> 8) & 0xFF},
		{MAX96717_REG_VTX(p, 24), (MAX96717_TPG_DE_HIGH >> 0) & 0xFF},

		/* 16-bit DE_LOW */
		{MAX96717_REG_VTX(p, 25), (MAX96717_TPG_DE_LOW >> 8) & 0xFF},
		{MAX96717_REG_VTX(p, 26), (MAX96717_TPG_DE_LOW >> 0) & 0xFF},

		/* 16-bit DE_CNT */
		{MAX96717_REG_VTX(p, 27), (MAX96717_TPG_DE_CNT >> 8) & 0xFF},
		{MAX96717_REG_VTX(p, 28), (MAX96717_TPG_DE_CNT >> 0) & 0xFF},

		/* VTX0 — VTG mode + sync generation enables */
		{MAX96717_REG_VTX0(p),
		 FIELD_PREP(MAX96717_VTX0_VTG_MODE_MASK,
			    MAX96717_VTX0_VTG_FREE_RUNNING) |
		 MAX96717_VTX0_VS_INV |
		 MAX96717_VTX0_GEN_DE |
		 MAX96717_VTX0_GEN_HS |
		 MAX96717_VTX0_GEN_VS},
	};

	return video_write_cci_multiregs(i2c, regs, ARRAY_SIZE(regs));
}

static int max96717_write_gradient_pattern(const struct i2c_dt_spec *i2c)
{
	return video_write_cci_reg(i2c, MAX96717_REG_VTX30_GRAD_INCR(MAX96717_PIPE_HW_ID), 16);
}

static int max96717_write_checkerboard_pattern(const struct i2c_dt_spec *i2c)
{
	const int p = MAX96717_PIPE_HW_ID;
	const struct video_reg regs[] = {
		{MAX96717_REG_VTX31_CHKR_A_L(p), MAX96717_TPG_CHKR_A_R},
		{MAX96717_REG_VTX32_CHKR_A_M(p), MAX96717_TPG_CHKR_A_G},
		{MAX96717_REG_VTX33_CHKR_A_H(p), MAX96717_TPG_CHKR_A_B},
		{MAX96717_REG_VTX34_CHKR_B_L(p), MAX96717_TPG_CHKR_B_R},
		{MAX96717_REG_VTX35_CHKR_B_M(p), MAX96717_TPG_CHKR_B_G},
		{MAX96717_REG_VTX36_CHKR_B_H(p), MAX96717_TPG_CHKR_B_B},
		{MAX96717_REG_VTX37_CHKR_RPT_A(p), MAX96717_TPG_CHKR_RPT_A},
		{MAX96717_REG_VTX38_CHKR_RPT_B(p), MAX96717_TPG_CHKR_RPT_B},
		{MAX96717_REG_VTX39_CHKR_ALT(p),   MAX96717_TPG_CHKR_ALT},
	};

	return video_write_cci_multiregs(i2c, regs, ARRAY_SIZE(regs));
}

static int max96717_patgen_configure(const struct i2c_dt_spec *i2c)
{
	int ret;

	/* PCLK = 25 MHz */
	ret = video_modify_cci_reg(i2c, MAX96717_REG_VTX1(MAX96717_PIPE_HW_ID),
				   MAX96717_VTX1_PATGEN_CLK_SRC_MASK,
				   FIELD_PREP(MAX96717_VTX1_PATGEN_CLK_SRC_MASK,
					      MAX96717_VTX1_PATGEN_CLK_25MHZ));
	if (ret < 0) {
		return ret;
	}

	ret = max96717_write_pattern_timings(i2c);
	if (ret < 0) {
		return ret;
	}

#if IS_ENABLED(CONFIG_VIDEO_MAX96717_PATGEN_PATTERN_CHECKERBOARD)
	ret = max96717_write_checkerboard_pattern(i2c);
	if (ret < 0) {
		return ret;
	}

	return video_modify_cci_reg(i2c, MAX96717_REG_VTX29(MAX96717_PIPE_HW_ID),
				    MAX96717_VTX29_PATGEN_MODE_MASK,
				    FIELD_PREP(MAX96717_VTX29_PATGEN_MODE_MASK,
					       MAX96717_VTX29_PATGEN_CHECKER));
#else
	ret = max96717_write_gradient_pattern(i2c);
	if (ret < 0) {
		return ret;
	}

	return video_modify_cci_reg(i2c, MAX96717_REG_VTX29(MAX96717_PIPE_HW_ID),
				    MAX96717_VTX29_PATGEN_MODE_MASK,
				    FIELD_PREP(MAX96717_VTX29_PATGEN_MODE_MASK,
					       MAX96717_VTX29_PATGEN_GRADIENT));
#endif
}

#endif /* CONFIG_VIDEO_MAX96717_PATGEN_ENABLE */

static int max96717_init(const struct device *dev)
{
	const struct max96717_config *cfg = dev->config;
	uint32_t chip_id;
	int ret;

	if (!i2c_is_ready_dt(&cfg->i2c)) {
		LOG_ERR("I2C bus not ready");
		return -ENODEV;
	}

	ret = video_read_cci_reg(&cfg->i2c, MAX96717_REG_DEVID, &chip_id);
	if (ret < 0) {
		LOG_ERR("Failed to read device ID (err %d)", ret);
		return ret;
	}

	LOG_INF("Found %s (ID=0x%02X) on %s@0x%02x", max96717_variant_model(chip_id), chip_id,
		cfg->i2c.bus->name, cfg->i2c.addr);

	ret = max96717_check_link_lock(&cfg->i2c);
	if (ret < 0) {
		return ret;
	}

#if IS_ENABLED(CONFIG_VIDEO_MAX96717_PATGEN_ENABLE)
	/* Mandatory CMU2 PFDDIV programming (datasheet) */
	ret = video_modify_cci_reg(&cfg->i2c, MAX96717_REG_CMU2,
				   MAX96717_CMU2_PFDDIV_RSHORT_MASK,
				   FIELD_PREP(MAX96717_CMU2_PFDDIV_RSHORT_MASK,
					      MAX96717_CMU2_PFDDIV_RSHORT_1_1V));
	if (ret < 0) {
		return ret;
	}

	/*
	 * Disable tunnel mode — PATGEN data must be processed by the
	 * FRONTTOP, not passed through as raw CSI-2 packets.
	 */
	ret = video_modify_cci_reg(&cfg->i2c, MAX96717_REG_EXT11,
				   MAX96717_EXT11_TUN_MODE, 0);
	if (ret < 0) {
		return ret;
	}

	ret = max96717_patgen_configure(&cfg->i2c);
	if (ret < 0) {
		LOG_ERR("PATGEN configure failed (err %d)", ret);
		return ret;
	}

	/*
	 * Configure HW pipe 2 for RGB888 (24 BPP). PATGEN bypasses the
	 * external parallel/CSI input, so AUTO_BPP detection (which samples
	 * the external PCLK) won't work — set the BPP explicitly.
	 */
	ret = video_modify_cci_reg(&cfg->i2c,
				   MAX96717_REG_VIDEO_TX0(MAX96717_PIPE_HW_ID),
				   MAX96717_VIDEO_TX0_AUTO_BPP, 0);
	if (ret < 0) {
		return ret;
	}

	ret = video_modify_cci_reg(&cfg->i2c,
				   MAX96717_REG_VIDEO_TX1(MAX96717_PIPE_HW_ID),
				   MAX96717_VIDEO_TX1_BPP_MASK,
				   FIELD_PREP(MAX96717_VIDEO_TX1_BPP_MASK, 24));
	if (ret < 0) {
		return ret;
	}

	/*
	 * Disable DRIFT_DET_EN — PATGEN has no external PCLK so
	 * drift detection is meaningless and could flag false errors.
	 */
	ret = video_modify_cci_reg(&cfg->i2c,
				   MAX96717_REG_VIDEO_TX2(MAX96717_PIPE_HW_ID),
				   MAX96717_VIDEO_TX2_DRIFT_DET_EN, 0);
	if (ret < 0) {
		return ret;
	}

	/*
	 * Set SOFT_BPP = 24 for RGB888. With AUTO_BPP disabled above,
	 * the FRONTTOP has no way to auto-detect the bits-per-pixel.
	 * SOFT_BPP tells it how to packetize the PATGEN data into GMSL
	 * frames — without this the packets are malformed and the
	 * deserializer cannot achieve VID_LOCK.
	 */
	ret = video_modify_cci_reg(&cfg->i2c,
				   MAX96717_REG_FRONTTOP_20(MAX96717_PIPE_HW_ID),
				   MAX96717_FRONTTOP_20_SOFT_BPP |
				   MAX96717_FRONTTOP_20_SOFT_BPP_EN,
				   FIELD_PREP(MAX96717_FRONTTOP_20_SOFT_BPP, 24) |
				   MAX96717_FRONTTOP_20_SOFT_BPP_EN);
	if (ret < 0) {
		return ret;
	}

	/* Start the video pipe output path through FRONTTOP */
	ret = video_modify_cci_reg(&cfg->i2c, MAX96717_REG_FRONTTOP_0,
				   MAX96717_FRONTTOP_0_START_PORT(MAX96717_PIPE_HW_ID),
				   MAX96717_FRONTTOP_0_START_PORT(MAX96717_PIPE_HW_ID));
	if (ret < 0) {
		return ret;
	}

	/* Route pipe → PHY 1 (GMSL link) via FRONTTOP_9 */
	ret = video_modify_cci_reg(&cfg->i2c, MAX96717_REG_FRONTTOP_9,
				   MAX96717_FRONTTOP_9_START_PORT(MAX96717_PIPE_HW_ID, 1),
				   MAX96717_FRONTTOP_9_START_PORT(MAX96717_PIPE_HW_ID, 1));
	if (ret < 0) {
		return ret;
	}

	/*
	 * Set CSI-2 data type to RGB888 (0x24) and enable the MEM_DT slot.
	 * Without this the serializer doesn't embed a valid data type in the
	 * GMSL packets, so the deserializer can't map the incoming data to
	 * a CSI-2 output stream.
	 */
	ret = video_write_cci_reg(&cfg->i2c,
				  MAX96717_REG_FRONTTOP_12(MAX96717_PIPE_HW_ID, 0),
				  FIELD_PREP(MAX96717_FRONTTOP_12_MEM_DT_SEL,
					     MIPI_CSI2_DT_RGB888) |
				  MAX96717_FRONTTOP_12_MEM_DT_EN);
	if (ret < 0) {
		return ret;
	}

	/*
	 * Set GMSL stream ID = 0 for HW pipe 2. The deserializer's
	 * VIDEO_PIPE_SEL maps pipe 0 to stream 0 on link A.
	 */
	ret = video_modify_cci_reg(&cfg->i2c,
				   MAX96717_REG_TX3(MAX96717_PIPE_HW_ID),
				   MAX96717_TX3_TX_STR_SEL,
				   FIELD_PREP(MAX96717_TX3_TX_STR_SEL, 0));
	if (ret < 0) {
		return ret;
	}

	/* Enable video TX on HW pipe 2 so PATGEN data leaves over GMSL */
	ret = video_modify_cci_reg(&cfg->i2c, MAX96717_REG_REG2,
				   MAX96717_REG2_VID_TX_EN_P(MAX96717_PIPE_HW_ID),
				   MAX96717_REG2_VID_TX_EN_P(MAX96717_PIPE_HW_ID));
	if (ret < 0) {
		return ret;
	}

	LOG_INF("MAX96717 PATGEN streaming on pipe %d (800x480 RGB888)",
		MAX96717_PIPE_HW_ID);
#endif /* CONFIG_VIDEO_MAX96717_PATGEN_ENABLE */

	return 0;
}

static int max96717_get_caps(const struct device *dev, struct video_caps *caps)
{
	const struct max96717_config *cfg = dev->config;

	/* Forward to upstream sensor if present */
	if (cfg->source_dev != NULL) {
		return video_get_caps(cfg->source_dev, caps);
	}

	return -ENOTSUP;
}

static int max96717_get_format(const struct device *dev, struct video_format *fmt)
{
	const struct max96717_config *cfg = dev->config;

	/* Forward to upstream sensor if present */
	if (cfg->source_dev != NULL) {
		return video_get_format(cfg->source_dev, fmt);
	}

	return -ENOTSUP;
}

static int max96717_set_format(const struct device *dev, struct video_format *fmt)
{
	const struct max96717_config *cfg = dev->config;
	struct max96717_data *data = dev->data;

	/* Forward to upstream sensor if present */
	if (cfg->source_dev != NULL) {
		int ret;

		ret = video_set_format(cfg->source_dev, fmt);
		if (ret < 0) {
			return ret;
		}
	}

	data->fmt = *fmt;
	return 0;
}

static uint8_t max96717_pixfmt_to_csi2_dt(uint32_t pixfmt)
{
	switch (pixfmt) {
	case VIDEO_PIX_FMT_RGB24:
	case VIDEO_PIX_FMT_BGR24:
		return MIPI_CSI2_DT_RGB888;
	case VIDEO_PIX_FMT_SBGGR8:
		return MIPI_CSI2_DT_RAW8;
	case VIDEO_PIX_FMT_SBGGR10P:
		return MIPI_CSI2_DT_RAW10;
	default:
		return 0;
	}
}

/*
 * Configure the MIPI CSI-2 receiver PHY on the serializer.
 * Sets the lane count from DT and enables non-continuous clock mode
 * (required by sensors like the IMX219).
 */
static int max96717_configure_csi_rx_phy(const struct i2c_dt_spec *i2c,
					 uint8_t num_lanes)
{
	int ret;

	/* Set CSI-2 data lane count (register encodes num_lanes - 1) */
	ret = video_modify_cci_reg(i2c, MAX96717_REG_MIPI_RX1,
				   MAX96717_MIPI_RX1_CTRL_NUM_LANES,
				   FIELD_PREP(MAX96717_MIPI_RX1_CTRL_NUM_LANES,
					      num_lanes - 1));
	if (ret < 0) {
		return ret;
	}

	/* Enable non-continuous clock mode (required by most camera sensors) */
	ret = video_modify_cci_reg(i2c, MAX96717_REG_MIPI_RX0,
				   MAX96717_MIPI_RX0_NONCONTCLK_EN,
				   MAX96717_MIPI_RX0_NONCONTCLK_EN);
	if (ret < 0) {
		return ret;
	}

	LOG_DBG("MIPI RX PHY: %u data lanes, non-continuous clock", num_lanes);
	return 0;
}

/*
 * Configure the serializer to forward CSI-2 input from an external sensor
 * over the GMSL link. Uses pipeline mode (not tunnel mode) so the
 * deserializer receives proper video-pipe data and can assert VID_LOCK.
 *
 * This mirrors the PATGEN setup in max96717_init() but adapted for
 * external CSI-2 input: AUTO_BPP detects the bits-per-pixel from the
 * CSI-2 clock, and the data type is set from the sensor's pixel format.
 */
static int max96717_configure_csi_passthrough(const struct i2c_dt_spec *i2c,
					      uint8_t csi2_dt, uint8_t num_lanes)
{
	const int p = MAX96717_PIPE_HW_ID;
	int ret;

	/* Configure CSI-2 RX PHY for external sensor input */
	ret = max96717_configure_csi_rx_phy(i2c, num_lanes);
	if (ret < 0) {
		return ret;
	}

	/* Disable tunnel mode — route through FRONTTOP pipeline */
	ret = video_modify_cci_reg(i2c, MAX96717_REG_EXT11,
				   MAX96717_EXT11_TUN_MODE, 0);
	if (ret < 0) {
		return ret;
	}

	/* Enable AUTO_BPP — detect from CSI-2 input */
	ret = video_modify_cci_reg(i2c, MAX96717_REG_VIDEO_TX0(p),
				   MAX96717_VIDEO_TX0_AUTO_BPP,
				   MAX96717_VIDEO_TX0_AUTO_BPP);
	if (ret < 0) {
		return ret;
	}

	/* Disable SOFT_BPP override — AUTO_BPP handles it */
	ret = video_modify_cci_reg(i2c, MAX96717_REG_FRONTTOP_20(p),
				   MAX96717_FRONTTOP_20_SOFT_BPP_EN, 0);
	if (ret < 0) {
		return ret;
	}

	/* Set CSI-2 data type so the GMSL packets have a valid DT header */
	ret = video_write_cci_reg(i2c,
				  MAX96717_REG_FRONTTOP_12(p, 0),
				  FIELD_PREP(MAX96717_FRONTTOP_12_MEM_DT_SEL,
					     csi2_dt) |
				  MAX96717_FRONTTOP_12_MEM_DT_EN);
	if (ret < 0) {
		return ret;
	}

	/* Start pipe output through FRONTTOP */
	ret = video_modify_cci_reg(i2c, MAX96717_REG_FRONTTOP_0,
				   MAX96717_FRONTTOP_0_START_PORT(p),
				   MAX96717_FRONTTOP_0_START_PORT(p));
	if (ret < 0) {
		return ret;
	}

	/* Route pipe → PHY 1 (GMSL link) */
	ret = video_modify_cci_reg(i2c, MAX96717_REG_FRONTTOP_9,
				   MAX96717_FRONTTOP_9_START_PORT(p, 1),
				   MAX96717_FRONTTOP_9_START_PORT(p, 1));
	if (ret < 0) {
		return ret;
	}

	/* GMSL stream ID = 0 — deserializer pipe 0 expects stream 0, link A */
	ret = video_modify_cci_reg(i2c, MAX96717_REG_TX3(p),
				   MAX96717_TX3_TX_STR_SEL,
				   FIELD_PREP(MAX96717_TX3_TX_STR_SEL, 0));
	if (ret < 0) {
		return ret;
	}

	/* Enable video TX on the pipe */
	ret = video_modify_cci_reg(i2c, MAX96717_REG_REG2,
				   MAX96717_REG2_VID_TX_EN_P(p),
				   MAX96717_REG2_VID_TX_EN_P(p));
	if (ret < 0) {
		return ret;
	}

	LOG_INF("MAX96717 CSI-2 passthrough on pipe %d (DT=0x%02X)", p, csi2_dt);
	return 0;
}

static int max96717_set_stream(const struct device *dev, bool enable, enum video_buf_type type)
{
	const struct max96717_config *cfg = dev->config;
	struct max96717_data *data = dev->data;
	int ret;

	if (cfg->source_dev == NULL) {
		return 0;
	}

	if (enable) {
		uint8_t dt = max96717_pixfmt_to_csi2_dt(data->fmt.pixelformat);

		if (dt == 0) {
			LOG_ERR("Unsupported format %s for CSI-2 passthrough",
				VIDEO_FOURCC_TO_STR(data->fmt.pixelformat));
			return -ENOTSUP;
		}

		ret = max96717_configure_csi_passthrough(&cfg->i2c, dt,
						cfg->csi_rx_num_lanes);
		if (ret < 0) {
			LOG_ERR("CSI passthrough configure failed (err %d)", ret);
			return ret;
		}

		return video_stream_start(cfg->source_dev, type);
	}

	/* Disable video TX */
	(void)video_modify_cci_reg(&cfg->i2c, MAX96717_REG_REG2,
				   MAX96717_REG2_VID_TX_EN_P(MAX96717_PIPE_HW_ID), 0);
	return video_stream_stop(cfg->source_dev, type);
}

static DEVICE_API(video, max96717_api) = {
	.get_caps = max96717_get_caps,
	.get_format = max96717_get_format,
	.set_format = max96717_set_format,
	.set_stream = max96717_set_stream,
};

/* Resolve upstream source device from CSI input port@0.                       \
 * When no sensor is wired (e.g. PATGEN mode), port@0 does not exist           \
 * and src_dev is NULL — the MAX96717 itself is the frame source.              \
 */
#define SOURCE_DEV(inst)                                                                  \
	COND_CODE_1(                                                                               \
		DT_NODE_EXISTS(DT_INST_ENDPOINT_BY_ID(inst, 0, 0)),                                \
		(DEVICE_DT_GET(DT_NODE_REMOTE_DEVICE(DT_INST_ENDPOINT_BY_ID(inst, 0, 0)))),        \
		(NULL))

/* Number of CSI-2 data lanes from port@0 endpoint data-lanes property.        \
 * When port@0 has no endpoint (PATGEN mode), defaults to 0 (unused).          \
 */
#define CSI_RX_NUM_LANES(inst)                                                             \
	COND_CODE_1(                                                                               \
		DT_NODE_EXISTS(DT_INST_ENDPOINT_BY_ID(inst, 0, 0)),                                \
		(DT_PROP_LEN(DT_INST_ENDPOINT_BY_ID(inst, 0, 0), data_lanes)),                    \
		(0))

#define MAX96717_INIT(inst)                                                                        \
	static const struct max96717_config max96717_config_##inst = {                             \
		.i2c = I2C_DT_SPEC_INST_GET(inst),                                                 \
		.source_dev = SOURCE_DEV(inst),                                                    \
		.csi_rx_num_lanes = CSI_RX_NUM_LANES(inst),                                        \
	};                                                                                         \
	static struct max96717_data max96717_data_##inst;                                          \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(inst, max96717_init, NULL, &max96717_data_##inst,                    \
			      &max96717_config_##inst, POST_KERNEL,                                \
			      CONFIG_VIDEO_MAX96717_INIT_PRIORITY, &max96717_api);                 \
                                                                                                   \
	VIDEO_DEVICE_DEFINE(max96717_##inst, DEVICE_DT_INST_GET(inst),                             \
			    SOURCE_DEV(inst));

DT_INST_FOREACH_STATUS_OKAY(MAX96717_INIT)
