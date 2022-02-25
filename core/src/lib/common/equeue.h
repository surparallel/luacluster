/* equeue.h
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

#ifndef __EQUEUE_H
#define __EQUEUE_H

typedef void(*QueuerDestroyFun)(void* value);
void* EqCreate();
void EqPush(void* pEventQueue, void* value);
int EqIfNoPush(void* pvEventQueue, void* value, unsigned int maxQueue);
int EqTimeWait(void* pEventQueue, unsigned long long milliseconds);
int EqWait(void* pEventQueue);
void* EqPop(void* pEventQueue);
void EqEmpty(void* pvEventQueue, QueuerDestroyFun fun);
void* EqPopWithLen(void* pvEventQueue, size_t*len);
void EqDestory(void* pEventQueue, QueuerDestroyFun fun);
void EqPopNodeWithLen(void* pvEventQueue, size_t limite, list* out, size_t* len);
void EqPushNode(void* pvEventQueue, listNode* node);
void EqPushList(void* pvEventQueue, list* in);

#endif