 shell\_evq\_set
----------------

.. code:: c

    void shell_evq_set(struct os_eventq *evq)

Specifies an event queue to use for shell events. You must create the
event queue and the task to process events from the queue before calling
this function.

By default, shell uses the OS default event queue and executes in the
context of the main task that Mynewt creates.

Arguments
^^^^^^^^^

+-------------+-------------------------------------------------------+
| Arguments   | Description                                           |
+=============+=======================================================+
| ``evq``     | Pointer to the event queue to use for shell events.   |
+-------------+-------------------------------------------------------+

Returned values
^^^^^^^^^^^^^^^

None

Notes
^^^^^
