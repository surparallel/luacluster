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

#include "plateform.h"
#include "sds.h"
#include "dict.h"

unsigned int sdsHashCallback(const void* key) {
	return dictGenHashFunction((unsigned char*)key, (int)sdslen((char*)key));
}

int sdsCompareCallback(void* privdata, const void* key1, const void* key2) {
	DICT_NOTUSED(privdata);
	return sdscmp((sds)key1, (sds)key2) == 0;
}

void sdsDestructor(void* privdata, void* key) {
	DICT_NOTUSED(privdata);
	sdsfree((sds)key);
}

static void sdsFreeCallback(void* privdata, void* val) {
	DICT_NOTUSED(privdata);
	sdsfree(val);
}

unsigned int hashCallback(const void* key) {
	return dictGenHashFunction((unsigned char*)key, sizeof(unsigned int));
}

unsigned int longlongHashCallback(const void* key) {
	return dictGenHashFunction((unsigned char*)key, sizeof(unsigned long long));
}

int uintCompareCallback(void* privdata, const void* key1, const void* key2) {
	DICT_NOTUSED(privdata);
	if (*(unsigned int*)key1 != *(unsigned int*)key2)
		return 0;
	else
		return 1;
}

int LonglongCompareCallback(void* privdata, const void* key1, const void* key2) {
	DICT_NOTUSED(privdata);
	if (*(unsigned long long*)key1 != *(unsigned long long*)key2)
		return 0;
	else
		return 1;
}

void memFreeCallback(void* privdata, void* val) {
	DICT_NOTUSED(privdata);
	free(val);
}

static dictType NoneDictType = {
	hashCallback,
	NULL,
	NULL,
	uintCompareCallback,
	NULL,
	NULL
};

static dictType BenchmarkDictType = {
	sdsHashCallback,
	NULL,
	NULL,
	sdsCompareCallback,
	sdsFreeCallback,
	NULL
};

static dictType SdsDictType = {
	sdsHashCallback,
	NULL,
	NULL,
	sdsCompareCallback,
	NULL,
	NULL
};

static unsigned int ptrHashCallback(const void* key) {
	return dictGenHashFunction((unsigned char*)&key, sizeof(void*));
}

static dictType PtrDictType = {
	ptrHashCallback,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

static dictType UintDictType = {
	hashCallback,
	NULL,
	NULL,
	uintCompareCallback,
	memFreeCallback,
	NULL
};

static dictType LongLongDictType = {
	longlongHashCallback,
	NULL,
	NULL,
	LonglongCompareCallback,
	memFreeCallback,
	NULL
};

void* DefaultLonglongPtr() {
	return &LongLongDictType;
}

void* DefaultUintPtr() {
	return &UintDictType;
}

void* DefaultBenchmarkPtr() {
	return &BenchmarkDictType;
}

void* DefaultNoneDictPtr() {
	return &NoneDictType;
}

void* DefaultSdsDictPtr() {
	return &SdsDictType;
}

void* DefaultPtrDictPtr() {
	return &PtrDictType;
}