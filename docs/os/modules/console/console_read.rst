 console\_read 
---------------

.. code:: c

    int console_read(char *str, int cnt, int *newline)

Copies up to *cnt* bytes of received data to buffer pointed by *str*.
Function tries to break the input into separate lines; once it
encounters a newline character, it replaces that with end-of-string and
returns.

Arguments
^^^^^^^^^

+--------------+----------------+
| Arguments    | Description    |
+==============+================+
| ``str``      | Buffer where   |
|              | data is copied |
|              | to.            |
+--------------+----------------+
| ``cnt``      | Maximum number |
|              | of characters  |
|              | to copy.       |
+--------------+----------------+
| ``newline``  | Pointer to an  |
|              | integer        |
|              | variable that  |
|              | is set to 1    |
|              | when an        |
|              | newline        |
|              | character is   |
|              | received and   |
|              | set to 0       |
|              | otherwise.     |
+--------------+----------------+

Returned values
^^^^^^^^^^^^^^^

Returns the number of characters copied. 0 if there was no data
available, or if the first received character was ':raw-latex:`\n`'.

Example
^^^^^^^

.. code:: c

    #define MAX_INPUT 128

    static void
    read_function(void *arg)
    {
        char buf[MAX_INPUT];
        int rc; 
        int full_line;
        int off;

        off = 0;
        while (1) {
            rc = console_read(buf + off, MAX_INPUT - off, &full_line);
            if (rc <= 0 && !full_line) {
                continue;
            }
            off += rc;
            if (!full_line) {
                if (off == MAX_INPUT) {
                    /*
                     * Full line, no newline yet. Reset the input buffer.
                     */
                    off = 0;
                }
                continue;
            }
            /* Got full line - break out of the loop and process the input data */
            break;
        }
      
        /* Process the input line here */
         ....

        return;
    }    
