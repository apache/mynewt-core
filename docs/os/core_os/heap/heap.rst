Heap
====

API for doing dynamic memory allocation.

Description
------------

This provides malloc()/free() functionality with locking. The shared
resource heap needs to be protected from concurrent access when OS has
been started. *os\_malloc()* function grabs a mutex before calling
*malloc()*.

Data structures
-----------------

N/A

API
----

See the API for OS level function calls.

-  `Mynewt Core OS <../mynewt_os.html>`__

