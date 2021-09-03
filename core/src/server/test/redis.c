
#include "plateform.h"
#include "hiredis.h"

void doRedisTest()
{
	//redis默认监听端口为6387 可以再配置文件中修改
	redisContext* c = redisConnect("127.0.0.1", 6379);
	if (c->err)
	{
		redisFree(c);
		printf("Connect to redisServer faile\n");
		return;
	}
	printf("Connect to redisServer Success\n");

	const char* command1 = "set stest1 value1";
	redisReply* r = (redisReply*)redisCommand(c, command1);

	if (NULL == r)
	{
		printf("Execut command1 failure\n");
		redisFree(c);
		return;
	}
	if (!(r->type == REDIS_REPLY_STATUS && strcasecmp(r->str, "OK") == 0))
	{
		printf("Failed to execute command[%s]\n", command1);
		freeReplyObject(r);
		redisFree(c);
		return;
	}
	freeReplyObject(r);
	printf("Succeed to execute command[%s]\n", command1);

	const char* command2 = "strlen stest1";
	r = (redisReply*)redisCommand(c, command2);
	if (r->type != REDIS_REPLY_INTEGER)
	{
		printf("Failed to execute command[%s]\n", command2);
		freeReplyObject(r);
		redisFree(c);
		return;
	}
	int length = r->integer;
	freeReplyObject(r);
	printf("The length of 'stest1' is %d.\n", length);
	printf("Succeed to execute command[%s]\n", command2);


	const char* command3 = "get stest1";
	r = (redisReply*)redisCommand(c, command3);
	if (r->type != REDIS_REPLY_STRING)
	{
		printf("Failed to execute command[%s]\n", command3);
		freeReplyObject(r);
		redisFree(c);
		return;
	}
	printf("The value of 'stest1' is %s\n", r->str);
	freeReplyObject(r);
	printf("Succeed to execute command[%s]\n", command3);

	const char* command4 = "get stest2";
	r = (redisReply*)redisCommand(c, command4);
	if (r->type != REDIS_REPLY_NIL)
	{
		printf("Failed to execute command[%s]\n", command4);
		freeReplyObject(r);
		redisFree(c);
		return;
	}
	freeReplyObject(r);
	printf("Succeed to execute command[%s]\n", command4);


	redisFree(c);

}