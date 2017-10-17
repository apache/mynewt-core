hal\_uart
=========

The hardware independent UART interface for Mynewt.

Description
~~~~~~~~~~~

Contains the basic operations to send and receive data over a UART
(Universal Asynchronous Receiver Transmitter). It also includes the API
to apply settings such as speed, parity etc. to the UART. The UART port
should be closed before any reconfiguring.

Definition
~~~~~~~~~~

`hal\_uart.h <https://github.com/apache/incubator-mynewt-core/blob/master/hw/hal/include/hal/hal_uart.h>`__

Examples
~~~~~~~~

This example shows a user writing a character to the uart in blocking
mode where the UART has to block until character has been sent.

.. code-block:: console

    /* write to the console with blocking */
    {
        char *str = "Hello World!";
        char *ptr = str;

        while(*ptr) {
            hal_uart_blocking_tx(MY_UART, *ptr++);
        }
        hal_uart_blocking_tx(MY_UART, '\n');
    }
