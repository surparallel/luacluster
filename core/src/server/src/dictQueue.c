/* dictQueue.c
*
* Copyright(C) 2019 - 2022, sun shuo <sun.shuo@surparallel.org>
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

#include "semwarp.h"
#include "plateform.h"
#include "uv.h"
#include "dict.h"
#include "adlist.h"
#include "sds.h"
#include "locks.h"
#include "dictQueue.h"
#include "dicthelp.h"
#include "elog.h"

#define INCBUF 10000

typedef struct _DictValue {
	unsigned long long id;
	listNode node;
	uv_buf_t* uvBuf;
	unsigned int uvBufLen;
	unsigned int uvBufSize;
}*PDictValue, DictValue;

typedef struct _DictQueue {
	void* mutexHandle;
	sds objecName;
	PDictList pDictList;
	unsigned long long stamp;
}*PDictQueue, DictQueue;

PDictValue DictValueCreate(unsigned long long id) {
	PDictValue pDictValue = malloc(sizeof(DictValue));

	if (pDictValue == NULL) {
		n_error("DictValueCreate malloc error");
		return NULL;
	}

	pDictValue->id = id;
	pDictValue->node.value = pDictValue;

	pDictValue->uvBuf = malloc(sizeof(uv_buf_t) * INCBUF);

	if (pDictValue->uvBuf == NULL) {
		n_error("DictValueCreate malloc1 error");
		return NULL;
	}

	pDictValue->uvBufSize = INCBUF;
	pDictValue->uvBufLen = 0;

	return pDictValue;
}

void DictValueDestory(PDictValue pDictValue) {
	if (pDictValue->uvBuf) {
		for (int i = 0; i < pDictValue->uvBufLen; i++)
		{
			sdsfree(pDictValue->uvBuf[i].base);
		}
		free(pDictValue->uvBuf);
	}
	free(pDictValue);
}

PDictList DictListCreate() {
	PDictList pDictList = malloc(sizeof(DictList));
	if (pDictList == NULL) {
		n_error("DictListCreate malloc error");
		return NULL;
	}

	pDictList->IDList = listCreate();
	pDictList->IDDict = dictCreate(DefaultLonglongPtr(), NULL);
	pDictList->len = 0;
	return pDictList;
}

void DictListDestory(PDictList pDictList) {

	listClearNode(pDictList->IDList);
	listRelease(pDictList->IDList);

	dictIterator* iter = dictGetIterator(pDictList->IDDict);
	dictEntry* node;
	while ((node = dictNext(iter)) != NULL) {

		PDictValue pDictValue = (PDictValue)dictGetVal(node);
		DictValueDestory(pDictValue);
	}
	dictReleaseIterator(iter);

	dictRelease(pDictList->IDDict);
	free(pDictList);
}

void* DqCreate() {
	PDictQueue pDictQueue = malloc(sizeof(DictQueue));

	if (pDictQueue == NULL) {
		n_error("DqCreate malloc error");
		return NULL;
	}

	pDictQueue->mutexHandle = MutexCreateHandle(LockLevel_4);

	pDictQueue->objecName = sdsnew("equeue");
	pDictQueue->pDictList = DictListCreate();
	pDictQueue->stamp = GetTick();

	return pDictQueue;
}

void DqDestory(void* pVoid) {
	PDictQueue pDictQueue = (PDictQueue)pVoid;
	MutexDestroyHandle(pDictQueue->mutexHandle);

	DictListDestory(pDictQueue->pDictList);
	sdsfree(pDictQueue->objecName);
	free(pDictQueue);
}

void DqPush(void* pVoid, unsigned long long id, void* value, DqBeatFun fun, void* pAram) {
	PDictQueue pDictQueue = pVoid;
	MutexLock(pDictQueue->mutexHandle, pDictQueue->objecName);
	PDictValue pDictValue;
	unsigned int len = pDictQueue->pDictList->len;
	dictEntry* entry = dictFind(pDictQueue->pDictList->IDDict, &id);
	if (entry == NULL) {
		//创建
		pDictValue = DictValueCreate(id);
		dictAddWithLonglong(pDictQueue->pDictList->IDDict, id, pDictValue);
	}
	else {
		pDictValue = dictGetVal(entry);
	}

	if (pDictValue->uvBufLen >= pDictValue->uvBufSize) {
		//扩容
		uv_buf_t* t = malloc(sizeof(uv_buf_t) * (pDictValue->uvBufSize + INCBUF));

		if (t == NULL) {
			n_error("DqPush::t malloc error");
			return;
		}

		memcpy(t, pDictValue->uvBuf, sizeof(uv_buf_t) * pDictValue->uvBufLen);
		free(pDictValue->uvBuf);
		pDictValue->uvBuf = t;
		pDictValue->uvBufSize += INCBUF;
	}

	if (pDictValue->uvBufLen == 0)
		listAddNodeHeadForNode(pDictQueue->pDictList->IDList, &pDictValue->node);

	pDictValue->uvBuf[pDictValue->uvBufLen++] = uv_buf_init(value, sdslen(value));

	pDictQueue->pDictList->len++;
	unsigned long long stamp = GetTick();
	if (stamp - pDictQueue->stamp> 10 && len == 0) {
		pDictQueue->stamp = stamp;
		fun(pAram);
	}
	MutexUnlock(pDictQueue->mutexHandle, pDictQueue->objecName);
}

PDictList DqPop(void* pVoid, PDictList pDictList) {

	PDictQueue pDictQueue = pVoid;
	PDictList td;

	MutexLock(pDictQueue->mutexHandle, pDictQueue->objecName);
	td = pDictQueue->pDictList;
	pDictQueue->pDictList = pDictList;
	MutexUnlock(pDictQueue->mutexHandle, pDictQueue->objecName);
	return td;
}

void DqLoopValue(void* pVoid, PDictList pDictList, DqSendFun fun, void* pAram) {

	listIter* iter = listGetIterator(pDictList->IDList, AL_START_HEAD);
	listNode* node;
	while ((node = listNext(iter)) != NULL) {
		PDictValue pDictValue = listNodeValue(node);
		if (1 == fun(pDictValue->id, pDictValue->uvBuf, pDictValue->uvBufLen, pAram)) {
			pDictValue->uvBuf = 0;
			pDictValue->uvBufLen = 0;
			pDictValue->uvBufSize = 0;
		}
		else {
			//没有找到id删除
			listPickNode(pDictList->IDList, node);
			dictDelete(pDictList->IDDict, &pDictValue->id);
			DictValueDestory(pDictValue);
		}
	}
	listReleaseIterator(iter);
}