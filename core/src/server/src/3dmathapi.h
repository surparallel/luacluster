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

int LuaOpenMath3d(lua_State* L);
void CurrentPosition(struct Vector3* position,
	struct Vector3* angles, 
	float velocity, 
	unsigned int stamp, 
	unsigned int stampStop, 
	struct Vector3* out,
	unsigned int current
);

void LookVector(struct Vector3* a, struct Vector3* b, struct Vector3* euler, float* distance);