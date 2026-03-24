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

To build the sample for IMX219 over the GMSL tunnel:

west build -p -b stm32n6570_dk//fsbl \
  --shield raspberry_pi_camera_module_2 \
  --shield adi_gmsl2_camera_bridge \
  samples/drivers/video/adi_gmsl2_camera_bridge \
  -- -DDTC_OVERLAY_FILE="boards/adi_gmsl2_camera_bridge_1x_imx219.overlay"

Sample Output
*************

.. code-block:: console

    [00:00:00.000,000] <inf> main: GMSL tunnel validation start
