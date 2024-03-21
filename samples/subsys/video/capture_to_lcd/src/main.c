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

#define CHUNK_SIZE 16  // Size of each chunk in bytes

int main(void)
{
	struct video_buffer *buffers[2], *vbuf;
	struct video_format fmt;
	struct video_caps caps;
	const struct device *video;
	unsigned int frame = 0;
	size_t bsize;
	int i = 0;
	int err;

	const struct device *cam_dev = DEVICE_DT_GET_ONE(st_stm32_dcmi);

	if (!device_is_ready(cam_dev)) {
		LOG_ERR("%s: device not ready.\n", cam_dev->name);
		return 0;
	}

	video = cam_dev;

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
	err = video_stream_start(video);
	if (err) {
		LOG_ERR("Unable to start capture (interface)");
		LOG_ERR("err code %d: %s", err, strerror(-err));
		return 0;
	}

	printk("Capture started\n");

	/* Grab video frames */
	// while (1) {
	{
		err = video_dequeue(video, VIDEO_EP_OUT, &vbuf, K_FOREVER);
		if (err) {
			LOG_ERR("Unable to dequeue video buf");
			LOG_ERR("err code %d: %s", err, strerror(-err));

			return 0;
		}

		video_stream_stop(video);

		printk("\rGot frame %u! size: %u; timestamp %u ms\n",
		       frame++, vbuf->bytesused, vbuf->timestamp);

		{
			uint8_t *buffer_ptr = vbuf->buffer;
			size_t remaining_bytes = vbuf->bytesused;
			// Send the buffer through the printk
			while(remaining_bytes > 0) {
				size_t chunk_size = remaining_bytes < CHUNK_SIZE ? remaining_bytes : CHUNK_SIZE;
				for(int i = 0; i < chunk_size; i++)
				{
					printk("%02x ", *buffer_ptr);
					buffer_ptr++;
				}
				printk("\n");
				remaining_bytes -= chunk_size;
			}
		}

		// err = video_enqueue(video, VIDEO_EP_OUT, vbuf);
		// if (err) {
		// 	LOG_ERR("Unable to requeue video buf");
		// 	LOG_ERR("err code %d: %s", err, strerror(-err));
		// 	return 0;
		// }
	}
}
