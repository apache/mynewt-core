Creating and Configuring a Sensor Device
----------------------------------------

The steps to create and configure OS devices for onboard and off-board
sensors are very similar. The BSP creates the OS devices for onboard
sensors and the ``hw/sensor/creator/`` package creates the OS devices
for off-board sensors.

We discuss how a BSP creates a device for an onboard sensor and then
discuss what the ``hw/sensor/creator`` package does differently to
create an off-board sensor. We also discuss how an application can
change the default configuration for a sensor device.

Creating an Onboard Sensor
~~~~~~~~~~~~~~~

To create and initialize a sensor device named ``SENSORNAME``, the BSP implements the following in the
``hal_bsp.c`` file.

**Note**: All example excerpts are from the code that creates the
LIS2DH12 onboard sensor in the nrf52\_thingy BSP.

1. Define a ``<SENSORNAME>_ONB`` syscfg setting to specify whether the
onboard sensor named ``SENSORNAME`` is enabled. The setting is disabled
by default. The setting is used to conditionally include the code to
create a sensor device for ``SENSORNAME`` when it is enabled by the
application. For example:

.. code-block:: console


    syscfg.defs:
        LIS2DH12_ONB:
            description: 'NRF52 Thingy onboard lis2dh12 sensor'
            value:  0

2. Include the "<sensorname>/<sensorname>.h" header file. The BSP uses
the functions and data structures that a device driver package exports.
See the `Sensor Device
Driver </os/modules/sensor_framework/sensor_driver.html>`__ page for
details.

3. Declare a variable named ``sensorname`` of type
``struct sensorname``. For example:

.. code:: c


    #if MYNEWT_VAL(LIS2DH12_ONB)
    #include <lis2dh12/lis2dh12.h>
    static struct lis2dh12 lis2dh12;
    #endif

4. Declare and specify the values for a variable of type
``struct sensor_itf`` that the sensor device driver uses to communicate
with the sensor device. For example:

.. code:: c


    #if MYNEWT_VAL(LIS2DH12_ONB)
    static struct sensor_itf i2c_0_itf_lis = {
        .si_type = SENSOR_ITF_I2C,
        .si_num  = 0,
        .si_addr = 0x19
    <br>

5. In the ``hal_bsp_init()`` function, create an OS device for the
sensor device. Call the ``os_dev_create()`` function and pass the
following to the function:

-  A pointer to the ``sensorname`` variable from step 3.
-  A pointer to the ``<sensorname>_init()`` callback function. Note that
   the device driver package exports this function.
-  A pointer to the ``struct sensor_itf`` variable from step 4.

For example:

\`\`\`hl\_lines="7 8 9 10 11"

static void sensor\_dev\_create(void) { int rc; (void)rc;

if MYNEWT\_VAL(LIS2DH12\_ONB)
=============================

::

    rc = os_dev_create((struct os_dev *) &lis2dh12, "lis2dh12_0",
      OS_DEV_INIT_PRIMARY, 0, lis2dh12_init, (void *)&i2c_0_itf_lis);
    assert(rc == 0);

endif
=====

}

void hal\_bsp\_init(void) { int rc;

::

      ...


    sensor_dev_create();

}

\`\`\ ``<br> 6. Define a``\ config\_\_sensor()\ ``function to set the default configuration for the sensor. This function opens the OS device for the sensor device, initializes the a``\ cfg\ ``variable of type``\ struct
\_cfg\ ``with the default settings, calls the``\ \_config()\` driver
function to configure the device, and closes the device. This function
is called when the BSP is initialized during sysinit(). For example:

.. code:: c


    int
    config_lis2dh12_sensor(void)
    {
    #if MYNEWT_VAL(LIS2DH12_ONB)
        int rc;
        struct os_dev *dev;
        struct lis2dh12_cfg cfg;

        dev = (struct os_dev *) os_dev_open("lis2dh12_0", OS_TIMEOUT_NEVER, NULL);
        assert(dev != NULL);

        memset(&cfg, 0, sizeof(cfg));

        cfg.lc_s_mask = SENSOR_TYPE_ACCELEROMETER;
        cfg.lc_rate = LIS2DH12_DATA_RATE_HN_1344HZ_L_5376HZ;
        cfg.lc_fs = LIS2DH12_FS_2G;
        cfg.lc_pull_up_disc = 1;

        rc = lis2dh12_config((struct lis2dh12 *)dev, &cfg);
        SYSINIT_PANIC_ASSERT(rc == 0);

        os_dev_close(dev);
    #endif
        return 0;
    }

7. Add the following in the BSP ``pkg.yml`` file:

-  A conditional package dependency for the
   ``hw/drivers/sensors/<sensorname>`` package when the
   ``<SENSORNAME>_ONB`` setting is enabled.

-  The ``config_<sensorname>_sensor`` function with an init stage of 400
   to the ``pkg.init`` parameter.

For example:

.. code-block:: console


    pkg.deps.LIS2DH12_ONB:
        - hw/drivers/sensors/lis2dh12

    pkg.init:
        config_lis2dh12_sensor: 400

Creating an Off-Board Sensor
~~~~~~~~~~~~~~~


The steps to create an off-board sensor is very similar to the steps for
a BSP. The ``hw/sensor/creator/`` package also declares the variables
and implements the ``config_<sensorname>_sensor()`` function described
for a BSP. The package does the following differently.

**Note**: All example excerpts are from the code that creates the BNO055
off-board sensor in ``hw/sensor/creator`` package.

1. Define a ``<SENSORNAME>_OFB`` syscfg setting to specify whether the
off-board sensor named ``SENSORNAME`` is enabled. This setting is
disabled by default. The ``hw/sensor/creator`` package uses the setting
to conditionally include the code to create the sensor device when it is
enabled by the application.

.. code-block:: console


    # Package: hw/sensor/creator

    syscfg.defs:
          ...

        BNO055_OFB:
            description: 'BNO055 is present'
            value : 0

           ...

2. Add the calls to the ``os_dev_create()`` and the
``config_<sensorname>_sensor()`` functions in the
``sensor_dev_create()`` function defined in the ``sensor_creator.c``
file . The ``sensor_dev_create()`` function is the ``hw/sensor/creator``
package initialization function that ``sysinit()`` calls.

For example:

.. code:: c


    void
    sensor_dev_create(void)
    {
        int rc;

         ...

    #if MYNEWT_VAL(BNO055_OFB)
        rc = os_dev_create((struct os_dev *) &bno055, "bno055_0",
          OS_DEV_INIT_PRIMARY, 0, bno055_init, (void *)&i2c_0_itf_bno);
        assert(rc == 0);

        rc = config_bno055_sensor();
        assert(rc == 0);
    #endif

         ....

    }

3. Add a conditional package dependency for the
``hw/drivers/sensors/<sensorname>`` package when the
``<SENSORNAME>_OFB`` setting is enabled. For example:

.. code-block:: console


    pkg.deps.BNO055_OFB:
        - hw/drivers/sensors/bno055

Reconfiguring A Sensor Device by an Application
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The BSP and sensor creator package use a default configuration and
enable all supported sensors on a sensor device by default. If the
default configuration does not meet your application requirements, you
may change the default configuration for a sensor device. As in the
``config_<sensorname>_sensor`` function, an application must open the OS
device for the sensor, set up the values for the ``<sensorname>_cfg``
structure, call the ``<sensorname>_config()`` device driver function to
change the configuration in the device, and close the OS device.

We recommend that you copy the ``config_<sensorname>_sensor()`` function
from the BSP or the sensor creator package in to your application code
and change the desired settings. Note that you must keep all the fields
in the ``<sensorname>_cfg`` structure initialized with the default
values for the settings that you do not want to change.

See the `Changing the Default Configuration for a Sensor
Tutorial </os/tutorials/sensors/sensor_offboard_config/>`__ for more
details on how to change the default sensor configuration from an
application.
