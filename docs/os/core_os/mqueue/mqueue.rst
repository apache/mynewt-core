Mqueue
======

The mqueue construct allows a task to wake up when it receives data.
Typically, this data is in the form of packets received over a network.
A common networking stack operation is to put a packet on a queue and
post an event to the task monitoring that queue. When the task handles
the event, it processes each packet on the packet queue.

Using Mqueue
--------------

The following code sample demonstrates how to use an mqueue. In this
example:

-  packets are put on a receive queue
-  a task processes each packet on the queue (increments a receive
   counter)

Not shown in the code example is a call ``my_task_rx_data_func``.
Presumably, some other code will call this API.

.. code:: c

    uint32_t pkts_rxd;
    struct os_mqueue rxpkt_q;
    struct os_eventq my_task_evq;

    /**
     * Removes each packet from the receive queue and processes it.
     */
    void
    process_rx_data_queue(void)
    {
        struct os_mbuf *om;

        while ((om = os_mqueue_get(&rxpkt_q)) != NULL) {
            ++pkts_rxd;
            os_mbuf_free_chain(om);
        }
    }

    /**
     * Called when a packet is received.
     */
    int
    my_task_rx_data_func(struct os_mbuf *om)
    {
        int rc;

        /* Enqueue the received packet and wake up the listening task. */
        rc = os_mqueue_put(&rxpkt_q, &my_task_evq, om);
        if (rc != 0) {
            return -1;
        }

        return 0;
    }

    void
    my_task_handler(void *arg)
    {
        struct os_event *ev;
        struct os_callout_func *cf;
        int rc;

        /* Initialize eventq */
        os_eventq_init(&my_task_evq);

        /* Initialize mqueue */
        os_mqueue_init(&rxpkt_q, NULL);

        /* Process each event posted to our eventq.  When there are no events to
         * process, sleep until one arrives.
         */
        while (1) {
            os_eventq_run(&my_task_evq);
        }
    }

Data Structures
----------------

.. code:: c

    struct os_mqueue {
        STAILQ_HEAD(, os_mbuf_pkthdr) mq_head;
        struct os_event mq_ev;
    };

API
-----------------

.. doxygengroup:: OSMqueue
    :content-only:
