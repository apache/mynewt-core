fcb\_getnext
------------

.. code-block:: console

    int fcb_getnext(struct fcb *, struct fcb_entry *loc);

Given element in location in loc, return with loc filled in with
information about next element.

If loc->le\_elem\_off is set to 0, fcb\_getnext() will return info about
the oldest element in FCB.

Entry data can be read within the callback using flash\_area\_read(),
using loc->fe\_area, loc->fe\_data\_off, and loc->fe\_data\_len as
arguments.

Arguments
^^^^^^^^^

+-------------+-------------------------------------------+
| Arguments   | Description                               |
+=============+===========================================+
| fcb         | Points to FCB where data is written to.   |
+-------------+-------------------------------------------+
| loc         | Info about element. On successful call    |
+-------------+-------------------------------------------+

Returned values
^^^^^^^^^^^^^^^

Returns 0 on success; nonzero on failure. Returns FCB\_ERR\_NOVAR when
there are no more elements left.

Notes
^^^^^

Example
^^^^^^^
