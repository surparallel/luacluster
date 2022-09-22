/* equeue.c 
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
#include "adlist.h"
#include "equeue.h"
#include "sds.h"
#include "locks.h"
#include "timesys.h"

typedef struct _EventQueue
{
	void* mutexHandle;
	sds objecName;
	semwarp_t semaphore;
	list* listQueue;
	unsigned long long stamp;
} *PEventQueue, EventQueue;

void* EqCreate() {
	PEventQueue pEventQueue = malloc(sizeof(EventQueue));
	if (pEventQueue == 0)
		return 0;

	pEventQueue->mutexHandle = MutexCreateHandle(LockLevel_4);

	int r = semwarp_init(&pEventQueue->semaphore, 0);
	if (r != 0) {
		free(pEventQueue);
		return 0;
	}
	pEventQueue->listQueue = listCreate();
	pEventQueue->objecName = sdsnew("equeue");
	return pEventQueue;
}

int EqIfNoPush(void* pvEventQueue, void* value, unsigned int maxQueue) {

	size_t r = 0;
	PEventQueue pEventQueue = pvEventQueue;
	MutexLock(pEventQueue->mutexHandle, pEventQueue->objecName);
	if (maxQueue && listLength(pEventQueue->listQueue) > maxQueue) {
	} else {
		listAddNodeHead(pEventQueue->listQueue, value);
		r = listLength(pEventQueue->listQueue);

		unsigned long long stamp = GetTick();
		if (stamp - pEventQueue->stamp > 10) {
			pEventQueue->stamp = stamp;
			semwarp_post(&pEventQueue->semaphore);
		}
	}
	MutexUnlock(pEventQueue->mutexHandle, pEventQueue->objecName);
	return (int)r;
}

void EqPushList(void* pvEventQueue, list* in) {
	if (listLength(in) == 0) return;
	PEventQueue pEventQueue = pvEventQueue;
	MutexLock(pEventQueue->mutexHandle, pEventQueue->objecName);
	size_t r = listLength(pEventQueue->listQueue);
	listAddNodeHeadForList(pEventQueue->listQueue, in);
	unsigned long long stamp = GetTick();
	if (stamp - pEventQueue->stamp > 10 && r == 0) {
		pEventQueue->stamp = stamp;
		semwarp_post(&pEventQueue->semaphore);
	}
	MutexUnlock(pEventQueue->mutexHandle, pEventQueue->objecName);
}

void EqPushNode(void* pvEventQueue, listNode* node) {
	PEventQueue pEventQueue = pvEventQueue;
	MutexLock(pEventQueue->mutexHandle, pEventQueue->objecName);
	size_t r = listLength(pEventQueue->listQueue);
	listAddNodeHeadForNode(pEventQueue->listQueue, node);
	unsigned long long stamp = GetTick();
	if (stamp - pEventQueue->stamp > 10 && r == 0) {
		pEventQueue->stamp = stamp;
		semwarp_post(&pEventQueue->semaphore);
	}
	MutexUnlock(pEventQueue->mutexHandle, pEventQueue->objecName);
}

void EqPush(void* pvEventQueue, void* value) {
	PEventQueue pEventQueue = pvEventQueue;
	MutexLock(pEventQueue->mutexHandle, pEventQueue->objecName);
	size_t r = listLength(pEventQueue->listQueue);
	listAddNodeHead(pEventQueue->listQueue, value);	
	unsigned long long stamp = GetTick();
	if (stamp - pEventQueue->stamp > 10 && r == 0) {
		pEventQueue->stamp = stamp;
		semwarp_post(&pEventQueue->semaphore);
	}
	MutexUnlock(pEventQueue->mutexHandle, pEventQueue->objecName);
}

int EqTimeWait(void* pvEventQueue, unsigned long long milliseconds) {

	PEventQueue pEventQueue = pvEventQueue;
	return semwarp_timedwait(&pEventQueue->semaphore, milliseconds);
}

int EqWait(void* pvEventQueue) {
	PEventQueue pEventQueue = pvEventQueue;
	return semwarp_wait(&pEventQueue->semaphore);
}

void* EqPop(void* pvEventQueue) {
	PEventQueue pEventQueue = pvEventQueue;
	void* value = 0;
	MutexLock(pEventQueue->mutexHandle, pEventQueue->objecName);
	if (listLength(pEventQueue->listQueue) != 0) {
		listNode *node = listLast(pEventQueue->listQueue);
		value = listNodeValue(node);
		listDelNode(pEventQueue->listQueue, node);
	}
	MutexUnlock(pEventQueue->mutexHandle, pEventQueue->objecName);
	return value;
}

void* EqPopWithLen(void* pvEventQueue, size_t *len) {
	PEventQueue pEventQueue = pvEventQueue;
	void* value = 0;
	*len = 0;
	MutexLock(pEventQueue->mutexHandle, pEventQueue->objecName);
	if (listLength(pEventQueue->listQueue) != 0) {
		listNode *node = listLast(pEventQueue->listQueue);
		value = listNodeValue(node);
		listDelNode(pEventQueue->listQueue, node);
		if (len) {
			*len = listLength(pEventQueue->listQueue);
		}
	}
	MutexUnlock(pEventQueue->mutexHandle, pEventQueue->objecName);
	return value;
}

void EqPopNodeWithLen(void* pvEventQueue, size_t limite, list* out, size_t* len) {
	PEventQueue pEventQueue = pvEventQueue;
	*len = 0;
	MutexLock(pEventQueue->mutexHandle, pEventQueue->objecName);
	if (listLength(pEventQueue->listQueue) != 0) {
		listPickListAddHead(pEventQueue->listQueue, out, limite);

		if (len) {
			*len = listLength(pEventQueue->listQueue);
		}
	}
	MutexUnlock(pEventQueue->mutexHandle, pEventQueue->objecName);
}

void EqEmpty(void* pvEventQueue, QueuerDestroyFun fun) {
	PEventQueue pEventQueue = pvEventQueue;

	MutexLock(pEventQueue->mutexHandle, pEventQueue->objecName);
	listSetFreeMethod(pEventQueue->listQueue, fun);
	listRelease(pEventQueue->listQueue);
	pEventQueue->listQueue = listCreate();
	MutexUnlock(pEventQueue->mutexHandle, pEventQueue->objecName);
}

void EqDestory(void* pvEventQueue, QueuerDestroyFun fun) {
	PEventQueue pEventQueue = pvEventQueue;

	semwarp_destroy(&pEventQueue->semaphore);
	MutexDestroyHandle(pEventQueue->mutexHandle);

	sdsfree(pEventQueue->objecName);
	listSetFreeMethod(pEventQueue->listQueue, fun);
	listRelease(pEventQueue->listQueue);
	free(pEventQueue);
}

void EqPushWithFun(void* pvEventQueue, void* value, BeatFun fun) {
	PEventQueue pEventQueue = pvEventQueue;
	MutexLock(pEventQueue->mutexHandle, pEventQueue->objecName);
	size_t r = listLength(pEventQueue->listQueue);
	listAddNodeHead(pEventQueue->listQueue, value);
	unsigned long long stamp = GetTick();
	if (stamp - pEventQueue->stamp > 10 && r == 0) {
		pEventQueue->stamp = stamp;
		fun();
	}
	MutexUnlock(pEventQueue->mutexHandle, pEventQueue->objecName);
}