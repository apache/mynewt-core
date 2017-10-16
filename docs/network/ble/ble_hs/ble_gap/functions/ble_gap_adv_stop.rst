ble\_gap\_adv\_stop
-------------------

.. code:: c

    int
    ble_gap_adv_stop(void)

Description
~~~~~~~~~~~

Stops the currently-active advertising procedure. A success return code
indicates that advertising has been fully aborted; a new advertising
procedure can be initiated immediately.

Parameters
~~~~~~~~~~

None

Returned values
~~~~~~~~~~~~~~~

+-----------------------------------------------------------------------+---------------------------------------------+
| *Value*                                                               | *Condition*                                 |
+=======================================================================+=============================================+
| 0                                                                     | Success.                                    |
+-----------------------------------------------------------------------+---------------------------------------------+
| BLE\_HS\_EALREADY                                                     | There is no active advertising procedure.   |
+-----------------------------------------------------------------------+---------------------------------------------+
| `Core return code <../../ble_hs_return_codes/#return-codes-core>`__   | Unexpected error.                           |
+-----------------------------------------------------------------------+---------------------------------------------+
