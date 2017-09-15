.. _tutorials:

Tutorials
##########

.. toctree::
    :maxdepth: 1

    arduino_zero_converted.rst
   

If the introduction to Mynewt has piqued your interest and you want to
familiarize yourself with some of its functionality, this series of
tutorials is for you. The lessons are aimed at the beginner.

The full list of tutorials can be seen in the navigation bar on the
left. New ones are being constantly added and will show up there
automatically.

Prerequisites:
~~~~~~~~~~~~~~

-  You have installed Docker container of Newt tool and toolchains or
   you have installed them natively on your machine
-  You have created a new project space (directory structure) and
   populated it with the core code repository (apache-mynewt-core) or
   know how to as explained in `Creating Your First
   Project <../get_started/project_create>`__.
-  You have at least one of the supported development boards:

   -  `Arduino Zero hardware <arduino_zero.html>`__
   -  `Olimex/STM32F407ZGT6 Cortex-M4 hardware <olimex.html>`__
   -  `nRF52 Development Kit from Nordic Semiconductor <nRF52.html>`__

The Nordic nrf52 developer kit supports Bluetooth Low Energy. We are
always looking to add new hardware to the list, so if you want to
develop the required Board Support Package (bsp) and/or Hardware
Abstraction Layer (HAL) for a new board, you can look
`here <../core_os/porting/port_os/>`__ to get started.

Tutorial categories:
~~~~~~~~~~~~~~~~~~~~

The tutorials fall into a few broad categories. Some examples in each
category are listed below.

-  Making an LED Blink (the "Hello World" equivalent in the electronics
   world)

   -  `Blinky on Arduino Zero hardware <arduino_zero.html>`__
   -  `Blinky on Olimex/STM32F407ZGT6 Cortex-M4 hardware <olimex.html>`__
   -  `Blinky on nRF52 Development Kit from Nordic
      Semiconductor <nRF52.html>`__ **Note:** This supports BLE.
   -  `Blinky on Redbear Nano2 <rbnano2.html>`__

-  Navigating the Code and Adding Functionality

   -  `Adding More Repositories to Your Project <repo/add_repos.html>`__
   -  `Adding a Unit Test For a Package <unit_test.html>`__

-  Using Newtmgr

   -  `Enabling Remote Communication With a Device Running Mynewt
      OS <project-slinky.html>`__

-  Bluetooth Low Energy

   -  `Building a Bare Bones BLE Application <ble_bare_bones.html>`__
   -  `Building a BLE iBeacon Application <ibeacon.html>`__

-  OS Fundamentals

   -  `Events and Event Queues <event_queue.html>`__
   -  `Task and Priority Management <tasks_lesson.html>`__

-  Remote Device Management

   -  `Enabling Newt Manager in Any App <add_newtmgr.html>`__
   -  `Upgrading an Image Over-The-Air <ota_upgrade_nrf52.html>`__

-  Sensors

   -  `Enabling an Off-Board Sensor in an Existing
      Application <sensors/sensor_nrf52_bno055.html>`__
   -  `Developing an Application for an Onboard
      Sensor <sensors/sensor_thingy_lis2dh12_onb.html>`__
   -  `Enabling OIC Sensor Data
      Monitoring <sensors/sensor_oic_overview.html>`__

-  Tooling

   -  `SEGGER RTT <segger_rtt.html>`__
   -  `SEGGER SystemView <segger_sysview.html>`__

**Send us an email on the dev@ mailing list if you have comments or
suggestions!** If you haven't joined the mailing list, you will find the
links `here <../../community.html>`__.

