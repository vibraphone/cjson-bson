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
its BSON serialization like so::

    cJSON* node = cJSON_Parse(data);
    if (!node)
      return usage(argc, argv, "Could parse input file.");

    // Ask for UUID strings to be serialized as binary UUIDs:
    cJSON_BSON_SetDetectUUIDs(1);

    size_t sz; // Will hold returned size of buf
    char* buf = cJSON_PrintBSON(node, &sz);

As you can see above, there is an option to serialize UUIDs as
binary data, which is a significant savings (a 37-byte string
plus the surrounding BSON tokens = 42 bytes per UUID vs 22 bytes
in binary form).
However, recognizing strings that are UUIDs is not without
cost, so it is not done by default.

You can also parse data from a BSON byte-stream into a
cJSON record like so::

    FILE* fid;
    size_t bson_size;
    char* bson = malloc(bson_size); 
    fread(bson, bson_size, 1, fid);
    fclose(fid);

    cJSON* node = cJSON_ParseBSON(bson, bson_size);
    if (!node)
      return usage(argc, argv, "Unable to parse input file.", 5);
    char* json = cJSON_Print(node);
    cJSON_Delete(node);

.. _cJSON: https://sourceforge.net/projects/cjson/
.. _BSON: http://bsonspec.org/
