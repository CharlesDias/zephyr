.. zephyr:code-sample:: video-gmsl-tunnel
   :name: GMSL tunnel validation
   :relevant-api: video_interface

   Validate a minimal MAX96724/MAX96717 video path.

Description
***********


Requirements
************


Building and Running
********************

GMSL tunnel diagnostic mode (default) — IMX219 sensor over the GMSL2 link:

.. code-block:: console

    west build -p -b stm32n6570_dk//fsbl \
      --shield raspberry_pi_camera_module_2 \
      --shield adi_gmsl2_camera_bridge \
      samples/drivers/video/adi_gmsl2_camera_bridge \
      -- -DDTC_OVERLAY_FILE="boards/adi_gmsl2_camera_bridge_1x_imx219.overlay"

VPG (TPG) validation mode — drives the host capture+display path from the
MAX96724's internal Video Pattern Generator, with no GMSL link, no
serializer and no camera sensor in the loop. Useful to validate the
host-side CSI-2, DCMIPP and LTDC plumbing in isolation before stacking the
GMSL tunnel on top.

Build and flash with the default gradient pattern:

.. code-block:: console

    west build -p -b stm32n6570_dk//fsbl \
      --shield adi_gmsl2_camera_bridge \
      samples/drivers/video/adi_gmsl2_camera_bridge \
      -- -DDTC_OVERLAY_FILE="boards/adi_gmsl2_camera_bridge_tpg.overlay" \
         -DEXTRA_CONF_FILE="boards/stm32n6570_dk_stm32n657xx_fsbl_tpg.conf"
    west flash

To run the checkerboard variant instead, either append
``CONFIG_VIDEO_MAX96724_VPG_PATTERN_CHECKERBOARD=y`` to
``boards/stm32n6570_dk_stm32n657xx_fsbl_tpg.conf`` or pass it on the west
command line:

.. code-block:: console

    west build -p -b stm32n6570_dk//fsbl \
      --shield adi_gmsl2_camera_bridge \
      samples/drivers/video/adi_gmsl2_camera_bridge \
      -- -DDTC_OVERLAY_FILE="boards/adi_gmsl2_camera_bridge_tpg.overlay" \
         -DEXTRA_CONF_FILE="boards/stm32n6570_dk_stm32n657xx_fsbl_tpg.conf" \
         -DCONFIG_VIDEO_MAX96724_VPG_PATTERN_CHECKERBOARD=y
    west flash

Expected results in TPG mode:

* UART logs the start banner, ``Capture started`` and a periodic per-frame
  line (size 1152000 bytes for 800x480 BGR24, ~50 Hz cadence).
* The on-board LTDC display shows either:

  * a vertical color gradient cycling B->G->R->K->W (gradient mode), or
  * a blue / red checkerboard tile pattern matching Figure 22 of the
    MAX96724 user guide (checkerboard mode).

Switching between the two patterns without touching any other code proves
the VPG configuration path is actually programming the deserializer.

Sample Output
*************

Default (diagnostic) mode:

.. code-block:: console

    [00:00:00.000,000] <inf> main: GMSL tunnel validation start

VPG mode:

.. code-block:: console

    [00:00:00.000,000] <inf> main: MAX96724 VPG (TPG) validation start
    [00:00:00.123,000] <inf> main: Capture started
    [00:00:00.156,000] <inf> main: Frame 0: 921600 bytes, ts=156 ms (delta 156 ms)
