Using the OIC Framework
-----------------------

Apache Mynewt includes support for the OIC interoperability standard
through the ``oicmgr`` framework. Mynewt defines and exposes oicmgr as
an OIC Server resource with the following identity and properties:

.. raw:: html

   <table style="width:50%" align="center">

.. raw:: html

   <tr>

.. raw:: html

   <td>

**URI**

.. raw:: html

   </td>

.. raw:: html

   <td>

/omgr

.. raw:: html

   </td>

.. raw:: html

   </tr>

.. raw:: html

   <tr>

.. raw:: html

   <td>

**Resource Type**\ (rt)

.. raw:: html

   </td>

.. raw:: html

   <td>

x.mynewt.nmgr

.. raw:: html

   </td>

.. raw:: html

   </tr>

.. raw:: html

   <td>

**Interface**\ (if)

.. raw:: html

   </td>

.. raw:: html

   <td>

oic.if\_rw (default), oic.if.baseline

.. raw:: html

   </td>

.. raw:: html

   </tr>

.. raw:: html

   <td>

**Discoverable**

.. raw:: html

   </td>

.. raw:: html

   <td>

Yes

.. raw:: html

   </td>

.. raw:: html

   </tr>

.. raw:: html

   </table>

| The newtmgr application tool uses CoAP (Constrained Application
  Protocol) requests to send commands to oicmgr.
| It sends a CoAP request for **/omgr** as follows:

-  Specifies the newtmgr command to execute in the URI query string.
-  Uses a GET method for newtmgr commands that retreive information from
   your application, for example, the ``taskstat`` and ``mpstat``
   commands.
-  Uses a PUT method for newtmgr commands that send data to or modify
   the state of your application, for example, the ``echo`` or
   ``datetime`` commands.
-  Sends the CBOR-encoded command request data in the CoAP message
   payload.

The ``oicmgr`` framework supports transport over BLE, serial, and IP
connections to the device.
