Changing the Default Configuration for a Sensor
-----------------------------------------------

This tutorial shows you how to change default configuration values for
an off-board sensor. It continues with the example in the `Enabling an
Off-Board Sensor in an Existing Application
tutorial </os/tutorials/sensors/sensor_offboard_config.html>`__.

\*\* Note:\*\* You can also follow most of the instructions in this
tutorial to change the default configuration for an onboard sensor. The
difference is that the BSP, instead of the sensor creator package,
creates and configures the onboard sensor devices in the ``hal_bsp.c``
file. You should check the BSP to determine whether the default
configuration for a sensor meets your application requirements.

Prerequisite
~~~~~~~~~~~~

Complete the tasks described in the `Enabling an Off-Board Sensor in an
Existing Application
tutorial </os/tutorials/sensors/sensor_offboard_config.html>`__.

Overview on How to Initialize the Configuration Values for a Sensor
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The sensor creator package, ``hw/sensor/creator``, creates, for each
enabled sensor, an os device in the kernel for the sensor and
initializes the sensor with its default configuration when the package
is initialized. The steps to configure a sensor device are:

1. Open the os device for the sensor.
2. Initialize the sensor driver configuration data structure with
   default values.
3. Call the ``<sensorname>_config()`` function that the sensor device
   driver package exports.
4. Close the os device for the sensor.

For the BNO055 sensor device, the creator package calls the local
``config_bno055_sensor()`` function to configure the sensor. A code
excerpt for this function is shown below:

.. code:: c

    static int
    config_bno055_sensor(void)
    {
        int rc;
        struct os_dev *dev;
        struct bno055_cfg bcfg;

        dev = (struct os_dev *) os_dev_open("bno055_0", OS_TIMEOUT_NEVER, NULL);
        assert(dev != NULL);

        bcfg.bc_units = BNO055_ACC_UNIT_MS2   | BNO055_ANGRATE_UNIT_DPS |
                        BNO055_EULER_UNIT_DEG | BNO055_TEMP_UNIT_DEGC   |
                        BNO055_DO_FORMAT_ANDROID;

        bcfg.bc_opr_mode = BNO055_OPR_MODE_NDOF;
        bcfg.bc_pwr_mode = BNO055_PWR_MODE_NORMAL;
        bcfg.bc_acc_bw = BNO055_ACC_CFG_BW_125HZ;
        bcfg.bc_acc_range =  BNO055_ACC_CFG_RNG_16G;
        bcfg.bc_mask = SENSOR_TYPE_ACCELEROMETER|
                       SENSOR_TYPE_MAGNETIC_FIELD|
                       SENSOR_TYPE_GYROSCOPE|
                       SENSOR_TYPE_EULER|
                       SENSOR_TYPE_GRAVITY|
                       SENSOR_TYPE_LINEAR_ACCEL|
                       SENSOR_TYPE_ROTATION_VECTOR;

        rc = bno055_config((struct bno055 *) dev, &bcfg);

        os_dev_close(dev);
        return rc;
    }

 ### Changing the Default Configuration

To change the default configuration, you can directly edit the fields in
the ``config_bno055_sensor()`` function in the
``hw/sensor/creator/sensor_creator.c`` file or add code to your
application to reconfigure the sensor during application initialization.

This tutorial shows you how to add the code to the
``apps/sensors_test/src/main.c`` file to configure the sensor without
the accelerometer sensor type. When you reconfigure a sensor in the
application, you must initialize all the fields in the sensor
configuration data structure even if you are not changing the default
values.

 #### Step 1: Adding the Sensor Device Driver Header File

Add the bno055 device driver header file:

::

    #include <bno055/bno055.h> 

 #### Step 2: Adding a New Configuration Function

Add the ``sensors_test_config_bno055()`` function and copy the code from
the ``config_bno055_sensor()`` function in the
``hw/sensor/creator/sensor_creator.c`` file to the body of the
``sensors_test_config_bno055()`` function. The content of the
``sensors_test_config_bno055()`` function should look like the example
below:

.. code:: c

    static int
    sensors_test_config_bno055(void)
    {
        int rc;
        struct os_dev *dev;
        struct bno055_cfg bcfg;

        dev = (struct os_dev *) os_dev_open("bno055_0", OS_TIMEOUT_NEVER, NULL);
        assert(dev != NULL);

        bcfg.bc_units = BNO055_ACC_UNIT_MS2   | BNO055_ANGRATE_UNIT_DPS |
                        BNO055_EULER_UNIT_DEG | BNO055_TEMP_UNIT_DEGC   |
                        BNO055_DO_FORMAT_ANDROID;

        bcfg.bc_opr_mode = BNO055_OPR_MODE_NDOF;
        bcfg.bc_pwr_mode = BNO055_PWR_MODE_NORMAL;
        bcfg.bc_acc_bw = BNO055_ACC_CFG_BW_125HZ;
        bcfg.bc_acc_range =  BNO055_ACC_CFG_RNG_16G;
        bcfg.bc_use_ext_xtal = 1;
        bcfg.bc_mask = SENSOR_TYPE_ACCELEROMETER|
                       SENSOR_TYPE_MAGNETIC_FIELD|
                       SENSOR_TYPE_GYROSCOPE|
                       SENSOR_TYPE_EULER|
                       SENSOR_TYPE_GRAVITY|
                       SENSOR_TYPE_LINEAR_ACCEL|
                       SENSOR_TYPE_ROTATION_VECTOR;

        rc = bno055_config((struct bno055 *) dev, &bcfg);

        os_dev_close(dev);
        return rc;
    }

 #### Step 3: Changing the Default Configuration Settings

Delete the ``SENSOR_TYPE_ACCELEROMETER`` type from the ``bcfg.bc_mask``
initialization setting values:

.. code:: hl_lines="8"


    static int
    sensors_test_config_bno055(void)
    {
       int rc
           ...

       /* Delete the SENSOR_TYPE_ACCELEROMETER from the mask */
       bcfg.bc_mask = SENSOR_TYPE_MAGNETIC_FIELD|
                      SENSOR_TYPE_GYROSCOPE|
                      SENSOR_TYPE_EULER|
                      SENSOR_TYPE_GRAVITY|
                      SENSOR_TYPE_LINEAR_ACCEL|
                      SENSOR_TYPE_ROTATION_VECTOR;

        rc = bno055_config((struct bno055 *) dev, &bcfg);

        os_dev_close(dev);
        return rc;

Step 4: Calling the Configuration Function From main()
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Add the ``int rc`` declaration and the call to the
``sensors_test_config_bno055()`` function in ``main()``:

.. code:: c

    int
    main(int argc, char **argv)
    {

        /* Add rc for the return value from sensors_test_config_bno055() */
        int rc;

            ....
        /* Add call to sensors_test_config_bno055() and abort on error */
        rc = sensors_test_config_bno055();
        assert(rc == 0);

        /* log reboot */
        reboot_start(hal_reset_cause());

        /*
         * As the last thing, process events from default event queue.
         */
        while (1) {
            os_eventq_run(os_eventq_dflt_get());
        }

        return (0);
    }

 #### Step 5: Building a New Application Image

Run the ``newt build nrf52_bno055_test`` and the
``newt create-image nrf52_bno055_test 2.0.0`` commands to rebuild and
create a new application image.

 #### Step 6: Loading the New Image and Rebooting the Device

Run the ``newt load nrf52_bno055_test`` command and power the device OFF
and On.

 #### Step 7: Verifing the Sensor is Configured with the New Values

Start a terminal emulator, and run the ``sensor list`` command to verify
the accelerometer (0x1) is not configured. The ``configured type``
listed for the sensor should not have the value ``0x1``.

.. code:: hl_lines="2"


    045930 compat> sensor list
    046482 sensor dev = bno055_0, configured type = 0x2 0x4 0x200 0x1000 0x2000 0x4000 
    046484 compat>

 #### Step 8: Verifying that the Accelerometer Data Samples Cannot be
Read

Run the ``sensor read`` command to read data samples from the
accelerometer to verify that the sensor cannot be read:

.. code-block:: console


    046484 compat> sensor read bno055_0 0x1 -n 5
    092387 Cannot read sensor bno055_0

