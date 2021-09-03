/* locks.c - Lock record of critical point
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
#include "sds.h"
#include "adlist.h"
#include "locks.h"

typedef struct _SafeMutex
{
	unsigned int rank;
	pthread_mutex_t lock;
}*PSafeMutex, SafeMutex;

static pthread_key_t exclusionZone = 0;
static pthread_key_t environmental = 0;
static pthread_key_t logfile = 0;

void _LocksCreate() {
	if (0 != pthread_key_create(&exclusionZone, NULL)) {
		assert(0);
	}
	if (0 != pthread_key_create(&environmental, NULL)) {
		assert(0);
	}
	if (0 != pthread_key_create(&logfile, NULL)) {
		assert(0);
	}
}

void _LocksDestroy() {
	pthread_key_delete(exclusionZone);
	pthread_key_delete(environmental);
	pthread_key_delete(logfile);
}

char _LocksEntry(void* pvSafeMutex) {

	PSafeMutex pSafeMutex = pvSafeMutex;
	//Currently internal thread needs to check lock status
	if (LocksGetSpecific() != 0) {
		assert(exclusionZone);
		list* ptr = pthread_getspecific(exclusionZone);
		//Current non lock state enters lock state creation
		if (ptr == 0) {
			ptr = listCreate();
			listAddNodeHead(ptr, pSafeMutex);
			pthread_setspecific(exclusionZone, ptr);
			return 1;
		} else {
			listNode *node = listFirst(ptr);
			if (node != 0) {
				PSafeMutex pHeadMutex = listNodeValue(node);
				//Access is allowed only when the permission is high
				if (pSafeMutex->rank <= pHeadMutex->rank) {
					return 0;
				}
			}
			listAddNodeHead(ptr, pSafeMutex);
			return 1;
		}
	} else {
		return 1;
	}
}

char _LocksLeave(void* pvSafeMutex) {

	PSafeMutex pSafeMutex = pvSafeMutex;
	//Currently internal thread needs to check lock status
	if (LocksGetSpecific() != 0) {
		assert(exclusionZone);
		list* ptr = pthread_getspecific(exclusionZone);
		
		//Repeated release will result in a critical error
		if (ptr != 0) {
			listNode *node = listFirst(ptr);
			if (node != 0) {

				PSafeMutex pHeadMutex = listNodeValue(node);
				//Lock not released in pairs, serious error reported
				if (pHeadMutex != pSafeMutex) {
					return 0;
				}
				listDelNode(ptr, node);
				return 1;
			}
			return 0;
		} else {
			return 0;
		}
	} else {
		return 1;
	}
}

void LocksSetSpecific(void* ptr) {
	assert(environmental);
	pthread_setspecific(environmental, ptr);
}

void* LocksGetSpecific() {
	if (!environmental) {
		return 0;
	}
	void* ptr = pthread_getspecific(environmental);
	return ptr;
}

void* MutexCreateHandle(unsigned int rank) {
	PSafeMutex pSafeMutex = malloc(sizeof(SafeMutex));
	pSafeMutex->rank = rank;
	pthread_mutex_init(&pSafeMutex->lock, 0);
	return pSafeMutex;
}

void MutexThreadDestroy() {
	assert(exclusionZone);
	list* ptr = pthread_getspecific(exclusionZone);
	if (ptr != 0) {
		listRelease(ptr);
		pthread_setspecific(exclusionZone, 0);
	}
}
void MutexDestroyHandle(void* pvSafeMutex) {
	PSafeMutex pSafeMutex = pvSafeMutex;
	pthread_mutex_destroy(&pSafeMutex->lock);
	free(pSafeMutex);
}

int _MutexLock(void* pvSafeMutex) {
	PSafeMutex pSafeMutex = pvSafeMutex;
	return pthread_mutex_lock(&pSafeMutex->lock);
}

int _MutexUnlock(void* pvSafeMutex) {
	PSafeMutex pSafeMutex = pvSafeMutex;
	return pthread_mutex_unlock(&pSafeMutex->lock);
}