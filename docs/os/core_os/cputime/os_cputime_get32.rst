os\_cputime\_get32
------------------

.. code:: c

    uint32_t os_cputime_get32(void)

Gets the current cputime. If a timer is 64 bits, only the lower 32 bit
is returned.

Arguments
^^^^^^^^^

N/A

Returned values
^^^^^^^^^^^^^^^

The current cputime.

Notes
^^^^^

Example
^^^^^^^

.. code:: c

    uint32 cur_cputime;
    cur_cputime = os_cputime_get32();
