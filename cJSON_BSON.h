#ifndef __cJSON_BSON_h
#define __cJSON_BSON_h

#include "cJSON.h"
#include <stddef.h>

/* Extra cJSON tags for extended BSON types.
 *
 * These will cause cJSON_Print to fail, but
 * make it possible to store binary data in
 * cJSON data structures without string
 * conversion.
 *
 * You can have cJSON_BSON_Parse() produce
 * these or not by calling the
 * cJSON_BSON_SetUseExtendedTypes() function.
 *
 * Note that these enums start with "cJSON_"
 * while those below start with "cBSON_"!
 */
#define cJSON_Binary 16 /* subtype is specified in item->valueint, data in item->valuestring */
#define cJSON_UUID   17 /* subtype is implicit, data stored in item->valuestring */

/* BSON tags we support */
#define cBSON_Float   0x01
#define cBSON_String  0x02
#define cBSON_Array   0x04
#define cBSON_Binary  0x05
#define cBSON_Bool    0x08
#define cBSON_NULL    0x0a
#define cBSON_Int     0x12

/* BSON tags we don't support */
#define cBSON_Document   0x03
#define cBSON_Undefined  0x06 /* deprecated */
#define cBSON_ObjectId   0x07
#define cBSON_UTC_Time   0x09
#define cBSON_Regex      0x0b
#define cBSON_DBPointer  0x0c /* deprecated */
#define cBSON_JS_Code    0x0d
#define cBSON_Deprecated 0x0e /* deprecated? */
#define cBSON_JS_Code_WS 0x0f /* Javascript code with scope */
#define cBSON_Int32      0x10
#define cBSON_Timestamp  0x11
#define cBSON_Min_Key    0xff
#define cBSON_Max_Key    0x7f

/* BSON binary data subtypes we support */
#define cBSON_UUID     0x04

/* BSON binary data subtypes we don't support */
#define cBSON_Generic  0x00
#define cBSON_Function 0x01
#define cBSON_Old      0x02 /* deprecated? */
#define cBSON_OldUUID  0x03 /* deprecated */
#define cBSON_MD5      0x05
#define cBSON_User     0x80

#ifdef __cplusplus
extern "C" {
#endif

char* cJSON_PrintBSON(cJSON *item, size_t* bson_size_out);
void cJSON_DeleteBSON(char* bson);
cJSON* cJSON_ParseBSON(const char* bson, size_t bson_size);

void cJSON_BSON_SetDetectUUIDs(int yes);
int cJSON_BSON_WillDetectUUIDs();

void cJSON_BSON_SetUseExtendedTypes(int yes);
int cJSON_BSON_WillUseExtendedTypes();

char* bson_doc_value(cJSON* item, char* buf, size_t bufsize, ptrdiff_t* idxName);
size_t bson_item_name(cJSON* item, char* buf, size_t bufsize, ptrdiff_t* idxName);
size_t bson_item_value(cJSON* item, char* buf, size_t bufsize, ptrdiff_t* idxName);

size_t bson_get_doc_size(cJSON* item);
size_t bson_get_array_item_size(cJSON* item);
size_t bson_get_array_size(cJSON* item);
size_t bson_get_object_size(cJSON* item);
size_t bson_get_size(cJSON* item);

cJSON* bson_parse_doc(const char* bson, size_t bson_size);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* __cJSON_BSON_h */
