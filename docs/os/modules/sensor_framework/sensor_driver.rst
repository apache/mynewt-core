Sensor Device Driver
--------------------

A Mynewt sensor device driver uses the sensor framework abstraction and
API to enable applications to access sensor data from any Mynewt sensor
device using a common interface. The sensor device driver must also use
the Mynewt HAL interface to communicate with and control a sensor
device.

This guide describes what a sensor device driver must implement to
enable a sensor device within the sensor framework. For information on
using the HAL API to communicate with a sensor device, see the `Hardware
Layer Abstraction Guide </os/modules/hal/hal.html>`__.

The ``hw/drivers/sensors/<sensorname>`` package implements the device
driver for the sensor named ``SENSORNAME``.

**Note:** All example excerpts are from the BNO055 sensor device driver
package.

 ### Initializing and Configuring a Sensor Device

A driver package for a sensor named ``SENSORNAME`` must define and
export the following data structures and functions to initialize and
configure a device:

-  ``struct <sensorname>``: This data structure represents a sensor
   device. The structure must include a ``dev`` field of type
   ``struct os_dev`` and a ``sensor`` field of type ``struct sensor``.
   For example:

   ::

       struct bno055 {
           struct os_dev dev;
           struct sensor sensor;
           struct bno055_cfg cfg;
           os_time_t last_read_time;
       };

-  ``struct <sensorname>_cfg``: This data structure defines the
   configuration for a sensor device. The structure fields are specific
   to the device. This is the data structure that a BSP, the sensor
   creator package, or an application sets to configure a sensor device.
   For example:

   ::

       struct bno055_cfg {
           uint8_t bc_opr_mode;
           uint8_t bc_pwr_mode;
           uint8_t bc_units;
           uint8_t bc_placement;
           uint8_t bc_acc_range;
           uint8_t bc_acc_bw;

                ...

           uint32_t bc_mask;
       };

-  ``<sensorname>_init()``: This is the os device initialization
   callback of type
   ``int (*os_dev_init_func_t)(struct os_dev *, void *)`` that the
   ``os_dev_create()`` function calls to initialize the device. For
   example, the bno055 device driver package defines the following
   function:

   ::

       int bno055_init(struct os_dev *dev, void *arg)

   The BSP, which creates a device for an onboard sensor, and the sensor
   creator package, which creates a device for an off-board sensor,
   calls the ``os_dev_create()`` function and passes:

   -  A pointer to a ``struct <sensorname>`` variable.
   -  The ``<sensorname>_init()`` function pointer.
   -  A pointer to a ``struct sensor_itf`` variable that specifies the
      interface the driver uses to communicate with the sensor device.

   See the `Creating Sensor
   Devices </os/modules/sensor_framework/sensor_create.html>`__ page for
   more details.

   The ``os_dev_create()`` function calls the ``<sensorname>_init()``
   function with a pointer to the ``struct <sensorname>`` for the
   ``dev`` parameter and a pointer to the ``struct sensor_itf`` for the
   ``arg`` parameter.

-  ``<sensorname>_config()``: This is the sensor configuration function
   that the BSP, sensor creator package, or an application calls to
   configure the sensor device. For example:

   ::

       int bno055_config(struct bno055 *bno055, struct bno055_cfg *cfg)

Defining Functions to Read Sensor Data and Get Sensor Value Type
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A device driver must implement the following functions that the sensor
API uses to read sensor data and to get the configuration value type for
a sensor:

-  A function of type
   ``int (*sensor_read_func_t)(struct sensor *, sensor_type_t, sensor_data_func_t, void *, uint32_t)``
   that the sensor framework uses to read a single value from a sensor
   for the specified sensor types. The device driver must implement this
   function such that it reads, for each sensor type set in the bit
   mask, a single value from the sensor and calls the
   ``sensor_data_funct_t`` callback with the opaque callback argument ,
   the sensor data, and the sensor type.

-  A function of type
   ``int (*sensor_get_config_func_t)(struct sensor *, sensor_type_t, struct sensor_cfg *)``
   that returns the value type for the specified sensor type. For
   example, the value type for a ``SENSOR_VALUE_TYPE_TEMPERATURE``
   sensor might be ``SENSOR_VALUE_TYPE_FLOAT`` and the value type for a
   ``SENSOR_TYPE_ACCELEROMETER`` sensor might be
   ``SENSOR_VALUE_TYPE_FLOAT_TRIPLET``.

The driver initializes a ``sensor_driver`` structure, shown below, with
the pointers to these functions:

.. code:: c


    struct sensor_driver {
        sensor_read_func_t sd_read;
        sensor_get_config_func_t sd_get_config;
    };

For example:

.. code:: c


    static int bno055_sensor_read(struct sensor *, sensor_type_t,
            sensor_data_func_t, void *, uint32_t);
    static int bno055_sensor_get_config(struct sensor *, sensor_type_t,
            struct sensor_cfg *);

    static const struct sensor_driver g_bno055_sensor_driver = {
        bno055_sensor_read,
        bno055_sensor_get_config
    };

 ### Registering the Sensor in the Sensor Framework

The device driver must initialize and register a ``struct sensor``
object with the sensor manager. See the `Sensor
API </os/modules/sensor_framework/sensor_api.html>`__ and the `Sensor
Manager API </os/modules/sensor_framework/sensor_manager_api.html>`__
pages for more details.

The device driver ``<sensorname>_init()`` function initializes and
registers a sensor object as follows:

-  Calls the ``sensor_init()`` function to initialize the
   ``struct sensor`` object.

-  Calls the ``sensor_set_driver()`` function to specify the sensor
   types that the sensor device supports, and the pointer to the
   ``struct sensor_driver`` variable that specifies the driver functions
   to read the sensor data and to get the value type for a sensor.

-  Calls the ``sensor_set_interface()`` function to set the interface
   that the device driver uses to communicate with the sensor device.
   The BSP, or sensor creator package for an off-board sensors, sets up
   the ``sensor_itf`` and passes it to the ``<sensorname>_init()``
   function. The ``sensor_set_interface()`` functions saves this
   information in the sensor object. The device driver uses the
   ``SENSOR_GET_ITF()`` macro to retrieve the sensor\_itf when it needs
   to communicate with the sensor device.

-  Calls the ``sensor_mgr_register()`` function to register the sensor
   with the sensor manager.

For example:

\`\`\`hl\_lines="13 20 25 31 32 33 34 35 41 46"

int bno055\_init(struct os\_dev *dev, void *\ arg) { struct bno055
*bno055; struct sensor *\ sensor; int rc;

::

    if (!arg || !dev) {
        rc = SYS_ENODEV;
        goto err;
    }

    bno055 = (struct bno055 *) dev;

    rc = bno055_default_cfg(&bno055->cfg);
    if (rc) {
        goto err;
    }

    sensor = &bno055->sensor;

    /* Code to setup logging and stats may go here */
    .... 

    rc = sensor_init(sensor, dev);
    if (rc != 0) {
        goto err;
    }

    /* Add the accelerometer/magnetometer driver */
    rc = sensor_set_driver(sensor, SENSOR_TYPE_ACCELEROMETER         |
            SENSOR_TYPE_MAGNETIC_FIELD | SENSOR_TYPE_GYROSCOPE       |
            SENSOR_TYPE_TEMPERATURE    | SENSOR_TYPE_ROTATION_VECTOR |
            SENSOR_TYPE_GRAVITY        | SENSOR_TYPE_LINEAR_ACCEL    |
            SENSOR_TYPE_EULER, (struct sensor_driver *) &g_bno055_sensor_driver);
    if (rc != 0) {
        goto err;
    }

    /* Set the interface */
    rc = sensor_set_interface(sensor, arg);
    if (rc) {
        goto err;
    }

    rc = sensor_mgr_register(sensor);
    if (rc != 0) {
        goto err;
    }

    return (0);

err: return (rc); }

\`\`\`

Configuring the Sensor Device and Setting the Configured Sensor Types
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

After the BSP, or the sensor creator package for an off-board sensor,
creates the OS device for a sensor, it calls the
``<sensorname>_config()`` function to configure sensor device
information such as mode, power mode, and to set the configured sensor
types. The ``<sensorname>_config()`` function configures the settings on
the sensor device. It must also call the ``sensor_set_type_mask()``
function to set the configured sensor types in the sensor object. The
configured sensor types are a subset of the sensor types that the sensor
device supports and the sensor framework only reads sensor data for
configured sensor types.

**Notes:**

-  The device driver uses the ``SENSOR_GET_ITF()`` macro to retrieve the
   sensor interface to communicate with the sensor.

-  If a sensor device has a chip ID that can be queried, we recommend
   that the device driver read and verify the chip ID with the data
   sheet.

-  An application may call the ``<sensorname>_config()`` function to
   configure the sensor device.

For example:

\`\`\`hl\_lines="7 9 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27
28 29 37 38 39 40 41 42"

int bno055\_config(struct bno055 *bno055, struct bno055\_cfg *\ cfg) {
int rc; uint8\_t id; uint8\_t mode; struct sensor\_itf \*itf;

::

    itf = SENSOR_GET_ITF(&(bno055->sensor));

    /* Check if we can read the chip address */
    rc = bno055_get_chip_id(itf, &id);
    if (rc) {
        goto err;
    }

    if (id != BNO055_ID) {
        os_time_delay((OS_TICKS_PER_SEC * 100)/1000 + 1);

        rc = bno055_get_chip_id(itf, &id);
        if (rc) {
            goto err;
        }

        if(id != BNO055_ID) {
            rc = SYS_EINVAL;
            goto err;
        }
    }

           ....

    /* Other code to set the configuration on the sensor device. */

           .... 

    rc = sensor_set_type_mask(&(bno055->sensor), cfg->bc_mask);
    if (rc) {
        goto err;
    }

    bno055->cfg.bc_mask = cfg->bc_mask;

    return 0;

err: return rc; }

\`\`\`

 ### Implementing a Sensor Device Shell Command

A sensor device driver package may optionally implement a sensor device
shell command that retrieves and sets sensor device information to aid
in testing and debugging. While the sensor framework `sensor shell
command </os/modules/sensor_framework/sensor_shell.html>`__ reads sensor
data for configured sensor types and is useful for testing an
application, it does not access low level device information, such as
reading register values and setting hardware configurations, that might
be needed to test a sensor device or to debug the sensor device driver
code. A sensor device shell command implementation is device specific
but should minimally support reading sensor data for all the sensor
types that the device supports because the sensor framework ``sensor``
shell command only reads sensor data for configured sensor types.

The package should:

-  Name the sensor device shell command ``<sensorname>``. For example,
   the sensor device shell command for the BNO055 sensor device is
   ``bno055``.

-  Define a ``<SENSORNAME>_CLI`` syscfg setting to specify whether the
   shell command is enabled and disable the setting by default.

-  Export a ``<sensorname>_shell_init()`` function that an application
   calls to initialize the sensor shell command when the
   ``SENSORNAME_CLI`` setting is enabled.

For an example on how to implement a sensor device shell command, see
the `bno055 shell
command <https://github.com/apache/mynewt-core/blob/master/hw/drivers/sensors/bno055/src/bno055_shell.c>`__
source code. See the `Enabling an Off-Board Sensor in an Existing
Application Tutorial </os/tutorials/sensors/sensor_nrf52_bno055.html>`__
for examples of the bno055 shell command.

Defining Logs
~~~~~~~~~~~~~

A sensor device driver should define logs for testing purposes. See the
`Log OS Guide <os/modules/logs/logs.html>`__ for more details on how to
add logs. The driver should define a ``<SENSORNAME>_LOG`` syscfg setting
to specify whether logging is enabled and disable the setting by
default.

Here is an example from the BNO055 sensor driver package:

\`\`\`hl\_lines="1 2 3 5 6 7 8 9 10 11 12 13 14 28 29 30"

if MYNEWT\_VAL(BNO055\_LOG)
===========================

include "log/log.h"
===================

endif
=====

if MYNEWT\_VAL(BNO055\_LOG) #define LOG\_MODULE\_BNO055 (305) #define
BNO055\_INFO(...) LOG\_INFO(&\_log, LOG\_MODULE\_BNO055, **VA\_ARGS**)
#define BNO055\_ERR(...) LOG\_ERROR(&\_log, LOG\_MODULE\_BNO055,
**VA\_ARGS**) static struct log \_log; #else #define BNO055\_INFO(...)
#define BNO055\_ERR(...) #endif

::

     ...

int bno055\_init(struct os\_dev *dev, void *\ arg) {

::

      ...

    rc = bno055_default_cfg(&bno055->cfg);
    if (rc) {
        goto err;
    }

if MYNEWT\_VAL(BNO055\_LOG)
===========================

::

    log_register(dev->od_name, &_log, &log_console_handler, NULL, LOG_SYSLEVEL);

endif
=====

::

      ...

}

\`\`\`

 ### Defining Stats

A sensor device driver may also define stats for the sensor. See the
`Stats OS Guide <os/modules/stats/stats.html>`__ for more details on how
to add stats. The driver should define a ``<SENSORNAME>_STATS`` syscfg
setting to specify whether stats is enabled and disable the setting by
default.

Here is an example from the BNO055 sensor driver package:

\`\`\`hl\_lines="1 2 3 5 6 7 8 9 11 12 13 14 16 17 18 29 30 31 32 33 34
35 36 37 38 39 "

if MYNEWT\_VAL(BNO055\_STATS)
=============================

include "stats/stats.h"
=======================

endif
=====

if MYNEWT\_VAL(BNO055\_STATS)
=============================

/\* Define the stats section and records \*/
STATS\_SECT\_START(bno055\_stat\_section) STATS\_SECT\_ENTRY(errors)
STATS\_SECT\_END

/\* Define stat names for querying \*/
STATS\_NAME\_START(bno055\_stat\_section)
STATS\_NAME(bno055\_stat\_section, errors)
STATS\_NAME\_END(bno055\_stat\_section)

/\* Global variable used to hold stats data \*/
STATS\_SECT\_DECL(bno055\_stat\_section) g\_bno055stats; #endif

...

int bno055\_init(struct os\_dev *dev, void *\ arg) {

::

      ...

if MYNEWT\_VAL(BNO055\_STATS)
=============================

::

    /* Initialise the stats entry */
    rc = stats_init(
        STATS_HDR(g_bno055stats),
        STATS_SIZE_INIT_PARMS(g_bno055stats, STATS_SIZE_32),
        STATS_NAME_INIT_PARMS(bno055_stat_section));
    SYSINIT_PANIC_ASSERT(rc == 0);
    /* Register the entry with the stats registry */
    rc = stats_register(dev->od_name, STATS_HDR(g_bno055stats));
    SYSINIT_PANIC_ASSERT(rc == 0);

endif
=====

::

      ...

}

\`\`\`
