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

#ifndef __LOCK_H
#define __LOCK_H

#include "sds.h"

enum _LockLevel
{
	LockLevel_1 = 1,
	LockLevel_2,
	LockLevel_3,
	LockLevel_4
};
void LevelLocksCreate();
void LevelLocksDestroy();
char LevelLocksEntry(void* pSafeMutex);
char LevelLocksLeave(void* pSafeMutex);

void* MutexCreateHandle(unsigned int rank);
void MutexDestroyHandle(void* pSafeMutex);
int _MutexLock(void* pSafeMutex);
int _MutexUnlock(void* pSafeMutex);
void MutexThreadDestroy();


#define MutexLock(lockObj, lockName) do {\
if(0==LevelLocksEntry(lockObj)){\
		sds x = sdscatprintf(sdsempty(),"entry mutex %p %s!", lockObj, lockName);\
		sdsfree(x);}else {\
	_MutexLock(lockObj);}\
} while (0)

#define MutexUnlock(lockObj, lockName) do {\
if(0==LevelLocksLeave(lockObj)){\
		sds x = sdscatprintf(sdsempty(),"leave mutex %p %s!", lockObj, lockName);\
		sdsfree(x);}else{\
	_MutexUnlock(lockObj);}\
} while (0)

void LocksSetSpecific(void* ptr);
void* LocksGetSpecific();
#endif