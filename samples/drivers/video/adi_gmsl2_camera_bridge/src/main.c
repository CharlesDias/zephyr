/*
 * Copyright (c) 2026 Charles Dias
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/drivers/video.h>
#include <zephyr/drivers/video-controls.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#if IS_ENABLED(CONFIG_DISPLAY)
#include <zephyr/drivers/display.h>
#endif

LOG_MODULE_REGISTER(main, CONFIG_APP_LOG_LEVEL);

#if !DT_HAS_CHOSEN(zephyr_camera)
#error No camera chosen in devicetree.
#endif

#define CAMERA_DEV DEVICE_DT_GET(DT_CHOSEN(zephyr_camera))

#define CAPTURE_NUM_BUFS  2
#define CAPTURE_LOG_EVERY 60

#if IS_ENABLED(CONFIG_DISPLAY)

/*
 * Clear the LCD once at startup. The LTDC framebuffer lives in PSRAM and is
 * not zero-initialised, so the area outside the capture region would otherwise
 * show stale content from a previous run.
 */
static void clear_display(const struct device *display_dev,
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

static int setup_display(const struct device *display_dev, uint32_t pixfmt)
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

	clear_display(display_dev, &caps);

	ret = display_blanking_off(display_dev);
	if (ret == -ENOSYS) {
		ret = 0;
	}
	return ret;
}

static void display_frame(const struct device *display_dev, const struct video_buffer *vbuf,
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

#endif /* CONFIG_DISPLAY */

int main(void)
{
	const struct device *camera = CAMERA_DEV;
	struct video_format fmt = {.type = VIDEO_BUF_TYPE_OUTPUT};
	struct video_buffer *vbuf;
	unsigned int frame = 0;
	uint32_t last_ts = 0;
	int ret;

#if IS_ENABLED(CONFIG_DISPLAY)
	const struct device *display = DEVICE_DT_GET_OR_NULL(DT_CHOSEN(zephyr_display));
#endif

	if (IS_ENABLED(CONFIG_APP_TPG_MODE)) {
		LOG_INF("MAX96724 VPG (TPG) validation start");
	} else {
		LOG_INF("GMSL2 camera bridge — capture start");
	}

	if (!device_is_ready(camera)) {
		LOG_ERR("Camera %s not ready", camera->name);
		return -ENODEV;
	}

	/*
	 * Set up the capture format.
	 * In TPG mode: hard-code 800x480 BGR24 (the VPG's only output).
	 * In camera mode: use 800x480 BGR24 to match the LTDC panel.
	 */
	fmt.pixelformat = VIDEO_PIX_FMT_BGR24;
	fmt.width = 800;
	fmt.height = 480;
	fmt.pitch = fmt.width * 3U;
	fmt.size = fmt.pitch * fmt.height;

	ret = video_set_compose_format(camera, &fmt);
	if (ret < 0) {
		LOG_ERR("video_set_compose_format failed (err %d)", ret);
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

	/* Enable test pattern on the sensor if requested via Kconfig */
	if (CONFIG_APP_TEST_PATTERN > 0) {
		struct video_control ctrl = {
			.id = VIDEO_CID_TEST_PATTERN,
			.val = CONFIG_APP_TEST_PATTERN,
		};

		ret = video_set_ctrl(camera, &ctrl);
		if (ret < 0 && ret != -ENOTSUP) {
			LOG_WRN("Failed to set test pattern %d (err %d)",
				CONFIG_APP_TEST_PATTERN, ret);
		} else if (ret == 0) {
			LOG_INF("Test pattern %d enabled", CONFIG_APP_TEST_PATTERN);
		}
	}

#if IS_ENABLED(CONFIG_DISPLAY)
	if (display != NULL) {
		ret = setup_display(display, fmt.pixelformat);
		if (ret < 0) {
			LOG_WRN("Display setup failed (err %d) — continuing headless", ret);
			display = NULL;
		}
	} else {
		LOG_INF("No zephyr,display chosen — running headless");
	}
#endif

	for (int i = 0; i < CAPTURE_NUM_BUFS; i++) {
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

		if ((frame % CAPTURE_LOG_EVERY) == 0) {
			LOG_INF("Frame %u: %u bytes, ts=%u ms (delta %u ms)", frame,
				vbuf->bytesused, vbuf->timestamp, vbuf->timestamp - last_ts);
		}
		frame++;
		last_ts = vbuf->timestamp;

#if IS_ENABLED(CONFIG_DISPLAY)
		if (display != NULL) {
			display_frame(display, vbuf, &fmt);
		}
#endif

		ret = video_enqueue(camera, vbuf);
		if (ret < 0) {
			LOG_ERR("video_enqueue (requeue) failed (err %d)", ret);
			return ret;
		}
	}
}
