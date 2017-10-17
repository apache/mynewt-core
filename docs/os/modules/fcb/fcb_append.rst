fcb\_append
-----------

.. code-block:: console

    int fcb_append(struct fcb *fcb, uint16_t len, struct fcb_entry *append_loc);

Start writing a new element to flash. This routine reserves the space in
the flash by writing out the element header.

When writing the contents for the entry, use append\_loc->fl\_area and
append\_loc->fl\_data\_off as arguments to flash\_area\_write(). When
finished, call fcb\_append\_finish() with append\_loc as argument.

Arguments
^^^^^^^^^

+--------------+----------------+
| Arguments    | Description    |
+==============+================+
| fcb          | Points to FCB  |
|              | where data is  |
|              | written to.    |
+--------------+----------------+
| len          | Number of      |
|              | bytes to       |
|              | reserve for    |
|              | the element.   |
+--------------+----------------+
| loc          | Pointer to     |
|              | fcb\_entry.    |
|              | fcb\_append()  |
|              | will fill this |
|              | with info      |
|              | about where    |
|              | the element    |
|              | can be written |
|              | to.            |
+--------------+----------------+

Returned values
^^^^^^^^^^^^^^^

Returns 0 on success; nonzero on failure. FCB\_ERR\_NOSPACE is returned
if FCB is full.

Notes
^^^^^

If FCB is full, you need to make more space. This can be done by calling
fcb\_rotate(). Or if you've reserved scratch sectors, you can take those
into use by calling fcb\_append\_to\_scratch().

Example
^^^^^^^
