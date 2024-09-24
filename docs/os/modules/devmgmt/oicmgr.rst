Using the OIC Framework
-----------------------

Apache Mynewt includes support for the OIC interoperability standard
through the ``oicmgr`` framework. Mynewt defines and exposes oicmgr as
an OIC Server resource with the following identity and properties:

+-----------------------+--------------------------------------+
| **URI**               | /omgr                                |
+-----------------------+--------------------------------------+
| **Resource Type** (rt)| x.mynewt.nmgr                        |
+-----------------------+--------------------------------------+
| **Interface** (if)    | oic.if_rw (default), oic.if.baseline |
+-----------------------+--------------------------------------+
| **Discoverable**      |  Yes                                 |
+-----------------------+--------------------------------------+

The newtmgr application tool uses CoAP (Constrained ApplicationProtocol) requests to send commands to oicmgr.
It sends a CoAP request for **/omgr** as follows:

-  Specifies the newtmgr command to execute in the URI query string.
-  Initially the GET method was used for newtmgr commands that retreive information from
   your application, for example, the ``taskstat`` and ``mpstat``
   commands. Now it uses the PUT operation as described below..
-  Uses a PUT method for newtmgr commands that send data to or modify
   the state of your application, for example, the ``echo`` or
   ``datetime`` commands.
-  Sends the CBOR-encoded command request data in the CoAP message
   payload.

The ``oicmgr`` framework supports transport over BLE, serial, and IP
connections to the device.

NewtMgr Protocol Specifics
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Requests
^^^^^^^^^
1. The NMP op is indicated by the OIC op. The OIC op is always the same: put.
2. There are no URI Query CoAP options.
3. The NMP header is included in the payload as a key-value pair (key="_h"). This pair is in the root map of the request and is a sibling of the other request fields. The value of this pair is the big-endian eight-byte NMP header with a length field of 0.

Responses
^^^^^^^^^
1. As with requests, the NMP header is included in the payload as a key-value pair (key="_h").
2. There is no "r" or "w" field. The response fields are inserted into the root map as a sibling of the "_h" pair.
3. Errors encountered during processing of NMP requests are reported identically to other NMP responses (embedded NMP response).

Notes
^^^^^
Keys that start with an underscore are reserved to the OIC manager protocol (e.g., "_h"). NMP requests and responses must not name any of their fields with a leading underscore.
