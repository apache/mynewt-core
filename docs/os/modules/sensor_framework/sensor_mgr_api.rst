Sensor Manager API
------------------

The sensor manager API manages the sensors that are enabled in an
application. The API allows a sensor device to register itself with the
sensor manager and an application to look up the sensors that are
enabled. The sensor manager maintains a list of sensors, each
represented by a ``struct sensor`` object defined in the the `Sensor
API </os/modules/sensor_framework/sensor_api.html>`__.

Registering Sensors
~~~~~~~~~~~~~~~~~~~

When the BSP or the sensor creator package creates a sensor device in
the kernel, via the ``os_dev_create()`` function, the sensor device init
function must initialize a ``struct sensor`` object and call the
``sensor_mgr_register()`` function to register itself with the sensor
manager.

Looking Up Sensors
~~~~~~~~~~~~~~~~~~

An application uses the sensor manager API to look up the
``struct sensor`` object for a sensor. The `sensor
API </os/modules/sensor_framework/sensor_api.html>`__ and the `sensor
listener API </os/modules/sensor_framework/sensor_listener_api.html>`__
perform operations on a sensor object. The sensor manager API provides
the following sensor lookup functions:

-  ``sensor_mgr_find_next_bytype()``: This function returns the next
   sensor, on the sensors list, whose configured sensor types match one
   of the specified sensor types to search for. The sensor types to
   search are specified as a bit mask. You can use the function to
   search for the first sensor that matches or to iterate through the
   list and search for all sensors that match. If you are iterating
   through the list to find the next match, you must call the
   ``sensor_mgr_lock()`` function to lock the sensors list before you
   start the iteration and call the ``sensor_mgr_unlock()`` unlock the
   list when you are done. You do not need to lock the sensor list if
   you use the function to find the first match because the
   ``sensor_mgr_find_next_bytype()`` locks the sensors list before the
   search.

-  ``sensor_mgr_find_next_bydevname()``: This function returns the
   sensor that matches the specified device name.

   **Note:** There should only be one sensor that matches a specified
   device name even though the function name suggests that there can be
   multiple sensors with the same device name.

Checking Configured Sensor Types
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

An application may configure a sensor device to only support a subset of
supported sensor types. The ``sensor_mgr_match_bytype()`` function
allows an application to check whether a sensor is configured for one of
the specified sensor types. The type to check is a bit mask and can
include multiple types. The function returns a match when there is a
match for one, not all, of the specified types in the bit mask.

Polling Sensors
~~~~~~~~~~~~~~~

The sensor manager implements a poller that reads sensor data from
sensors at specified poll rates. If an application configures a sensor
to be polled, using the ``sensor_set_poll_rate_ms()`` function defined
in the `sensor API </os/modules/sensor_framework/sensor_api.html>`__, the
sensor manager poller will poll and read the configured sensor data from
the sensor at the specified interval.

The sensor manager poller uses an OS callout to set up a timer event to
poll the sensors, and uses the default OS event queue and the OS main
task to process timer events. The ``SENSOR_MGR_WAKEUP_RATE`` syscfg
setting specifies the default wakeup rate the sensor manager poller
wakes up to poll sensors. The sensor manager poller uses the poll rate
for a sensor if the sensor is configured for a higher poll rate than the
``SENSOR_MGR_WAKEUP_RATE`` setting value.

**Note:** An application needs to register a `sensor
listener </os/modules/sensor_framework/sensor_listener_api.html>`__ to
receive the sensor data that the sensor manager poller reads from a
sensor.

Data Structures
~~~~~~~~~~~~~~~

The sensor manager API uses the ``struct sensor`` and ``sensor_type_t``
types.

List of Functions:
~~~~~~~~~~~~~~~~~~

These are the functions defined by the sensor manager API. Please see
the
`sensor.h <https://github.com/apache/mynewt-core/blob/master/hw/sensor/include/sensor/sensor.h>`__
include file for details.

+------------+----------------+
| Function   | Description    |
+============+================+
| sensor\_mg | Returns the    |
| r\_find\_n | next sensor    |
| ext\_bynam | from the       |
| e          | sensor manager |
|            | list that      |
|            | matches the    |
|            | specified      |
|            | device name.   |
|            | An application |
|            | uses this      |
|            | function. You  |
|            | must call the  |
|            | sensor\_mgr\_l |
|            | ock()          |
|            | function to    |
|            | lock the       |
|            | sensor manager |
|            | list if you    |
|            | are iterating  |
|            | through the    |
|            | sensor manager |
|            | list.          |
+------------+----------------+
| sensor\_mg | Returns the    |
| r\_find\_n | next sensor    |
| ext\_bytyp | from the       |
| e          | sensor manager |
|            | list that      |
|            | matches one of |
|            | the specified  |
|            | sensor types.  |
|            | An application |
|            | uses this      |
|            | function.You   |
|            | must call the  |
|            | sensor\_mgr\_l |
|            | ock()          |
|            | function to    |
|            | lock the       |
|            | sensor manager |
|            | list if you    |
|            | are iterating  |
|            | through the    |
|            | sensor manager |
|            | list.          |
+------------+----------------+
| sensor\_mg | Locks the      |
| r\_lock    | sensor manager |
|            | list. An       |
|            | application    |
|            | uses this      |
|            | function.      |
+------------+----------------+
| sensor\_mg | Indicates      |
| r\_match\_ | whether a      |
| bytype     | given sensor   |
|            | is configured  |
|            | for one of the |
|            | specified      |
|            | sensor types.  |
|            | An application |
|            | uses this      |
|            | function.      |
|            | Note: The      |
|            | function takes |
|            | a pointer to a |
|            | variable with  |
|            | the bit mask   |
|            | of the types   |
|            | to match.      |
+------------+----------------+
| sensor\_mg | Registers a    |
| r\_registe | sensor object. |
| r          | A sensor       |
|            | device driver  |
|            | uses this      |
|            | function.      |
+------------+----------------+
| sensor\_mg | Unlocks the    |
| r\_unlock  | sensor manager |
|            | list. An       |
|            | application    |
|            | uses this      |
|            | function.      |
+------------+----------------+
