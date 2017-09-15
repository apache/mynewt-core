 os\_eventq\_put
----------------

.. code:: c

    void
    os_eventq_put(struct os_eventq *evq, struct os_event *ev)

Enqueues an event onto the tail of an event queue. It puts a task, if
any, that is in the ``sleeping`` state waiting for an event into the
``ready-to-run`` state.

Arguments
^^^^^^^^^

+-------------+-----------------------------------+
| Arguments   | Description                       |
+=============+===================================+
| ``evq``     | Event queue to add the event to   |
+-------------+-----------------------------------+
| ``ev``      | Event to enqueue                  |
+-------------+-----------------------------------+

Returned values
^^^^^^^^^^^^^^^

N/A

Example
^^^^^^^

 This example shows the ble host adding a host controller interface
event to the ``ble_hs_eventq`` event queue.

.. code:: c


    /* Returns the ble_hs_eventq */
    static struct os_eventq *
    ble_hs_evq_get(void)
    {
        return ble_hs_evq;
    }

    /* Event callback function */
    static void
    ble_hs_event_rx_hci_ev(struct os_event *ev)
    {
        uint8_t *hci_evt;
        int rc;

        hci_evt = ev->ev_arg;
        rc = os_memblock_put(&ble_hs_hci_ev_pool, ev);
        BLE_HS_DBG_ASSERT_EVAL(rc == 0);

        ble_hs_hci_evt_process(hci_evt);
    }

    void
    ble_hs_enqueue_hci_event(uint8_t *hci_evt)
    {
        struct os_event *ev;

        /* Allocates memory for the event */
        ev = os_memblock_get(&ble_hs_hci_ev_pool);

        if (ev == NULL) {
            ble_hci_trans_buf_free(hci_evt);
        } else {
            /* 
             * Initializes the event with the ble_hs_event_rx_hci_ev callback function 
             * and the hci_evt data for the callback function to use. 
             */ 
            ev->ev_queued = 0;
            ev->ev_cb = ble_hs_event_rx_hci_ev;
            ev->ev_arg = hci_evt;

            /* Enqueues the event on the ble_hs_evq */
            os_eventq_put(ble_hs_evq_get(), ev);
        }
    }
