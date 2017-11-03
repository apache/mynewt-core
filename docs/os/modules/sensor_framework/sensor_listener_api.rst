Sensor Listener API
-------------------

The sensor listener API allows an application to register listeners for
sensors and get notified whenever sensor data are read from the sensor
devices. An application calls the ``sensor_register_listener()``
function to register a listener that specifies the callback function and
the types of sensor data to listen for from a sensor device.

When the ``sensor_read()`` function defined in the `sensor
API </os/modules/sensor_framework/sensor_api.html>`__ is called to read
the sensor data for the specified sensor types from a sensor, the
``sensor_read()`` function calls the listener callback, passing it the
sensor data that is read from the sensor.

An application calls the ``sensor_unregister_listener()`` function to
unregister a listener if it no longer needs notifications when data is
read from a sensor.

An application can use listeners in conjunction with the sensor manager
poller. An application can configure a polling interval for a sensor and
register a listener for the sensor types it wants to listen for from the
sensor. When the sensor manager poller reads the sensor data from the
sensor at each polling interval, the listener callback is called with
the sensor data passed to it.

An application can also use listeners for other purposes. For example,
an application that uses the OIC sensor server may want to register
listeners. The OIC sensor server handles all the OIC requests for the
sensor resources and an application does not know about the OIC
requests. An application can install a listener if it wants to know
about a request for a sensor resource or use the sensor data that the
OIC sensor server reads from the sensor.

Data Structures
~~~~~~~~~~~~~~~

The ``struct sensor_listener`` data structure represents a listener. You
must initialize a listener structure with a bit mask of the sensor types
to listen for from a sensor, a callback function that is called when
sensor data is read for one of the sensor types, and an opaque argument
to pass to the callback before you call the
``sensor_register_listener()`` function to register a listener.

.. code:: c


    struct sensor_listener {
    {
        /* The type of sensor data to listen for, this is interpreted as a
         * mask, and this listener is called for all sensor types on this
         * sensor that match the mask.
         */
        sensor_type_t sl_sensor_type;

        /* Sensor data handler function, called when has data */
        sensor_data_func_t sl_func;

        /* Argument for the sensor listener */
        void *sl_arg;

        /* Next item in the sensor listener list.  The head of this list is
         * contained within the sensor object.
         */
        SLIST_ENTRY(sensor_listener) sl_next;
    };

List of Functions:
~~~~~~~~~~~~~~~~~~

These are the functions defined by the sensor listener API. Please see
the
`sensor.h <https://github.com/apache/mynewt-core/blob/master/hw/sensor/include/sensor/sensor.h>`__
include file for details.

+------------+----------------+
| Function   | Description    |
+============+================+
| sensor\_re | Registers the  |
| gister\_li | specified      |
| stener     | listener for a |
|            | given sensor.  |
+------------+----------------+
| sensor\_un | Unregisters    |
| register\_ | the specified  |
| listener   | listener for a |
|            | given sensor.  |
+------------+----------------+
