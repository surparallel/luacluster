/* packjson.c --pack or unpack to json for User interaction
*
* Copyright(C) 2021 - 2022, sun shuo <sun.shuo@surparallel.org>
* All rights reserved.
*
* This program is free software : you can redistribute it and / or modify
* it under the terms of the GNU Affero General Public License as
* published by the Free Software Foundation, either version 3 of the
* License, or(at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
* GNU Affero General Public License for more details.
*
* You should have received a copy of the GNU Affero General Public License
* along with this program.If not, see < https://www.gnu.org/licenses/>.
*/


#include "plateform.h"
#include "lua_cmsgpack.h"
#include "cJSON.h"
#include "packjson.h"

static void PackJsonObject(cJSON* item, mp_buf* pk);

static void PackJsonArray(cJSON* item, mp_buf* pk)
{
	int arraySize = cJSON_GetArraySize(item);
	if (item->string != NULL)
	{
		mp_encode_bytes(pk, item->string, strlen(item->string));
	}
	mp_encode_array(pk, arraySize);
	for (int i = 0; i < arraySize; i++)
	{
		cJSON* child = cJSON_GetArrayItem(item, i);
		switch ((child->type) & 0XFF)
		{
		case cJSON_True:
			mp_encode_bool(pk, 1);
			break;
		case cJSON_False:
			mp_encode_bool(pk, 0);
			break;
		case cJSON_String:
			mp_encode_bytes(pk, child->valuestring, strlen(child->valuestring));
			break;
		case cJSON_Number:
			mp_encode_double(pk, child->valuedouble);
			break;
		case cJSON_Array:
			PackJsonArray(child, pk);
			break;
		case cJSON_Object:
			PackJsonObject(child, pk);
			break;
		}
	}
}

static int getItemsCount(cJSON* object)
{
	int count = 0;
	while (object != NULL)
	{
		object = object->next;
		count++;
	}
	return count;
}

static void PackJsonObject(cJSON* item, mp_buf* pk)
{
	if (item->string != NULL)
	{
		mp_encode_bytes(pk, item->string, strlen(item->string));
	}
	cJSON* child = item->child;
	mp_encode_map(pk, getItemsCount(child));
	while (child != NULL)
	{
		switch ((child->type) & 0XFF)
		{
		case cJSON_True:
			mp_encode_bytes(pk, child->string, strlen(child->string));
			mp_encode_bool(pk, 1);
			break;
		case cJSON_False:
			mp_encode_bytes(pk, child->string, strlen(child->string));
			mp_encode_bool(pk, 0);
			break;
		case cJSON_String:
			mp_encode_bytes(pk, child->string, strlen(child->string));
			mp_encode_bytes(pk, child->valuestring, strlen(child->valuestring));
			break;
		case cJSON_Number:
			mp_encode_bytes(pk, child->string, strlen(child->string));
			mp_encode_double(pk, child->valuedouble);
			break;
		case cJSON_Array:
			PackJsonArray(child, pk);
			break;
		case cJSON_Object:
			PackJsonObject(child, pk);
			break;
		}
		child = child->next;
	}
}

int PackJson(const char* str, mp_buf* pk) {
	cJSON* json = cJSON_Parse(str);
	if (!json) {
		return 0;
	}

	if (((json->type) & 0XFF) == cJSON_Object)
		PackJsonObject(json, pk);
	else if (((json->type) & 0XFF) == cJSON_Array)
		PackJsonArray(json, pk);

	cJSON_Delete(json);
	return 1;
}

void UnpackArray(mp_cur* pcursor, cJSON* json);
void UnpackObj(mp_cur* pcursor, cJSON* json);

void UnpackArrayItem(mp_cur* pcursor, cJSON* json) {

	int type;
	mp_decode_type(pcursor, &type);
	if (pcursor->err == 0) {
		switch (type)
		{
		case code_byte: {
			unsigned char* p;
			size_t l;
			mp_decode_bytes(pcursor, &p, &l);
			char* str = calloc(1, l + 1);
			memcpy(str, p, l);
			cJSON* jsonstr = cJSON_CreateString(str);
			cJSON_AddItemToArray(json, jsonstr);
			free(str);
			break;
		}
		case code_double:
		{
			double value;
			mp_decode_double(pcursor, &value);
			cJSON* jsondouble = cJSON_CreateNumber(value);
			cJSON_AddItemToArray(json, jsondouble);
			break;
		}
		case code_unint:
		{
			unsigned long long value;
			mp_decode_unint(pcursor, &value);
			cJSON* jsondouble = cJSON_CreateNumber((double)value);
			cJSON_AddItemToArray(json, jsondouble);
			break;
		}
		case code_int:
		{
			long long value;
			mp_decode_int(pcursor, &value);
			cJSON* jsondouble = cJSON_CreateNumber((double)value);
			cJSON_AddItemToArray(json, jsondouble);
			break;
		}
		case code_bool:
		{
			int value;
			mp_decode_bool(pcursor, &value);
			cJSON* jsonbool = cJSON_CreateBool(value);
			cJSON_AddItemToArray(json, jsonbool);
			break;
		}
		case code_nil:
			break;
		case code_array:
		{
			cJSON* array = cJSON_CreateArray();
			cJSON_AddItemToArray(json, array);
			UnpackArray(pcursor, array);
			break;
		}
		case code_map:
		{
			cJSON* map = cJSON_CreateObject();
			cJSON_AddItemToArray(json, map);
			UnpackObj(pcursor, map);
			break;
		}
		default:
			break;
		}
	}
	else {
		return;
	}
}

void UnpackArray(mp_cur* pcursor, cJSON* json) {

	int type = 0;
	size_t loop = 0;
	mp_decode_type(pcursor, &type);
	if (pcursor->err == 0 && type == code_array) {
		mp_decode_array(pcursor, &loop);
		for (int i = 0; i < loop; i++)
		{
			mp_decode_type(pcursor, &type);
			if (pcursor->err == 0) {
				switch (type)
				{
				case code_byte: {
					unsigned char* p;
					size_t l;
					mp_decode_bytes(pcursor, &p, &l);
					char* str = calloc(1, l + 1);
					memcpy(str, p, l);
					cJSON* jsonstr = cJSON_CreateString(str);
					cJSON_AddItemToArray(json, jsonstr);
					free(str);
					break;
				}
				case code_double:
				{
					double value;
					mp_decode_double(pcursor, &value);
					cJSON* jsondouble = cJSON_CreateNumber(value);
					cJSON_AddItemToArray(json, jsondouble);
					break;
				}
				case code_unint:
				{
					unsigned long long value;
					mp_decode_unint(pcursor, &value);
					cJSON* jsondouble = cJSON_CreateNumber((double)value);
					cJSON_AddItemToArray(json, jsondouble);
					break;
				}
				case code_int:
				{
					long long value;
					mp_decode_int(pcursor, &value);
					cJSON* jsondouble = cJSON_CreateNumber((double)value);
					cJSON_AddItemToArray(json, jsondouble);
					break;
				}
				case code_bool:
				{
					int value;
					mp_decode_bool(pcursor, &value);
					cJSON*  jsonbool = cJSON_CreateBool(value);
					cJSON_AddItemToArray(json, jsonbool);
					break;
				}
				case code_nil:
					break;
				case code_array:
				{
					cJSON* array = cJSON_CreateArray();
					cJSON_AddItemToArray(json, array);
					UnpackArray(pcursor, array);
					break;
				}
				case code_map:
				{
					cJSON* map = cJSON_CreateObject();
					cJSON_AddItemToArray(json, map);
					UnpackObj(pcursor, map);
					break;
				}
				default:
					break;
				}
			}
			else {
				return;
			}
		}
	}
}

void UnpackObj(mp_cur* pcursor, cJSON* json) {

	int type = 0;
	size_t loop = 0;
	mp_decode_type(pcursor, &type);
	if (pcursor->err == 0 && type == code_map) {
		mp_decode_map(pcursor, &loop);
		for(int i = 0; i < loop; i++)
		{
			mp_decode_type(pcursor, &type);
			if (pcursor->err == 0  && type == code_byte) {
				//key
				unsigned char* p;
				size_t l;
				mp_decode_bytes(pcursor, &p, &l);
				char* key = calloc(1, l + 1);
				memcpy(key, p, l);

				mp_decode_type(pcursor, &type);
				if (pcursor->err == 0) {
					switch (type)
					{
					case code_byte: {
						unsigned char* p;
						size_t l;
						mp_decode_bytes(pcursor, &p, &l);
						char* str = calloc(1, l + 1);
						memcpy(str, p, l);
						cJSON_AddStringToObject(json, key, str);
						free(str);
						break;
					}
					case code_double:
					{
						double value;
						mp_decode_double(pcursor, &value);
						cJSON_AddNumberToObject(json, key, value);
						break;
					}
					case code_unint:
					{
						unsigned long long value;
						mp_decode_unint(pcursor, &value);
						cJSON_AddNumberToObject(json, key, (double)value);
						break;
					}
					case code_int:
					{
						long long value;
						mp_decode_int(pcursor, &value);
						cJSON_AddNumberToObject(json, key, (double)value);
						break;
					}
					case code_bool:
					{
						int value;
						mp_decode_bool(pcursor, &value);
						cJSON_AddBoolToObject(json, key, value);
						break;
					}
					case code_nil:
						break;
					case code_array:
					{
						cJSON* array = cJSON_CreateArray();
						cJSON_AddItemToObject(json, key, array);
						UnpackArray(pcursor, array);
						break;
					}
					case code_map:
					{
						cJSON* map = cJSON_CreateObject();
						cJSON_AddItemToObject(json, key, map);
						UnpackObj(pcursor, map);
						break;
					}
					default:
						break;
					}
				}
				else {
					return;
				}
				free(key);
			}
			else {
				return;
			}
		}
	}
}

void UnpackJson(const char* buf, size_t len, cJSON** json) {

	mp_cur cursor;
	mp_cur* pcursor = &cursor;
	mp_cur_init(pcursor, buf, len);

	int type;
	mp_decode_type(pcursor, &type);
	if (type == code_map) {
		*json = cJSON_CreateObject();
		UnpackObj(pcursor, *json);
	}
	else if (type == code_array) {
		*json = cJSON_CreateArray();
		UnpackArray(pcursor, *json);
	}
	else {
		*json = cJSON_CreateArray();
		while (1) {
			UnpackArrayItem(pcursor, *json);
			if (pcursor->err != 0)
				break;
		}
	}
}