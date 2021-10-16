/* grypania.c - main function
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
#include "args.h"
#include "conio.h"
#include "version.h"
#include "elog.h"
#include "lua.h"
#include "docker.h"
#include "hiredis.h"
#include "proto.h"
#include "lua_cmsgpack.h"
#include "redishelp.h"
#include "cJSON.h"
#include "redis.h"
#include "filesys.h"
#include "3dmath.h"
#include "space.h"
#include "uvnet.h"

static const char* assetsPath = 0;
static unsigned int dockerid = 0;
static unsigned char* ip = 0;
static unsigned short port = 0;
static unsigned short dockerSize = 1;
static unsigned char nodetype = 0;//默认情况下是有外网的
static unsigned short listentcp = 0;

/* Print generic help. */
void CliOutputGenericHelp(void) {
	printf(
		"\n"
		"      \"quit\" to exit\n"
		"      \"redis\" --.\n"
		"      \"net\" --.\n"
		"      \"create [...]\" --.\n"
		"      \"call [entityid][...]\" --.\n"
		"      \"connect [ip][port][docker]\" --.\n"
		"      \"run [script]\" --.\n"
		"      \"client [did][pid]\" --.\n"
	);
}

int IssueCommand(int argc, char** argv, int noFind) {

	if (!argc) {
		return 1;
	}
	char* command = argv[0];

	if (!strcasecmp(command, "help") || !strcasecmp(command, "?")) {
		CliOutputGenericHelp();
		return 1;
	}
	else if (!strcasecmp(command, "version")) {
		Version();
		return 1;
	}
	else if (!strcasecmp(command, "quit")) {
		printf("bye!\n");
		return 0;
	}
	else if (!strcasecmp(command, "redis")) {
		doRedisTest();
	}
	else if (!strcasecmp(command, "3dmath")) {
		do3dlibTest();
	}
	else if (!strcasecmp(command, "net")) {
		NetSendToClient(atoi(argv[1]), argv[2], strlen(argv[2]));
	}
	else if (!strcasecmp(command, "create")) {

		if (argc < 2 ) {
			printf("The parameters of the create instruction must be even\n");
			return 1;
		}
		
		mp_buf* pmp_buf = mp_buf_new();
		mp_encode_bytes(pmp_buf, argv[2], strlen(argv[2]));

		if ((argc - 2) % 2 != 0) {

			return 1;
		}

		if (argc - 2 > 0) {
			mp_encode_map(pmp_buf, (argc - 2)/2);
			for (int i = 3; i < argc; i++) {
				mp_encode_bytes(pmp_buf, argv[i], strlen(argv[i]));
			}
		}

		DockerCreateEntity(NULL, atoll(argv[1]), pmp_buf->b, pmp_buf->len);
		mp_buf_free(pmp_buf);
	}
	else if (!strcasecmp(command, "call")) {

		if (argc < 2) {
			printf("Parameter does not enough the requirement\n");
			return 1;
		}

		mp_buf* pmp_buf = mp_buf_new();
		for (int i = 2; i < argc; i++) {
			mp_encode_bytes(pmp_buf, argv[i], strlen(argv[i]));
		}

		DockerSend(strtoull(argv[1], NULL, 0), pmp_buf->b, pmp_buf->len);
		mp_buf_free(pmp_buf);
	}
	else if (!strcasecmp(command, "connect")) {

		if (argc < 4) {
			printf("Parameter does not enough the requirement\n");
			return 1;
		}

		//注意这里可能要实现文字转ip或其他
		if (ip != 0)
			sdsfree(ip);
		ip = sdsnew(argv[1]);
		port = atoi(argv[2]);
		dockerid = atoi(argv[3]);
	}
	else if (!strcasecmp(command, "run")) {

		if (argc < 2) {
			printf("Parameter does not enough the requirement\n");
			return 1;
		}

		int strsize = strlen(argv[1]);
		unsigned short len = sizeof(ProtoRPC) + strsize;
		PProtoHead pProtoHead = malloc(len);
		pProtoHead->len = len;
		pProtoHead->proto = proto_run_lua;

		PProtoRunLua pProtoRunLua = (PProtoRunLua)pProtoHead;
		memcpy(pProtoRunLua->luaString, argv[1], strsize);

		if (ip == 0 && port == 0) {
			DockerPushMsg(dockerid, (unsigned char*)pProtoHead, len);
		}
		else
		{
			NetSendToNode(ip, port, (unsigned char*)pProtoHead, len);
		}
		free(pProtoHead);
	}
	else if (!strcasecmp(command, "client")) {

		if (argc < 3) {
			printf("Parameter does not enough the requirement\n");
			return 1;
		}

		mp_buf* pmp_buf = mp_buf_new();
		mp_encode_bytes(pmp_buf, argv[2], strlen(argv[2]));

		for (int i = 3; i < argc; i++) {
			mp_encode_bytes(pmp_buf, argv[i], strlen(argv[i]));
		}

		DockerSendToClient(NULL, atoll(argv[1]), atoll(argv[2]), pmp_buf->b, pmp_buf->len);
		mp_buf_free(pmp_buf);
	}
	else if (!strcasecmp(command, "btcp")) {

		if (argc < 3) {
			printf("botstcp Parameter does not enough the requirement\n");
			return 1;
		}

		int len = sizeof(ProtoConnect);
		PProtoConnect pbuf = calloc(1, len);
		pbuf->protoHead.len = len;
		pbuf->protoHead.proto = proto_net_connect;
		memcpy(pbuf->ip, argv[1], sdslen(argv[1]));
		pbuf->port = atoi(argv[2]);

		NetSendToClient(0, (const char*)pbuf, len);
	}
	else if (!strcasecmp(command, "sudoku")) {
		test_sudoku();
	}
	else {
		printf("not find command %s\n", command);
	}

	return 1;
}

static int checkArg(char* argv) {
	if (strcmp(argv, "--") == 0 || strcmp(argv, "-") == 0)
		return 0;
	else
		return 1;
}

int ReadArgFromParam(int argc, char** argv) {

	for (int i = 0; i < argc; i++) {
		/* Handle special options --help and --version */
		if (strcmp(argv[i], "-v") == 0 ||
			strcmp(argv[i], "--version") == 0)
		{
			Version();
			return 0;
		}
		else if (strcmp(argv[i], "--help") == 0 ||
			strcmp(argv[i], "-h") == 0)
		{
			printf(
				"\n"
				"      \"-v --version\" to version\n"
				"      \"-h --help\" to help\n"
			);
			return 0;
		}
		else if (strcmp(argv[i], "--assets") == 0)
		{
			if (checkArg(argv[i + 1])) {
				assetsPath = argv[i + 1];

				char path[260];
				strcat(path, "GrypaniaAssetsPath=");
				strcat(path, assetsPath);
				putenv(path);
			}
			else {
				printf("Not enough parameters found!\n");
			}

		}
		else if (strcmp(argv[i], "--dockersize") == 0)
		{
			if (checkArg(argv[i + 1])) {
				dockerSize = atoi(argv[i + 1]);
			}
			else {
				printf("Not enough parameters found!\n");
			}
		}
		else if (strcmp(argv[i], "--inside") == 0)
		{
			if (checkArg(argv[i + 1])) {
				nodetype = atoi(argv[i + 1]);
			}
			else {
				printf("Not enough parameters found!\n");
			}
		}
		else if (strcmp(argv[i], "--listentcp") == 0)
		{
			if (checkArg(argv[i + 1])) {
				listentcp = atoi(argv[i + 1]);
			}
			else {
				printf("Not enough parameters found!\n");
			};
		}
	}

	return 1;
}

//getenv
static void doJsonParseFile(char* config)
{
	if (config == NULL) {
		config = getenv("GrypaniaAssetsPath");
		if (config == 0 || access_t(config, 0) != 0) {
			config = "../../res/server/config_defaults.json";
			if (access_t(config, 0) != 0) {
				return;
			}
		}
	}

	FILE* f; size_t len; char* data;
	cJSON* json;

	f = fopen(config, "rb");

	if (f == NULL) {
		//printf("Error Open File: [%s]\n", filename);
		return;
	}

	fseek(f, 0, SEEK_END);
	len = ftell(f);
	fseek(f, 0, SEEK_SET);
	data = (char*)malloc(len + 1);
	fread(data, 1, len, f);
	fclose(f);

	json = cJSON_Parse(data);
	if (!json) {
		//printf("Error before: [%s]\n", cJSON_GetErrorPtr());	
		free(data);
		return;
	}

	cJSON* logJson = cJSON_GetObjectItem(json, "base_config");
	if (logJson) {

		cJSON* item = logJson->child;
		while ((item != NULL) && (item->string != NULL))
		{
			if (strcmp(item->string, "nodetype") == 0) {
				nodetype = (int)cJSON_GetNumberValue(item);
			}

			item = item->next;
		}
	}

	cJSON_Delete(json);
	free(data);
}

int main(int argc, char** argv) {

	srand(time(0));
	LogInit(NULL);
	//这里有需要输入参数的，指定要绑定的地址之类的
	n_details("********************************************");
	n_details("*              hello grypania!             *");
	n_details("********************************************");
	s_details("********************************************");
	s_details("*              hello grypania!             *");
	s_details("********************************************");

	doJsonParseFile(NULL);

	Version();
	int ret = 1;
	if (ReadArgFromParam(argc, argv)) {

		InitRedisHelp();
		NetCreate(nodetype, listentcp);
		unsigned int ip = 0;
		unsigned char uportOffset = 0;
		unsigned short uport = 0;
		NetUDPAddr2(&ip, &uportOffset, &uport);
		DocksCreate(ip, uportOffset, uport, assetsPath, dockerSize, nodetype);

		ret = ArgsInteractive(IssueCommand);
		DocksDestory();
		NetDestory();
		LogDestroy();
		FreeRedisHelp();
		return ret;
	}
	else {
		DocksDestory();
		NetDestory();
		LogDestroy();
		FreeRedisHelp();
		return ret;
	}
}
