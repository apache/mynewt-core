Mynewt Sensor Framework Overview
================================

The Mynewt sensor framework is an abstraction layer between an
application and sensor devices. The sensor framework provides the
following support:

-  A set of APIs that allows developers to develop sensor device drivers
   within a common framework and enables application developers to
   develop applications that can access sensor data from any Mynewt
   sensor device using a common interface.

-  Support for onboard and off-board sensors.

-  An OIC sensor server that exposes sensors as OIC resources and
   handles OIC CoAP requests for the sensor resources. A developer can
   easily develop a MyNewt OIC sensor enabled application that serves
   sensor resource requests from OIC client applications.

-  A sensor shell command and optional sensor device driver shell
   commands to view sensor data and manage sensor device settings for
   testing and debugging sensor enabled applications and sensor device
   drivers.

|Alt Layout - Sensor Framework|

Overview of Sensor Support Packages
-----------------------------------

In this guide and the package source code, we use ``SENSORNAME`` (all
uppercase) to refer to a sensor product name. For each sensor named
``SENSORNAME``:

-  The package name for the sensor device driver is ``<sensorname>``.
   All functions and data structures that the sensor device driver
   exports are prefixed with ``<sensorname>``.

-  All syscfg settings defined for the sensor are prefixed with
   ``<SENSORNAME>``. For example:

   -  The ``<SENSORNAME>_CLI`` syscfg setting defined in the device
      driver package to specify whether to enable the sensor device
      shell command.
   -  The ``<SENSORNAME>_ONB`` syscfg setting defined in the BSP to
      specify whether the onboard sensor is enabled.
   -  The ``<SENSORNAME>_OFB`` syscfg setting defined in the sensor
      creator package to specify whether to enable the off-board sensor.

The following Mynewt packages provide sensor support and are needed to
build a sensor enabled application.

-  ``hw/sensor``: The sensor framework package. This package implements
   the `sensor framework
   APIs </os/modules/sensor_framework/sensor_api.html>`__, the `OIC sensor
   server </os/modules/sensor_framework/sensor_oic.html>`__, and the
   `sensor shell
   command </os/modules/sensor_framework/sensor_shell.html>`__.

-  ``hw/sensor/creator``: The sensor creator package. This package
   supports off-board sensor devices. It creates the OS devices for the
   sensor devices that are enabled in an application and configures the
   sensor devices with default values. See the `Creating and Configuring
   a Sensor Device </os/modules/sensor_framework/sensor_create.html>`__
   page for more information.

   **Note:** This package is only needed if you are building an
   application with off-board sensors enabled.

-  ``hw/bsp/``: The BSP for boards that support onboard sensors. The BSP
   creates the OS devices for the onboard sensors that the board
   supports and configures the sensors with default values. See the
   `Creating and Configuring a Sensor
   Device </os/modules/sensor_framework/sensor_create.html>`__ page for
   more information.

-  ``hw/drivers/sensors/*``: These are the sensor device driver
   packages. The ``hw/drivers/sensors/<sensorname>`` package is the
   device driver for a sensor named ``SENSORNAME``. See the `Sensor
   Device Driver </os/modules/sensor_framework/sensor_driver.html>`__ page
   for more information.

.. |Alt Layout - Sensor Framework| image:: /os/modules/sensor_framework/sensor_framework.png

