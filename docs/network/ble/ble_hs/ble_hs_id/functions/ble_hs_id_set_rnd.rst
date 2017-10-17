ble\_hs\_id\_set\_rnd
---------------------

.. code:: c

    int
    ble_hs_id_set_rnd(const uint8_t *rnd_addr)

Description
~~~~~~~~~~~

Sets the device's random address. The address type (static vs.
non-resolvable private) is inferred from the most-significant byte of
the address. The address is specified in host byte order
(little-endian!).

Parameters
~~~~~~~~~~

+---------------+------------------------------+
| *Parameter*   | *Description*                |
+===============+==============================+
| rnd\_addr     | The random address to set.   |
+---------------+------------------------------+

Returned values
~~~~~~~~~~~~~~~

+-----------------------------------------------------------------------+---------------------+
| *Value*                                                               | *Condition*         |
+=======================================================================+=====================+
| 0                                                                     | Success.            |
+-----------------------------------------------------------------------+---------------------+
| `Core return code <../../ble_hs_return_codes/#return-codes-core>`__   | Unexpected error.   |
+-----------------------------------------------------------------------+---------------------+
