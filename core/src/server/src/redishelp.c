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

#include "uv.h"
#include "plateform.h"
#include "hiredis.h"
#include "elog.h"
#include "sds.h"
#include "cJSON.h"
#include "filesys.h"

static uv_key_t redis = { 0 };

static sds redis_ip;
static unsigned redis_port;

static void doJsonParseFile(char* config)
{
    if (config == NULL) {
        config = getenv("AssetsPath");
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
        printf("Error Open File: [%s]\n", config);
        return;
    }

    fseek_t(f, 0, SEEK_END);
    len = ftell_t(f);
    fseek_t(f, 0, SEEK_SET);
    data = (char*)malloc(len + 1);
    fread(data, 1, len, f);
    fclose(f);

    json = cJSON_Parse(data);
    if (!json) {
        //printf("Error before: [%s]\n", cJSON_GetErrorPtr());	
        free(data);
        return;
    }

    cJSON* logJson = cJSON_GetObjectItem(json, "redis_config");
    if (logJson) {

        cJSON* item = logJson->child;
        while ((item != NULL) && (item->string != NULL))
        {
            if (strcmp(item->string, "redis_ip") == 0) {
                sdsfree(redis_ip);
                redis_ip = sdsnew(cJSON_GetStringValue(item));
            }
            else if (strcmp(item->string, "redis_port") == 0) {
                redis_port = (unsigned short)cJSON_GetNumberValue(item);
            }
            item = item->next;
        }
    }

    cJSON_Delete(json);
    free(data);
}

void InitRedisHelp() {
	redis_ip = sdsnew("127.0.0.1");
	redis_port = 6379;

	doJsonParseFile(NULL);

	if (0 != uv_key_create(&redis)) {
		assert(0);
	}
}

void FreeRedisHelp() {
	sdsfree(redis_ip);
	uv_key_delete(&redis);
}

void SetUpdToAll(redisContext* c, unsigned int ip, unsigned short port) {

	const char* command = "SADD cluster:nodeall %d:%d";
	redisReply* r = (redisReply*)redisCommand(c, command, ip, port);

	if (NULL == r)
	{
		n_error("Execut command failure");
		redisFree(c);
		uv_key_set(&redis, 0);
		return;
	}

	if (r->type == REDIS_REPLY_INTEGER && r->integer != 1)
	{
		n_warn("Failed to execute command[%s]", command);
		freeReplyObject(r);
		return;
	}

	freeReplyObject(r);
}

void GetUpdFromAll(unsigned int* ip, unsigned short* port) {

	//redis默认监听端口为6387 可以再配置文件中修改
	redisContext* c = uv_key_get(&redis);
	if (c == 0) {
		c = redisConnect(redis_ip, redis_port);
		uv_key_set(&redis, c);
	}

	if (c->err)
	{
		redisFree(c);
		uv_key_set(&redis, 0);
		n_error("Connect to redisServer faile");
		return;
	}
	const char* command = "SRANDMEMBER cluster:nodeall 1";
	redisReply* r = (redisReply*)redisCommand(c, command);

	if (NULL == r)
	{
		n_error("Execut command failure");
		redisFree(c);
		uv_key_set(&redis, 0);
		return;
	}

	if (r->type != REDIS_REPLY_ARRAY || (r->type == REDIS_REPLY_ARRAY && r->elements == 0))
	{
		n_warn("Failed to execute command[%s]", command);
		freeReplyObject(r);
		return;
	}

	sds s = sdsnew(r->element[0]->str);
	int count = 0;
	sds* tokens = sdssplitlen(s, sdslen(s), ":", 1, &count);
	if (count == 2) {
		*ip = atoi(tokens[0]);
		*port = atoi(tokens[1]);
	}
	sdsfree(s);
	sdsfreesplitres(tokens, count);
	freeReplyObject(r);
}

void SetUpdIPAndPortToOutside(unsigned int ip, unsigned short port) {

	//redis默认监听端口为6387 可以再配置文件中修改
	redisContext* c = uv_key_get(&redis);
	if (c == 0) {
		c = redisConnect(redis_ip, redis_port);
		uv_key_set(&redis, c);
	}
		
	if (c->err)
	{
		redisFree(c);
		uv_key_set(&redis, 0);
		n_error("Connect to redisServer faile");
		return;
	}

	SetUpdToAll(c, ip, port);

	const char* command = "SADD cluster:nodeoutside %d:%d";
	redisReply* r = (redisReply*)redisCommand(c, command, ip, port);

	if (NULL == r)
	{
		n_error("Execut command failure");
		redisFree(c);
		uv_key_set(&redis, 0);
		return;
	}

	if (r->type == REDIS_REPLY_INTEGER && r->integer != 1)
	{
		n_warn("Failed to execute command[%s]", command);
		freeReplyObject(r);
		return;
	}

	freeReplyObject(r);
}

void SetUpdIPAndPortToInside(unsigned int ip, unsigned short port) {

	//redis默认监听端口为6387 可以再配置文件中修改
	redisContext* c = uv_key_get(&redis);
	if (c == 0) {
		c = redisConnect(redis_ip, redis_port);
		uv_key_set(&redis, c);
	}
	if (c->err)
	{
		redisFree(c);
		uv_key_set(&redis, 0);
		n_error("Connect to redisServer faile");
		return;
	}

	SetUpdToAll(c, ip, port);

	const char* command = "SADD cluster:nodeinside %d:%d";
	redisReply* r = (redisReply*)redisCommand(c, command, ip, port);

	if (NULL == r)
	{
		n_error("Execut command failure");
		redisFree(c);
		uv_key_set(&redis, 0);
		return;
	}

	if (r->type == REDIS_REPLY_INTEGER && r->integer != 1)
	{
		n_warn("Failed to execute command[%s]", command);
		freeReplyObject(r);
		return;
	}

	freeReplyObject(r);
}

void GetUpdInside(unsigned int* ip, unsigned short* port) {

	//redis默认监听端口为6387 可以再配置文件中修改
	redisContext* c = uv_key_get(&redis);
	if (c == 0) {
		c = redisConnect(redis_ip, redis_port);
		uv_key_set(&redis, c);
	}
	if (c->err)
	{
		redisFree(c);
		uv_key_set(&redis, 0);
		n_error("Connect to redisServer faile");
		return;
	}
	const char* command = "SRANDMEMBER nodeinside 1";
	redisReply* r = (redisReply*)redisCommand(c, command);

	if (NULL == r)
	{
		n_error("Execut command failure");
		redisFree(c);
		uv_key_set(&redis, 0);
		return;
	}

	if (r->type != REDIS_REPLY_ARRAY || (r->type == REDIS_REPLY_ARRAY && r->elements == 0))
	{
		n_warn("Failed to execute command[%s]", command);
		freeReplyObject(r);
		//如果失败就从全局列表随机选一个
		GetUpdFromAll(ip, port);
		return;
	}

	sds s = sdsnew(r->element[0]->str);
	int count = 0;
	sds* tokens = sdssplitlen(s, sdslen(s), ":", 1, &count);
	if (count == 2) {
		*ip = atoi(tokens[0]);
		*port = atoi(tokens[1]);
	}
	sdsfree(s);
	sdsfreesplitres(tokens, count);
	freeReplyObject(r);
}

void GetUpdOutside(unsigned int* ip, unsigned short* port) {

	//redis默认监听端口为6387 可以再配置文件中修改
	redisContext* c = uv_key_get(&redis);
	if (c == 0) {
		c = redisConnect(redis_ip, redis_port);
		uv_key_set(&redis, c);
	}
	if (c->err)
	{
		redisFree(c);
		uv_key_set(&redis, 0);
		n_error("Connect to redisServer faile");
		return;
	}
	const char* command = "SRANDMEMBER cluster:nodeoutside 1";
	redisReply* r = (redisReply*)redisCommand(c, command);

	if (NULL == r)
	{
		n_error("Execut command failure");
		redisFree(c);
		uv_key_set(&redis, 0);
		return;
	}

	if (r->type != REDIS_REPLY_ARRAY || (r->type == REDIS_REPLY_ARRAY && r->elements == 0))
	{
		n_warn("Failed to execute command[%s]", command);
		freeReplyObject(r);
		//如果失败就从全局列表随机选一个
		GetUpdFromAll(ip, port);
		return;
	}

	sds s = sdsnew(r->element[0]->str);
	int count = 0;
	sds* tokens = sdssplitlen(s, sdslen(s), ":", 1, &count);
	if (count == 2) {
		*ip = atoi(tokens[0]);
		*port = atoi(tokens[1]);
	}
	sdsfree(s);
	sdsfreesplitres(tokens, count);
	freeReplyObject(r);
}

void SetEntityToDns(const char* name, unsigned long long id) {

	//redis默认监听端口为6387 可以再配置文件中修改
	redisContext* c = uv_key_get(&redis);
	if (c == 0) {
		c = redisConnect(redis_ip, redis_port);
		uv_key_set(&redis, c);
	}
	if (c->err)
	{
		redisFree(c);
		uv_key_set(&redis, 0);
		n_error("Connect to redisServer faile");
		return;
	}

	const char* command = "hset DNS %s %ull";
	redisReply* r = (redisReply*)redisCommand(c, command, name, id);

	if (NULL == r)
	{
		n_error("Execut command failure");
		redisFree(c);
		uv_key_set(&redis, 0);
		return;
	}

	if (r->type == REDIS_REPLY_INTEGER && r->integer != 1)
	{
		n_warn("Failed to execute command[%s]", command);
	}

	freeReplyObject(r);
}

void GetEntityFromDns(const char* name, unsigned long long* id) {

	//redis默认监听端口为6387 可以再配置文件中修改
	redisContext* c = uv_key_get(&redis);
	if (c == 0) {
		c = redisConnect(redis_ip, redis_port);
		uv_key_set(&redis, c);
	}
	if (c->err)
	{
		redisFree(c);
		uv_key_set(&redis, 0);
		n_error("Connect to redisServer faile");
		return;
	}
	const char* command = "hget DNS %s";
	redisReply* r = (redisReply*)redisCommand(c, command, name);

	if (NULL == r)
	{
		n_error("Execut command failure");
		redisFree(c);
		uv_key_set(&redis, 0);
		return;
	}

	if (r->type == REDIS_REPLY_STRING) {
		*id = strtoull(r->str, NULL, 0);
	} else if (r->type == REDIS_REPLY_INTEGER) {
		*id = r->integer;
	} else
	{
		n_warn("Failed to execute command[%s]", command);
	}

	freeReplyObject(r);
}