/*
  Copyright (c) 2009 Dave Gamble
                2015 Kitware, Inc.

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

/* cJSON */
/* JSON parser in C. */

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>
#include "cJSON.h"
#include "cJSON_BSON.h"

#include <assert.h>

#define cBSON_LinkSibling(prev, cur) \
  if (!**prev) \
    { \
    **prev = cur; \
    } \
  else \
    { \
    (**prev)->next = cur; \
    cur->prev = **prev; \
    *prev = &cur->next; \
    }

static void *(*cJSON_malloc)(size_t sz) = malloc;
static void (*cJSON_free)(void *ptr) = free;
static int shouldDetectUUIDsInStrings = 0; /* do not try this by default; it adds overhead. */
static int shouldUseExtendedTypes = 0; /* do not do this by default; it is incompatible. */

/* Duplicate the string using cJSON's malloc.
 * May return null if nullOK and given an empty \a str.
 * The length of the string is returned in len (including null terminator!).
 */
static char* cJSON_strdup(const char* str, size_t* len, int nullOK)
{
  char* copy;

  *len = str && str[0] ? (strlen(str) + 1) : 1;
  if (nullOK && *len == 1)
    return NULL;

  if (!(copy = (char*)cJSON_malloc(*len))) return 0;
  if (str)
    memcpy(copy,str,*len);
  copy[(*len) - 1] = '\0';
  return copy;
}

/**\brief Create a buffer holding a BSON enconding of \a item.
  *
  * You are responsible for calling cJSON_DeleteBSON() on the result.
  * Because the buffer includes null terminators, this function
  * returns the size of the allocated buffer in \a bufSizeOut,
  * a pointer to a size_t variable.
  */
char* cJSON_PrintBSON(cJSON *item, size_t* bufSizeOut)
{
  *bufSizeOut = bson_get_doc_size(item);
  char* bsonVal = (char*) cJSON_malloc(*bufSizeOut);
  bson_doc_value(item->child, bsonVal, *bufSizeOut, NULL);
  return bsonVal;
}

/**\brief Deallocate a BSON buffer created by cJSON_PrintBSON.
  */
void cJSON_DeleteBSON(char* bson)
{
  cJSON_free(bson);
}

/**\brief Call this with a positive integer to enable UUID detection.
  *
  * If set to a positive value, then cJSON_String entries being
  * processed by cJSON_PrintBSON will be examined and exported as
  * UUIDs if in the proper 37-character format (including null
  * terminator). Otherwise, cJSON_String entries will always appear
  * in BSON as cBSON_String entries.
  */
void cJSON_BSON_SetDetectUUIDs(int yes)
{
  shouldDetectUUIDsInStrings = yes ? 1 : 0;
}

/**\brief Return whether or not UUID detection is enabled.
  */
int cJSON_BSON_WillDetectUUIDs()
{
  return shouldDetectUUIDsInStrings;
}

/* Detect whether the given string encodes a UUID.
 *
 * This tests that the length of the string is correct
 * and that digits are all hexadecimal with hypens
 * in the correct places (i.e., of the form
 * "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx").
 */
int bson_is_string_uuid(const char* val)
{
  int i;

  if (!val || strlen(val) != 36)
    return 0;

  if (val[8] != '-' || val[13] != '-' || val[18] != '-' || val[23] != '-')
    return 0;

  for (i = 0; i < 4; ++i)
    if (
      !isxdigit(val[ 0 + i]) ||
      !isxdigit(val[ 4 + i]) ||
      !isxdigit(val[ 9 + i]) ||
      !isxdigit(val[14 + i]) ||
      !isxdigit(val[19 + i]) ||
      !isxdigit(val[24 + i]) ||
      !isxdigit(val[28 + i]) ||
      !isxdigit(val[32 + i]))
      return 0;
  return 1;
}

static void decode_hex_string(const char* in, size_t len, uint8_t* out)
{
  unsigned int i, t, hi, lo;

  for (t = 0, i = 0; i < len; i += 2, ++t)
    {
    int inhi = toupper(in[i]);
    int inlo = toupper(in[i + 1]);
    hi = inhi > '9' ? inhi - 'A' + 10 : inhi - '0';
    lo = inlo > '9' ? inlo - 'A' + 10 : inlo - '0';
    out[t] = (hi << 4 ) | lo;
    }
}

static void encode_hex_string(const uint8_t* in, size_t len, char* out)
{
  static const char convert[16] = "0123456789abcdef";
  size_t i;
  for (i = 0; i < len; ++i)
    {
    *(out++) = convert[((*in) & 0xf0) >> 4];
    *(out++) = convert[(*(in++)) & 0x0f];
    }
}

/* Write the UUID data (not including the type byte or name) to the output buffer.
 *
 * This writes the size, subtype, and UUID data to \a buf.
 * It presumes that \a src is of the correct form (i.e.,
 * "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx").
 */
char* bson_uuid_value_from_string(char* buf, const char* src)
{
  *((int32_t*)buf) = 16;
  char* loc = buf + 4;
  *(loc++) = cBSON_UUID;
  decode_hex_string(src +  0,  8, (uint8_t*)loc     );
  decode_hex_string(src +  9,  4, (uint8_t*)loc +  4);
  decode_hex_string(src + 14,  4, (uint8_t*)loc +  6);
  decode_hex_string(src + 19,  4, (uint8_t*)loc +  8);
  decode_hex_string(src + 24, 12, (uint8_t*)loc + 10);
  return buf + 21;
}

/**\brief Call with non-zero value to force the BSON parser to produce cJSON nodes with extended item->type values.
 */
void cJSON_BSON_SetUseExtendedTypes(int yes)
{
  shouldUseExtendedTypes = yes ? 1 : 0;
}

/**\brief Returns non-zero when the BSON parser will produce cJSON nodes with extended item->type values.
 */
int cJSON_BSON_WillUseExtendedTypes()
{
  return shouldUseExtendedTypes;
}

/* Return the size of the item treating it as the top-level
 * entry in a BSON document.
 */
size_t bson_get_doc_size(cJSON* item)
{
  /* 5 = 4 byte length + item size + 1-byte null terminator */
  size_t result = 5;
  cJSON* kid;
  for (kid = item->child; kid; kid = kid->next)
    result += bson_get_size(kid);
  return result;
}

/* Return the size of an item's contents (not including
 * its type byte or name.
 */
size_t bson_get_array_item_size(cJSON* item)
{
  size_t result = 0;
	if (!item) return result;
	switch ((item->type)&255)
	{
		case cJSON_NULL:
                      return 0; break; /* nothing */
		case cJSON_False:
		case cJSON_True:
                      return 1; break; /* bool(char) */
		case cJSON_Number:
                      return 8; break; /* double */
		case cJSON_String:
                      return item->valuestring ?
                        (shouldDetectUUIDsInStrings && bson_is_string_uuid(item->valuestring) ?
                         21 : /* size(4) + subtype(1) + UUID(16 bytes) */
                         5 + strlen(item->valuestring)) : /* size + value + terminator */
                        5; /* size + terminator */
                      break;
		case cJSON_Array:
                      return bson_get_array_size(item->child); break;
		case cJSON_Object: return bson_get_object_size(item->child); break;
	}
  /* TODO: Generate error of some sort. */
	return result;
}

/* Return the size of the arrays's contents (not including
 * its type byte or name.
 */
size_t bson_get_array_size(cJSON* item)
{
  /* arrays are tricky because BSON requires the keys to
   * be null-terminated, integer-valued strings (e.g., "0",
   * "1", ..., "42", ...), which vary in length depending
   * on the size of the array.
   */
  size_t numentries = 0;
  size_t valsize = 5; /* document size + terminator = 5 bytes */
  cJSON* kid;
  /* count the number of child items in the array and
   * calculate the size of the items' values without looking
   * at their item->string values. This is different than
   * dictionaries which use item->string as a key.
   */
  for (kid = item; kid; kid = kid->next, ++numentries)
    valsize += bson_get_array_item_size(kid);
  /* calculate the number of digits in all the keys.
   * The  first  10 (or fewer) have 1 digit (0-9).
   *     second  90 (or fewer) have 2 digits (10-99).
   *      third 900 (or fewer) have 3 digits (100-999).
   */
  /* every item has a type-byte plus a null terminator: */
  size_t keysize = 2 * numentries;
  size_t ndig = 10;
  size_t digitc = numentries;
  do
  {
    keysize += digitc;
    digitc -= (digitc > ndig ? ndig : digitc);
    ndig *= (ndig == 10 ? 9 : 10);
  } while (digitc > 0);
  return keysize + valsize;
}

/* Return the size of the object's contents (not including
 * its type byte or name.
 */
size_t bson_get_object_size(cJSON* item)
{
  size_t numentries = 0;
  size_t valsize = 4; /* 4 bytes for int32 record size */
  cJSON* kid;
  for (kid = item; kid; kid = kid->next, ++numentries)
    {
    /* TODO: Generate error when kid->string is NULL */
    valsize += bson_get_size(kid);
    }
  ++valsize; /* null terminator */
  return valsize;
}

/* Return the size of an item, including its type byte
 * and "C"-string name.
 */
size_t bson_get_size(cJSON* item)
{
  size_t result = 0;
	if (!item) return result;
  /* All elements have a type byte plus an optional name
   * string (which is always terminated): */
  result = 2 + (item->string ? strlen(item->string) : 0);
	switch ((item->type)&255)
	{
		case cJSON_NULL:	break;
		case cJSON_False:	++result; break;
		case cJSON_True:	++result; break;
		case cJSON_Number: result += sizeof(double); break;
		case cJSON_String:
      if (shouldDetectUUIDsInStrings && bson_is_string_uuid(item->valuestring))
        /* UUIDs are encoded as "binary" data with a subtype.
         * size(4) + subtype(1) + UUID(16 bytes) */
        result += 21;
      else
        /* UTF-8 strings are preceded by 4 bytes specifying length and
         * followed by a null-terminator. The length does not include
         * the item type or itself in the count, but does include the
         * null terminator. */
        result += item->valuestring ?
          strlen(item->valuestring) + 5 : 5;
      break;
		case cJSON_Array: result += bson_get_array_size(item->child); break;
		case cJSON_Object: result += bson_get_object_size(item->child); break;
	}
	return result;
}

/* Given an adequately sized buffer, encode the JSON \a item
 * in \a buf as a BSON document.
 */
char* bson_doc_value(cJSON* item, char* buf, size_t bufsize, ptrdiff_t* idxName)
{
  char* loc = buf;
  cJSON* cur;
  if (bufsize < 5)
    return NULL;

  /* set aside space for the byte count */
  loc += 4;
  for (cur = item; cur; cur = cur->next)
    {
    size_t delta = loc - buf + 1;
    if (delta > bufsize)
      { /* FIXME: error, not enough space. should do something more than this. */
      continue;
      }
    loc += bson_item_value(cur, loc, bufsize - (loc - buf) - 1, idxName);
    }
  /* add null terminator */
  *loc = 0x00;
  ++loc;
  /* go back and record the total size */
  ((int32_t*)buf)[0] = (int32_t)(loc - buf);
  /* unlike other XXX_value() methods, return the end pointer */
  return loc;
}

/* Print the item's name into the buffer at \a buf
 * and return the number of characters used (including
 * the null terminator.
 * If \a idxName is non-NULL, then it is used to generate
 * the name. Otherwise, the item's name is used.
 * A return value of 1 indicates that the item had no name.
 */
size_t bson_item_name(cJSON* item, char* buf, size_t bufsize, ptrdiff_t* idxName)
{
  size_t result = 0;
  if (idxName)
    {
    result = snprintf(buf, bufsize, "%lu", *idxName);
    if (result > bufsize)
      result = bufsize - 1; /* Prevent overruns */
    (*idxName) ++; /* increment the index for the next item. */
    }
  if (item->string)
    {
    result = snprintf(buf, bufsize, "%s", item->string);
    if (result > bufsize)
      result = bufsize - 1; /* Prevent overruns */
    }
  /* always null terminate */
  buf[result++] = 0x00;
  return result;
}

int countkids(cJSON* item)
{
  int c;
  for (c = 0; item; item = item->next)
    ++c;
  return c;
}

/* Given an adequately sized buffer, encode the JSON \a item
 * in \a buf as a BSON subitem.
 * This returns the number of bytes used from the start of
 * \a buf to encode the item.
 */
size_t bson_item_value(cJSON* item, char* buf, size_t bufsize, ptrdiff_t* idxName)
{
  size_t result = 0;
  if (!item || bufsize < 2)
    return result;
  char* loc = buf;
  switch ((item->type)&0xff)
    {
  case cJSON_NULL:
      {
      *(loc++) = cBSON_NULL;
      loc += bson_item_name(item, loc, bufsize - 1, idxName);
      }
    break;
  case cJSON_False:
      {
      *(loc++) = cBSON_Bool;
      loc += bson_item_name(item, loc, bufsize - 1, idxName);
      *(loc++) = 0x00;
      }
    break;
  case cJSON_True:
      {
      *(loc++) = cBSON_Bool;
      loc += bson_item_name(item, loc, bufsize - 1, idxName);
      *(loc++) = 0x01;
      }
    break;
  case cJSON_Number:
      {
      int isInt = (fmod(item->valuedouble, 1.0) == 0);
      *(loc++) = isInt ? cBSON_Int : cBSON_Float;
      loc += bson_item_name(item, loc, bufsize - 1, idxName);
      if (isInt)
        { /* item->valueint may only be a 32-bit integer. Promote it. */
        int64_t tmpVal = item->valueint;
        *((int64_t*)loc) = tmpVal;
        }
      else
        {
        *((double*)loc) = item->valuedouble;
        }
      loc += sizeof(double);
      }
    break;
  case cJSON_String:
    if (shouldDetectUUIDsInStrings && bson_is_string_uuid(item->valuestring))
      {
      *(loc++) = cBSON_Binary;
      loc += bson_item_name(item, loc, bufsize - 1, idxName);
      loc = bson_uuid_value_from_string(loc, item->valuestring);
      }
    else
      {
      char* lenptr;
      *(loc++) = cBSON_String;
      loc += bson_item_name(item, loc, bufsize - 1, idxName);
      lenptr = loc;
      loc += 4; /* skip the string length entry */
      loc = stpncpy(loc, item->valuestring, bufsize - 1);
      *(loc++) = 0x00;
      *((int32_t*)lenptr) = loc - (lenptr + 4); /* fill in the string length (inc. terminator) */
      }
    break;
  case cJSON_Array:
      {
      ptrdiff_t idx = 0;
      *(loc++) = cBSON_Array;
      loc += bson_item_name(item, loc, bufsize - 1, idxName);
      loc = bson_doc_value(item->child, loc, bufsize - (loc  - buf) - 1, &idx);
      }
    break;
  case cJSON_Object:
      {
      /*no gen name*/  *(loc++) = cBSON_Array;
      loc += bson_item_name(item, loc, bufsize - 1, idxName);
      loc = bson_doc_value(item->child, loc, bufsize - (loc  - buf) - 1, NULL);
      }
    break;
    }
  result = loc - buf;
  return result;
}

/* allocate and copy the null-terminated name into \a name_out. */
char* bson_parse_name(const char* bson, size_t* len)
{
  return cJSON_strdup(bson, len, 1);
}

void bson_prepare_uuid(cJSON* node, const char* loc)
{
  node->type = cJSON_UUID;
  node->valuestring = cJSON_malloc(16);
  memcpy(node->valuestring, loc, 16);
}

void bson_encode_uuid(cJSON* node, const char* loc)
{
  node->type = cJSON_String;
  node->valuestring = cJSON_malloc(37);
  encode_hex_string((const uint8_t*)loc     , 4, node->valuestring +  0);
  encode_hex_string((const uint8_t*)loc +  4, 2, node->valuestring +  9);
  encode_hex_string((const uint8_t*)loc +  6, 2, node->valuestring + 14);
  encode_hex_string((const uint8_t*)loc +  8, 2, node->valuestring + 19);
  encode_hex_string((const uint8_t*)loc + 10, 6, node->valuestring + 24);
  node->valuestring[ 8] = '-';
  node->valuestring[13] = '-';
  node->valuestring[18] = '-';
  node->valuestring[23] = '-';
  node->valuestring[36] = '\0';
}

void bson_prepare_binary(cJSON* node, const char* loc, size_t bloblen, uint8_t subtype)
{
  node->type = cJSON_Binary;
  node->valueint = subtype;
  node->valuestring = cJSON_malloc(bloblen);
  memcpy(node->valuestring, loc, bloblen);
}

void bson_encode_binary(cJSON* node, const char* loc, size_t bloblen, uint8_t subtype)
{
  node->type = cJSON_String;
  node->valuestring = cJSON_malloc(2 * bloblen + 1);
  node->valueint = subtype;
  encode_hex_string((const uint8_t*)loc, bloblen, node->valuestring);
  node->valuestring[2 * bloblen] = '\0';
}

size_t bson_parse_float(const char* bson, size_t remaining, cJSON*** prev)
{
  (void) remaining;
  size_t len;
  char* key = bson_parse_name(bson, &len);
  const char* loc = bson + len;
  cJSON* node = cJSON_CreateNumber(*(double*)loc);
  node->string = key;
  node->valueint = (int)node->valuedouble;
  cBSON_LinkSibling(prev, node);
  return sizeof(double) + loc - bson;
}

size_t bson_parse_string(const char* bson, size_t remaining, cJSON*** prev)
{
  (void) remaining;
  size_t len;
  char* key = bson_parse_name(bson, &len);
  const char* loc = bson + len;
  int32_t slen = *(const int32_t*)loc;
  loc += 4;
  /* note that we only handle ASCII, not full UTF-8 strings,
   * while loc may contain embedded NULLs that are not meant
   * to be terminators.
   */
  cJSON* node = cJSON_CreateString(loc);
  node->string = key;
  cBSON_LinkSibling(prev, node);
  return slen + loc - bson;
}


size_t bson_parse_document(const char* bson, size_t remaining, cJSON*** prev)
{
  (void) remaining;
  size_t len;
  char* key = bson_parse_name(bson, &len);
  const char* loc = bson + len;
  // peek at the size:
  int32_t dlen = *(const int32_t*)loc;
  cJSON* node = bson_parse_doc(loc, (size_t)dlen);
  if (node)
    {
    node->string = key;
    cBSON_LinkSibling(prev, node);
    }
  return dlen + (loc - bson); /* skip proper length even if we couldn't read it. */
}

size_t bson_parse_blob(const char* bson, size_t remaining, cJSON*** prev)
{
  (void) remaining;
  size_t len;
  char* key = bson_parse_name(bson, &len);

  /* we will change the node type and contents manually: */
  cJSON* node = cJSON_CreateNull();
  node->string = key;
  cBSON_LinkSibling(prev, node);

  const char* loc = bson + len;
  int32_t bloblen = *(const int32_t*)loc;
  loc += 4;
  int subtype = (*(loc++)) & 0xff;
  switch (subtype)
    {
  case cBSON_UUID: // yay
    if (shouldUseExtendedTypes)
      bson_prepare_uuid(node, loc);
    else
      bson_encode_uuid(node, loc);
    break;
  case cBSON_Generic:
  case cBSON_Function:
  case cBSON_Old:
  case cBSON_OldUUID:
  case cBSON_MD5:
  case cBSON_User:
  default:
    if (shouldUseExtendedTypes)
      bson_prepare_binary(node, loc, bloblen, subtype);
    else
      bson_encode_binary(node, loc, bloblen, subtype);
    break;
    }

  return bloblen + loc - bson;
}

size_t bson_parse_object_id(const char* bson, size_t remaining, cJSON*** prev)
{
  (void) bson;
  (void) remaining;
  (void) prev;
  return 0;
}

size_t bson_parse_bool(const char* bson, size_t remaining, cJSON*** prev)
{
  (void) remaining;
  size_t len;
  char* key = bson_parse_name(bson, &len);
  const char* loc = bson + len;
  cJSON* node = (*(loc++) ? cJSON_CreateTrue() : cJSON_CreateFalse());
  node->string = key;
  cBSON_LinkSibling(prev, node);
  return loc - bson;
}

size_t bson_parse_int(const char* bson, size_t remaining, cJSON*** prev)
{
  (void) remaining;
  size_t len;
  char* key = bson_parse_name(bson, &len);
  const char* loc = bson + len;
  int64_t val = *(int64_t*)loc;
  cJSON* node = cJSON_CreateNumber((double)val);
  node->string = key;
  node->valueint = (int)val; /* overwrite cJSON-double-cast with exact value */
  cBSON_LinkSibling(prev, node);
  return sizeof(int64_t) + loc - bson;
}

size_t bson_parse_int32(const char* bson, size_t remaining, cJSON*** prev)
{
  (void) remaining;
  size_t len;
  char* key = bson_parse_name(bson, &len);
  const char* loc = bson + len;
  int32_t val = *(int32_t*)loc;
  cJSON* node = cJSON_CreateNumber((double)val);
  node->string = key;
  node->valueint = (int)val; /* overwrite cJSON-double-cast with exact value */
  cBSON_LinkSibling(prev, node);
  return sizeof(int32_t) + loc - bson;
}

size_t bson_parse_null(const char* bson, size_t remaining, cJSON*** prev)
{
  (void) remaining;
  size_t len;
  char* key = bson_parse_name(bson, &len);
  cJSON* node = cJSON_CreateNull();
  node->string = key;
  cBSON_LinkSibling(prev, node);
  return len;
}

size_t bson_parse_regex(const char* bson, size_t remaining, cJSON*** prev)
{
  (void) remaining;
  size_t tot;
  size_t len;
  char* key = bson_parse_name(bson, &tot);
  cJSON* node = cJSON_CreateArray();

  char* regex = bson_parse_name(bson, &len);
  tot += len;
  char* opts = bson_parse_name(bson, &len);
  tot += len;

  cJSON_AddItemToArray(node, cJSON_CreateString(regex));
  cJSON_AddItemToArray(node, cJSON_CreateString(opts));
  node->string = key;
  cBSON_LinkSibling(prev, node);
  return tot;
}

size_t bson_parse_db_pointer(const char* bson, size_t remaining, cJSON*** prev)
{
  (void) bson;
  (void) remaining;
  (void) prev;
  return 0;
}

size_t bson_parse_code_ws(const char* bson, size_t remaining, cJSON*** prev)
{
  (void) bson;
  (void) remaining;
  (void) prev;
  return 0;
}

cJSON* bson_parse_doc(const char* bson, size_t bson_size)
{
  cJSON* result = cJSON_CreateObject(); /* TODO: detect array vs object */
  cJSON** next_child = &result->child;
  cJSON* prev = NULL;
  const char* loc = bson;
  /* read size for comparison */
  int32_t actual_size = *(uint32_t*)loc;
  loc += 4;
  int itype;
  int allIndicesAreInts = 1;
  long lastKey = -1;
  size_t remaining = bson_size - (loc - bson);
  assert(bson_size == (size_t)actual_size && "BSON size mismatch");
  while (remaining > 0)
    {
    itype = (*(loc++) & 0xff);
    switch (itype)
      {
    case 0: /* null terminator marking end of document */
                           if (allIndicesAreInts)
                             result->type = cJSON_Array;
                           return result;
    case cBSON_Float:
                           loc += bson_parse_float(loc, remaining, &next_child); break;
    case cBSON_String:
    case cBSON_JS_Code:
    case cBSON_Deprecated:
                           loc += bson_parse_string(loc, remaining, &next_child); break;
    case cBSON_Document:
    case cBSON_Array:
                           loc += bson_parse_document(loc, remaining, &next_child); break;
    case cBSON_Binary:
                           loc += bson_parse_blob(loc, remaining, &next_child); break;
    case cBSON_ObjectId:
                           loc += bson_parse_object_id(loc, remaining, &next_child); break;
    case cBSON_Bool:
                           loc += bson_parse_bool(loc, remaining, &next_child); break;
    case cBSON_UTC_Time:
    case cBSON_Timestamp:
    case cBSON_Int:
                           loc += bson_parse_int(loc, remaining, &next_child); break;
    case cBSON_Int32:
                           loc += bson_parse_int32(loc, remaining, &next_child); break;
    case cBSON_Undefined:
    case cBSON_NULL:
    case cBSON_Min_Key:
    case cBSON_Max_Key:
                           loc += bson_parse_null(loc, remaining, &next_child); break;
    case cBSON_Regex:
                           loc += bson_parse_regex(loc, remaining, &next_child); break;
    case cBSON_DBPointer:
                           loc += bson_parse_db_pointer(loc, remaining, &next_child); break;
    case cBSON_JS_Code_WS:
                           loc += bson_parse_code_ws(loc, remaining, &next_child); break;
    default:
                           cJSON_Delete(result);
                           return NULL;
      }
    remaining = bson_size - (loc - bson);
    /* See if the node we just added has an integer index
     * (used to decide whether this should be an object or
     * an array). */
    if (allIndicesAreInts)
      {
      if (!prev)
        prev = result->child;
      else
        prev = prev->next;
      if (prev && prev->string)
        {
        char* dummy;
        long idx = strtol(prev->string, &dummy, 10);
        if (!dummy || *dummy || idx <= lastKey)
          allIndicesAreInts = 0;
        else
          lastKey = idx;
        }
      else
        {
        allIndicesAreInts = 0;
        }
      }
    }
  if (allIndicesAreInts)
    result->type = cJSON_Array;
  return result;
}

cJSON* cJSON_ParseBSON(const char* bson, size_t bson_size)
{
  return bson_parse_doc(bson, bson_size);
}
