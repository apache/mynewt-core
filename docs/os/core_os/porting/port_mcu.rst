Porting Mynewt to a new MCU
===========================

Porting Mynewt to a new MCU is not a difficult task if the core CPU
architectures is already supported.

The depth of work depends on the amount of HAL (Hardware Abstraction
Layer) support you need and provide in your port.

To get started:

-  Create a ``hw/mcu/mymcu`` directory where ``mymcu`` is the MCU you
   are porting to. Replace the name ``mymcu`` with a description of the
   MCU you are using.
-  Create a ``hw/mcu/mymcu/variant`` directory where the variant is the
   specific variant of the part you are usuing. Many MCU parts have
   variants with different capabilities (RAM, FLASH etc) or different
   pinouts. Replace ``variant`` with a description of the variant of the
   part you are using.
-  Create a ``hw/mcu/mymcu/variant/pkg.yml`` file. Copy from another mcu
   and fill out the relevant information
-  Create
   ``hw/mcu/mymcu/variant/include``, ``hw/mcu/mymcu/variant/include/mcu``,
   and ``hw/mcu/mymcu/variant/src`` directories to contain the code for
   your mcu.

At this point there are two main tasks to complete.

-  Implement any OS-specific code required by the OS
-  Implement the HAL functionality that you are looking for

Please contact the Mynewt development list for help and advice porting
to new MCU.
