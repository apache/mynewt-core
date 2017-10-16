fcb\_append\_to\_scratch
------------------------

.. code-block:: console

    int fcb_append_to_scratch(struct fcb *fcb);

This can be used if FCB created to have scratch block(s). Once FCB fills
up with data, fcb\_append() will fail. This routine can be called to
start using the reserve block.

Arguments
^^^^^^^^^

+-------------+------------------+
| Arguments   | Description      |
+=============+==================+
| fcb         | Points to FCB.   |
+-------------+------------------+

Returned values
^^^^^^^^^^^^^^^

Returns 0 on success; nonzero on failure.

Notes
^^^^^

Example
^^^^^^^
