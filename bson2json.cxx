#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON_BSON.h"

#include <vector>
#include <iostream>

int usage(int argc, char* argv[], const char* msg, int status)
{
  std::cerr
    << "\nUsage\n"
    << "=====\n\n"
    << "  " << (argc > 0 ? argv[0] : "bson2json") << " input.bson output.json\n"
    << "\n";
  if (msg)
    std::cerr
      << "\nStatus\n"
      << "======\n\n"
      << msg << "\n\n";

  return status;
}

int main(int argc, char* argv[])
{
  if (argc < 3)
    return usage(argc, argv, "Please specify input and output filenames.", 1);

  // Read in the BSON data.
  FILE* fid = fopen(argv[1], "rb");
  if (!fid)
    return usage(argc, argv, "Unable to open input file.", 3);
  fseek(fid, 0L, SEEK_END);
  size_t bson_size = ftell(fid);
  std::vector<char> bson;
  bson.resize(bson_size);
  fseek(fid, 0L, SEEK_SET);
  fread(&bson[0], bson_size, 1, fid);
  fclose(fid);

  cJSON* node = cJSON_ParseBSON(&bson[0], bson_size, cJSON_NULL);
  if (!node)
    return usage(argc, argv, "Unable to parse input file.", 5);
  char* json = cJSON_Print(node);
  cJSON_Delete(node);

  fid = fopen(argv[2], "w");
  if (!fid)
    return usage(argc, argv, "Unable to open output file.", 7);
  fwrite(json, strlen(json), 1, fid);
  fclose(fid);
  free(json);

  return 0;
}
