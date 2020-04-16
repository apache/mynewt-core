..
  #
  # Copyright 2020 Casper Meijn <casper@meijn.net>
  #
  # Licensed under the Apache License, Version 2.0 (the "License");
  # you may not use this file except in compliance with the License.
  # You may obtain a copy of the License at
  #
  #     http://www.apache.org/licenses/LICENSE-2.0
  #
  # Unless required by applicable law or agreed to in writing, software
  # distributed under the License is distributed on an "AS IS" BASIS,
  # WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  # See the License for the specific language governing permissions and
  # limitations under the License.
  #
  
Charge control drivers
----------------------

.. toctree::
   :hidden:

   SGM4056 <sgm4056>

The charge control drivers provides support for various battery chargers. They 
provide a abstract ``hw/charge-control`` Battery Charge Controller IC Interface.
The available drivers are for:

- ADP5061
- BQ24040
- DA1469x
- :doc:`sgm4056`

Initialization
^^^^^^^^^^^^^^

Most of the drivers need some driver-specific initialization. This is normally 
done in the BSP by making a call to ``os_dev_create`` with a reference to the 
driver init function and a driver-specific configuration.

For the SGM4056 this will look this:

.. code-block:: c

    #include "sgm4056/sgm4056.h"

    ...

    static struct sgm4056_dev_config os_bsp_charger_config = {
        .power_presence_pin = CHARGER_POWER_PRESENCE_PIN,
        .charge_indicator_pin = CHARGER_CHARGE_PIN,
    };

    ...

    rc = os_dev_create(&os_bsp_charger.dev, "charger",
                       OS_DEV_INIT_KERNEL, OS_DEV_INIT_PRIO_DEFAULT,
                       sgm4056_dev_init, &os_bsp_charger_config);


Usage
^^^^^

After initialization the general charge control interface can be used to obtain 
the data from the charger. First you need to obtain a reference to the charger.

.. code-block:: c

    #include "charge-control/charge_control.h"

    ...

    struct charge_control *charger;

    charger = charge_control_mgr_find_next_bytype(CHARGE_CONTROL_TYPE_STATUS, NULL);
    assert(charger);

This will find any charger that has the abbility to report ``STATUS``. Then you 
can obtain the status of the charger using ``charge_control_read``. This function
will retreive the charger status and call a callback function. Lets first define 
that callback function:

.. code-block:: c

    static int 
    charger_data_callback(struct charge_control *chg_ctrl, void *arg,
            void *data, charge_control_type_t type) 
    {
        if (type == CHARGE_CONTROL_TYPE_STATUS) {
            charge_control_status_t charger_status = *(charge_control_status_t*)(data);

            console_printf("Charger state is %i", charger_status);
            console_flush();
        }
        return 0;
    }

The important arguments are the data pointer and the type of data. This function 
prints the status to the console as an integer. You need to 
extend it for your use-case. Now we can call the ``charge_control_read`` function:

.. code-block:: c

    rc = charge_control_read(charger, CHARGE_CONTROL_TYPE_STATUS, 
        charger_data_callback, NULL, OS_TIMEOUT_NEVER);
    assert(rc == 0);

This causes the charge control code to obtain the status and call the callback. 

Alternatively you can register a listener that will be called periodically and 
on change. For this you need to define a ``charge_control_listener`` and 
register it with charge control:

.. code-block:: c

    struct charge_control_listener charger_listener = {
        .ccl_type = CHARGE_CONTROL_TYPE_STATUS,
        .ccl_func = charger_data_callback,
    };

    ...

    rc = charge_control_set_poll_rate_ms("charger", 10000);
    assert(rc == 0);

    rc = charge_control_register_listener(charger, &charger_listener);
    assert(rc == 0);

This sets the interval to 10 seconds and registers our callback as listener.
This means that the callback will be called every 10 seconds and on interrupt 
of the charger.

Dependencies
^^^^^^^^^^^^

To include a charge control driver on a project, just include it as a
dependency in your pkg.yml. This should be done in the BSP. For example:

.. code-block:: yaml

    pkg.deps:
        - "@apache-mynewt-core/hw/drivers/chg_ctrl/sgm4056"


