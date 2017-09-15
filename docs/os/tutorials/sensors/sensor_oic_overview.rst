Enabling OIC Sensor Data Monitoring
-----------------------------------

This tutorial shows you how to enable sensor data monitoring via the OIC
protocol over BLE transport in an application. It extends the example
application in the `Enabling an Off-Board Sensor in an Existing
Application Tutorial </os/tutorials/sensors/sensor_nrf52_bno055.html>`__
and assumes that you have worked through that tutorial.

This tutorial has two parts:

-  `Part 1 </os/tutorials/sensors/sensor_nrf52_bno055_oic.html>`__ shows
   you how to enable OIC sensor support in the sensors\_test
   application. The procedure only requires setting the appropriate
   syscfg setting values for the application target. The objective is to
   show you to quickly bring up the sensors\_test application and view
   the sensor data with the Mynewt Smart Device Controller app that is
   available for iOS and Android devices.

-  `Part 2 </os/tutorials/sensors/sensor_bleprph_oic.html>`__ shows you
   how to modify the bleprph\_oic application code to add OIC sensor
   support. The objective is to show you how to use the Sensor Framework
   API and OIC server API to develop an OIC over BLE sensor server
   application. ### Prerequisites Ensure that you meet the following
   prerequisites before continuing with the tutorials:

-  Complete the `Enabling an Off-Board Sensor in an Existing Application
   Tutorial </os/tutorials/sensors/sensor_nrf52_bno055.html>`__.
-  Install the Mynewt Smart Device Controller on an iOS or Android
   Device. See the `Sensor Tutorials
   Overview </os/tutorials/sensors/sensors.html>`__ on how to install the
   Mynewt Smart Device Controller app.

Overview of OIC Support in the Sensor Framework
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The sensor framework provides support for a sensor enabled application
to host the sensor devices as OIC resources. The sensor framework
provides the following OIC support:

-  Creates OIC resources for each sensor device that is enabled in the
   application. It creates an OIC discoverable and observable resource
   for each sensor type that the sensor device is configured for.
-  Processes CoAP GET requests for the sensor OIC resources. It reads
   the sensor data samples, encodes the data, and sends back a response.

The sensor package (``hw/sensor``) defines the following syscfg settings
for OIC support:

-  ``SENSOR_OIC``: This setting specifies whether to enable sensor OIC
   server support. The setting is enabled by default. The sensor package
   includes the ``net/oic`` package for the OIC support when this
   setting is enabled. The ``OC_SERVER`` syscfg setting that specifies
   whether to enable OIC server support in the ``net/oic`` package must
   also be enabled.
-  ``SENSOR_OIC_OBS_RATE``: Sets the OIC server observation rate.
