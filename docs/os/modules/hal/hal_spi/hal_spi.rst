hal\_spi
========

SPI (Serial Peripheral Interface) is a synchronous 4-wire serial
interface commonly used to connect components in embedded systems.

For a detailed description of SPI, see
`Wikipedia <https://en.wikipedia.org/wiki/Serial_Peripheral_Interface_Bus>`__.

Description
~~~~~~~~~~~

The Mynewt HAL interface supports the SPI master functionality with both
blocking and non-blocking interface. SPI slave functionality is
supported in non-blocking mode.

Definition
~~~~~~~~~~

`hal\_spi.h <https://github.com/apache/incubator-mynewt-core/blob/master/hw/hal/include/hal/hal_spi.h>`__

HAL\_SPI Theory Of Operation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

SPI is called a 4-wire interface because of the 4 signals, MISO, MOSI,
CLK, and SS. The SS signal (slave select) is an active low signal that
activates a SPI slave device. This is how a master "addresses" a
particular slave device. Often this signal is also referred to as "chip
select" as it selects particular slave device for communications.

The Mynewt SPI HAL has blocking and non-blocking transfers. Blocking
means that the API call to transfer a byte will wait until the byte
completes transmissions before the function returns. Blocking interface
can be used for only the master slave SPI type. Non-blocking means he
function returns control to the execution environment immediately after
the API call and a callback function is executed at the completion of
the transmission. Non-blocking interface can be used for both master and
slave SPI types.

The ``hal_spi_config`` method in the API above allows the SPI to be
configured with appropriate settings for master or slave. It Must be
called after the spi is initialized (i.e. after hal\_spi\_init is
called) and when the spi is disabled (i.e. user must call
hal\_spi\_disable if the spi has been enabled through hal\_spi\_enable
prior to calling this function). It can also be used to reconfigure an
initialized SPI (assuming it is disabled as described previously).

.. code:: c

    int hal_spi_config(int spi_num, struct hal_spi_settings *psettings);

The SPI settings consist of the following:

.. code:: c

    struct hal_spi_settings {
        uint8_t         data_mode;
        uint8_t         data_order;
        uint8_t         word_size;
        uint32_t        baudrate;           /* baudrate in kHz */
    };

The Mynewt SPI HAL does not include built-in SS (Slave Select)
signaling. It's up to the hal\_spi user to control their own SS pins.
Typically applications will do this with GPIO.
