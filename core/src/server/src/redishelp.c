/*
 * Copyright (c) 2009-2012, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "plateform.h"
#include "hiredis.h"
#include "elog.h"
#include "sds.h"
#include "cJSON.h"
#include "filesys.h"

static sds redis_ip;
static unsigned redis_port;

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
}

void FreeRedisHelp() {
	sdsfree(redis_ip);
}

void SetUpdToAll(redisContext* c, unsigned int ip, unsigned short port) {

	const char* command = "SADD cluster:nodeall %d:%d";
	redisReply* r = (redisReply*)redisCommand(c, command, ip, port);

	if (NULL == r)
	{
		n_error("Execut command failure\n");
		return;
	}

	if (r->type == REDIS_REPLY_INTEGER && r->integer != 1)
	{
		n_warn("Failed to execute command[%s]\n", command);
		freeReplyObject(r);
		return;
	}

	freeReplyObject(r);
}

void GetUpdFromAll(unsigned int* ip, unsigned short* port) {
	//redis默认监听端口为6387 可以再配置文件中修改
	redisContext* c = redisConnect(redis_ip, redis_port);
	if (c->err)
	{
		redisFree(c);
		n_error("Connect to redisServer faile");
		return;
	}
	const char* command = "SRANDMEMBER cluster:nodeall 1";
	redisReply* r = (redisReply*)redisCommand(c, command);

	if (NULL == r)
	{
		n_error("Execut command failure\n");
		redisFree(c);
		return;
	}

	if (r->type != REDIS_REPLY_ARRAY && r->elements == 0)
	{
		n_error("Failed to execute command[%s]", command);
		freeReplyObject(r);
		redisFree(c);
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
	redisFree(c);
}

void SetUpdIPAndPortToOutside(unsigned int ip, unsigned short port) {

	//redis默认监听端口为6387 可以再配置文件中修改
	redisContext* c = redisConnect(redis_ip, redis_port);
	if (c->err)
	{
		redisFree(c);
		n_error("Connect to redisServer faile\n");
		return;
	}

	SetUpdToAll(c, ip, port);

	const char* command = "SADD cluster:nodeoutside %d:%d";
	redisReply* r = (redisReply*)redisCommand(c, command, ip, port);

	if (NULL == r)
	{
		n_error("Execut command failure\n");
		redisFree(c);
		return;
	}

	if (r->type == REDIS_REPLY_INTEGER && r->integer != 1)
	{
		n_warn("Failed to execute command[%s]\n", command);
		freeReplyObject(r);
		redisFree(c);
		return;
	}

	freeReplyObject(r);
	redisFree(c);
}

void SetUpdIPAndPortToInside(unsigned int ip, unsigned short port) {

	//redis默认监听端口为6387 可以再配置文件中修改
	redisContext* c = redisConnect(redis_ip, redis_port);
	if (c->err)
	{
		redisFree(c);
		n_error("Connect to redisServer faile");
		return;
	}

	SetUpdToAll(c, ip, port);

	const char* command = "SADD cluster:nodeinside %d:%d";
	redisReply* r = (redisReply*)redisCommand(c, command, ip, port);

	if (NULL == r)
	{
		n_error("Execut command failure\n");
		redisFree(c);
		return;
	}

	if (r->type == REDIS_REPLY_INTEGER && r->integer != 1)
	{
		n_warn("Failed to execute command[%s]", command);
		freeReplyObject(r);
		redisFree(c);
		return;
	}

	freeReplyObject(r);
	redisFree(c);
}

void GetUpdInside(unsigned int* ip, unsigned short* port) {
	//redis默认监听端口为6387 可以再配置文件中修改
	redisContext* c = redisConnect(redis_ip, redis_port);
	if (c->err)
	{
		redisFree(c);
		n_error("Connect to redisServer faile");
		return;
	}
	const char* command = "SRANDMEMBER nodeinside 1";
	redisReply* r = (redisReply*)redisCommand(c, command);

	if (NULL == r)
	{
		n_error("Execut command failure\n");
		redisFree(c);
		return;
	}

	if (r->type != REDIS_REPLY_ARRAY || (r->type == REDIS_REPLY_ARRAY && r->elements == 0))
	{
		n_warn("Failed to execute command[%s]", command);
		freeReplyObject(r);
		redisFree(c);

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
	redisFree(c);
}

void GetUpdOutside(unsigned int* ip, unsigned short* port) {
	//redis默认监听端口为6387 可以再配置文件中修改
	redisContext* c = redisConnect(redis_ip, redis_port);
	if (c->err)
	{
		redisFree(c);
		n_error("Connect to redisServer faile");
		return;
	}
	const char* command = "SRANDMEMBER cluster:nodeoutside 1";
	redisReply* r = (redisReply*)redisCommand(c, command);

	if (NULL == r)
	{
		n_error("Execut command failure\n");
		redisFree(c);
		return;
	}

	if (r->type != REDIS_REPLY_ARRAY || (r->type == REDIS_REPLY_ARRAY && r->elements == 0))
	{
		n_warn("Failed to execute command[%s]", command);
		freeReplyObject(r);
		redisFree(c);

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
	redisFree(c);
}