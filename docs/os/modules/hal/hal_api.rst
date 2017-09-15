HAL Interfaces
==============

The HAL supports separate interfaces for many peripherals. A brief
description of the interfaces are shown below.

+-----------------+-------------------------+---------------------+
| **Hal Name**    | \*\* Interface File     | **Description **    |
|                 | \*\*                    |                     |
+=================+=========================+=====================+
| `bsp <hal_bsp/h | `hal\_bsp.h <https://gi | An hardware         |
| al_bsp.html>`__   | thub.com/apache/incubat | independent         |
|                 | or-mynewt-core/blob/dev | interface to        |
|                 | elop/hw/hal/include/hal | identify and access |
|                 | /hal_bsp.h>`__          | underlying BSP.     |
+-----------------+-------------------------+---------------------+
| `flash api for  | `hal\_flash.h <https:// | A blocking          |
| apps to         | github.com/apache/incub | interface to access |
| use <hal_flash/ | ator-mynewt-core/blob/d | flash memory.       |
| hal_flash.html>`_ | evelop/hw/hal/include/h |                     |
| _               | al/hal_flash.h>`__      |                     |
+-----------------+-------------------------+---------------------+
| `flash api for  | `flash\_map.h <https:// | An interface to     |
| drivers to      | github.com/apache/incub | query information   |
| implement <hal_ | ator-mynewt-core/blob/d | about the flash map |
| flash/hal_flash | evelop/hw/hal/include/h | (regions and        |
| _int.html>`__     | al/hal_flash_int.h>`__  | sectors)            |
+-----------------+-------------------------+---------------------+
| `gpio <hal_gpio | `hal\_gpio.h <https://g | An interface for    |
| /hal_gpio.html>`_ | ithub.com/apache/incuba | manipulating        |
| _               | tor-mynewt-core/blob/de | General Purpose     |
|                 | velop/hw/hal/include/ha | Inputs and Outputs. |
|                 | l/hal_gpio.h>`__        |                     |
+-----------------+-------------------------+---------------------+
| `i2c <hal_i2c/h | `hal\_i2c.h <https://gi | An interface for    |
| al_i2c.html>`__   | thub.com/apache/incubat | controlling         |
|                 | or-mynewt-core/blob/dev | Inter-Integrated-Ci |
|                 | elop/hw/hal/include/hal | rcuit               |
|                 | /hal_i2c.h>`__          | (i2c) devices.      |
+-----------------+-------------------------+---------------------+
| `OS             | `hal\_os\_tick.h <https | An interface to set |
| tick <hal_os_ti | ://github.com/apache/in | up interrupt timers |
| ck/hal_os_tick. | cubator-mynewt-core/blo | or halt CPU in      |
| md>`__          | b/develop/hw/hal/includ | terms of OS ticks.  |
|                 | e/hal/hal_os_tick.h>`__ |                     |
+-----------------+-------------------------+---------------------+
| `spi <hal_spi/h | `hal\_spi.h <https://gi | An interface for    |
| al_spi.html>`__   | thub.com/apache/incubat | controlling Serial  |
|                 | or-mynewt-core/blob/dev | Peripheral          |
|                 | elop/hw/hal/include/hal | Interface (SPI)     |
|                 | /hal_spi.h>`__          | devices.            |
+-----------------+-------------------------+---------------------+
| `system <hal_sy | `hal\_system.h <https:/ | An interface for    |
| stem/hal_sys.md | /github.com/apache/incu | starting and        |
| >`__            | bator-mynewt-core/blob/ | resetting the       |
|                 | develop/hw/hal/include/ | system.             |
|                 | hal/hal_system.h>`__    |                     |
+-----------------+-------------------------+---------------------+
| `timer <hal_tim | `hal\_cputime.h <https: | An interface for    |
| er/hal_timer.md | //github.com/apache/inc | configuring and     |
| >`__            | ubator-mynewt-core/tree | running HW timers   |
|                 | /develop/hw/hal/include |                     |
|                 | /hal/hal_timer.h>`__    |                     |
+-----------------+-------------------------+---------------------+
| `uart <hal_uart | `hal\_uart.h <https://g | An interface for    |
| /hal_uart.html>`_ | ithub.com/apache/incuba | communicating via   |
| _               | tor-mynewt-core/blob/de | asynchronous serial |
|                 | velop/hw/hal/include/ha | interface.          |
|                 | l/hal_uart.h>`__        |                     |
+-----------------+-------------------------+---------------------+
| `watchdog <hal_ | `hal\_watchdog.h <https | An interface for    |
| watchdog/hal_wa | ://github.com/apache/in | enabling internal   |
| tchdog.html>`__   | cubator-mynewt-core/blo | hardware watchdogs. |
|                 | b/develop/hw/hal/includ |                     |
|                 | e/hal/hal_watchdog.h>`_ |                     |
|                 | _                       |                     |
+-----------------+-------------------------+---------------------+
