/*
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
//https://stackoverflow.com/questions/641126/posix-semaphores-on-mac-os-x-sem-timedwait-alternative

int semwarp_init(semwarp_t *psem, unsigned count) {

	int result;
	result = uv_mutex_init(&psem->lock);
	if (result) {
		return result;
	}
	result = uv_cond_init(&psem->cond);
	if (result) {
		return result;
	}
	psem->count = count;
	return 0;
}

int semwarp_destroy(semwarp_t *psem) {

	uv_mutex_destroy(&psem->lock);
	uv_cond_destroy(&psem->cond);
	return 0;
}

int semwarp_post(semwarp_t *psem) {

	if (!psem) {
		return -1;
	}

	uv_mutex_lock(&psem->lock);
	psem->count = psem->count + 1;
	uv_cond_signal(&psem->cond);
	uv_mutex_unlock(&psem->lock);

	return 0;
}

int semwarp_trywait(semwarp_t*psem) {
	int xresult = 0;

	if (!psem) {
		return -1;
	}

	uv_mutex_lock(&psem->lock);
	if (psem->count > 0) {
		psem->count--;
	} else {
		xresult = -1;
	}
	uv_mutex_unlock(&psem->lock);

	if (xresult)
		return -1;
	else
		return 0;
}

int semwarp_wait(semwarp_t *psem) {

	int xresult;

	if (!psem) {
		return -1;
	}

	uv_mutex_lock(&psem->lock);
	xresult = 0;

	if (psem->count == 0) {
		uv_cond_wait(&psem->cond, &psem->lock);
	}
	if (!xresult) {
		if (psem->count > 0) {
			psem->count--;
		}
	}
	uv_mutex_unlock(&psem->lock);

	if (xresult) {
		return -1;
	}
	return 0;
}

int semwarp_timedwait(semwarp_t*psem, unsigned long long milliseconds) {

	int xresult;
	if (!psem) {
		return -1;
	}

	uv_mutex_lock(&psem->lock);
	xresult = 0;

	if (psem->count == 0) {
		xresult = uv_cond_timedwait(&psem->cond, &psem->lock, milliseconds * 1e6);
	}
	if (!xresult) {
		if (psem->count > 0) {
			psem->count--;
		}
	}
	uv_mutex_unlock(&psem->lock);
	if (xresult) {
		return -1;
	}
	return 0;
}