ble\_gap\_connect
-----------------

.. code:: c

    int
    ble_gap_connect(
                                 uint8_t  own_addr_type,
                        const ble_addr_t *peer_addr,
                                 int32_t  duration_ms,
        const struct ble_gap_conn_params *conn_params,
                        ble_gap_event_fn *cb,
                                    void *cb_arg
    )

Description
~~~~~~~~~~~

Initiates a connect procedure.

Parameters
~~~~~~~~~~

+---------------+-----------------+
| *Parameter*   | *Description*   |
+===============+=================+
+---------------+-----------------+

\| own\_addr\_type \| The type of address the stack should use for
itself during connection establishment.

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

\| \| peer\_addr \| The address of the peer to connect to. If this
parameter is NULL, the white list is used. \| \| duration\_ms \| The
duration of the discovery procedure. On expiration, the procedure ends
and a BLE\_GAP\_EVENT\_DISC\_COMPLETE event is reported. Units are
milliseconds. \| \| conn\_params \| Additional arguments specifying the
particulars of the connect procedure. Specify null for default values.
\| \| cb \| The callback to associate with this connect procedure. When
the connect procedure completes, the result is reported through this
callback. If the connect procedure succeeds, the connection inherits
this callback as its event-reporting mechanism. \| \| cb\_arg \| The
optional argument to pass to the callback function. \|

Returned values
~~~~~~~~~~~~~~~

+------------+----------------+
| *Value*    | *Condition*    |
+============+================+
| 0          | Success.       |
+------------+----------------+
| BLE\_HS\_E | A connection   |
| ALREADY    | attempt is     |
|            | already in     |
|            | progress.      |
+------------+----------------+
| BLE\_HS\_E | Initiating a   |
| BUSY       | connection is  |
|            | not possible   |
|            | because        |
|            | scanning is in |
|            | progress.      |
+------------+----------------+
| BLE\_HS\_E | The specified  |
| DONE       | peer is        |
|            | already        |
|            | connected.     |
+------------+----------------+
| `Core      | Unexpected     |
| return     | error.         |
| code <../. |                |
| ./ble_hs_r |                |
| eturn_code |                |
| s/#return- |                |
| codes-core |                |
| >`__       |                |
+------------+----------------+
