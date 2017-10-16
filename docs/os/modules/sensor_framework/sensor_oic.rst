OIC Sensor Support
------------------

The sensor framework provides support for an OIC enabled application to
host the sensor devices as OIC resources. The sensor framework provides
the following OIC support:

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

An application defines an OIC application initialization handler that
sets up the OIC resources it supports and calls the ``oc_main_init()``
function to initialize the OC server. The application must call the
``sensor_oic_init()`` function from the the OIC application
initialization handler. The ``sensor_oic_init()`` function creates all
the OIC resources for the sensors and registers request callbacks to
process CoAP GET requests for the sensor OIC resources.

See the `Enabling OIC Sensor Data Monitoring
Tutorials </os/tutorials/sensors/sensor_oic_overview.html>`__ to run and
develop sample OIC sensor server applications.
