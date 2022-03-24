/* dictQueue.h
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

typedef struct _DictList {
	list* IDList;//竖向定位
	dict* IDDict;//横向定位
	unsigned int len;
}*PDictList, DictList;

typedef void(*DqBeatFun)();
typedef int(*DqSendFun)(unsigned long long id, uv_buf_t* uvBuf, unsigned int uvBufLen, void* pVoid);

void DqLoopValue(void* pVoid, PDictList pDictList, DqSendFun fun, void* pAram);
PDictList DqPop(void* pVoid, PDictList pDictList);
void DqPush(void* pVoid, unsigned long long id, void* value, DqBeatFun fun, void* pAram);
void DqDestory(void* pVoid);
void* DqCreate();
void DictListDestory(PDictList pDictList);
PDictList DictListCreate();