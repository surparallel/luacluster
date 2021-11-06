/* packtest.c
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
#include "filesys.h"
#include "lua_cmsgpack.h"
#include "cJSON.h"
#include "packjson.h"

void DoTestPack() {
	FILE* f; size_t len; char* data;
	cJSON* json = 0;

	f = fopen_t("./test.json", "rb");

	if (f == NULL) {
		return;
	}

	fseek_t(f, 0, SEEK_END);
	len = ftell_t(f);
	fseek_t(f, 0, SEEK_SET);
	data = (char*)malloc(len + 1);
	fread(data, 1, len, f);
	fclose(f);
	
	json = cJSON_Parse(data);
	char* fdata = cJSON_Print(json);
	cJSON_Delete(json);

	mp_buf* pk = mp_buf_new();
	PackJson(fdata, pk);
	UnpackJson(pk->b, pk->len, &json);
	if (json != 0) {
		char* out = cJSON_Print(json);
		if (0 != memcmp(out, fdata, len)) {
			printf("error %s", out);
		}
		free(out);
		cJSON_Delete(json);
	}

	free(fdata);
	free(data);
}