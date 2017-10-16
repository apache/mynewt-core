 shell\_nlip\_input\_register 
------------------------------

.. code-block:: console

    int shell_nlip_input_register(shell_nlip_input_func_t nf, void *arg)

Registers a handler for incoming newtmgr messages. The shell receives
the incoming data stream from the UART, and when it detects a NLIP
frame, decodes the datagram, and calls the registered handler,\ ``nf``.

The handler function is of type
``int (*shell_nlip_input_func_t)(struct os_mbuf *m, void *arg)``. The
shell passes the incoming newtmgr message stored in a ``os_mbuf`` and
the ``arg`` that was passed to the ``shell_nlip_input_register()``
function as arguments to the handler function.

Arguments
^^^^^^^^^

+-------------+---------------------------------------------------------------------+
| Arguments   | Description                                                         |
+=============+=====================================================================+
| ``nf``      | Handler for incoming newtmgr datagrams.                             |
+-------------+---------------------------------------------------------------------+
| ``arg``     | Argument that is passed to this handler, along with the datagram.   |
+-------------+---------------------------------------------------------------------+

Returned values
^^^^^^^^^^^^^^^

Returns 0 on success.

Non-zero on failure.

Example
^^^^^^^

.. code-block:: console

    static int
    nmgr_shell_in(struct os_mbuf *m, void *arg)
    {
        ....
    }

    int 
    nmgr_task_init(uint8_t prio, os_stack_t *stack_ptr, uint16_t stack_len)
    {
        int rc;
        ....
        rc = shell_nlip_input_register(nmgr_shell_in, 
                (void *) &g_nmgr_shell_transport);
        if (rc != 0) {
            goto err;
        }
        ....
    }
