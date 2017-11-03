Sensor Shell Command
--------------------

The sensor framework, ``hw/sensor``, package implements a ``sensor``
shell command. The command allows a user to view the sensors that are
enabled in an application and to read the sensor data from the sensors.
See the `Enabling an Off-Board Sensor in an Existing Application
Tutorial </os/tutorials/sensors/sensor_nrf52_bno055.html>`__

The package defines the ``SENSOR_CLI`` syscfg setting to specify whether
to enable to ``sensor`` shell command and enables the setting by
default.
