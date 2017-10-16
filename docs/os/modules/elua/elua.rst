elua
====

Description
~~~~~~~~~~~

This package contains a Lua interpreter. See http://lua.org for
documentation of the language.

You can execute lua scripts either from console with shell or start the
execution programmatically.

Data structures
~~~~~~~~~~~~~~~

Notes
~~~~~

Currently we don't have language extension modules which would go
together with this one, but those should be added.

List of Functions
~~~~~~~~~~~~~~~~~

+------------+----------------+
| Function   | Description    |
+============+================+
| `lua\_init | Registers      |
|  <lua_init | 'lua' command  |
| .html>`__    | with shell.    |
+------------+----------------+
| `lua\_main | Executes lua   |
|  <lua_main | script in      |
| .html>`__    | current task's |
|            | context.       |
|            | Arguments      |
|            | given are      |
|            | passed to lua  |
|            | interpreter.   |
+------------+----------------+


