/* equeue.c 
*
* Copyright(C) 2019 - 2020, sun shuo <sun.shuo@surparallel.org>
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
#include <pthread.h>
#include "adlist.h"
#include "equeue.h"
#include "sds.h"
#include "locks.h"

#ifdef __APPLE__
#include "psemaphore.h"
#else
#include <semaphore.h>
#endif

typedef struct _EventQueue
{
	void* mutexHandle;
	sds objecName;
	sem_t semaphore;
	list* listQueue;
} *PEventQueue, EventQueue;

void* EqCreate() {
	PEventQueue pEventQueue = malloc(sizeof(EventQueue));
	pEventQueue->mutexHandle = MutexCreateHandle(LockLevel_4);

	if (sem_init(&pEventQueue->semaphore, PTHREAD_PROCESS_PRIVATE, 0) != 0) {
		free(pEventQueue);
		return 0;
	}
	pEventQueue->listQueue = listCreate();
	pEventQueue->objecName = sdsnew("equeue");
	return pEventQueue;
}

int EqIfNoPush(void* pvEventQueue, void* value, unsigned int maxQueue) {

	unsigned r = 0;
	PEventQueue pEventQueue = pvEventQueue;
	MutexLock(pEventQueue->mutexHandle, pEventQueue->objecName);
	if (maxQueue && listLength(pEventQueue->listQueue) > maxQueue) {
	} else {
		r = 1;
		listAddNodeHead(pEventQueue->listQueue, value);
	}
	MutexUnlock(pEventQueue->mutexHandle, pEventQueue->objecName);

	if (r && sem_post(&pEventQueue->semaphore) != 0) {
		return r;
	}
	return r;
}

void EqPush(void* pvEventQueue, void* value) {

	PEventQueue pEventQueue = pvEventQueue;
	MutexLock(pEventQueue->mutexHandle, pEventQueue->objecName);
	listAddNodeHead(pEventQueue->listQueue, value);
	MutexUnlock(pEventQueue->mutexHandle, pEventQueue->objecName);

	if (sem_post(&pEventQueue->semaphore) != 0) {
		return;
	}
}

int EqTimeWait(void* pvEventQueue, long long sec, long long nsec) {

	PEventQueue pEventQueue = pvEventQueue;
	struct timespec ts;
	ts.tv_sec = sec;
	ts.tv_nsec =  (long)nsec;
	return sem_timedwait(&pEventQueue->semaphore, &ts);
}

int EqWait(void* pvEventQueue) {
	PEventQueue pEventQueue = pvEventQueue;
	return sem_wait(&pEventQueue->semaphore);
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
	sdsfree(pEventQueue->objecName);
	listSetFreeMethod(pEventQueue->listQueue, fun);
	listRelease(pEventQueue->listQueue);

	sem_destroy(&pEventQueue->semaphore);
	MutexDestroyHandle(pEventQueue->mutexHandle);
	free(pEventQueue);
}