Heap
====

API for doing dynamic memory allocation.

Description
~~~~~~~~~~~

This provides malloc()/free() functionality with locking. The shared
resource heap needs to be protected from concurrent access when OS has
been started. *os\_malloc()* function grabs a mutex before calling
*malloc()*.

Data structures
~~~~~~~~~~~~~~~

N/A

List of Functions
~~~~~~~~~~~~~~~~~

The functions available in heap are:

+--------------+----------------+
| **Function** | **Description* |
|              | *              |
+==============+================+
| `os\_free <o | Frees          |
| s_free>`__   | previously     |
|              | allocated      |
|              | memory back to |
|              | the heap.      |
+--------------+----------------+
| `os\_malloc  | Allocates the  |
| <os_malloc>` | given number   |
| __           | of bytes from  |
|              | heap and       |
|              | returns a      |
|              | pointer to it. |
+--------------+----------------+
| `os\_realloc | Tries to       |
|  <os_realloc | resize         |
| >`__         | previously     |
|              | allocated      |
|              | memory block,  |
|              | and returns    |
|              | pointer to     |
|              | resized        |
|              | memory.        |
+--------------+----------------+
