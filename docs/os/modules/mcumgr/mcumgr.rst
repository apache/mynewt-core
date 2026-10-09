MCU Manager (mcumgr)
--------------------

.. toctree::
   :maxdepth: 1

mcumgr, a derivative of newtmgr, is the device management library of Apache Mynewt. It implements the device side of
the management protocol and provides command handlers for image, file system, log, statistics and OS management. mcumgr
is located in ``mgmt/mcumgr`` directory of the ``apache-mynewt-core`` repository. Previously it was maintained in a
separate ``mynewt-mcumgr`` repository with an OS abstraction layer.

Two wire formats are supported:

- SMP (Simple Management Protocol, previously called NMP): simple binary format with 8-byte header plus CBOR payload
- OMP: SMP requests sent as CoAP requests to the ``/omgr`` CoAP resource (see ``mgmt/oicmgr``)

A request has the same effect on the receiving device regardless of wire format, but OMP fits more cleanly in a system
that is already using CoAP.

Protocol and transport documentation can be found in ``mgmt/mcumgr/docs`` directory and an example application is
available in ``apps/smp_svr``.
