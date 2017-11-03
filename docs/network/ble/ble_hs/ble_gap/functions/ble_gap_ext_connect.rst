ble\_gap\_ext\_connect [experimental] 
--------------------------------------

.. code:: c

    int
    ble_gap_ext_connect(
                                 uint8_t  own_addr_type,
                        const ble_addr_t  *peer_addr,
                                 int32_t  duration_ms,
                                 uint8_t  phy_mask,
        const struct ble_gap_conn_params  *phy_1m_conn_params,
        const struct ble_gap_conn_params  *phy_2m_conn_params,
        const struct ble_gap_conn_params  *phy_coded_conn_params,
                        ble_gap_event_fn  *cb,
                                    void  *cb_arg
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

\| \| peer\_addr \| The identity address of the peer to connect to.
White list is used when this parameters is set to NULL. \| \|
duration\_ms \| The duration of the discovery procedure. On expiration,
the procedure ends and a BLE\_GAP\_EVENT\_DISC\_COMPLETE event is
reported. Units are milliseconds. \| \| phy\_mask \| Define on which
PHYs connection attempt should be done \| \| phy\_1m\_conn\_params \|
Additional arguments specifying the particulars of the connect
procedure. When BLE\_GAP\_LE\_PHY\_1M\_MASK is set in phy\_mask this
parameter can be specify to null for default values. \| \|
phy\_2m\_conn\_params \| Additional arguments specifying the particulars
of the connect procedure. When BLE\_GAP\_LE\_PHY\_2M\_MASK is set in
phy\_mask this parameter can be specify to null for default values. \|
\| phy\_coded\_conn\_params \| Additional arguments specifying the
particulars of the connect procedure. When
BLE\_GAP\_LE\_PHY\_CODED\_MASK is set in phy\_mask this parameter can be
specify to null for default values. \| \| cb \| The callback to
associate with this connect procedure. When the connect procedure
completes, the result is reported through this callback. If the connect
procedure succeeds, the connection inherits this callback as its
event-reporting mechanism. \| \| cb\_arg \| The optional argument to
pass to the callback function. \|

Returned values
~~~~~~~~~~~~~~~

+-----------------------------------------------------------------------+---------------------+
| *Value*                                                               | *Condition*         |
+=======================================================================+=====================+
| 0                                                                     | Success.            |
+-----------------------------------------------------------------------+---------------------+
| `Core return code <../../ble_hs_return_codes/#return-codes-core>`__   | Unexpected error.   |
+-----------------------------------------------------------------------+---------------------+
