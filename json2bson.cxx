#include <stdlib.h>

#include "cJSON_BSON.h"

#include <fstream>
#include <iostream>

int usage(int argc, char* argv[], const char* msg, int status)
{
  std::cerr
    << "\nUsage\n"
    << "=====\n\n"
    << "  " << (argc > 0 ? argv[0] : "json2bson") << " input.json output.bson\n"
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

  std::ifstream file(argv[1]);
  if (!file.good())
    return usage(argc, argv, "Could not open input file.", 3);
  std::string data(
    (std::istreambuf_iterator<char>(file)),
    (std::istreambuf_iterator<char>()));

  cJSON* node = cJSON_Parse(data.c_str());
  if (!node)
    return usage(argc, argv, "Could parse input file.", 5);

  // Ask for UUID strings to be serialized as binary UUIDs:
  cJSON_BSON_SetDetectUUIDs(1);

  size_t sz; // will hold returned size of buf
  char* buf = cJSON_PrintBSON(node, &sz);

  cJSON_Delete(node);
  FILE* fid = fopen(argv[2], "wb");
  if (!fid)
    return usage(argc, argv, "Could not open output file.", 7);
  fwrite(buf, sz, 1, fid);
  fclose(fid);
  free(buf);

  return 0;
}
