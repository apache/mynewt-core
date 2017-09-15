os\_mbuf\_get
-------------

.. code:: c

    struct os_mbuf *os_mbuf_get(struct os_mbuf_pool *omp, uint16_t leadingspace)

Get an mbuf from the mbuf pool. The mbuf is allocated, and initialized
prior to being returned. The *leadingspace* parameter allows the user to
specify the amount of leading space in the allocated mbuf.

Arguments
^^^^^^^^^

+--------------+----------------+
| Arguments    | Description    |
+==============+================+
| om           | Pointer to     |
|              | mbuf pool from |
|              | which to       |
|              | allocate mbuf  |
+--------------+----------------+
| leadingspace | Amount of      |
|              | leading space  |
|              | in allocated   |
|              | mbuf. Request  |
|              | cannot exceed  |
|              | the mbuf data  |
|              | buffer size.   |
+--------------+----------------+

Returned values
^^^^^^^^^^^^^^^

Returns a pointer to the allocated mbuf or NULL if there are no mbufs
available or *leadingspace* was too large.

Notes
^^^^^

In most typical applications, the application developer does not need to
call ``os_mbuf_get()``; the other API will do this automatically.
However, this API is provided for convenience as mbufs can also be a
simple way to allocate temporary chunks of memory.

Example
^^^^^^^

.. code:: c

        struct os_mbuf *om;

        /* Get an mbuf */
        om = os_mbuf_get(&g_mbuf_pool, 0);
        if (om) {
            /* we have allocated an mbuf from the pool */
        }
