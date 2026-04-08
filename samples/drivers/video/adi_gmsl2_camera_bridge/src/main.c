/*
 * Copyright (c) 2026 Charles Dias
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/drivers/video.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#if IS_ENABLED(CONFIG_APP_TPG_MODE)
#include <zephyr/drivers/display.h>
#else
#include "gmsl_diag.h"
#endif

LOG_MODULE_REGISTER(main, CONFIG_APP_LOG_LEVEL);

#if !DT_HAS_CHOSEN(zephyr_camera)
#error No camera chosen in devicetree.
#endif

#define CAMERA_DEV DEVICE_DT_GET(DT_CHOSEN(zephyr_camera))

#if IS_ENABLED(CONFIG_APP_TPG_MODE)

#define TPG_NUM_BUFS  2
#define TPG_LOG_EVERY 60

/*
 * Clear the LCD once at startup. The LTDC framebuffer lives in PSRAM and is
 * not zero-initialised, so the area outside the 640x480 capture region (the
 * rightmost 160 columns on an 800x480 panel) would otherwise show stale
 * content from a previous run.
 */
static void tpg_clear_display(const struct device *display_dev,
			      const struct display_capabilities *caps)
{
	static uint8_t black_line[800 * 3];
	struct display_buffer_descriptor desc = {
		.buf_size = sizeof(black_line),
		.width = caps->x_resolution,
		.pitch = caps->x_resolution,
		.height = 1,
	};

	for (uint16_t y = 0; y < caps->y_resolution; y++) {
		(void)display_write(display_dev, 0, y, &desc, black_line);
	}
}

static int tpg_setup_display(const struct device *display_dev, uint32_t pixfmt)
{
	struct display_capabilities caps;
	int ret = 0;

	if (!device_is_ready(display_dev)) {
		LOG_ERR("Display %s not ready", display_dev->name);
		return -ENODEV;
	}

	display_get_capabilities(display_dev, &caps);
	LOG_INF("Display %s: %ux%u, current pixfmt=%u", display_dev->name, caps.x_resolution,
		caps.y_resolution, caps.current_pixel_format);

	if ((pixfmt == VIDEO_PIX_FMT_RGB24 || pixfmt == VIDEO_PIX_FMT_BGR24) &&
	    caps.current_pixel_format != PIXEL_FORMAT_RGB_888) {
		ret = display_set_pixel_format(display_dev, PIXEL_FORMAT_RGB_888);
		if (ret < 0) {
			LOG_WRN("Display does not accept RGB_888 (err %d) — frames will still be"
				" logged",
				ret);
		} else {
			display_get_capabilities(display_dev, &caps);
		}
	}

	tpg_clear_display(display_dev, &caps);

	ret = display_blanking_off(display_dev);
	if (ret == -ENOSYS) {
		ret = 0;
	}
	return ret;
}

static void tpg_display_frame(const struct device *display_dev, const struct video_buffer *vbuf,
			      const struct video_format *fmt)
{
	struct display_buffer_descriptor desc = {
		.buf_size = vbuf->bytesused,
		.width = fmt->width,
		.pitch = fmt->width,
		.height = vbuf->bytesused / fmt->pitch,
	};

	(void)display_write(display_dev, 0, vbuf->line_offset, &desc, vbuf->buffer);
}

static int tpg_main(void)
{
	const struct device *camera = CAMERA_DEV;
	const struct device *display = DEVICE_DT_GET_OR_NULL(DT_CHOSEN(zephyr_display));
	struct video_caps caps = {.type = VIDEO_BUF_TYPE_OUTPUT};
	struct video_format fmt = {.type = VIDEO_BUF_TYPE_OUTPUT};
	struct video_buffer *vbuf;
	unsigned int frame = 0;
	uint32_t last_ts = 0;
	int ret;

	LOG_INF("MAX96724 VPG (TPG) validation start");

	if (!device_is_ready(camera)) {
		LOG_ERR("Camera %s not ready", camera->name);
		return -ENODEV;
	}

	ret = video_get_caps(camera, &caps);
	if (ret < 0) {
		LOG_ERR("video_get_caps failed (err %d)", ret);
		return ret;
	}

	ret = video_get_format(camera, &fmt);
	if (ret < 0) {
		LOG_ERR("video_get_format failed (err %d)", ret);
		return ret;
	}

	/*
	 * The DCMIPP pipe has no format until video_set_format() is called, so
	 * the get above returns pitch=0/size=0. Force the VPG geometry here and
	 * push it down. Re-read the result so pitch/size reflect what the
	 * DCMIPP driver actually programmed (it computes pitch internally on
	 * its private pipe state and does NOT write back to the caller).
	 */
	fmt.pixelformat = VIDEO_PIX_FMT_BGR24;
	fmt.width = 800;
	fmt.height = 480;
	fmt.pitch = fmt.width * 3U;
	fmt.size = fmt.pitch * fmt.height;

	ret = video_set_format(camera, &fmt);
	if (ret < 0) {
		LOG_ERR("video_set_format failed (err %d)", ret);
		return ret;
	}

	ret = video_get_format(camera, &fmt);
	if (ret < 0) {
		LOG_ERR("video_get_format (post set) failed (err %d)", ret);
		return ret;
	}

	LOG_INF("Format: %s %ux%u (pitch %u, size %u)", VIDEO_FOURCC_TO_STR(fmt.pixelformat),
		fmt.width, fmt.height, fmt.pitch, fmt.size);

	if (fmt.size == 0U) {
		LOG_ERR("DCMIPP returned size=0 — pipe format not configured");
		return -EINVAL;
	}

	if (display != NULL) {
		ret = tpg_setup_display(display, fmt.pixelformat);
		if (ret < 0) {
			LOG_WRN("Display setup failed (err %d) — continuing headless", ret);
			display = NULL;
		}
	} else {
		LOG_INF("No zephyr,display chosen — running headless");
	}

	for (int i = 0; i < TPG_NUM_BUFS; i++) {
		/*
		 * STM32 DCMIPP HAL requires the destination buffer to be 16-byte
		 * aligned (HAL_DCMIPP_CSI_PIPE_Start rejects unaligned addresses
		 * with HAL_ERROR). The plain video_buffer_alloc() only asks for
		 * sizeof(void *) alignment, so use the explicit aligned variant
		 * with the alignment from CONFIG_VIDEO_BUFFER_POOL_ALIGN — which
		 * the STM32 SoC defconfig already pins to 16 when DCMIPP is on.
		 */
		vbuf = video_buffer_aligned_alloc(fmt.size, CONFIG_VIDEO_BUFFER_POOL_ALIGN,
						  K_NO_WAIT);
		if (vbuf == NULL) {
			LOG_ERR("Failed to alloc video buffer %d", i);
			return -ENOMEM;
		}
		vbuf->type = VIDEO_BUF_TYPE_OUTPUT;
		ret = video_enqueue(camera, vbuf);
		if (ret < 0) {
			LOG_ERR("video_enqueue failed (err %d)", ret);
			return ret;
		}
	}

	ret = video_stream_start(camera, VIDEO_BUF_TYPE_OUTPUT);
	if (ret < 0) {
		LOG_ERR("video_stream_start failed (err %d)", ret);
		return ret;
	}

	LOG_INF("Capture started");

	vbuf = NULL;
	while (1) {
		ret = video_dequeue(camera, &vbuf, K_MSEC(2000));
		if (ret == -EAGAIN) {
			LOG_WRN("video_dequeue timed out — DCMIPP not delivering frames");
			continue;
		}
		if (ret < 0) {
			LOG_ERR("video_dequeue failed (err %d)", ret);
			return ret;
		}

		/* Log only every TPG_LOG_EVERY-th frame so the UART isn't drowned. */
		if ((frame % TPG_LOG_EVERY) == 0) {
			LOG_INF("Frame %u: %u bytes, ts=%u ms (delta %u ms)", frame,
				vbuf->bytesused, vbuf->timestamp, vbuf->timestamp - last_ts);
		}
		frame++;
		last_ts = vbuf->timestamp;

		if (display != NULL) {
			tpg_display_frame(display, vbuf, &fmt);
		}

		ret = video_enqueue(camera, vbuf);
		if (ret < 0) {
			LOG_ERR("video_enqueue (requeue) failed (err %d)", ret);
			return ret;
		}
	}
}

#else /* CONFIG_APP_TPG_MODE */

#define I2C_BUS DEVICE_DT_GET(DT_NODELABEL(csi_i2c))

static int diag_main(void)
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

	i2c_scan_devices(i2c_dev);
	verify_device_ids(i2c_dev);

	LOG_INF("GMSL tunnel validation succeeded");
	return 0;
}

#endif /* CONFIG_APP_TPG_MODE */

int main(void)
{
#if IS_ENABLED(CONFIG_APP_TPG_MODE)
	return tpg_main();
#else
	return diag_main();
#endif
}
