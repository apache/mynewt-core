ble\_gap\_adv\_start
--------------------

.. code:: c

    int
    ble_gap_adv_start(
                                uint8_t  own_addr_type,
                       const ble_addr_t *direct_addr,
                                int32_t  duration_ms,
        const struct ble_gap_adv_params *adv_params,
                       ble_gap_event_fn *cb,
                                   void *cb_arg
    )

Description
~~~~~~~~~~~

Initiates advertising.

Parameters
~~~~~~~~~~

+---------------+-----------------+
| *Parameter*   | *Description*   |
+===============+=================+
+---------------+-----------------+

\| own\_addr\_type \| The type of address the stack should use for
itself. Valid values are:

.. raw:: html

   <ul>

.. raw:: html

   <li>

BLE\_OWN\_ADDR\_PUBLIC

.. raw:: html

   </li>

.. raw:: html

   <li>

BLE\_OWN\_ADDR\_RANDOM

.. raw:: html

   </li>

.. raw:: html

   <li>

BLE\_OWN\_ADDR\_RPA\_PUBLIC\_DEFAULT

.. raw:: html

   </li>

.. raw:: html

   <li>

BLE\_OWN\_ADDR\_RPA\_RANDOM\_DEFAULT

.. raw:: html

   </li>

.. raw:: html

   </ul>

\| \| direct\_addr \| The peer's address for directed advertising. This
parameter shall be non-NULL if directed advertising is being used. \| \|
duration\_ms \| The duration of the advertisement procedure. On
expiration, the procedure ends and a BLE\_GAP\_EVENT\_ADV\_COMPLETE
event is reported. Units are milliseconds. Specify BLE\_HS\_FOREVER for
no expiration. \| \| adv\_params \| Additional arguments specifying the
particulars of the advertising procedure. \| \| cb \| The callback to
associate with this advertising procedure. If advertising ends, the
event is reported through this callback. If advertising results in a
connection, the connection inherits this callback as its event-reporting
mechanism. \| \| cb\_arg \| The optional argument to pass to the
callback function. \|

Returned values
~~~~~~~~~~~~~~~

+-----------------------------------------------------------------------+---------------------+
| *Value*                                                               | *Condition*         |
+=======================================================================+=====================+
| 0                                                                     | Success.            |
+-----------------------------------------------------------------------+---------------------+
| `Core return code <../../ble_hs_return_codes/#return-codes-core>`__   | Unexpected error.   |
+-----------------------------------------------------------------------+---------------------+
