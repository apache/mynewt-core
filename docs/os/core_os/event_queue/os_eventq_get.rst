 os\_eventq\_get
----------------

.. code:: c

    struct os_event *
    os_eventq_get(struct os_eventq *evq)

Dequeues and returns an event from the head of an event queue. When the
event queue is empty, the function puts the task into the ``sleeping``
state.

Arguments
^^^^^^^^^

+-------------+------------------------------------------+
| Arguments   | Description                              |
+=============+==========================================+
| ``evq``     | Event queue to retrieve the event from   |
+-------------+------------------------------------------+

Returned values
^^^^^^^^^^^^^^^

A pointer to the ``os_event`` structure for the event dequeued from the
queue.

Notes
^^^^^

In most cases, you do not need to call this function directly. You
should call the ``os_eventq_run()`` wrapper function that calls this
function to retrieve an event and then calls the callback function to
process the event.

Example
^^^^^^^

Example of the ``os_eventq_run()`` wrapper function that calls this
function:

.. code:: c

    void
    os_eventq_run(struct os_eventq *evq)
    {
        struct os_event *ev;

        ev = os_eventq_get(evq);
        assert(ev->ev_cb != NULL);

        ev->ev_cb(ev);
    }

