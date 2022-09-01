/* luacluster.c - main function
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

#include "uv.h"
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
#include "hiredis.h"
#include "filesys.h"
#include "3dmath.h"
#include "space.h"
#include "locks.h"
#include "redis.h"
#include "packtest.h"
#include "packjson.h"
#include "timesys.h"
#include "entityid.h"
#include "uvnetmng.h"
#include "configfile.h"

static char binPath[256] = { 0 };
static unsigned int dockerid = 0;
static unsigned char* ip = 0;
static unsigned short port = 0;
static unsigned char nodetype = 0;//默认情况下是有外网的
static unsigned short listentcp = 0;
static unsigned short bots = 0;
static char* bip = 0;
static unsigned short bport = 0;
static unsigned short bcount = 0;
static unsigned short brang = 0;

/* Print generic help. */
void CliOutputGenericHelp(void) {
	printf(
		"\n"
		"      \"quit\" to exit\n"
		"      \"new [type][class name]\" --[type]DockerCurrent = 0,DockerRandom,NodeInside,NodeOutside,NodeRandom,DockerGlobe\n"
		"      \"call [entityid][...]\" --.\n"
		"      \"connect [ip][port][docker]\" --Set UDP link parameters.\n"
		"      \"run [script]\" --Run UDP script.\n"
		"      \"btcp (bc) [ip][port][count]\" --bots tcp connect.\n"
		"      \"redis\" -- test redis.\n"
		"      \"3dmath\" -- test 3dmath.\n"
		"      \"pack\" -- test pack.\n"
		"      \"sudoku\" -- test sudoku.\n"
		"      \"testid\" -- test id.\n"
	); 
}

void btcp2(unsigned int count, sds ip, unsigned short len, unsigned short port, unsigned short rang)
{
	for (size_t i = 0; i < count; i++)
	{
		unsigned short r = rand() % rang;
		int len = sizeof(ProtoConnect);
		PProtoConnect pbuf = calloc(1, len);
		if (pbuf == NULL)continue;
		pbuf->protoHead.len = len;
		pbuf->protoHead.proto = proto_net_connect;
		memcpy(pbuf->ip, ip, len);
		pbuf->port = port + r;

		MngRandomSendTo((const char*)pbuf, len);
	}

	printf("btcp2 start %d done!\n", count);
}

void btcp(unsigned int count, sds ip, unsigned short len, unsigned short port)
{
	for (size_t i = 0; i < count; i++)
	{
		int len = sizeof(ProtoConnect);
		PProtoConnect pbuf = calloc(1, len);
		pbuf->protoHead.len = len;
		pbuf->protoHead.proto = proto_net_connect;
		memcpy(pbuf->ip, ip, len);
		pbuf->port = port;

		MngRandomSendTo((const char*)pbuf, len);
	}

	printf("btcp start %d done!\n", count);
}

int IssueCommand(int argc, char** argv, int noFind) {

	if (!argc) {
		return 1;
	}
	char* command = argv[0];

	if (!strcasecmp(command, "help") || !strcasecmp(command, "?") || !strcasecmp(command, "h")) {
		CliOutputGenericHelp();
		return 1;
	}
	else if (!strcasecmp(command, "version") || !strcasecmp(command, "v")) {
		Version();
		return 1;
	}
	else if (!strcasecmp(command, "quit") || !strcasecmp(command, "q")) {
		printf("bye!\n");
		return 0;
	}
	else if (!strcasecmp(command, "redis")) {
		doRedisTest();
	}
	else if (!strcasecmp(command, "3dmath")) {
		do3dlibTest();
	}
	else if (!strcasecmp(command, "pack")) {
		DoTestPack();
	}
	else if (!strcasecmp(command, "new")) {

		if (argc != 4) {
			printf("The parameters of the create instruction must be even\n");
			return 1;
		}
		
		mp_buf* pmp_buf = mp_buf_new();
		mp_encode_bytes(pmp_buf, argv[2], strlen(argv[2]));
		PackJson(argv[3], pmp_buf);

		DockerCreateEntity(NULL, atoi(argv[1]), pmp_buf->b, pmp_buf->len);
		mp_buf_free(pmp_buf);
	}
	else if (!strcasecmp(command, "call")) {
		if (argc < 3) {
			printf("Parameter does not enough the requirement\n");
			return 1;
		}

		unsigned long long id = strtoull(argv[1], NULL, 0);
		if (id == 0) {
			GetEntityFromDns(argv[1], &id);
		}

		mp_buf* pmp_buf = mp_buf_new();

		//function name
		mp_encode_bytes(pmp_buf, argv[2], strlen(argv[2]));

		if (argc >= 3) {
			//arg call ******* fun2 111 {"a"：1} aaa
			for (int i = 3; i < argc; i++) {
			
				int r = IsNumber(argv[i]);
				if (r == 1) {
					mp_encode_int(pmp_buf, atoi(argv[i]));
				}else if (r == 3) {
						mp_encode_double(pmp_buf, atof(argv[i]));
				}else if (0 == PackJson(argv[i], pmp_buf)) {			
					if (r == 0) {
						mp_encode_bytes(pmp_buf, argv[i], strlen(argv[i]));
					} else {
						mp_encode_int(pmp_buf, 0);
					}
				}
			}
		}

		DockerSend(id, pmp_buf->b, pmp_buf->len);
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

		DockerRunScript(ip, port, dockerid, (unsigned char*)argv[1], strlen(argv[1]));
	}
	else if (!strcasecmp(command, "btcp") || !strcasecmp(command, "bc")) {

		if (argc < 4) {
			printf("Bots tcp connect Parameter does not enough the requirement\n");
			return 1;
		}
		btcp(atoi(argv[3]), argv[1], sdslen(argv[1]), atoi(argv[2]));
	}
	else if (!strcasecmp(command, "sudoku")) {
		test_sudoku();
	}
	else if (!strcasecmp(command, "testid")) {
		test_id();
	}
	else if (!strcasecmp(command, "list")) {

		if (argc == 2) {
			unsigned int id = atoi(argv[1]);
			DockerList(0, 1, id);
		}
		else {
			printf("Parameter does not enough the requirement\n");
			return 1;
		}
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

int ReadArgFromParam(int argc, char** argv, sds* allarg) {
	for (int i = 0; i < argc; i++) {

		if (i == argc - 1) {
			*allarg = sdscatprintf(*allarg, "%s", argv[i]);
		}
		else {
			*allarg = sdscatprintf(*allarg, "%s ", argv[i]);
		}
		
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
		else if (strcmp(argv[i], "--binPath") == 0)
		{
			if (checkArg(argv[i + 1])) {
				strcpy(binPath, argv[i + 1]);

				char path[260] = {0};
				strcat(path, "BinPath=");
				strcat(path, binPath);
				putenv(path);
			}
			else {
				printf("Not enough parameters found!\n");
			}

		}
		else if (strcmp(argv[i], "--inside") == 0)
		{
			nodetype |= NO_TCP_LISTEN;
		}
		else if (strcmp(argv[i], "--noudp") == 0)
		{
			nodetype |= NO_UDP_LISTEN;
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
		else if (strcmp(argv[i], "--bots") == 0)
		{
			bots = 1;
		}
		else if (strcmp(argv[i], "--btcp") == 0)
		{
			if (checkArg(argv[i + 1])) {
				bip = argv[i + 1];
			}
			if (checkArg(argv[i + 2])) {
				bport = atoi(argv[i + 2]);
			}
			if (checkArg(argv[i + 3])) {
				bcount = atoi(argv[i + 3]);
			}
		}
		else if (strcmp(argv[i], "--btcp2") == 0)
		{
			if (checkArg(argv[i + 1])) {
				bip = argv[i + 1];
			}
			if (checkArg(argv[i + 2])) {
				bport = atoi(argv[i + 2]);
			}
			if (checkArg(argv[i + 3])) {
				bcount = atoi(argv[i + 3]);
			}
			if(checkArg(argv[i + 4])) {
				brang = atoi(argv[i + 4]);
			}
		}
	}

	return 1;
}

//getenv
static void doJsonParseFile(void* pVoid, char* data)
{
	cJSON* json = cJSON_Parse(data);
	if (!json) {
		//printf("Error before: [%s]\n", cJSON_GetErrorPtr());	
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
}

int main(int argc, char** argv) {
	srand(time(0));

	char path[260] = {0};
	size_t pbs = 256;
	uv_exepath(binPath, &pbs);
	RmFileName(binPath);
	strcat(path, "BinPath=");
	strcat(path, binPath);
	putenv(path);

	LevelLocksCreate();
	Version();

	int ret = 1;
	sds allgrg = sdsempty();
	if (ReadArgFromParam(argc, argv, &allgrg)) {

		DoJsonParseFile(NULL, doJsonParseFile);
		LogInit();
		//这里有需要输入参数的，指定要绑定的地址之类的
		n_details("**********************************************");
		n_details("*              hello luacluster!              *");
		n_details("**********************************************");
		s_details("**********************************************");
		s_details("*              hello luacluster!              *");
		s_details("**********************************************");
		u_details("**********************************************");
		u_details("*              hello luacluster!              *");
		u_details("**********************************************");

		n_details("ALLGRG:'%s'", allgrg);
		n_details("BINPATH:'%s'", binPath);

		InitRedisHelp();
		//创建网络层
		NetMngCreate(nodetype);
		DocksCreate(binPath, nodetype, bots);

		if (bip != 0) {
			if(brang == 0)
				btcp(bcount, bip, strlen(bip), bport);
			else
				btcp2(bcount, bip, strlen(bip), bport, brang);
		}
		ret = ArgsInteractive(IssueCommand);
	}

	sdsfree(allgrg);
	//删除网络层
	NetMngDestory();
	DocksDestory();
	LogDestroy();
	FreeRedisHelp();
	LevelLocksDestroy();
	return ret;
}
