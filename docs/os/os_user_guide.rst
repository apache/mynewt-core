OS User Guide
=============

.. toctree::
   :hidden:

   Kernel <core_os/mynewt_os>
   System <modules/system_modules>
   Hardware Abstraction <modules/hal/hal>
   Secure Bootloader <modules/bootloader/bootloader>
   Split Images <modules/split/split>
   Porting Guide <core_os/porting/port_os>
   Baselibc <modules/baselibc>
   Drivers <modules/drivers/driver>
   Device Management with Newt Manager <modules/devmgmt/newtmgr>
   Device Management with MCUmgr <modules/mcumgr/mcumgr>
   Image Manager <modules/imgmgr/imgmgr>
   Compile-Time Configuration <modules/sysinitconfig/sysinitconfig>
   File System <modules/fs/fs>
   Flash Circular Buffer <modules/fcb/fcb>
   Sensor Framework <modules/sensor_framework/sensor_framework>
   Test Utilities <modules/testutil/testutil>
   JSON <modules/json/json>
   Manufacturing support <modules/mfg/mfg>

This guide provides comprehensive information about Mynewt OS, the
real-time operating system for embedded systems. It is intended both for
an embedded real-time software developer as well as higher-level
applications developer wanting to understand the features and benefits
of the system. Mynewt OS is highly composable and flexible. Only those
features actually used by the application are compiled into the run-time
image. Hence, the actual size of Mynewt is completely determined by the
application. The guide covers all the features and services available in
the OS and contains several examples showing how they can be used.
**Send us an email on the dev@ mailing list if you have comments or
suggestions!** If you haven't joined the mailing list, you will find the
links `here <https://mynewt.apache.org/community/>`_.
