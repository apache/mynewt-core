GPIO
=========

This is the hardware independent GPIO (General Purpose Input Output)
Interface for Mynewt.

Description
~~~~~~~~~~~

Contains the basic operations to set and read General Purpose Digital
I/O Pins within a Mynewt system.

Individual GPIOs are referenced in the APIs as ``pins``. However, in
this interface the ``pins`` are virtual GPIO pins. The MCU header file
maps these virtual ``pins`` to the physical GPIO ports and pins.

Typically, the BSP code may define named I/O pins in terms of these
virtual ``pins`` to describe the devices attached to the physical pins.

Here's a brief example so you can get the gist of the translation.

Suppose my product uses the stm32F4xx processor. There already exists
support for this processor within Mynewt. The processor has N ports
(A,B,C..) of 16 GPIO pins per port. The MCU hal\_gpio driver maps these
to a set of virtual pins 0-N where port A maps to 0-15, Port B maps to
16-31, Port C maps to 32-47 and so on. The exact number of physical port
(and virtual port pins) depends on the specific variant of the
stm32F4xx.

So if I want to turn on port B pin 3, that would be virtual pin 1\*16 +
3 = 19. This translation is defined in the MCU implementation of
`hal\_gpio.c <https://github.com/apache/incubator-mynewt-core/blob/master/hw/mcu/stm/stm32f4xx/src/hal_gpio.c>`__
for the stmf32F4xx. Each MCU will typically have a different translation
method depending on its GPIO architecture.

Now, when writing a BSP, it's common to give names to the relevant port
pins that you are using. Thus, the BSP may define a mapping between a
function and a virtual port pin in the ``bsp.h`` header file for the
BSP. For example,

.. code-block:: console

    #define SYSTEM_LED              (37)
    #define FLASH_SPI_CHIP_SELECT   (3)

would map the system indicator LED to virtual pin 37 which on the
stm32F4xx would be Port C pin 5 and the chip select line for the
external SPI flash to virtual pin 3 which on the stm32F4xxis port A pin
3.

Said another way, in this specific system we get

.. code-block:: console

    SYSTEM_LED --> hal_gpio virtual pin 37 --> port C pin 5 on the stm34F4xx

API
~~~~~~~~~~

.. doxygengroup:: HALGpio
    :content-only:
    :members:


