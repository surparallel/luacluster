/*
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
#include <float.h>
#include "plateform.h"
#include "testid.h"
#include "entityid.h"

void test_id() {

	for (int j = 0; j < 256; j++)
	{
		for (int i = 0; i < 128; i++)
		{

			if (j == 255 && i == 127) {
				int a = 0;
			}
			idl64 a;
			a.eid.addr = 0;
			a.eid.dock = j;
			a.eid.port = MakeUp(a.eid.dock,i);
			a.eid.id = 0;
			
			if (0 != _isnan(a.d)) {
				printf("%d %d error _isnan \n", i, j);
			}
			/*
			if (0 != _finite(a.d)) {
				printf("%d error _finite \n", i);
			}*/
		}
	}
	printf("test_id done \n");
}