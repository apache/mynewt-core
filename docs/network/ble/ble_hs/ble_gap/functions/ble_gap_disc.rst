ble\_gap\_disc
--------------

.. code:: c

    int
    ble_gap_disc(
                                 uint8_t  own_addr_type,
                                 int32_t  duration_ms,
        const struct ble_gap_disc_params *disc_params,
                        ble_gap_event_fn *cb,
                                    void *cb_arg
    )

Description
~~~~~~~~~~~

Performs the Limited or General Discovery Procedures.

Parameters
~~~~~~~~~~

+---------------+-----------------+
| *Parameter*   | *Description*   |
+===============+=================+
+---------------+-----------------+

\| own\_addr\_type \| The type of address the stack should use for
itself when sending scan requests. Valid values are:

.. raw:: html

   <ul>

.. raw:: html

   <li>

BLE\_ADDR\_TYPE\_PUBLIC

.. raw:: html

   </li>

.. raw:: html

   <li>

BLE\_ADDR\_TYPE\_RANDOM

.. raw:: html

   </li>

.. raw:: html

   <li>

BLE\_ADDR\_TYPE\_RPA\_PUB\_DEFAULT

.. raw:: html

   </li>

.. raw:: html

   <li>

BLE\_ADDR\_TYPE\_RPA\_RND\_DEFAULT

.. raw:: html

   </li>

.. raw:: html

   </ul>

This parameter is ignored unless active scanning is being used. \| \|
duration\_ms \| The duration of the discovery procedure. On expiration,
the procedure ends and a BLE\_GAP\_EVENT\_DISC\_COMPLETE event is
reported. Units are milliseconds. Specify BLE\_HS\_FOREVER for no
expiration. \| \| disc\_params \| Additional arguments specifying the
particulars of the discovery procedure. \| \| cb \| The callback to
associate with this discovery procedure. Advertising reports and
discovery termination events are reported through this callback. \| \|
cb\_arg \| The optional argument to pass to the callback function. \|

Returned values
~~~~~~~~~~~~~~~

+-----------------------------------------------------------------------+---------------------+
| *Value*                                                               | *Condition*         |
+=======================================================================+=====================+
| 0                                                                     | Success.            |
+-----------------------------------------------------------------------+---------------------+
| `Core return code <../../ble_hs_return_codes/#return-codes-core>`__   | Unexpected error.   |
+-----------------------------------------------------------------------+---------------------+
