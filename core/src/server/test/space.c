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
#include "3dmathapi.h"

void TestGirdDir(void* s) {

	void* pSudoku = s;
	enum SudokuDir {
		up = 0,
		down,
		left,
		right,
		upleft,
		upright,
		downleft,
		downright,
		dircentre,
		SudokuDirError
	};

	if (GirdDir(pSudoku, 0, 4) != downleft) {
		printf("error 1");
	}else if (GirdDir(pSudoku, 1, 4) != down) {
		printf("error 2");
	}
	else if (GirdDir(pSudoku, 2, 4) != downright) {
		printf("error 3");
	}
	else if (GirdDir(pSudoku, 3, 4) != left) {
		printf("error 4");
	}
	else if (GirdDir(pSudoku, 4, 4) != dircentre) {
		printf("error 5");
	}
	else if (GirdDir(pSudoku, 5, 4) != right) {
		printf("error 6");
	}
	else if (GirdDir(pSudoku, 6, 4) != upleft) {
		printf("error 7");
	}
	else if (GirdDir(pSudoku, 7, 4) != up) {
		printf("error 8");
	}
	else if (GirdDir(pSudoku, 8, 4) != upright) {
		printf("error 9");
	}
}
#define Round(x) round(x * 1000) / 1000

void test_sudoku() {

	struct Vector3 cp_p = { 8.132000f, 13.683000f, 28.117000f };
	struct Vector3 cp_r = { 41.789000f, -16.738000f, -137.851000f };
	struct Vector3 cp_out;
	/*
	for (size_t i = 0; i < 105; i+=10)
	{
		CurrentPosition(&cp_p, &cp_r, 0.2f, 641796365.f, 1641796471.4f, &cp_out, 641796365 + i);
	}
	//105
	*/
	struct Vector3 a = {99.f, 0.f, 30.f};
	struct Vector3 b = { 99.08f, 0.f, 99.08f};
	struct Vector3 euler;
	float distance;

	LookVector(&a, &b, &euler, &distance);
/*
	for (float i = -1.0f; i <= 1.1f; i+=0.1)
	{
		for (float j = -1.0f; j <= 1.1f; j+=0.1)
		{
			if (i == 0 && j == 0)
				continue;
			struct Vector3 b = { i, 0, j };
			struct Vector3 a = { 0, 0, 0 };

			LookVector(&a, &b, &euler, &distance);

			printf("%f %f %f %f %f\r\n", i, j, euler.x, euler.y, euler.z);
		}
	}
*/

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
	struct Vector3 pos = { 29.9f, 0, 29.9f };
	int id = GirdId(hand, &pos);

	TestGirdDir(hand);
/*
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

*/
	SudokuDestory(hand);
}