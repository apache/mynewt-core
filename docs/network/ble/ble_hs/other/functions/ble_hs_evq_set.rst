ble\_hs\_evq\_set
-----------------

.. code:: c

    void
    ble_hs_evq_set(struct os_eventq *evq)

Description
~~~~~~~~~~~

Designates the specified event queue for NimBLE host work. By default,
the host uses the default event queue and runs in the main task. This
function is useful if you want the host to run in a different task.

Parameters
~~~~~~~~~~

+---------------+-----------------------------------------+
| *Parameter*   | *Description*                           |
+===============+=========================================+
| evq           | The event queue to use for host work.   |
+---------------+-----------------------------------------+

Returned values
~~~~~~~~~~~~~~~

None
