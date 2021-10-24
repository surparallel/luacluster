/* psemaphore.h - Time system related
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
#ifndef __PSEMAPHORE_H
#define __PSEMAPHORE_H


#include "uv.h"
typedef struct
{
	uv_mutex_t lock;
	uv_cond_t  cond;
	unsigned count;
} semwarp_t;

int semwarp_init(semwarp_t *psem, unsigned count);
int semwarp_destroy(semwarp_t *psem);
int semwarp_trywait(semwarp_t *psem);
int semwarp_wait(semwarp_t *psem);
int semwarp_timedwait(semwarp_t *psem, unsigned long long milliseconds);
int semwarp_post(semwarp_t *psem);
#endif
