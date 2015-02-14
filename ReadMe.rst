=========================
cJSON_ with BSON_ Support
=========================

.. image:: https://travis-ci.org/vibraphone/cjson-bson.svg
   :alt: Travis-CI Build Status
   :target: https://travis-ci.org/vibraphone/cjson-bson
   :align: right


This is a redistribution of cJSON_ with additional files
that allow you to generate BSON_ from ``cJSON*`` records
and parse BSON_ into ``cJSON*`` records.


-----
Usage
-----

Given a cJSON object, you can obtain a buffer containing
its BSON serialization like so:

.. code:: c

    cJSON* node = cJSON_Parse(data);
    if (!node)
      return usage(argc, argv, "Could parse input file.");

    size_t sz; // Will hold returned size of buf
    char* buf = cJSON_PrintBSON(node, &sz);

You can also parse data from a BSON byte-stream into a
cJSON record like so:

.. code:: c

    size_t bson_size;
    char* bson = malloc(bson_size);
    fread(bson, bson_size, 1, fid);

    cJSON* node = cJSON_ParseBSON(bson, bson_size);
    free(bson);
    // Now you can traverse node or print, like so:
    char* json = cJSON_Print(node);

-----------------
Optional features
-----------------

When generating BSON from a cJSON structure,
there is an option to serialize UUIDs as binary data,
which is a significant savings (a 37-byte string
plus the surrounding BSON tokens = 42 bytes per UUID vs 22 bytes
in binary form).
However, recognizing strings that are UUIDs is not without
cost, so it is not done by default:

.. code:: c

    // Ask for UUID strings to be serialized as binary UUIDs:
    cJSON_BSON_SetDetectUUIDs(1);

When parsing BSON into a cJSON structure,
you can avoid the overhead of some string conversions
by having ``cJSON_ParseBSON`` abuse some members of cJSON
records:

.. code:: c

    // Ask for binary blobs to be stored raw rather than
    // converted to hexadecimal strings.
    cJSON_BSON_SetUseExtendedTypes(1);

The blobs themselves are just stored as non-null-terminated
data in the valuestring member and the *subtype* of the the
binary data is stored in valuestring.
The ``cJSON_BSON.h`` header provides some additional
item type values for binary blobs of data.

.. _cJSON: https://sourceforge.net/projects/cjson/
.. _BSON: http://bsonspec.org/
