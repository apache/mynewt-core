ble\_hs\_id\_gen\_rnd
---------------------

.. code:: c

    int
    ble_hs_id_gen_rnd(
               int  nrpa,
        ble_addr_t *out_addr
    )

Description
~~~~~~~~~~~

Generates a new random address. This function does not configure the
device with the new address; the caller can use the address in
subsequent operations.

Parameters
~~~~~~~~~~

+----------------+------------------+
| *Parameter*    | *Description*    |
+================+==================+
| nrpa           | The type of      |
|                | random address   |
|                | to generate: 0:  |
|                | static 1:        |
|                | non-resolvable   |
|                | private          |
+----------------+------------------+
| out\_addr      | On success, the  |
|                | generated        |
|                | address gets     |
|                | written here.    |
+----------------+------------------+

Returned values
~~~~~~~~~~~~~~~

+-----------------------------------------------------------------------+---------------------+
| *Value*                                                               | *Condition*         |
+=======================================================================+=====================+
| 0                                                                     | Success.            |
+-----------------------------------------------------------------------+---------------------+
| `Core return code <../../ble_hs_return_codes/#return-codes-core>`__   | Unexpected error.   |
+-----------------------------------------------------------------------+---------------------+
