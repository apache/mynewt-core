Creating New HAL Interfaces
===========================

HAL API
-------

A HAL always includes header file with function declarations for the HAL
functionality in ``/hw/hal/include/hal``. The first argument of all
functions in the interface typically include the virtual device\_id of
the device you are controlling.

For example, in
```hal_gpio.h`` <https://github.com/apache/incubator-mynewt-core/blob/master/hw/hal/include/hal/hal_gpio.h>`__
the device enumeration is the first argument to most methods and called
``pin``.

.. code-block:: console

        void hal_gpio_write(int pin, int val);

The device\_id (in this case called ``pin``) is not a physical device
(actual hardware pin), but a virtual pin which is defined by the
implementation of the HAL (and documented in the implementation of the
HAL).
