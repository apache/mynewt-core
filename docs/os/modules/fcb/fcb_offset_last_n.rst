fcb\_offset\_last\_n
--------------------

.. code-block:: console

    int fcb_offset_last_n(struct fcb *fcb, uint8_t entries, uint32_t *last_n_off);

Returns the offset of n-th last element.

Arguments
^^^^^^^^^

+----------------+------------------------------+
| Arguments      | Description                  |
+================+==============================+
| fcb            | Points to FCB.               |
+----------------+------------------------------+
| entries        | How many entries to leave.   |
+----------------+------------------------------+
| last\_n\_off   | Returned offset.             |
+----------------+------------------------------+

Returned values
^^^^^^^^^^^^^^^

0 on success; non-zero on failure.

Notes
^^^^^

Returned offset is relative to beginning of the sector where the element
is. Therefore, n-th last element must be found within the last sector of
FCB.

Example
^^^^^^^
