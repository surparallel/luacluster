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

#include "plateform.h"
#include "entityid.h"
#include "elog.h"
#include "vector.h"
#include "sudoku.h"
#include "timesys.h"

void TestGirdDir(void* s) {

	void* pSudoku = s;
	printf("%d ", GirdDir(pSudoku, 1, 5));
	printf("%d ", GirdDir(pSudoku, 2, 5));
	printf("%d ", GirdDir(pSudoku, 3, 5));
	printf("%d ", GirdDir(pSudoku, 4, 5));
	printf("%d ", GirdDir(pSudoku, 6, 5));
	printf("%d ", GirdDir(pSudoku, 7, 5));
	printf("%d ", GirdDir(pSudoku, 8, 5));
	printf("%d \n", GirdDir(pSudoku, 9, 5));

}

void test_sudoku() {

	struct Vector3 gird;
	struct Vector3 begin;
	struct Vector3 end;
	struct Vector3 position = { 0 }, rotation = { 0 };
	void* hand;
	gird.x = 10;
	gird.z = 10;
	begin.x = 0;
	begin.z = 0;
	end.x = 100;
	end.z = 100;
/*	hand = SudokuCreate(gird, begin, end);
	
	
	for (int i = 0; i < 10000; i++) {
		position.x = rand() % 100;
		position.z = rand() % 100;

		SudokuEntry(hand, i, position, rotation, 0, GetCurrentSec());
	}
	
	SudokuEntry(hand, 9999999, position, rotation, 0, GetCurrentSec());

	SudokuUpdate(hand);
	SudokuDestory(hand);
*/
	gird.x = 10;
	gird.z = 10;
	begin.x = 0;
	begin.z = 0;
	end.x = 30;
	end.z = 30;
	hand = SudokuCreate(gird, begin, end, 0, 0, 0);

	TestGirdDir(hand);

	for (int i = 0; i < 5; i++) {
		position.x = (float)(rand() % 30);
		position.z = (float)(rand() % 30);

		rotation.x = (float)(rand() % 360);
		rotation.z = (float)(rand() % 360);

		SudokuEntry(hand, i, position, rotation, 1, GetCurrentSec(), 0, 0);
	}

	for (int i = 0; i < 100; i++) {
		sleep(1);
		printf("************  %d  *****************\n", i);
		SudokuUpdate(hand, i, 10);
		PrintAllPoition(hand);
	}


	SudokuDestory(hand);
}