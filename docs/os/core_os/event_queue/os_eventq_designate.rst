 os\_eventq\_designate
----------------------

.. code:: c

       void
        os_eventq_designate(struct os_eventq **cur_evq, struct os_eventq *new_evq,  struct os_event *start_ev)

Reassigns a package's current event queue pointer to point to a new
event queue. If a startup event is specified, the event is added to the
new event queue and removed from the current event queue.

Arguments
^^^^^^^^^

+----------------+---------------------------------------------+
| Arguments      | Description                                 |
+================+=============================================+
| ``cur_evq``    | Current event queue pointer to reassign     |
+----------------+---------------------------------------------+
| ``new_evq``    | New event queue to use for the package      |
+----------------+---------------------------------------------+
| ``start_ev``   | Start event to add to the new event queue   |
+----------------+---------------------------------------------+

Returned values
^^^^^^^^^^^^^^^

None

Notes
^^^^^

Example
^^^^^^^

This example shows the ``mgmt_evq_set()`` function that the
``mgmt/newtmgr`` package implements. The function allows an application
to specify an event queue for ``newtmgr`` to use. The ``mgmt_evq_set()``
function calls the ``os_eventq_designate()`` function to reassign the
``nmgr_evq`` to the event queue that the application specifies.

.. code:: c

    void
    mgmt_evq_set(struct os_eventq *evq)
    {
        os_eventq_designate(&nmgr_evq, evq, NULL);
    }


