.. _mipi_dsi_stm32_sample:

MIPI DSI STM32 HAL Sample
#########################

Overview
********

This sample demonstrates how to use the MIPI DSI (Display Serial Interface)
functionality on STM32 microcontrollers using the STM32 HAL drivers. The sample
is specifically designed for the STM32U5G9J-DK1 board and shows how to:

* Initialize the DSI host controller
* Configure the LTDC (LCD-TFT Display Controller)
* Set up DMA2D for graphics operations
* Configure the connected DSI panel (Himax HX8379)
* Display a simple test pattern

The sample is based on the working STM32CubeU5 DSI VideoMode SingleBuffer example
and has been adapted to work as a minimal Zephyr application without dependencies
on the Zephyr display subsystem.

Requirements
************

* STM32U5G9J-DK1 development board
* Connected DSI LCD panel (comes with the board)

Building and Running
********************

This sample can be found under :zephyr_file:`samples/drivers/mipi_dsi` in the
Zephyr tree.

To build and run the sample:

.. zephyr-app-commands::
   :zephyr-app: samples/drivers/mipi_dsi
   :board: stm32u5g9j_dk1
   :goals: build flash
   :compact:

Sample Output
*************

.. code-block:: console

   [00:00:00.123,000] <inf> dsi_minimal: STM32U5G9J-DK1 DSI Minimal Application Starting...
   [00:00:00.145,000] <inf> dsi_minimal: GPIO initialization completed
   [00:00:00.167,000] <inf> dsi_minimal: DMA2D initialized successfully
   [00:00:00.189,000] <inf> dsi_minimal: Initializing DSI Host...
   [00:00:00.234,000] <inf> dsi_minimal: DSI Host initialized successfully
   [00:00:00.245,000] <inf> dsi_minimal: DSI Configuration:
   [00:00:00.256,000] <inf> dsi_minimal:   Lanes: 2
   [00:00:00.267,000] <inf> dsi_minimal:   Mode: Video Burst
   [00:00:00.278,000] <inf> dsi_minimal:   Color Coding: RGB888
   [00:00:00.289,000] <inf> dsi_minimal:   Resolution: 480x481
   [00:00:00.301,000] <inf> dsi_minimal: Initializing LTDC...
   [00:00:00.345,000] <inf> dsi_minimal: LTDC initialized successfully
   [00:00:00.356,000] <inf> dsi_minimal: LTDC Configuration:
   [00:00:00.367,000] <inf> dsi_minimal:   Frame Buffer: 0x200D0000
   [00:00:00.378,000] <inf> dsi_minimal:   Resolution: 480x481
   [00:00:00.389,000] <inf> dsi_minimal:   Pixel Format: ARGB8888
   [00:00:00.401,000] <inf> dsi_minimal:   Frame Buffer Size: 924960 bytes
   [00:00:00.412,000] <inf> dsi_minimal: Configuring DSI panel...
   [00:00:00.567,000] <inf> dsi_minimal: DSI panel configured successfully
   [00:00:00.578,000] <inf> dsi_minimal: Display subsystem initialized successfully!
   [00:00:00.589,000] <inf> dsi_minimal: Running demo pattern...

Features Demonstrated
*********************

* **DSI Host Configuration**: Two-lane DSI configuration with video burst mode
* **LTDC Setup**: Layer configuration with ARGB8888 pixel format
* **DMA2D Integration**: Hardware acceleration for graphics operations
* **Panel Initialization**: Complete initialization sequence for HX8379 controller
* **Frame Buffer Management**: Direct frame buffer manipulation for graphics
* **Clock Configuration**: Proper PLL and peripheral clock setup for DSI/LTDC

Hardware Details
****************

The sample uses the following hardware components on the STM32U5G9J-DK1:

* **DSI Host**: Two-lane MIPI DSI interface running at 500MHz
* **LTDC**: LCD-TFT Display Controller with 480x481 resolution
* **DMA2D**: 2D graphics accelerator for efficient memory-to-memory transfers
* **LCD Panel**: 2.4" IPS LCD with Himax HX8379 controller
* **Frame Buffer**: Located at 0x200D0000 in external PSRAM

Code Structure
**************

The sample is organized into the following main functions:

* ``system_clock_config()``: Configures the main system clocks
* ``periph_common_clock_config()``: Sets up DSI and LTDC specific clocks
* ``gpio_init()``: Initializes required GPIO pins for DSI
* ``dsi_init()``: Configures the DSI host controller
* ``ltdc_init()``: Sets up the LTDC controller and layer
* ``dma2d_init()``: Initializes the DMA2D accelerator
* ``dsi_panel_config()``: Sends initialization commands to the LCD panel
* ``demo_pattern()``: Creates and displays test patterns

This sample provides a foundation for more complex graphics applications using
the MIPI DSI interface on STM32 microcontrollers.
