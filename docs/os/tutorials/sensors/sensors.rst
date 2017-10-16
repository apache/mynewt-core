Sensor Tutorials Overview
-------------------------

This set of sensor tutorials allows you to explore the Mynewt Sensor
Framework features and learn how to develop sensor-enabled Mynewt
applications.

The Mynewt Sensor framework supports:

-  Onboard and off-board sensors.
-  Retrieving sensor data and controlling sensor devices via the Mynewt
   OS Shell.
-  Retrieving sensor data over the OIC protocol and BLE transport.

Available Tutorials
~~~~~~~~~~~~~~~~~~~

The tutorials are:

-  `Enabling an Off-Board Sensor in an Existing
   Application </os/tutorials/sensors/sensor_nrf52_bno055.html>`__ - This
   is an introductory tutorial that shows you to how to quickly bring up
   a sensor enabled application that retrieves data from a sensor
   device. We recommend that you work through this tutorial before
   trying one of the other tutorials.

-  `Changing the Default Configuration for a
   Sensor </os/tutorials/sensors/sensor_offboard_config.html>`__ - This
   tutorial shows you how to change the default configuration values for
   a sensor.

-  `Developing an Application for an Onboard
   Sensor </os/tutorials/sensors/sensor_thingy_lis2dh12_onb.html>`__ -
   This tutorial shows you how to develop a simple application for a
   device with an onboard sensor.

-  `Enabling OIC Sensor Data Monitoring in an Existing
   Application </os/tutorials/sensors/sensor_oic_overview.html>`__ - This
   tutorial shows you how to enable support for sensor data monitoring
   via OIC in an existing application.

Mynewt Smart Device Controller OIC App
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

We use the ``Mynewt Sensor Monitor`` App on iOS or Android to retrieve
and display sensor data from the Mynewt OS OIC sensor applications
described in the OIC Sensor Data Monitoring tutorials. You can download
the app from either the Apple Store or Google Play Store.

**Note:** At the time of writing this tutorial, the iOS app was still in
the queue waiting to be placed in the App Store. You can build the iOS
app from source as indicated below.

If you would like to contribute or modify the Mynewt Smart Device
Controller App, see the `Android Sensor
source <https://github.com/runtimeco/android_sensor>`__ and `iOS Sensor
source <https://github.com/runtimeco/iOS_oic>`__ on github.

 ### Prerequisites Ensure that you meet the following prerequisites
before continuing with one of the tutorials.

-  Have Internet connectivity to fetch remote Mynewt components.
-  Have a computer to build a Mynewt application and connect to the
   board over USB.
-  Have a Micro-USB cable to connect the board and the computer.
-  Install the newt tool and toolchains (See `Basic
   Setup </os/get_started/get_started.html>`__).
-  Read the Mynewt OS `Concepts </os/get_started/vocabulary.html>`__
   section.
-  Create a project space (directory structure) and populate it with the
   core code repository (apache-mynewt-core) explained in `Creating Your
   First Project </os/get_started/project_create>`__.
-  Work through one of the `Blinky
   Tutorials </os/tutorials/blinky.html>`__.
