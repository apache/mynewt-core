ble\_gatts\_svc\_set\_visibility
--------------------------------

.. code:: c

    int
    ble_gatts_svc_set_visibility(uint16_t handle, int visible)

Description
~~~~~~~~~~~

Changes visibility of a service with specified handle. This function
allow any service to be hidden (and then restored) from clients. Note:
From GATT point of view, the service is still registered and has the
same handle range assigned and it is ATT server which hides the
attributes from the client.

Parameters
~~~~~~~~~~

+----------------+------------------+
| *Parameter*    | *Description*    |
+================+==================+
| handle         | The handle of a  |
|                | service for      |
|                | which the        |
|                | visibility       |
|                | should be        |
|                | changed.         |
+----------------+------------------+
| visible        | The value of     |
|                | visibility to    |
|                | set.             |
+----------------+------------------+

Returned values
~~~~~~~~~~~~~~~

+-------------------+--------------------------------------------------+
| *Value*           | *Condition*                                      |
+===================+==================================================+
| 0                 | Success.                                         |
+-------------------+--------------------------------------------------+
| BLE\_HS\_ENOENT   | The svcs with specified handle does not exist.   |
+-------------------+--------------------------------------------------+
