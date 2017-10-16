fcb\_append\_finish
-------------------

.. code-block:: console

    int fcb_append_finish(struct fcb *fcb, struct fcb_entry *append_loc);

Finalizes the write of new element. FCB computes the checksum over the
element and updates it in flash.

Arguments
^^^^^^^^^

+--------------+----------------+
| Arguments    | Description    |
+==============+================+
| fcb          | Points to FCB  |
|              | where data is  |
|              | written to.    |
+--------------+----------------+
| append\_loc  | Pointer to     |
|              | fcb\_entry.    |
|              | Use the        |
|              | fcb\_entry     |
|              | returned by    |
|              | fcb\_append(). |
+--------------+----------------+

Returned values
^^^^^^^^^^^^^^^

Returns 0 on success; nonzero on failure.

Notes
^^^^^

You need to call fcb\_append\_finish() after writing the element
contents. Otherwise FCB will consider this entry to be invalid, and
skips over it when reading.

Example
^^^^^^^
