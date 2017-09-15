os\_mqueue\_init
----------------

.. code:: c

    int
    os_mqueue_init(struct os_mqueue *mq, os_event_fn *ev_cb, void *arg)

Initializes an mqueue. An mqueue is a queue of mbufs that ties to a
particular task's event queue. Mqueues form a helper API around a common
paradigm: wait on an event queue until at least one packet is available,
then process a queue of packets.

Arguments
^^^^^^^^^

+--------------+----------------+
| Arguments    | Description    |
+==============+================+
| ``mq``       | The mqueue to  |
|              | initialize     |
+--------------+----------------+
| ``ev_cb``    | The callback   |
|              | to associate   |
|              | with the       |
|              | mqeueue event. |
|              | Typically,     |
|              | this callback  |
|              | pulls each     |
|              | packet off the |
|              | mqueue and     |
|              | processes      |
|              | them.          |
+--------------+----------------+
| ``arg``      | The argument   |
|              | to associate   |
|              | with the       |
|              | mqueue event.  |
+--------------+----------------+

@return 0 on success, non-zero on failure.

Initializes an mqueue. Sets the event argument in the os event of the
mqueue to ``arg``.

Arguments
^^^^^^^^^

+-------------+---------------------------------+
| Arguments   | Description                     |
+=============+=================================+
| mq          | Pointer to a mqueue structure   |
+-------------+---------------------------------+
| arg         | Event argument                  |
+-------------+---------------------------------+

Returned values
^^^^^^^^^^^^^^^

0: success. All other values indicate an error

Example
^^^^^^^

.. code:: c

    /* Event callback to execute when a packet is received. */
    extern void process_rx_data_queue(void);

    /* Declare mqueue */
    struct os_mqueue rxpkt_q;

    /* Initialize mqueue */
    os_mqueue_init(&rxpkt_q, process_rx_data_queue, NULL);
