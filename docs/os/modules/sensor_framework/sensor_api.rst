Sensor API
----------

The sensor API implements the sensor abstraction and the functions:

-  For a sensor device driver package to initialize a sensor object with
   the device specific information.

-  For an application to read sensor data from a sensor and to configure
   a sensor for polling.

A sensor is represented by the ``struct sensor`` object.

Sensor API Functions Used by a Sensor Device Driver Package
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A sensor device driver package must use the sensor API to initialize
device specific information for a sensor object and to change sensor
configuration types.

Initializing a Sensor Object
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

When the BSP or the sensor creator package creates an OS device for a
sensor named ``SENSORNAME``, it specifies the ``<sensorname>_init()``
callback function, that the device driver exports, for the
``os_dev_create()`` function to call to initialize the device. The
``<sensorname>_init()`` function must use the following sensor API
functions to set the driver and interface information in the sensor
object:

-  The ``sensor_init()`` function to initialize the ``struct sensor``
   object for the device.

-  The ``sensor_set_driver()`` function to set the types that the sensor
   device supports and the sensor driver functions to read the sensor
   data from the device and to retrieve the value type for a given
   sensor type.

-  The ``sensor_set_interface()`` function to set the interface to use
   to communicate with the sensor device.

**Notes**:

-  See the `Sensor Device
   Driver </os/modules/sensor_framework/sensor_driver.html>`__ page for
   the functions and data structures that a sensor driver package
   exports.

-  The ``<sensorname>_init()`` function must also call the
   ``sensor_mgr_register()`` function to register the sensor with the
   sensor manager. See the `Sensor Manager
   API </os/modules/sensor_framework/sensor_manager_api.html>`__ for
   details.

Setting the Configured Sensor Types
^^^^^^^^^^^^^^^^^^^


The BSP, or the sensor creator package, also calls the
``<sensorname>_config()`` function to configure the sensor device with
default values. The ``<sensorname>_config()`` function is exported by
the sensor device driver and must call the sensor API
``sensor_set_type_mask()`` function to set the configured sensor types
in the sensor object. The configured sensor types are a subset of the
sensor types that the device supports. The sensor framework must know
the sensor types that a sensor device is configured for because it only
reads sensor data for the configured sensor types.

**Note:** An application may also call the ``<sensorname>_config()``
function to configure the sensor device.

Sensor API Functions Used By an Application
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The sensor API provides the functions for an application to read sensor
data from a sensor and to configure a sensor for polling.

Reading Sensor Data
^^^^^^^^^^^^^^^^^^^

An application calls the ``sensor_read()`` function to read sensor data
from a sensor device. You specify a bit mask of the configured sensor
types to read from a sensor device and a callback function to call when
the sensor data is read. The callback is called for each specified
configured type and the data read for that sensor type is passed to the
callback.

Setting a Poll Rate for A Sensor
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The sensor manager implements a poller that reads sensor data from a
sensor at specified poll intervals. An application must call the
``sensor_set_poll_rate_ms()`` function to set the poll rate for a sensor
in order for poller to poll the sensor.

**Note:** An application needs to register a `sensor
listener </os/modules/sensor_framework/sensor_listener_api.html>`__ to
receive the sensor data that the sensor manager poller reads from a
sensor.

Data Structures
~~~~~~~~~~~~~~~

We list the main data structures that the sensor API uses and mention
things to note. For more details, see the
`sensor.h <https://github.com/apache/mynewt-core/blob/master/hw/sensor/include/sensor/sensor.h>`__
include file.

Sensor Object
^^^^^^^^^^^^^

The ``struct sensor`` data structure represents the sensor device. The
sensor API, the `sensor manager
API </os/modules/sensor_framework/sensor_mgr_api.html>`__, and the `sensor
listener API </os/modules/sensor_framework/sensor_listener_api.html>`__
all operate on the ``sensor`` object abstraction. A sensor is maintained
in the sensor manager global sensors list.

.. code:: c

    struct sensor {
        /* The OS device this sensor inherits from, this is typically a sensor
         * specific driver.
         */
        struct os_dev *s_dev;

        /* The lock for this sensor object */
        struct os_mutex s_lock;


        /* A bit mask describing the types of sensor objects available from this
         * sensor. If the bit corresponding to the sensor_type_t is set, then this
         * sensor supports that variable.
         */
        sensor_type_t s_types;

        /* Sensor mask of the configured sensor type s*/
        sensor_type_t s_mask;
        /**
         * Poll rate in MS for this sensor.
         */
        uint32_t s_poll_rate;

        /* The next time at which we want to poll data from this sensor */
        os_time_t s_next_run;

        /* Sensor driver specific functions, created by the device registering the
         * sensor.
         */
        struct sensor_driver *s_funcs;

        /* Sensor last reading timestamp */
        struct sensor_timestamp s_sts;

        /* Sensor interface structure */
        struct sensor_itf s_itf;

        /* A list of listeners that are registered to receive data off of this
         * sensor
         */
        SLIST_HEAD(, sensor_listener) s_listener_list;
        /* The next sensor in the global sensor list. */
        SLIST_ENTRY(sensor) s_next;
    };

**Note:** There are two fields, ``s_types`` and ``s_mask``, of type
``sensor_type_t``. The ``s_types`` field is a bit mask that specifies
the sensor types that the sensor device supports. The ``s_mask`` field
is a bit mask that specifies the sensor types that the sensor device is
configured for. Only sensor data for a configured sensor type can be
read.

Sensor Types
^^^^^^^^^^^^^^^^^^^


The ``sensor_type_t`` type is an enumeration of a bit mask of sensor
types, with each bit representing one sensor type. Here is an excerpt of
the enumeration values. See the
`sensor.h <https://github.com/apache/mynewt-core/blob/master/hw/sensor/include/sensor/sensor.h>`__
for details:

.. code:: c


    typedef enum {
     /* No sensor type, used for queries */
        SENSOR_TYPE_NONE                 = 0,
        /* Accelerometer functionality supported */
        SENSOR_TYPE_ACCELEROMETER        = (1 << 0),
        /* Magnetic field supported */
        SENSOR_TYPE_MAGNETIC_FIELD       = (1 << 1),
        /* Gyroscope supported */
        SENSOR_TYPE_GYROSCOPE            = (1 << 2),
        /* Light supported */
        SENSOR_TYPE_LIGHT                = (1 << 3),
        /* Temperature supported */
        SENSOR_TYPE_TEMPERATURE          = (1 << 4),

                    ....

         SENSOR_TYPE_USER_DEFINED_6       = (1 << 31),
        /* A selector, describes all sensors */
        SENSOR_TYPE_ALL                  = 0xFFFFFFFF

    } sensor_type_t;

Sensor Interface
^^^^^^^^^^^^^^^^

The ``struct sensor_itf`` data structure represents the interface the
sensor device driver uses to communicate with the sensor device.

.. code:: c

    struct sensor_itf {

        /* Sensor interface type */
        uint8_t si_type;

        /* Sensor interface number */
        uint8_t si_num;

        /* Sensor CS pin */
        uint8_t si_cs_pin;

        /* Sensor address */
        uint16_t si_addr;
    };

The ``si_cs_pin`` specifies the chip select pin and is optional. The
``si_type`` field must be of the following types:

.. code:: c


    #define SENSOR_ITF_SPI    (0)
    #define SENSOR_ITF_I2C    (1)
    #define SENSOR_ITF_UART   (2) 

Sensor Value Type
^^^^^^^^^^^^^^^^^^^


The ``struct sensor_cfg`` data structure represents the configuration
sensor type:

.. code:: c

    /**
     * Configuration structure, describing a specific sensor type off of
     * an existing sensor.
     */
    struct sensor_cfg {
        /* The value type for this sensor (e.g. SENSOR_VALUE_TYPE_INT32).
         * Used to describe the result format for the value corresponding
         * to a specific sensor type.
         */
        uint8_t sc_valtype;
        /* Reserved for future usage */
        uint8_t _reserved[3];
    };

Only the ``sc_valtype`` field is currently used and specifies the data
value type of the sensor data. The valid value types are:

.. code:: c


    /**
     * Opaque 32-bit value, must understand underlying sensor type
     * format in order to interpret.
     */
    #define SENSOR_VALUE_TYPE_OPAQUE (0)
    /**
     * 32-bit signed integer
     */
    #define SENSOR_VALUE_TYPE_INT32  (1)
    /**
     * 32-bit floating point
     */
    #define SENSOR_VALUE_TYPE_FLOAT  (2)
    /**
     * 32-bit integer triplet.
     */
    #define SENSOR_VALUE_TYPE_INT32_TRIPLET (3)
    /**
     * 32-bit floating point number triplet.
     */
    #define SENSOR_VALUE_TYPE_FLOAT_TRIPLET (4)

Sensor Driver Functions
^^^^^^^^^^^^^^^^^^^


The ``struct sensor_device`` data structure represents the device driver
functions. The sensor device driver must implement the functions and set
up the function pointers.

::

    struct sensor_driver {
        sensor_read_func_t sd_read;
        sensor_get_config_func_t sd_get_config;
    };

List of Functions:
~~~~~~~~~~~~~~~


These are the functions defined by the sensor API. Please see the
`sensor.h <https://github.com/apache/mynewt-core/blob/master/hw/sensor/include/sensor/sensor.h>`__
include file for details.

+------------+----------------+
| Function   | Description    |
+============+================+
| sensor\_in | Initializes a  |
| it         | sensor. A      |
|            | sensor device  |
|            | driver uses    |
|            | this function. |
+------------+----------------+
| sensor\_se | Sets the       |
| t\_driver  | sensor types   |
|            | that the       |
|            | sensor device  |
|            | supports, and  |
|            | the driver     |
|            | functions to   |
|            | read data and  |
|            | to get value   |
|            | type for a     |
|            | sensor type. A |
|            | sensor device  |
|            | driver uses    |
|            | this function. |
+------------+----------------+
| sensor\_se | Sets the       |
| t\_interfa | sensor         |
| ce         | interface to   |
|            | use to         |
|            | communicate    |
|            | with the       |
|            | sensor device. |
|            | A sensor       |
|            | device driver  |
|            | uses this      |
|            | function.      |
+------------+----------------+
| sensor\_se | Specifies the  |
| t\_type\_m | sensor types   |
| ask        | that a sensor  |
|            | device is      |
|            | configured     |
|            | for. A sensor  |
|            | device driver  |
|            | uses this      |
|            | function.      |
+------------+----------------+
| sensor\_re | Reads sensor   |
| ad         | data for the   |
|            | specified      |
|            | sensor types.  |
|            | An application |
|            | uses this      |
|            | function.      |
+------------+----------------+
| sensor\_se | Sets poll rate |
| t\_poll\_r | for the sensor |
| ate\_ms    | manager to     |
|            | poll the       |
|            | sensor device. |
|            | An application |
|            | uses this      |
|            | function.      |
+------------+----------------+
| sensor\_lo | Locks the      |
| ck         | sensor object  |
|            | for exclusive  |
|            | access.        |
+------------+----------------+
| sensor\_un | Unlocks the    |
| lock       | sensor object. |
+------------+----------------+
| SENSOR\_GE | Macro that the |
| T\_DEV     | sensor device  |
|            | driver uses to |
|            | retrieve the   |
|            | os\_dev from   |
|            | the sensor     |
|            | object.        |
+------------+----------------+
| SENSOR\_GE | Macro that the |
| T\_ITF     | sensor device  |
|            | driver uses to |
|            | retrieve the   |
|            | sensor\_itf    |
|            | from the       |
|            | sensor object. |
+------------+----------------+
