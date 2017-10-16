 shell\_nlip\_output 
---------------------

.. code-block:: console

    int shell_nlip_output(struct os_mbuf *m)

Queues the outgoing newtmgr message for transmission. The shell encodes
the message, frames the message into packets, and writes each packet to
the console.

Arguments
^^^^^^^^^

+-------------+-----------------------------------+
| Arguments   | Description                       |
+=============+===================================+
| m           | os\_mbuf containing the message   |
+-------------+-----------------------------------+

Returned values
^^^^^^^^^^^^^^^

Returns 0 on success.

Non-zero on failure.

Example
^^^^^^^

.. code-block:: console

    static int 
    nmgr_shell_out(struct nmgr_transport *nt, struct os_mbuf *m)
    {
        int rc;

        rc = shell_nlip_output(m);
        if (rc != 0) {
            goto err;
        }

        return (0);
    err:
        return (rc);
    }
