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

#include "bitorder.h"

unsigned char bitltoh(unsigned char bit) {

	unsigned char mov, out;
	out = 0;
	mov = bit;

	for (int i = 0; i < 8; i++)
	{
		out = (out << 1) | (mov & 1);
		mov = mov >> 1;
	}
	return out;
}

unsigned char bithtol(unsigned char bit) {

	unsigned char mov, out;
	out = 0;
	mov = bit;

	for (int i = 0; i < 8; i++)
	{
		out = (out >> 1) | (mov & 128);
		mov = mov << 1;	
	}
	return out;
}