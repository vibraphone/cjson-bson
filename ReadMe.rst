=======================
cJSON with BSON Support
=======================

This is a redistribution of cJSON_ with additional files
that allow you to generate BSON_ from `cJSON*` records
and parse BSON into `cJSON*` records.


-----
Usage
-----

Given a cJSON object, you can obtain a buffer containing
its BSON serialization like so:

.. literalinclude:: json2bson.cxx
   :start-after: // ++ 1 ++
   :end-before: // -- 1 --
   :linenos:

There is an option to serialize UUIDs as binary data, which
is a significant savings (a 37-byte string plus the surrounding
BSON tokens = 42 bytes per UUID vs 22 bytes in binary form).
However, recognizing strings that are UUIDs is not without
cost, so it is not done by default.

You can also parse data from a BSON byte-stream into a
cJSON record like so:

.. literalinclude:: bson2json.cxx
   :start-after: // ++ 1 ++
   :end-before: // -- 1 --
   :linenos:

.. _cJSON: https://sourceforge.net/projects/cjson/
.. _BSON: http://bsonspec.org/
