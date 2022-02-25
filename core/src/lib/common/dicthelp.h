/* dicthelp.c
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

#define dictAddWithNum(d, key, val) do {\
	sds keyStr = sdsfromlonglong(key);\
	if (DICT_ERR == dictAdd(d, keyStr, val)) {\
		plg_sdsFree(keyStr);\
						}\
} while (0);

#define dictAddWithLonglong(d, key, val) do {\
	unsigned long long* mkey = malloc(sizeof(unsigned long long));\
	if (mkey == 0)break;\
	*mkey = key;\
	if (DICT_ERR == dictAdd(d, mkey, val)) {\
		free(mkey);\
						}\
} while (0);


#define dictAddWithUint(d, key, val) do {\
	unsigned int* mkey = malloc(sizeof(unsigned int));\
	if (mkey == 0)break;\
	*mkey = key;\
	if (DICT_ERR == dictAdd(d, mkey, val)) {\
		free(mkey);\
						}\
} while (0);

#define dictAddValueWithUint(d, key, val) do {\
	dictEntry * entry = dictFind(d, key);\
	if (entry) {\
		unsigned int* mvalue = dictGetVal(entry);\
		*mvalue += val;\
	} else {\
		unsigned int* mvalue = malloc(sizeof(unsigned int));\
		if (mkey == 0)break;\
		*mvalue = val; \
		if (DICT_ERR == dictAdd(d, key, mvalue)) {\
			free(mvalue); \
		}\
	}\
} while (0);

void* DefaultLonglongPtr();
void* DefaultUintPtr();
void* DefaultBenchmarkPtr();
void* DefaultNoneDictPtr();
void* DefaultPtrDictPtr();
void* DefaultSdsDictPtr();

unsigned int sdsHashCallback(const void* key);
int sdsCompareCallback(void* privdata, const void* key1, const void* key2);
void sdsDestructor(void* privdata, void* key);

unsigned int hashCallback(const void* key);
int uintCompareCallback(void* privdata, const void* key1, const void* key2);
void memFreeCallback(void* privdata, void* val);

unsigned int longlongHashCallback(const void* key);
int LonglongCompareCallback(void* privdata, const void* key1, const void* key2);
