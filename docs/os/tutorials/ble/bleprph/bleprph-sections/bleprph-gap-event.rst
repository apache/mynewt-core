GAP Event callbacks
===================

Overview
^^^^^^^^

Every BLE connection has a *GAP event callback* associated with it. A
GAP event callback is a bit of application code which NimBLE uses to
inform you of connection-related events. For example, if a connection is
terminated, NimBLE lets you know about it with a call to that
connection's callback.

In the `advertising section <bleprph-adv/>`__ of this tutorial, we saw
how the application specifies a GAP event callback when it begins
advertising. NimBLE uses this callback to notify the application that a
central has connected to your peripheral after receiving an
advertisement. Let's revisit how *bleprph* specifies its connection
callback when advertising:

.. code:: c

        /* Begin advertising. */
        memset(&adv_params, 0, sizeof adv_params);
        adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
        adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
        rc = ble_gap_adv_start(BLE_ADDR_TYPE_PUBLIC, 0, NULL, BLE_HS_FOREVER,
                               &adv_params, bleprph_gap_event, NULL);
        if (rc != 0) {
            BLEPRPH_LOG(ERROR, "error enabling advertisement; rc=%d\n", rc);
            return;
        }

bleprph\_gap\_event()
^^^^^^^^^^^^^^^^^^^^^

The ``bleprph_gap_event()`` function is *bleprph*'s GAP event callback;
NimBLE calls this function when the advertising operation leads to
connection establishment. Upon connection establishment, this callback
becomes permanently associated with the connection; all subsequent
events related to this connection are communicated through this
callback.

Now let's look at the function that *bleprph* uses for all its
connection callbacks: ``bleprph_gap_event()``.

.. code:: c

    /**
     * The nimble host executes this callback when a GAP event occurs.  The
     * application associates a GAP event callback with each connection that forms.
     * bleprph uses the same callback for all connections.
     *
     * @param event                 The type of event being signalled.
     * @param ctxt                  Various information pertaining to the event.
     * @param arg                   Application-specified argument; unuesd by
     *                                  bleprph.
     *
     * @return                      0 if the application successfully handled the
     *                                  event; nonzero on failure.  The semantics
     *                                  of the return code is specific to the
     *                                  particular GAP event being signalled.
     */
    static int
    bleprph_gap_event(struct ble_gap_event *event, void *arg)
    {
        struct ble_gap_conn_desc desc;
        int rc;

        switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            /* A new connection was established or a connection attempt failed. */
            BLEPRPH_LOG(INFO, "connection %s; status=%d ",
                           event->connect.status == 0 ? "established" : "failed",
                           event->connect.status);
            if (event->connect.status == 0) {
                rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
                assert(rc == 0);
                bleprph_print_conn_desc(&desc);
            }
            BLEPRPH_LOG(INFO, "\n");

            if (event->connect.status != 0) {
                /* Connection failed; resume advertising. */
                bleprph_advertise();
            }
            return 0;

        case BLE_GAP_EVENT_DISCONNECT:
            BLEPRPH_LOG(INFO, "disconnect; reason=%d ", event->disconnect.reason);
            bleprph_print_conn_desc(&event->disconnect.conn);
            BLEPRPH_LOG(INFO, "\n");

            /* Connection terminated; resume advertising. */
            bleprph_advertise();
            return 0;

        case BLE_GAP_EVENT_CONN_UPDATE:
            /* The central has updated the connection parameters. */
            BLEPRPH_LOG(INFO, "connection updated; status=%d ",
                        event->conn_update.status);
            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            assert(rc == 0);
            bleprph_print_conn_desc(&desc);
            BLEPRPH_LOG(INFO, "\n");
            return 0;

        case BLE_GAP_EVENT_ENC_CHANGE:
            /* Encryption has been enabled or disabled for this connection. */
            BLEPRPH_LOG(INFO, "encryption change event; status=%d ",
                        event->enc_change.status);
            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            assert(rc == 0);
            bleprph_print_conn_desc(&desc);
            BLEPRPH_LOG(INFO, "\n");
            return 0;

        case BLE_GAP_EVENT_SUBSCRIBE:
            BLEPRPH_LOG(INFO, "subscribe event; conn_handle=%d attr_handle=%d "
                              "reason=%d prevn=%d curn=%d previ=%d curi=%d\n",
                        event->subscribe.conn_handle,
                        event->subscribe.attr_handle,
                        event->subscribe.reason,
                        event->subscribe.prev_notify,
                        event->subscribe.cur_notify,
                        event->subscribe.prev_indicate,
                        event->subscribe.cur_indicate);
            return 0;
        }

        return 0;
    }

Connection callbacks are used to communicate a variety of events related
to a connection. An application determines the type of event that
occurred by inspecting the value of the *event->type* parameter. The
full list of event codes can be found on the `GAP
events <../../../network/ble/ble_hs/ble_gap/definitions/ble_gap_defs/>`__
page.

Guarantees
^^^^^^^^^^

It is important to know what your application code is allowed to do from
within a connection callback.

**No restrictions on NimBLE operations**

Your app is free to make calls into the NimBLE stack from within a
connection callback. *bleprph* takes advantage of this freedom when it
resumes advertising upon connection termination. All other NimBLE
operations are also allowed (service discovery, pairing initiation,
etc).

**All context data is transient**

Pointers in the context object point to data living on the stack. Your
callback is free to read (or write, if appropriate) through these
pointers, but you should not store these pointers for later use. If your
application needs to retain some data from a context object, it needs to
make a copy.
