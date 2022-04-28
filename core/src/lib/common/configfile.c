/* configfile.c - Log related, the annoying output format to the callback function
*
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
#include "plateform.h"
#include "configfile.h"
#include "filesys.h"
#include "sds.h"

static char* LoadConfigFile(char* config) {

	FILE* f; size_t len; char* data;
	f = fopen_t(config, "rb");
	if (f == NULL) {
		printf("Error Open File: [%s]\n", config);
		return NULL;
	}

	fseek_t(f, 0, SEEK_END);
	len = ftell_t(f);
	fseek_t(f, 0, SEEK_SET);
	data = (char*)malloc(len + 1);
	fread(data, 1, len, f);
	fclose(f);

	return data;
}

void DoJsonParseFile(void* pVoid, ConfigCallBack ccb) {
	//bin
	char* config = getenv("BinPath");
	sds configPath = sdsnew(config);
	configPath = sdscat(configPath, "../../res/server/config_defaults.json");
	if (access_t(configPath, 0) == 0) {
		char* pData = LoadConfigFile(configPath);
		if (pData) {
			ccb(pVoid, pData);
			free(pData);
		}
	}
	sdsfree(configPath);
	//pwd
	if (access_t("./res/server/config.json", 0) == 0) {
		char* pData = LoadConfigFile("./res/server/config.json");
		if (pData) {
			ccb(pVoid, pData);
			free(pData);
		}
	}
}