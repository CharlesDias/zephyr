.. _mipi_dsi_hx8379c_test:

HX8379C MIPI DSI Display Test
#############################

Overview
********

This sample demonstrates the HX8379C MIPI DSI display panel driver on the
STM32U5G9J-DK1 Discovery Kit. The sample initializes the display, turns
on the backlight, and displays a colorful test pattern with horizontal
color bars to verify proper MIPI DSI functionality.

The HX8379C is a 480x480 pixel MIPI DSI display controller with RGB888 pixel
format support, commonly used in embedded display applications.

Features Demonstrated
*********************

* MIPI DSI host initialization and configuration
* HX8379C panel driver initialization
* Display capabilities detection
* Backlight control via GPIO
* Frame buffer allocation and management  
* RGB888 color display with test pattern
* Display refresh functionality

Test Pattern
************

The sample displays 8 horizontal color bars:
* Red
* Green  
* Blue
* White
* Yellow
* Cyan
* Magenta
* Black

The pattern refreshes every 10 seconds to demonstrate display update capability.

Requirements
************

* STM32U5G9J-DK1 Discovery Kit
* HX8379C MIPI DSI display panel (integrated on the board)

Building and Running
********************

This sample can be found under :zephyr_file:`samples/drivers/mipi_dsi/test_hx8379c`
in the Zephyr tree.

Build and flash the sample for the STM32U5G9J-DK1 board:

.. zephyr-app-commands::
   :zephyr-app: samples/drivers/mipi_dsi/test_hx8379c
   :board: stm32u5g9j_dk1
   :goals: build flash
   :compact:

Sample Output
=============

The sample will output the following to the console:

.. code-block:: console

   *** Booting Zephyr OS build v4.1.0 ***
   [00:00:00.253,000] <inf> test_display: STM32U5G9J-DK1 HX8379-C Display Test
   [00:00:00.253,000] <inf> test_display: Display device ready
   [00:00:00.253,000] <inf> test_display: Display capabilities:
   [00:00:00.253,000] <inf> test_display:   Resolution: 480x480
   [00:00:00.253,000] <inf> test_display:   Pixel format: 0x20
   [00:00:00.253,000] <inf> test_display:   Orientation: 0
   [00:00:00.253,000] <inf> test_display: Display turned on successfully

Hardware Configuration
***********************

The sample requires the following hardware configuration:

* MIPI DSI interface connected to HX8379C display controller
* Reset GPIO (PD5) for display panel reset control
* Backlight GPIO (PI6) for backlight control
* LTDC interface for pixel data transfer

The STM32U5G9J-DK1 board comes with the HX8379C display panel pre-installed and
properly connected.

DSI Configuration
*****************

The MIPI DSI configuration used in this sample is based on the STM32CubeU5
DSI_VideoMode_SingleBuffer example and includes:

* DSI PLL: NDIV=125, IDF=4, ODF=2
* PHY timings optimized for the HX8379C panel
* 2-lane DSI configuration with RGB888 pixel format
* Video burst mode operation

References
**********

* `STM32U5G9J-DK1 Discovery Kit Documentation`_
* `STM32CubeU5 DSI Examples`_
* `HX8379C Display Controller Datasheet`_

.. _STM32U5G9J-DK1 Discovery Kit Documentation:
   https://www.st.com/en/evaluation-tools/stm32u5g9j-dk1.html
.. _STM32CubeU5 DSI Examples:
   https://github.com/STMicroelectronics/STM32CubeU5
.. _HX8379C Display Controller Datasheet:
   https://www.himax.com.tw
