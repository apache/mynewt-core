Drivers
=======

Description
~~~~~~~~~~~

Device drivers in the Mynewt context includes libraries that interface
with devices external to the CPU. These devices are connected to the CPU
via standard peripherals such as SPI, GPIO, I2C etc. Device drivers
leverage the base HAL services in Mynewt to provide friendly
abstractions to application developers.

.. code-block:: console


    +———————————————————————————+
    |            app            |
    +———————————————————————————+
    |          (n)drivers       |
    +———————————————————————————+
    |     HAL     |     BSP     |
    +—————————————+—————————————+

-  The Board Support Package (BSP) abstracts board specific
   configurations e.g. CPU frequency, input voltage, LED pins, on-chip
   flash map etc.

-  The Hardware Abstraction Layer (HAL) abstracts architecture-specific
   functionality. It initializes and enables components within a master
   processor. It is designed to be portable across all the various MCUs
   supported in Mynewt (e.g. Nordic's nRF51, Nordic's nRF52, NXP's
   MK64F12 etc.). It includes code that initializes and manages access
   to components of the board such as board buses (I2C, PCI, PCMCIA,
   etc.), off-chip memory (controllers, level 2+ cache, Flash, etc.),
   and off-chip I/O (Ethernet, RS-232, display, mouse, etc.)

-  The driver sits atop the BSP and HAL. It abstracts the common modes
   of operation for each peripheral device connected via the standard
   interfaces to the processor. There may be multiple driver
   implementations of differing complexities for a particular peripheral
   device. For example, for an Analog to Digital Converter (ADC)
   peripheral you might have a simple driver that does blocking ADC
   reads and uses the HAL only. You might have a more complex driver
   that can deal with both internal and external ADCs, and has chip
   specific support for doing things like DMA’ing ADC reads into a
   buffer and posting an event to a task every ’n’ samples. The drivers
   are the ones that register with the kernel’s power management APIs,
   and manage turning on and off peripherals and external chipsets, etc.
   The Mynewt core repository comes with a base set of drivers to help
   the user get started.

General design principles
~~~~~~~~~~~~~~~~~~~~~~~~~

-  Device drivers should have a consistent structure and unified
   interface whenever possible. For example, we have a top-level
   package, “adc”, which contains the interface for all ADC drivers, and
   then we have the individual implementation of the driver itself. The
   following source files point to this:

   -  high-level ADC API: ``hw/drivers/adc/include/adc/adc.h``
   -  implementation of ADC for STM32F4:
      ``hw/drivers/adc/adc_stm32f4/src/adc_stm32f4.c`` (As of the
      1.0.0-beta release, ADC for nRF51 and nRF52 are available at an
      external
      `repo <https://github.com/runtimeco/mynewt_nordic/tree/master/hw/drivers/adc>`__.
      They are expected to be pulled into the core repo on Apache Mynewt
      after the license terms are clarified.). The only exported call in
      this example is
      ``int stm32f4_adc_dev_init(struct os_dev *, void *)`` which is
      passed as a function pointer to ``os_dev_create()`` in
      ``hal_bsp.c``, when the adc device is created.

-  Device drivers should be easy to use. In Mynewt, creating a device
   initializes it as well, making it readily available for the user to
   open, use (e.g. read, configure etc.) and close. Creating a device is
   simple using
   ``os_dev_create(struct os_dev *dev, char *name, uint8_t stage, uint8_t priority, os_dev_init_func_t od_init, void *arg)``.
   The ``od_init`` function is defined within the appropriate driver
   directory e.g. ``stm32f4_adc_dev_init`` in
   ``hw/drivers/adc/adc_stm32f4/src/adc_stm32f4.c`` for the ADC device
   initialization function.

-  The implementation should allow for builds optimized for minimal
   memory usage. Additional functionality can be enabled by writing a
   more complex driver (usually based on the simple implementation
   included by default in the core repository) and optionally compiling
   the relevant packages in. Typically, only a basic driver that
   addresses a device’s core functionality (covering ~90% of use cases)
   is included in the Mynewt core repository, thus keeping the footprint
   small.

-  The implementation should allow a user to be able to instantiate
   multiple devices of a certain kind. In the Mynewt environment the
   user can, for example, maintain separate contexts for multiple ADCs
   over different peripheral connections such as SPI, I2C etc. It is
   also possible for a user to use a single peripheral interface (e.g.
   SPI) to drive multiple devices (e.g. ADC), and in that case the
   device driver has to handle the proper synchronization of the various
   tasks.

-  Device drivers should be MCU independent. In Mynewt, device creation
   and operation functions are independent of the underlying MCU.
-  Device drivers should be able to offer high-level interfaces for
   generic operations common to a particular device group. An example of
   such a class or group of devices is a group for sensors with generic
   operations such as channel discovery, configure, and read values. The
   organization of the driver directory is work in progress - so we
   encourage you to hop on the dev@ mailing list and offer your
   insights!

-  Device drivers should be searchable. The plan is to have the newt
   tool offer a ``newt pkg search`` capability. This is work in
   progress. You are welcome to join the conversation on the dev@
   mailing list!

Example
~~~~~~~

The Mynewt core repo includes an example of a driver using the HAL to
provide extra functionality - the UART driver. It uses HAL GPIO and UART
to provide multiple serial ports on the NRF52 (but allowed on other
platforms too.)

The gist of the driver design is that there is an API for the driver
(for use by applications), and then sub-packages to that driver that
implement that driver API using the HAL and BSP APIs.

Implemented drivers
~~~~~~~~~~~~~~~~~~~

Drivers live under ``hw/drivers``. The current list of supported drivers
includes:

+----------------------------------------+--------------------------+
| Driver                                 | Description              |
+========================================+==========================+
| `adc <adc.html>`__                       | TODO: ADC driver.        |
+----------------------------------------+--------------------------+
| `flash <flash.html>`__                   | SPI/I2C flash drivers.   |
+----------------------------------------+--------------------------+
| `lwip <lwip.html>`__                     | TODO: LWIP.              |
+----------------------------------------+--------------------------+
| `mmc <mmc.html>`__                       | MMC/SD card driver.      |
+----------------------------------------+--------------------------+
| `nimble </network/ble/ble_intro/>`__   | NIMBLE.                  |
+----------------------------------------+--------------------------+
| `sensors <sensors.html>`__               | TODO: sensors.           |
+----------------------------------------+--------------------------+
| `uart <uart.html>`__                     | TODO: UART driver.       |
+----------------------------------------+--------------------------+
