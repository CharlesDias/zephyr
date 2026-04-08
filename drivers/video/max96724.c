/*
 * Copyright (c) 2026 Charles Dias
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#define DT_DRV_COMPAT adi_max96724

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/video-controls.h>
#include <zephyr/drivers/video.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include "video_common.h"
#include "video_ctrls.h"
#include "video_device.h"

LOG_MODULE_REGISTER(video_max96724, CONFIG_VIDEO_LOG_LEVEL);

#define MAX96724_REG8(addr)       ((addr) | VIDEO_REG_ADDR16_DATA8)

/* Device identification and link status */
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

/*
 * Video Pattern Generator (VPG / "TPG") and CSI-2 output registers.
 *
 * Names and addresses cross-checked against the MAX96724/F/R user guide
 * (rev 2, sections "Video Pattern Generator" pp. 56-59 and "MIPI TX")
 * and the upstream Linux driver in
 * drivers/media/i2c/maxim-serdes/max96724.c.
 */

/* Global PCLK source for VPG (sec. 5, Table 27 of user guide) */
#define MAX96724_REG_DEBUG_EXTRA           MAX96724_REG8(0x0009)
#define MAX96724_DEBUG_EXTRA_PCLK_SRC_MASK GENMASK(1, 0)
#define MAX96724_DEBUG_EXTRA_PCLK_25MHZ    0x0
#define MAX96724_DEBUG_EXTRA_PCLK_75MHZ    0x1
#define MAX96724_DEBUG_EXTRA_PCLK_USE_PIPE 0x2

/* Per-pipe VPG clock select (PATGEN_CLK_SRC, bit 7 of VPRBS) */
#define MAX96724_REG_VPRBS(p)              MAX96724_REG8(0x01DC + (p) * 0x20)
#define MAX96724_VPRBS_PATGEN_CLK_SRC      BIT(7)

/* Video pipe enable */
#define MAX96724_REG_VIDEO_PIPE_EN         MAX96724_REG8(0x00F4)
#define MAX96724_VIDEO_PIPE_EN_PIPE(p)     BIT(p)
#define MAX96724_VIDEO_PIPE_EN_MASK        GENMASK(3, 0)

/* MIPI PHY configuration */
#define MAX96724_REG_MIPI_PHY0             MAX96724_REG8(0x08A0)
#define MAX96724_MIPI_PHY0_PHY_CFG_MASK    GENMASK(4, 0)
#define MAX96724_MIPI_PHY0_PHY_4X2         BIT(0)
#define MAX96724_MIPI_PHY0_PHY_2X4         BIT(2)
#define MAX96724_MIPI_PHY0_FORCE_CSI_OUT   BIT(7)

#define MAX96724_REG_MIPI_PHY2             MAX96724_REG8(0x08A2)
/* In 2-lane mode, PHY index N standby-not bit lives at bit (4 + N) */
#define MAX96724_MIPI_PHY2_PHY_STDB_2(x)   BIT(4 + (x))

/* CSI-2 output enable (BACKTOP12) */
#define MAX96724_REG_BACKTOP12             MAX96724_REG8(0x040B)
#define MAX96724_BACKTOP12_CSI_OUT_EN      BIT(1)

/* Per-PHY CSI-2 lane count (MIPI_TX10) */
#define MAX96724_REG_MIPI_TX10(x)          MAX96724_REG8(0x090A + (x) * 0x40)
#define MAX96724_MIPI_TX10_CSI2_LANE_CNT_MASK GENMASK(7, 6)
#define MAX96724_MIPI_TX10_CSI2_CPHY_EN    BIT(5)

/* Pipe → MIPI controller mapping */
#define MAX96724_REG_MIPI_CTRL_SEL         MAX96724_REG8(0x08CA)
#define MAX96724_MIPI_CTRL_SEL_MASK(p)     (GENMASK(1, 0) << ((p) * 2))
#define MAX96724_MIPI_CTRL_SEL_PIPE(p, c)  ((c) << ((p) * 2))

/* DPLL frequency (BACKTOP22) */
#define MAX96724_REG_BACKTOP22(x)          MAX96724_REG8(0x0415 + (x) * 0x3)
#define MAX96724_BACKTOP22_DPLL_FREQ_MASK  GENMASK(4, 0)
#define MAX96724_BACKTOP22_DPLL_EN         BIT(5)

/* DPLL soft reset */
#define MAX96724_REG_DPLL_0(x)             MAX96724_REG8(0x1C00 + (x) * 0x100)
#define MAX96724_DPLL_0_SOFT_RST_N         BIT(0)

/* Pattern generator timing/colour registers (Table 26) */
#define MAX96724_REG_PATGEN_0              MAX96724_REG8(0x1050)
#define MAX96724_PATGEN_0_VTG_FREE_RUNNING 0x3 /* bits[1:0] */
#define MAX96724_PATGEN_0_DE_INV           BIT(2)
#define MAX96724_PATGEN_0_HS_INV           BIT(3)
#define MAX96724_PATGEN_0_VS_INV           BIT(4)
#define MAX96724_PATGEN_0_GEN_DE           BIT(5)
#define MAX96724_PATGEN_0_GEN_HS           BIT(6)
#define MAX96724_PATGEN_0_GEN_VS           BIT(7)

#define MAX96724_REG_PATGEN_1              MAX96724_REG8(0x1051)
#define MAX96724_PATGEN_1_MODE_MASK        GENMASK(5, 4)
#define MAX96724_PATGEN_1_MODE_DISABLED    0x0
#define MAX96724_PATGEN_1_MODE_CHECKER     0x1
#define MAX96724_PATGEN_1_MODE_GRADIENT    0x2

#define MAX96724_REG_VS_DLY_2              MAX96724_REG8(0x1052)
#define MAX96724_REG_VS_HIGH_2             MAX96724_REG8(0x1055)
#define MAX96724_REG_VS_LOW_2              MAX96724_REG8(0x1058)
#define MAX96724_REG_V2H_2                 MAX96724_REG8(0x105B)
#define MAX96724_REG_HS_HIGH_1             MAX96724_REG8(0x105E)
#define MAX96724_REG_HS_LOW_1              MAX96724_REG8(0x1060)
#define MAX96724_REG_HS_CNT_1              MAX96724_REG8(0x1062)
#define MAX96724_REG_V2D_2                 MAX96724_REG8(0x1064)
#define MAX96724_REG_DE_HIGH_1             MAX96724_REG8(0x1067)
#define MAX96724_REG_DE_LOW_1              MAX96724_REG8(0x1069)
#define MAX96724_REG_DE_CNT_1              MAX96724_REG8(0x106B)

#define MAX96724_REG_GRAD_INCR             MAX96724_REG8(0x106D)

#define MAX96724_LINK_LOCK_RETRIES  50
#define MAX96724_LINK_LOCK_DELAY_MS 20

/*
 * 800x480 @ ~50 Hz, 25 MHz pixel clock. Sized to exactly match the
 * STM32N6570-DK on-board LTDC panel so the VPG fills the screen with no
 * unwritten margin. Blanking values are scaled-up versions of the VESA
 * 640x480 timings used by the upstream Linux Maxim driver
 * (max_serdes_tpg_pixel_videomodes[]).
 *
 *   hactive=800, hfp=10,  hsync=96, hbp=40   -> htotal=946
 *   vactive=480, vfp=2,   vsync=24, vbp=24   -> vtotal=530
 *   25 MHz / (946 * 530) ≈ 49.9 Hz
 */
#define MAX96724_TPG_HACTIVE 800U
#define MAX96724_TPG_HFP     10U
#define MAX96724_TPG_HSYNC   96U
#define MAX96724_TPG_HBP     40U
#define MAX96724_TPG_HTOTAL  (MAX96724_TPG_HACTIVE + MAX96724_TPG_HFP + \
			      MAX96724_TPG_HSYNC + MAX96724_TPG_HBP)

#define MAX96724_TPG_VACTIVE 480U
#define MAX96724_TPG_VFP     2U
#define MAX96724_TPG_VSYNC   24U
#define MAX96724_TPG_VBP     24U
#define MAX96724_TPG_VTOTAL  (MAX96724_TPG_VACTIVE + MAX96724_TPG_VFP + \
			      MAX96724_TPG_VSYNC + MAX96724_TPG_VBP)

/* Computed VPG timing fields (matches max_serdes_get_vm_timings()) */
#define MAX96724_TPG_VS_HIGH (MAX96724_TPG_VSYNC * MAX96724_TPG_HTOTAL)
#define MAX96724_TPG_VS_LOW  ((MAX96724_TPG_VACTIVE + MAX96724_TPG_VFP + \
			       MAX96724_TPG_VBP) * MAX96724_TPG_HTOTAL)
#define MAX96724_TPG_HS_HIGH MAX96724_TPG_HSYNC
#define MAX96724_TPG_HS_LOW  (MAX96724_TPG_HACTIVE + MAX96724_TPG_HFP + \
			      MAX96724_TPG_HBP)
#define MAX96724_TPG_HS_CNT  (MAX96724_TPG_VACTIVE + MAX96724_TPG_VFP + \
			      MAX96724_TPG_VBP + MAX96724_TPG_VSYNC)
#define MAX96724_TPG_V2D     (MAX96724_TPG_HTOTAL * (MAX96724_TPG_VSYNC + \
							MAX96724_TPG_VBP) + \
			      (MAX96724_TPG_HSYNC + MAX96724_TPG_HBP))
#define MAX96724_TPG_DE_HIGH MAX96724_TPG_HACTIVE
#define MAX96724_TPG_DE_LOW  (MAX96724_TPG_HFP + MAX96724_TPG_HSYNC + \
			      MAX96724_TPG_HBP)
#define MAX96724_TPG_DE_CNT  MAX96724_TPG_VACTIVE

/*
 * Default CSI-2 output: PHY 2, 2 D-PHY data lanes.
 *
 * The shield overlay (boards/shields/adi_gmsl2_camera_bridge/) wires
 * max96724 port@6 (PHY index = reg - 4 = 2) to the host CSI-2 input
 * with `data-lanes = <1 2>`. We therefore configure 4x2 mode (four
 * independent 2-lane PHYs) and route pipe 0 to MIPI controller 2.
 */
#define MAX96724_TPG_PHY_INDEX 2U
#define MAX96724_TPG_NUM_LANES 2U

/*
 * D-PHY link frequency advertised to downstream (DCMIPP). Must match the
 * DPLL programming in max96724_csi2_configure() so the host PHY is set up
 * for the same rate that the deserializer actually drives.
 */
#define MAX96724_TPG_LINK_FREQ_HZ 600000000

static const int64_t max96724_link_frequencies[] = {
	MAX96724_TPG_LINK_FREQ_HZ,
};

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
};

struct max96724_data {
	struct video_format fmt;
	struct video_ctrl linkfreq;
	bool streaming;
};

static const struct video_format_cap max96724_fmts[] = {
	{
		.pixelformat = VIDEO_PIX_FMT_BGR24,
		.width_min = MAX96724_TPG_HACTIVE,
		.width_max = MAX96724_TPG_HACTIVE,
		.width_step = 0,
		.height_min = MAX96724_TPG_VACTIVE,
		.height_max = MAX96724_TPG_VACTIVE,
		.height_step = 0,
	},
	{0},
};

static int max96724_check_link_lock(const struct i2c_dt_spec *i2c)
{
	uint32_t val = 0;
	int ret;

	/* Poll LOCK_PIN — reflects all enabled GMSL links locked */
	for (int i = 0; i < MAX96724_LINK_LOCK_RETRIES; i++) {
		ret = video_read_cci_reg(i2c, MAX96724_REG_CTRL3, &val);
		if (ret < 0) {
			LOG_ERR("Failed to read CTRL3 (err %d)", ret);
			return ret;
		}

		if (val & MAX96724_CTRL3_LOCK_PIN) {
			LOG_INF("All enabled GMSL2 links locked (CTRL3=0x%02X)", val);
			return 0;
		}

		LOG_DBG("Waiting for GMSL2 link lock (%d/%d, CTRL3=0x%02X)", i + 1,
			MAX96724_LINK_LOCK_RETRIES, val);
		k_msleep(MAX96724_LINK_LOCK_DELAY_MS);
	}

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
			LOG_ERR("Failed to read link %s status (err %d)", links[l].name, ret);
			return ret;
		}

		if (val & MAX96724_CTRL_LOCKED) {
			LOG_INF("  Link %s: locked", links[l].name);
		} else {
			LOG_WRN("  Link %s: not locked", links[l].name);
		}
	}

	if (val & MAX96724_CTRL3_ERROR) {
		LOG_WRN("ERRB asserted — check GMSL link integrity");
	}

	return -ENODEV;
}

static int max96724_write_pattern_timings(const struct i2c_dt_spec *i2c)
{
	const struct video_reg regs[] = {
		/* 24-bit fields are stored MSB-first across 3 consecutive 8-bit regs */
		{MAX96724_REG_VS_DLY_2,  0},
		{MAX96724_REG8(0x1053), 0},
		{MAX96724_REG8(0x1054), 0},

		{MAX96724_REG_VS_HIGH_2, (MAX96724_TPG_VS_HIGH >> 16) & 0xFF},
		{MAX96724_REG8(0x1056),  (MAX96724_TPG_VS_HIGH >> 8)  & 0xFF},
		{MAX96724_REG8(0x1057),  (MAX96724_TPG_VS_HIGH >> 0)  & 0xFF},

		{MAX96724_REG_VS_LOW_2,  (MAX96724_TPG_VS_LOW >> 16) & 0xFF},
		{MAX96724_REG8(0x1059),  (MAX96724_TPG_VS_LOW >> 8)  & 0xFF},
		{MAX96724_REG8(0x105A),  (MAX96724_TPG_VS_LOW >> 0)  & 0xFF},

		{MAX96724_REG_V2H_2,    0},
		{MAX96724_REG8(0x105C), 0},
		{MAX96724_REG8(0x105D), 0},

		/* 16-bit fields */
		{MAX96724_REG_HS_HIGH_1, (MAX96724_TPG_HS_HIGH >> 8) & 0xFF},
		{MAX96724_REG8(0x105F),  (MAX96724_TPG_HS_HIGH >> 0) & 0xFF},

		{MAX96724_REG_HS_LOW_1,  (MAX96724_TPG_HS_LOW >> 8) & 0xFF},
		{MAX96724_REG8(0x1061),  (MAX96724_TPG_HS_LOW >> 0) & 0xFF},

		{MAX96724_REG_HS_CNT_1,  (MAX96724_TPG_HS_CNT >> 8) & 0xFF},
		{MAX96724_REG8(0x1063),  (MAX96724_TPG_HS_CNT >> 0) & 0xFF},

		/* v2d is 24-bit */
		{MAX96724_REG_V2D_2,    (MAX96724_TPG_V2D >> 16) & 0xFF},
		{MAX96724_REG8(0x1065), (MAX96724_TPG_V2D >> 8)  & 0xFF},
		{MAX96724_REG8(0x1066), (MAX96724_TPG_V2D >> 0)  & 0xFF},

		{MAX96724_REG_DE_HIGH_1, (MAX96724_TPG_DE_HIGH >> 8) & 0xFF},
		{MAX96724_REG8(0x1068),  (MAX96724_TPG_DE_HIGH >> 0) & 0xFF},

		{MAX96724_REG_DE_LOW_1,  (MAX96724_TPG_DE_LOW >> 8) & 0xFF},
		{MAX96724_REG8(0x106A),  (MAX96724_TPG_DE_LOW >> 0) & 0xFF},

		{MAX96724_REG_DE_CNT_1,  (MAX96724_TPG_DE_CNT >> 8) & 0xFF},
		{MAX96724_REG8(0x106C),  (MAX96724_TPG_DE_CNT >> 0) & 0xFF},

		{MAX96724_REG_PATGEN_0,
		 MAX96724_PATGEN_0_VTG_FREE_RUNNING |
		 MAX96724_PATGEN_0_VS_INV |
		 MAX96724_PATGEN_0_GEN_DE |
		 MAX96724_PATGEN_0_GEN_HS |
		 MAX96724_PATGEN_0_GEN_VS},

		/*
		 * Gradient increment per PCLK. Linux upstream uses 4 (matches the
		 * 1920x1080 modes it targets). On our narrower 800-pixel line that
		 * only fits ~1.5 phases of the B→G→R→K→W cycle, so we step it up
		 * to 16 to land at least two full cycles across hactive — gives a
		 * denser bar pattern that's easier to eyeball for tearing/jitter.
		 */
		{MAX96724_REG_GRAD_INCR, 16},
	};

	return video_write_cci_multiregs(i2c, regs, ARRAY_SIZE(regs));
}

static int max96724_vpg_configure(const struct i2c_dt_spec *i2c)
{
	int ret;

	/*
	 * 640x480p60 with a 25 MHz pixel clock — the simplest VPG mode.
	 * The internal 25 MHz REFCLK feeds PCLK directly (no PLL needed),
	 * so we set DEBUG_EXTRA[1:0] = 00 (PCLK_SRC = 25 MHz).
	 * VPRBS bit7 (PATGEN_CLK_SRC) is irrelevant when PCLK_SRC != USE_PIPE,
	 * but we clear it anyway for determinism.
	 */
	ret = video_modify_cci_reg(i2c, MAX96724_REG_VPRBS(0),
				   MAX96724_VPRBS_PATGEN_CLK_SRC, 0);
	if (ret < 0) {
		return ret;
	}

	ret = video_modify_cci_reg(i2c, MAX96724_REG_DEBUG_EXTRA,
				   MAX96724_DEBUG_EXTRA_PCLK_SRC_MASK,
				   MAX96724_DEBUG_EXTRA_PCLK_25MHZ);
	if (ret < 0) {
		return ret;
	}

	ret = max96724_write_pattern_timings(i2c);
	if (ret < 0) {
		return ret;
	}

	/* Select gradient pattern */
	return video_modify_cci_reg(i2c, MAX96724_REG_PATGEN_1,
				    MAX96724_PATGEN_1_MODE_MASK,
				    FIELD_PREP(MAX96724_PATGEN_1_MODE_MASK,
					       MAX96724_PATGEN_1_MODE_GRADIENT));
}

static int max96724_csi2_configure(const struct i2c_dt_spec *i2c)
{
	/*
	 * Lane data rate advertised to the host (DCMIPP) via VIDEO_CID_LINK_FREQ.
	 * Zephyr's CSI ecosystem treats VIDEO_CID_LINK_FREQ in Hz as the
	 * per-lane data rate in bps (see video_stm32_dcmipp.c which divides by
	 * MHZ(1) and looks the value up directly as Mbps in DCMIPP_PHY_BITRATE
	 * — IMX219 uses the same convention: 456 MHz → 456 Mbps/lane).
	 *
	 * MAX96724 BACKTOP22[4:0] (PHY_CSI_TX_DPLL) is "DPLL output / 100 MHz",
	 * where the DPLL output is the lane data rate. So the register value is
	 * simply lane_data_rate_mbps / 100. We must NOT double here.
	 */
	const uint32_t link_freq_hz = MAX96724_TPG_LINK_FREQ_HZ;
	const uint32_t backtop22_freq = (uint32_t)(link_freq_hz / 100000000ULL);
	int ret;

	/* PHY mode: 4x2-lane DPHY (four independent 2-lane PHYs). */
	ret = video_modify_cci_reg(i2c, MAX96724_REG_MIPI_PHY0,
				   MAX96724_MIPI_PHY0_PHY_CFG_MASK,
				   MAX96724_MIPI_PHY0_PHY_4X2);
	if (ret < 0) {
		return ret;
	}

	/*
	 * Map pipe 0 → controller MAX96724_TPG_PHY_INDEX (the PHY wired to
	 * the host) and route pipes 1, 2, 3 to controller 0 (an unused PHY
	 * left in standby).
	 *
	 * The MAX96724 VPG injects its data into ALL pipes simultaneously
	 * (see the comment in upstream Linux drivers/media/i2c/maxim-serdes/
	 * max_des.c around max_des_get_pipe_remaps() — "TPG mode is only
	 * handled on pipe 0, but the TPG pollutes other pipes with the same
	 * data"). The default MIPI_CTRL_SEL value (pipe N → controller N)
	 * therefore lets pipe TPG_PHY_INDEX collide with pipe 0 on the host
	 * controller, corrupting framing. Re-route the unused pipes to
	 * controller 0 so their copies are dropped at the standby PHY.
	 */
	ret = video_write_cci_reg(i2c, MAX96724_REG_MIPI_CTRL_SEL,
				  MAX96724_MIPI_CTRL_SEL_PIPE(0, MAX96724_TPG_PHY_INDEX) |
					  MAX96724_MIPI_CTRL_SEL_PIPE(1, 0) |
					  MAX96724_MIPI_CTRL_SEL_PIPE(2, 0) |
					  MAX96724_MIPI_CTRL_SEL_PIPE(3, 0));
	if (ret < 0) {
		return ret;
	}

	/* Set CSI-2 lane count on the chosen MIPI TX controller (D-PHY mode). */
	ret = video_modify_cci_reg(i2c, MAX96724_REG_MIPI_TX10(MAX96724_TPG_PHY_INDEX),
				   MAX96724_MIPI_TX10_CSI2_LANE_CNT_MASK |
					   MAX96724_MIPI_TX10_CSI2_CPHY_EN,
				   FIELD_PREP(MAX96724_MIPI_TX10_CSI2_LANE_CNT_MASK,
					      MAX96724_TPG_NUM_LANES - 1U));
	if (ret < 0) {
		return ret;
	}

	/* Configure DPLL frequency for the chosen PHY: lane data rate / 100 MHz. */
	ret = video_modify_cci_reg(i2c, MAX96724_REG_DPLL_0(MAX96724_TPG_PHY_INDEX),
				   MAX96724_DPLL_0_SOFT_RST_N, 0);
	if (ret < 0) {
		return ret;
	}

	ret = video_modify_cci_reg(i2c, MAX96724_REG_BACKTOP22(MAX96724_TPG_PHY_INDEX),
				   MAX96724_BACKTOP22_DPLL_FREQ_MASK |
					   MAX96724_BACKTOP22_DPLL_EN,
				   FIELD_PREP(MAX96724_BACKTOP22_DPLL_FREQ_MASK, backtop22_freq) |
					   MAX96724_BACKTOP22_DPLL_EN);
	if (ret < 0) {
		return ret;
	}

	ret = video_modify_cci_reg(i2c, MAX96724_REG_DPLL_0(MAX96724_TPG_PHY_INDEX),
				   MAX96724_DPLL_0_SOFT_RST_N,
				   MAX96724_DPLL_0_SOFT_RST_N);
	if (ret < 0) {
		return ret;
	}

	/* Bring PHY1 out of standby (2-lane mode bit) */
	return video_modify_cci_reg(i2c, MAX96724_REG_MIPI_PHY2,
				    MAX96724_MIPI_PHY2_PHY_STDB_2(MAX96724_TPG_PHY_INDEX),
				    MAX96724_MIPI_PHY2_PHY_STDB_2(MAX96724_TPG_PHY_INDEX));
}

static int max96724_get_caps(const struct device *dev, struct video_caps *caps)
{
	ARG_UNUSED(dev);

	if (caps->type != VIDEO_BUF_TYPE_OUTPUT) {
		return -EINVAL;
	}

	caps->format_caps = max96724_fmts;
	caps->min_vbuf_count = 2;
	return 0;
}

static int max96724_get_format(const struct device *dev, struct video_format *fmt)
{
	struct max96724_data *data = dev->data;

	*fmt = data->fmt;
	return 0;
}

static int max96724_set_format(const struct device *dev, struct video_format *fmt)
{
	struct max96724_data *data = dev->data;

	if (fmt->pixelformat != VIDEO_PIX_FMT_BGR24 ||
	    fmt->width != MAX96724_TPG_HACTIVE ||
	    fmt->height != MAX96724_TPG_VACTIVE) {
		LOG_ERR("Format %s %ux%u not supported by VPG MVP",
			VIDEO_FOURCC_TO_STR(fmt->pixelformat), fmt->width, fmt->height);
		return -ENOTSUP;
	}

	data->fmt = *fmt;
	return 0;
}

static int max96724_set_stream(const struct device *dev, bool enable, enum video_buf_type type)
{
	const struct max96724_config *cfg = dev->config;
	struct max96724_data *data = dev->data;
	int ret;

	if (type != VIDEO_BUF_TYPE_OUTPUT) {
		return -EINVAL;
	}

	if (enable == data->streaming) {
		return 0;
	}

	if (!enable) {
		(void)video_modify_cci_reg(&cfg->i2c, MAX96724_REG_PATGEN_1,
					   MAX96724_PATGEN_1_MODE_MASK,
					   FIELD_PREP(MAX96724_PATGEN_1_MODE_MASK,
						      MAX96724_PATGEN_1_MODE_DISABLED));
		(void)video_modify_cci_reg(&cfg->i2c, MAX96724_REG_VIDEO_PIPE_EN,
					   MAX96724_VIDEO_PIPE_EN_PIPE(0), 0);
		(void)video_modify_cci_reg(&cfg->i2c, MAX96724_REG_BACKTOP12,
					   MAX96724_BACKTOP12_CSI_OUT_EN, 0);
		(void)video_modify_cci_reg(&cfg->i2c, MAX96724_REG_MIPI_PHY0,
					   MAX96724_MIPI_PHY0_FORCE_CSI_OUT, 0);
		data->streaming = false;
		return 0;
	}

	ret = max96724_csi2_configure(&cfg->i2c);
	if (ret < 0) {
		LOG_ERR("CSI-2 configure failed (err %d)", ret);
		return ret;
	}

	ret = max96724_vpg_configure(&cfg->i2c);
	if (ret < 0) {
		LOG_ERR("VPG configure failed (err %d)", ret);
		return ret;
	}

	/*
	 * Enable pipe 0 only and disable pipes 1, 2, 3. The VPG broadcasts
	 * data into every pipe, so we keep just pipe 0 active and let the
	 * other pipes drop their copies. Bit 4 (STREAM_SEL_ALL) is preserved
	 * — it is not a pipe-enable bit.
	 */
	ret = video_modify_cci_reg(&cfg->i2c, MAX96724_REG_VIDEO_PIPE_EN,
				   MAX96724_VIDEO_PIPE_EN_MASK,
				   MAX96724_VIDEO_PIPE_EN_PIPE(0));
	if (ret < 0) {
		return ret;
	}

	/* Force CSI-2 output even with no incoming GMSL link (VPG-only mode). */
	ret = video_modify_cci_reg(&cfg->i2c, MAX96724_REG_MIPI_PHY0,
				   MAX96724_MIPI_PHY0_FORCE_CSI_OUT,
				   MAX96724_MIPI_PHY0_FORCE_CSI_OUT);
	if (ret < 0) {
		return ret;
	}

	/* Enable the global CSI-2 transmitter. */
	ret = video_modify_cci_reg(&cfg->i2c, MAX96724_REG_BACKTOP12,
				   MAX96724_BACKTOP12_CSI_OUT_EN,
				   MAX96724_BACKTOP12_CSI_OUT_EN);
	if (ret < 0) {
		return ret;
	}

	data->streaming = true;
	LOG_INF("MAX96724 VPG streaming started (%ux%u RGB24)",
		MAX96724_TPG_HACTIVE, MAX96724_TPG_VACTIVE);
	return 0;
}

static DEVICE_API(video, max96724_api) = {
	.get_caps = max96724_get_caps,
	.get_format = max96724_get_format,
	.set_format = max96724_set_format,
	.set_stream = max96724_set_stream,
};

static int max96724_init(const struct device *dev)
{
	const struct max96724_config *cfg = dev->config;
	struct max96724_data *data = dev->data;
	uint32_t chip_id;
	int ret;

	if (!i2c_is_ready_dt(&cfg->i2c)) {
		LOG_ERR("I2C bus not ready");
		return -ENODEV;
	}

	ret = video_read_cci_reg(&cfg->i2c, MAX96724_REG_DEVID, &chip_id);
	if (ret < 0) {
		LOG_ERR("Failed to read device ID (err %d)", ret);
		return ret;
	}

	LOG_INF("Found %s (ID=0x%02X) on %s@0x%02x", max96724_variant_model(chip_id), chip_id,
		cfg->i2c.bus->name, cfg->i2c.addr);

	ret = max96724_check_link_lock(&cfg->i2c);
	if (ret < 0) {
		if (IS_ENABLED(CONFIG_VIDEO_MAX96724_TPG_TOLERATE_NO_LINK)) {
			LOG_WRN("No GMSL link locked — continuing (TPG/VPG mode)");
		} else {
			return ret;
		}
	}

	data->fmt.type = VIDEO_BUF_TYPE_OUTPUT;
	data->fmt.pixelformat = VIDEO_PIX_FMT_BGR24;
	data->fmt.width = MAX96724_TPG_HACTIVE;
	data->fmt.height = MAX96724_TPG_VACTIVE;
	data->fmt.pitch = MAX96724_TPG_HACTIVE * 3U;
	data->fmt.size = data->fmt.pitch * MAX96724_TPG_VACTIVE;

	/*
	 * Advertise link frequency so downstream consumers (DCMIPP) can
	 * configure the host CSI-2 PHY via video_get_csi_link_freq().
	 */
	ret = video_init_int_menu_ctrl(&data->linkfreq, dev, VIDEO_CID_LINK_FREQ, 0,
				       max96724_link_frequencies,
				       ARRAY_SIZE(max96724_link_frequencies));
	if (ret < 0) {
		LOG_ERR("Failed to init link-frequency control (err %d)", ret);
		return ret;
	}
	data->linkfreq.flags |= VIDEO_CTRL_FLAG_READ_ONLY;

	return 0;
}

#define MAX96724_INIT(inst)                                                                        \
	static const struct max96724_config max96724_config_##inst = {                             \
		.i2c = I2C_DT_SPEC_INST_GET(inst),                                                 \
	};                                                                                         \
	static struct max96724_data max96724_data_##inst;                                          \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(inst, max96724_init, NULL, &max96724_data_##inst,                    \
			      &max96724_config_##inst, POST_KERNEL,                                \
			      CONFIG_VIDEO_MAX96724_INIT_PRIORITY, &max96724_api);                 \
                                                                                                   \
	VIDEO_DEVICE_DEFINE(max96724_##inst, DEVICE_DT_INST_GET(inst), NULL);

DT_INST_FOREACH_STATUS_OKAY(MAX96724_INIT)
