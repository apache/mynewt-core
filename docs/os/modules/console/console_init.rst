 console\_init 
---------------

.. code-block:: console

       int
       console_init(console_rx_cb rx_cb)

This function is a Mynewt 1.0 Console API. This function only needs to
be called if the Mynewt 1.0 console API is used to read input from the
console input.

If a callback function pointer of ``type void (*console_rx_cb)(void)``
is specfied, the callback is called when the console receives a full
line of data.

Note that this function is most likely getting called from an interrupt
context.

Arguments
^^^^^^^^^

+-------------+----------------------------------------------------------+
| Arguments   | Description                                              |
+=============+==========================================================+
| rx\_cb      | Function pointer, which gets called input is received.   |
+-------------+----------------------------------------------------------+

Returned values
^^^^^^^^^^^^^^^

Returns 0 on success.

Non-zero on failure.

Example
^^^^^^^

.. code-block:: console

    int
    main(int argc, char **argv)
    {
        ....

        console_init(NULL);
    }
