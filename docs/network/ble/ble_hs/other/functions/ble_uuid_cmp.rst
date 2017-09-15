ble\_uuid\_cmp
--------------

.. code:: c

    int
    ble_uuid_cmp(
        const ble_uuid_t *uuid1,
        const ble_uuid_t *uuid2
    )

Description
~~~~~~~~~~~

Compares two Bluetooth UUIDs.

Parameters
~~~~~~~~~~

+---------------+-------------------------------+
| *Parameter*   | *Description*                 |
+===============+===============================+
| uuid1         | The first UUID to compare.    |
+---------------+-------------------------------+
| uuid2         | The second UUID to compare.   |
+---------------+-------------------------------+

Returned values
~~~~~~~~~~~~~~~

+-----------+----------------------------+
| *Value*   | *Condition*                |
+===========+============================+
| 0         | The two uuids are equal.   |
+-----------+----------------------------+
| nonzero   | The uuids differ.          |
+-----------+----------------------------+
