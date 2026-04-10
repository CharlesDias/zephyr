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

Deserializer VPG (TPG) validation mode
=======================================

Drives the host capture+display path from the MAX96724's internal Video
Pattern Generator, with no GMSL link, no serializer and no camera sensor
in the loop. Useful to validate the host-side CSI-2, DCMIPP and LTDC
plumbing in isolation before stacking the GMSL tunnel on top.

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

Expected results in deserializer VPG mode:

* UART logs the start banner, ``Capture started`` and a periodic per-frame
  line (size 1152000 bytes for 800x480 BGR24, ~50 Hz cadence).
* The on-board LTDC display shows either:

  * a vertical color gradient cycling B->G->R->K->W (gradient mode), or
  * a blue / red checkerboard tile pattern matching Figure 22 of the
    MAX96724 user guide (checkerboard mode).

Switching between the two patterns without touching any other code proves
the VPG configuration path is actually programming the deserializer.

SerDes loopback VPG validation mode
====================================

Validates the full SerDes wire path: the MAX96717 serializer's internal
pattern generator (PATGEN) produces an 800x480 RGB888 frame, transmits it
over the GMSL2 coax link to the MAX96724 deserializer (running in
passthrough mode), which forwards it out MIPI CSI-2 to the host DCMIPP and
LTDC display. This proves the serializer, the GMSL2 link and the
deserializer forwarding path all work end-to-end without a real image
sensor.

**Requirement:** the GMSL2 coax cable must be connected between the
MAX96717 serializer board and the MAX96724 deserializer board.

Build and flash with the default gradient pattern:

.. code-block:: console

    west build -p -b stm32n6570_dk//fsbl \
      --shield adi_gmsl2_camera_bridge \
      samples/drivers/video/adi_gmsl2_camera_bridge \
      -- -DDTC_OVERLAY_FILE="boards/adi_gmsl2_camera_bridge_tpg_serdes.overlay" \
         -DEXTRA_CONF_FILE="boards/stm32n6570_dk_stm32n657xx_fsbl_tpg_serdes.conf"
    west flash

To run the checkerboard variant instead:

.. code-block:: console

    west build -p -b stm32n6570_dk//fsbl \
      --shield adi_gmsl2_camera_bridge \
      samples/drivers/video/adi_gmsl2_camera_bridge \
      -- -DDTC_OVERLAY_FILE="boards/adi_gmsl2_camera_bridge_tpg_serdes.overlay" \
         -DEXTRA_CONF_FILE="boards/stm32n6570_dk_stm32n657xx_fsbl_tpg_serdes.conf" \
         -DCONFIG_VIDEO_MAX96717_PATGEN_PATTERN_CHECKERBOARD=y
    west flash

Expected results in SerDes loopback mode:

* UART logs show both chips probed and the GMSL2 link locked:

  * ``Found MAX96724 (...) on csi_i2c@0x27``
  * ``All enabled GMSL2 links locked``
  * ``Found MAX96717 (ID=0xBF) on ...@0x40``
  * ``MAX96717 PATGEN streaming on pipe 0 (800x480 RGB888)``
  * ``MAX96724 streaming started (800x480 RGB24, GMSL passthrough)``

* ``Capture started`` and periodic per-frame lines (1152000 bytes, ~50 Hz).
* The LTDC display shows the same gradient or checkerboard pattern as the
  deserializer-only VPG mode, but the data has now travelled through the
  full serializer -> GMSL2 link -> deserializer path.

**Negative test:** disconnect the GMSL coax and re-flash. The deserializer
should fail to lock and the sample should refuse to stream, confirming true
passthrough mode (no silent fallback to the internal VPG).

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
