/* version.c - version function
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

#include "plateform.h"
#include "conio.h"
#include "version.h"

unsigned int NVersion() {
	return VERSION_NUMMINOR;
}

unsigned int MVersion() {
	return VERSION_NUMMAJOR;
}

void Version() {

	char path[256];
	getcwd(path, 256);

	Color(11);
	printf("luacluster version \"" VERSION_MAJOR "." VERSION_MINOR "\"\n");
	printf("Copyright(C) 2021-2022, sun shuo<sun.shuo@surparallel.org>\n");
	printf("* All rights reserved. *\n");
	printf("PWD:%s\n", path);
	ClearColor();
}