#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "cJSON_Utils.h"

// JSON Pointer implementation:
static int cJSONUtils_Pstrcasecmp(const char *a,const char *e)
{
	if (!a || !e) return (a==e)?0:1;
	for (;*a && *e && *e!='/';a++,e++) {
		if (*e=='~') {if (!(e[1]=='0' && *a=='~') && !(e[1]=='1' && *a=='/')) return 1;  else e++;}
		else if (tolower(*a)!=tolower(*e)) return 1;
	}
	if ((*e!=0 && *e!='/') != (*a!=0)) return 1;
	return 0;
}

static int cJSONUtils_PointerEncodedstrlen(const char *s)	{int l=0;for (;*s;s++,l++) if (*s=='~' || *s=='/') l++;return l;}

static void cJSONUtils_PointerEncodedstrcpy(char *d,const char *s)
{
	for (;*s;s++)
	{
		if (*s=='/') {*d++='~';*d++='1';}
		else if (*s=='~') {*d++='~';*d++='0';}
		else *d++=*s;
	}
	*d=0;
}

char *cJSONUtils_FindPointerFromObjectTo(cJSON *object,cJSON *target)
{
	if (object==target) return strdup("");

	int type=object->type,c=0;
	for (cJSON *obj=object->child;obj;obj=obj->next,c++)
	{
		char *found=cJSONUtils_FindPointerFromObjectTo(obj,target);
		if (found)
		{
			if (type==cJSON_Array)
			{
				char *ret=(char*)malloc(strlen(found)+23);
				sprintf(ret,"/%d%s",c,found);
				free(found);
				return ret;
			}
			else if (type==cJSON_Object)
			{
				char *ret=(char*)malloc(strlen(found)+cJSONUtils_PointerEncodedstrlen(obj->string)+2);
				*ret='/';cJSONUtils_PointerEncodedstrcpy(ret+1,obj->string);
				strcat(ret,found);
				free(found);
				return ret;
			}
			free(found);
			return 0;
		}
	}
	return 0;
}

cJSON *cJSONUtils_GetPointer(cJSON *object,const char *pointer)
{
	while (*pointer++=='/' && object)
	{
		if (object->type==cJSON_Array)
		{
			int which=0; while (*pointer>='0' && *pointer<='9') which=(10*which) + *pointer++ - '0';
			if (*pointer && *pointer!='/') return 0;
			object=cJSON_GetArrayItem(object,which);
		}
		else if (object->type==cJSON_Object)
		{
			object=object->child;	while (object && cJSONUtils_Pstrcasecmp(object->string,pointer)) object=object->next;	// GetObjectItem.
			while (*pointer && *pointer!='/') pointer++;
		}
		else return 0;
	}
	return object;
}

// JSON Patch implementation.
static void cJSONUtils_InplaceDecodePointerString(char *string)
{
	char *s2=string;
	for (;*string;s2++,string++) *s2=(*string!='~')?(*string):((*(++string)=='0')?'~':'/');
	*s2=0;
}

static cJSON *cJSONUtils_PatchDetach(cJSON *object,const char *path)
{
	char *parentptr=0,*childptr=0;cJSON *parent=0;

	parentptr=strdup(path);	childptr=strrchr(parentptr,'/');	if (childptr) *childptr++=0;
	parent=cJSONUtils_GetPointer(object,parentptr);
	cJSONUtils_InplaceDecodePointerString(childptr);

	cJSON *ret=0;
	if (!parent) ret=0;	// Couldn't find object to remove child from.
	else if (parent->type==cJSON_Array)		ret=cJSON_DetachItemFromArray(parent,atoi(childptr));
	else if (parent->type==cJSON_Object)	ret=cJSON_DetachItemFromObject(parent,childptr);
	free(parentptr);
	return ret;
}

static int cJSONUtils_Compare(cJSON *a,cJSON *b)
{
	if (a->type!=b->type)	return -1;	// mismatched type.
	switch (a->type)
	{
	case cJSON_Number:	return (a->valueint!=b->valueint || a->valuedouble!=b->valuedouble)?-2:0;	// numeric mismatch.
	case cJSON_String:	return (strcmp(a->valuestring,b->valuestring)!=0)?-3:0;						// string mismatch.
	case cJSON_Array:	for (a=a->child,b=b->child;a && b;a=a->next,b=b->next)	{int err=cJSONUtils_Compare(a,b);if (err) return err;}
						return (a || b)?-4:0;	// array size mismatch.
	case cJSON_Object:
						if (cJSON_GetArraySize(a)!=cJSON_GetArraySize(b))	return -5;	// object length mismatch.
						for (a=a->child;a;a=a->next)
						{
							int err=0;cJSON *s=cJSON_GetObjectItem(b,a->string); if (!s) return -6;	// missing object member.
							err=cJSONUtils_Compare(a,s);if (err) return err;
						}
						return 0;
	default:			break;
	}
	return 0;
}

static int cJSONUtils_ApplyPatch(cJSON *object,cJSON *patch)
{
	cJSON *op=0,*path=0,*value=0;int opcode=0;
	
	op=cJSON_GetObjectItem(patch,"op");
	path=cJSON_GetObjectItem(patch,"path");
	if (!op || !path) return 2;	// malformed patch.

	if		(!strcmp(op->valuestring,"add"))	opcode=0;
	else if (!strcmp(op->valuestring,"remove")) opcode=1;
	else if (!strcmp(op->valuestring,"replace"))opcode=2;
	else if (!strcmp(op->valuestring,"move"))	opcode=3;
	else if (!strcmp(op->valuestring,"copy"))	opcode=4;
	else if (!strcmp(op->valuestring,"test"))	return cJSONUtils_Compare(cJSONUtils_GetPointer(object,path->valuestring),cJSON_GetObjectItem(patch,"value"));
	else return 3; // unknown opcode.

	if (opcode==1 || opcode==2)	// Remove/Replace
	{
		cJSON_Delete(cJSONUtils_PatchDetach(object,path->valuestring));	// Get rid of old.
		if (opcode==1) return 0;	// For Remove, this is job done.
	}

	if (opcode==3 || opcode==4)	// Copy/Move uses "from".
	{
		cJSON *from=cJSON_GetObjectItem(patch,"from");	if (!from) return 4; // missing "from" for copy/move.

		if (opcode==3) value=cJSONUtils_PatchDetach(object,from->valuestring);
		if (opcode==4) value=cJSONUtils_GetPointer(object,from->valuestring);
		if (!value) return 5; // missing "from" for copy/move.
		if (opcode==4) value=cJSON_Duplicate(value,1);
		if (!value) return 6; // out of memory for copy/move.
	}
	else	// Add/Replace uses "value".
	{
		value=cJSON_GetObjectItem(patch,"value");
		if (!value) return 7; // missing "value" for add/replace.
		value=cJSON_Duplicate(value,1);
		if (!value) return 8; // out of memory for add/replace.
	}
		
	// Now, just add "value" to "path".
	char *parentptr=0,*childptr=0;cJSON *parent=0;

	parentptr=strdup(path->valuestring);	childptr=strrchr(parentptr,'/');	if (childptr) *childptr++=0;
	parent=cJSONUtils_GetPointer(object,parentptr);
	cJSONUtils_InplaceDecodePointerString(childptr);

	// add, remove, replace, move, copy, test.
	if (!parent) {free(parentptr); return 9;}	// Couldn't find object to add to.
	else if (parent->type==cJSON_Array)
	{
		if (!strcmp(childptr,"-"))	cJSON_AddItemToArray(parent,value);
		else						cJSON_InsertItemInArray(parent,atoi(childptr),value);
	}
	else if (parent->type==cJSON_Object)
	{
		cJSON_DeleteItemFromObject(parent,childptr);
		cJSON_AddItemToObject(parent,childptr,value);
	}
	free(parentptr);
	return 0;
}


int cJSONUtils_ApplyPatches(cJSON *object,cJSON *patches)
{
	int err;
	if (!patches->type==cJSON_Array) return 1;	// malformed patches.
	if (patches) patches=patches->child;
	while (patches)
	{
		if ((err=cJSONUtils_ApplyPatch(object,patches))) return err;
		patches=patches->next;
	}
	return 0;
}

static void cJSONUtils_GeneratePatch(cJSON *patches,const char *op,const char *path,const char *suffix,cJSON *val)
{
	cJSON *patch=cJSON_CreateObject();
	cJSON_AddItemToObject(patch,"op",cJSON_CreateString(op));
	if (suffix)
	{
		char *newpath=(char*)malloc(strlen(path)+cJSONUtils_PointerEncodedstrlen(suffix)+2);
		cJSONUtils_PointerEncodedstrcpy(newpath+sprintf(newpath,"%s/",path),suffix);
		cJSON_AddItemToObject(patch,"path",cJSON_CreateString(newpath));
		free(newpath);
	}
	else	cJSON_AddItemToObject(patch,"path",cJSON_CreateString(path));
	if (val) cJSON_AddItemToObject(patch,"value",cJSON_Duplicate(val,1));
	cJSON_AddItemToArray(patches,patch);
}

void cJSONUtils_AddPatchToArray(cJSON *array,const char *op,const char *path,cJSON *val)	{cJSONUtils_GeneratePatch(array,op,path,0,val);}

static void cJSONUtils_CompareToPatch(cJSON *patches,const char *path,cJSON *from,cJSON *to)
{
	if (from->type!=to->type)	{cJSONUtils_GeneratePatch(patches,"replace",path,0,to);	return;	}
	
	switch (from->type)
	{
	case cJSON_Number:	
		if (from->valueint!=to->valueint || from->valuedouble!=to->valuedouble)
			cJSONUtils_GeneratePatch(patches,"replace",path,0,to);
		return;
						
	case cJSON_String:	
		if (strcmp(from->valuestring,to->valuestring)!=0)
			cJSONUtils_GeneratePatch(patches,"replace",path,0,to);
		return;

	case cJSON_Array:
	{
		int c;char *newpath=(char*)malloc(strlen(path)+23);	// Allow space for 64bit int.
		for (c=0,from=from->child,to=to->child;from && to;from=from->next,to=to->next,c++){
										sprintf(newpath,"%s/%d",path,c);	cJSONUtils_CompareToPatch(patches,newpath,from,to);
		}
		for (;from;from=from->next,c++)	{sprintf(newpath,"%d",c);	cJSONUtils_GeneratePatch(patches,"remove",path,newpath,0);	}
		for (;to;to=to->next,c++)		cJSONUtils_GeneratePatch(patches,"add",path,"-",to);
		free(newpath);
		return;
	}

	case cJSON_Object:
		for (cJSON *a=from->child;a;a=a->next)
		{
			if (!cJSON_GetObjectItem(to,a->string))	cJSONUtils_GeneratePatch(patches,"remove",path,a->string,0);
		}
		for (cJSON *a=to->child;a;a=a->next)
		{
			cJSON *other=cJSON_GetObjectItem(from,a->string);
			if (!other)	cJSONUtils_GeneratePatch(patches,"add",path,a->string,a);
			else
			{
				char *newpath=(char*)malloc(strlen(path)+cJSONUtils_PointerEncodedstrlen(a->string)+2);
				cJSONUtils_PointerEncodedstrcpy(newpath+sprintf(newpath,"%s/",path),a->string);
				cJSONUtils_CompareToPatch(patches,newpath,other,a);
				free(newpath);
			}
		}
		return;

	default:			break;
	}
}


cJSON* cJSONUtils_GeneratePatches(cJSON *from,cJSON *to)
{
	cJSON *patches=cJSON_CreateArray();	
	cJSONUtils_CompareToPatch(patches,"",from,to);
	return patches;
}



