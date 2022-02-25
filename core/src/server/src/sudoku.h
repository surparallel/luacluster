/* docker.c - worker thread
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

#include "lua.h"

int LuaOpenSudoku(lua_State* L);

void* SudokuCreate(struct Vector3 gird, struct Vector3 begin, struct Vector3 end, int isBigWorld, unsigned long long spaceId, unsigned int outsideSec);
void SudokuDestory(void* pSudokus);
void SudokuUpdate(void* pSudoku, unsigned int count, float deltaTime);
void SudokuEntry(void* pSudoku, unsigned long long id, struct Vector3 position, 
	struct Vector3 rotation, float velocity, unsigned int stamp, unsigned int isGhost, unsigned int stampStop);
void SudokuLeave(void* pSudoku, unsigned long long id);
void SudokuMove(void* pSudoku, unsigned long long id, struct Vector3 position, struct Vector3 rotation, float velocity, unsigned int stamp, unsigned int stampStop);
void PrintAllPoition(void* pSudoku);
int GirdDir(void* pSudoku, unsigned int entry, unsigned int centre);
unsigned int GirdId(void* pVoid, struct Vector3* position);