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

#include "uv.h"
#include "plateform.h"
#include "sds.h"
#include "adlist.h"
#include "locks.h"

typedef struct _SafeMutex
{
	unsigned int rank;
	uv_mutex_t lock;
}*PSafeMutex, SafeMutex;

static uv_key_t exclusionZone = { 0 };
static uv_key_t environmental = { 0 };

void LevelLocksCreate() {
	if (0 != uv_key_create(&exclusionZone)) {
		assert(0);
	}
	if (0 != uv_key_create(&environmental)) {
		assert(0);
	}	
}

void LevelLocksDestroy() {
	uv_key_delete(&exclusionZone);
	uv_key_delete(&environmental);
}

char LevelLocksEntry(void* pvSafeMutex) {

	PSafeMutex pSafeMutex = pvSafeMutex;
	//Currently internal thread needs to check lock status
	if (LocksGetSpecific() != 0) {
		list* ptr = uv_key_get(&exclusionZone);
		//Current non lock state enters lock state creation
		if (ptr == 0) {
			ptr = listCreate();
			listAddNodeHead(ptr, pSafeMutex);
			uv_key_set(&exclusionZone, ptr);
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

char LevelLocksLeave(void* pvSafeMutex) {

	PSafeMutex pSafeMutex = pvSafeMutex;
	//Currently internal thread needs to check lock status
	if (LocksGetSpecific() != 0) {
		list* ptr = uv_key_get(&exclusionZone);
		
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
	uv_key_set(&environmental, ptr);
}

void* LocksGetSpecific() {
	void* ptr = uv_key_get(&environmental);
	return ptr;
}

void* MutexCreateHandle(unsigned int rank) {
	PSafeMutex pSafeMutex = malloc(sizeof(SafeMutex));
	pSafeMutex->rank = rank;
	uv_mutex_init(&pSafeMutex->lock);
	return pSafeMutex;
}

void MutexThreadDestroy() {
	list* ptr = uv_key_get(&exclusionZone);
	if (ptr != 0) {
		listRelease(ptr);
		uv_key_set(&exclusionZone, 0);
	}
}
void MutexDestroyHandle(void* pvSafeMutex) {
	PSafeMutex pSafeMutex = pvSafeMutex;
	uv_mutex_destroy(&pSafeMutex->lock);
	free(pSafeMutex);
}

int _MutexLock(void* pvSafeMutex) {
	PSafeMutex pSafeMutex = pvSafeMutex;
	uv_mutex_lock(&pSafeMutex->lock);
	return 0;
}

int _MutexUnlock(void* pvSafeMutex) {
	PSafeMutex pSafeMutex = pvSafeMutex;
	uv_mutex_unlock(&pSafeMutex->lock);
	return 0;
}