Config
======

Config subsystem is meant for persisting per-device configuration
and runtime state for packages.

Configuration items are stored as key-value pairs, where both the key and
the value are expected to be strings. Keys are divided into component
elements, where packages register their subtree using the first element.
E.g key ``id/serial`` consists of 2 components, ``id`` and ``serial``.
Package sys/id registered it's subtree of configuration elements to be
under ``id``.

There are convenience routines for converting value back and forth
from string.

Handlers
~~~~~~~~
Config handlers for subtree implement a set of handler functions.
These are registered using a call to ``conf_register()``.

- ch_get
    This gets called when somebody asks for a config element value
    by it's name using ``conf_get_value()``.

- ch_set
    This is called when value is being set using ``conf_set_value()``, and
    also when configuration is loaded from persisted storage with
    ``conf_load()``.

- ch_commit
    This gets called after configuration has been loaded in full.
    Sometimes you don't want individual configuration value to take
    effect right away, for example if there are multiple settings
    which are interdependent.

- ch_export
    This gets called to dump all current configuration. This happens
    when ``conf_save()`` tries to save the settings, or when CLI is
    dumping current system configuration to console.

Persistence
~~~~~~~~~~~
Backend storage for the config can be either FCB, a file in filesystem,
or both.

You can declare multiple sources for configuration; settings from
all of these are restored when ``conf_load()`` is called.

There can be only one target for writing configuration; this is where
data is stored when you call ``conf_save()``, or conf_save_one().

FCB read target is registered using ``conf_fcb_src()``, and write target
using ``conf_fcb_dst()``. ``conf_fcb_src()`` as side-effect initializes the
FCB area, so it must be called when calling ``conf_fcb_dst()``.
File read target is registered using ``conf_file_src()``, and write target
``conf_file_dst()``.

Convenience initialization of one config area can be enabled by
setting either syscfg variable CONFIG_FCB or CONFIG_NFFS. These
use other syscfg variables to figure which flash_map entry of BSP
defines flash area, or which file to use. Check the syscfg.yml in
sys/config package for more detailed description.

CLI
~~~
This can be enabled when shell package is enabled by setting syscfg
variable CONFIG_CLI to 1.

Here you can set config variables, inspect their values and print
both saved configuration as well as running configuration.

- config dump
    Dump the current running configuration

- config dump saved
    Dump the saved configuration. The values are printed in the ordered
    they're restored.

- config <key>
    Print the value of config variable identified by <key>

- config <key> <value>
    Set the value of config variable <key> to be <value>

Example: Device Configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This is a simple example, config handler only implements ``ch_set`` and
``ch_export``. ``ch_set`` is called when value is restored from storage (or
when set initially), and ``ch_export`` is used to write the value to
storage (or dump to console).

.. code:: c

    static int8 foo_val;

    struct conf_handler my_conf = {
	.ch_name = "foo",
	.ch_set = foo_conf_set,
	.ch_export = foo_conf_export
    }

    static int
    foo_conf_set(int argc, char **argv, char *val)
    {
         if (argc == 1) {
	      if (!strcmp(argv[0], "bar")) {
	             return CONF_VALUE_SET(val, CONF_INT8, foo_val);
	      }
	 }
	 return OS_ENOENT;
    }

    static int
    foo_conf_export(void (*func)(char *name, char *val), enum conf_export_tgt tgt)
    {
        char buf[4];

        conf_str_from_value(CONF_INT8, &foo_val, buf, sizeof(buf));
	func("foo/bar", buf)
	return 0;
    }

Example: Persist Runtime State
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This is a simple example showing how to persist runtime state. Here
there is only ``ch_set`` defined, which is used when restoring value from
persisted storage.

There's a os_callout function which increments foo_val, and then
persists the latest number. When system restarts, and calls ``conf_load()``,
foo_val will continue counting up from where it was before restart.

.. code:: c

    static int8 foo_val;

    struct conf_handler my_conf = {
	.ch_name = "foo",
	.ch_set = foo_conf_set
    }

    static int
    foo_conf_set(int argc, char **argv, char *val)
    {
         if (argc == 1) {
	      if (!strcmp(argv[0], "bar")) {
	             return CONF_VALUE_SET(val, CONF_INT8, foo_val);
	      }
	 }
	 return OS_ENOENT;
    }

    static void
    foo_callout(struct os_event *ev)
    {
        struct os_callout *c = (struct os_callout *)ev;
        char buf[4];

	foo_val++;
        conf_str_from_value(CONF_INT8, &foo_val, buf, sizeof(buf));
	conf_save_one("foo/bar", bar);

	callout_reset(c, OS_TICKS_PER_SEC * 120);
    }

API
~~~

.. doxygengroup:: sys_config
    :content-only:
