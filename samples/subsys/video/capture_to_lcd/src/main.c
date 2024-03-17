/*
 * Copyright (c) 2019 Linaro Limited
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>

#include <zephyr/drivers/video.h>

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main);

int main(void)
{
	struct video_buffer *buffers[2], *vbuf;
	struct video_format fmt;
	struct video_caps caps;
	const struct device *video;
	unsigned int frame = 0;
	size_t bsize;
	int i = 0;

	const struct device *ov_cam_dev = DEVICE_DT_GET(DT_ALIAS(ov_cam));

	if (!device_is_ready(ov_cam_dev)) {
		LOG_ERR("%s: device not ready.\n", ov_cam_dev->name);
		return 0;
	}

	video = ov_cam_dev;

	LOG_INF("- Device name: %s\n", video->name);

	/* Get capabilities */
	if (video_get_caps(video, VIDEO_EP_OUT, &caps)) {
		LOG_ERR("Unable to retrieve video capabilities");
		return 0;
	}

	printk("- Capabilities:\n");
	while (caps.format_caps[i].pixelformat) {
		const struct video_format_cap *fcap = &caps.format_caps[i];
		/* fourcc to string */
		printk("  %c%c%c%c width [%u; %u; %u] height [%u; %u; %u]\n",
		       (char)fcap->pixelformat,
		       (char)(fcap->pixelformat >> 8),
		       (char)(fcap->pixelformat >> 16),
		       (char)(fcap->pixelformat >> 24),
		       fcap->width_min, fcap->width_max, fcap->width_step,
		       fcap->height_min, fcap->height_max, fcap->height_step);
		i++;
	}

	/* Get default/native format */
	if (video_get_format(video, VIDEO_EP_OUT, &fmt)) {
		LOG_ERR("Unable to retrieve video format");
		return 0;
	}

	printk("- Default format: %c%c%c%c %ux%u %u\n", (char)fmt.pixelformat,
		(char)(fmt.pixelformat >> 8),
		(char)(fmt.pixelformat >> 16),
		(char)(fmt.pixelformat >> 24),
		fmt.width, fmt.height, fmt.pitch);

	/* Set 160x120 format */
	fmt.width = 160;
	fmt.height = 120;
	fmt.pitch = fmt.width * 2;

	/* Set format */
	if (video_set_format(video, VIDEO_EP_OUT, &fmt)) {
		LOG_ERR("Unable to set up video format");
		return 0;
	}

	/* Get format */
	if (video_get_format(video, VIDEO_EP_OUT, &fmt)) {
		LOG_ERR("Unable to retrieve video format");
		return 0;
	}

	printk("- New format: %c%c%c%c %ux%u %u\n", (char)fmt.pixelformat,
		(char)(fmt.pixelformat >> 8),
		(char)(fmt.pixelformat >> 16),
		(char)(fmt.pixelformat >> 24),
		fmt.width, fmt.height, fmt.pitch);

	/* Size to allocate for each buffer */
	bsize = fmt.pitch * fmt.height;

	/* Alloc video buffers and enqueue for capture */
	for (i = 0; i < ARRAY_SIZE(buffers); i++) {
		buffers[i] = video_buffer_alloc(bsize);
		if (buffers[i] == NULL) {
			LOG_ERR("Unable to alloc video buffer");
			return 0;
		}

		video_enqueue(video, VIDEO_EP_OUT, buffers[i]);
	}

	/* Start video capture */
	if (video_stream_start(video)) {
		LOG_ERR("Unable to start capture (interface)");
		return 0;
	}

	printk("Capture started\n");

	/* Grab video frames */
	while (1) {
		int err;

		err = video_dequeue(video, VIDEO_EP_OUT, &vbuf, K_FOREVER);
		if (err) {
			LOG_ERR("Unable to dequeue video buf");
			LOG_ERR("err code %d: %s", err, strerror(-err));

			return 0;
		}

		printk("\rGot frame %u! size: %u; timestamp %u ms",
		       frame++, vbuf->bytesused, vbuf->timestamp);

		err = video_enqueue(video, VIDEO_EP_OUT, vbuf);
		if (err) {
			LOG_ERR("Unable to requeue video buf");
			LOG_ERR("err code %d: %s", err, strerror(-err));
			return 0;
		}
	}
}
