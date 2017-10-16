ble\_uuid\_u16
--------------

.. code:: c

    uint16_t
    ble_uuid_u16(const ble_uuid_t *uuid)

Description
~~~~~~~~~~~

Converts the specified 16-bit UUID to a uint16\_t.

Parameters
~~~~~~~~~~

+---------------+-------------------------------+
| *Parameter*   | *Description*                 |
+===============+===============================+
| uuid          | The source UUID to convert.   |
+---------------+-------------------------------+

Returned values
~~~~~~~~~~~~~~~

+-------------------------+--------------------------------------+
| *Value*                 | *Condition*                          |
+=========================+======================================+
| The converted integer   | Success.                             |
+-------------------------+--------------------------------------+
| 0                       | The specified uuid is not 16 bits.   |
+-------------------------+--------------------------------------+
