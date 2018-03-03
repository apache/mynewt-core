Hardware Abstraction Layer
==========================

.. toctree::
    :hidden:

    hal_timer/hal_timer
    hal_gpio/hal_gpio
    hal_uart/hal_uart
    hal_spi/hal_spi
    hal_i2c/hal_i2c
    hal_flash/hal_flash
    hal_system/hal_system
    hal_watchdog/hal_watchdog
    hal_bsp/hal_bsp

Description
~~~~~~~~~~~

The Hardware Abstraction Layer (HAL) in Mynewt is a low-level, base
peripheral abstraction. HAL provides a core set of services that is
implemented for each MCU supported by Mynewt. Device drivers are
typically the software libraries that initialize the hardware and manage
access to the hardware by higher layers of software. In the Mynewt OS,
the layers can be depicted in the following manner.

::

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
   device. The drivers are the ones that register with the kernel’s
   power management APIs, and manage turning on and off peripherals and
   external chipsets, etc.

General design principles
~~~~~~~~~~~~~~~~~~~~~~~~~

-  The HAL API should be simple. It should be as easy to implement for
   hardware as possible. A simple HAL API makes it easy to bring up new
   MCUs quickly.

-  The HAL API should portable across all the various MCUs supported in
   Mynewt (e.g. Nordic's nRF51, Nordic's nRF52, NXP's MK64F12 etc.).

Example
~~~~~~~

A Mynewt contributor might write a light-switch driver that provides the
functionality of an intelligent light switch. This might involve using a
timer, a General Purpose Output (GPO) to set the light to the on or off
state, and flash memory to log the times the lights were turned on or
off. The contributor would like this package to work with as many
different hardware platforms as possible, but can't possibly test across
the complete set of hardware supported by Mynewt.

**Solution**: The contributor uses the HAL APIs to control the
peripherals. The Mynewt team ensures that the underlying HAL devices all
work equivalently through the HAL APIs. The contributors library is
independent of the specifics of the hardware.

Dependency
~~~~~~~~~~

To include the HAL within your project, simply add it to your package
dependencies as follows:

.. code-block:: console

    pkg.deps:
        . . .
        hw/hal

Platform Support
~~~~~~~~~~~~~~~~

Not all platforms (MCU and BSP) support all HAL devices. Consult your
MCU or BSP documentation to find out if you have hardware support for
the peripherals you are interested in using. Once you verify support,
then consult the MCU implementation and see if the specific HAL
interface (xxxx) you are using is in the
``mcu/<mcu-name>/src/hal_xxxx.c`` implementation. Finally, you can build
your project and ensure that there are no unresolved hal\_xxxx
externals.
