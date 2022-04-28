/* elog.h
*
* Copyright(C) 2019 - 2020, sun shuo <sun.shuo@surparallel.org>
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
#ifndef __ELOG_H
#define __ELOG_H

enum OutPutLevel
{
	log_null = 0,
	log_error,
	log_warn,
	log_stat,
	log_fun,
	log_details,
	log_all,
};

enum OutPutCategory
{
	ctg_null = 0,
	ctg_node,//整个节点管理器的日志
	ctg_dock,//每个线程容器的日志
	ctg_script,//脚本的日志
	ctg_user,//用户脚本的日志
};

enum PacketCmd
{
	cmd_null = 0,
	cmd_cancel,
	cmd_rule
};


#define prc_null 0//不做任何处理
#define	prc_print 0x2//整个节点管理器的日志
#define	prc_file 0x4//每个线程容器的日志
#define	prc_net_file 0x8//网络到本地文件


void LogTo(char level, char category, char* describe, const char* fileName, size_t line);
char* LogFormatDescribe(char const *fmt, ...);
void LogFreeForm(void* s);
short LogGetMaxLevel();
short LogGetMinLevel();

void LogInit();
void LogDestroy();

void* LogGetInstance();
char* LogGetFileName();
size_t LogGetFileLine();

#define __FILENAME__ (strrchr(__FILE__, '\\') ? (strrchr(__FILE__, '\\') + 1):__FILE__)

#define elog(level, category, describe, ...) do {\
	if (LogGetInstance() == NULL) break;\
	if (level >= LogGetMinLevel() && level <= LogGetMaxLevel()) {\
	char* fileName = __FILENAME__;\
	size_t fileLine = __LINE__;\
	if (LogGetFileName() != NULL && strstr(LogGetFileName(), fileName) == NULL) break;\
	if (LogGetFileLine() != 0 && LogGetFileLine() == fileLine) break;\
	char* sdsDescribe = LogFormatDescribe( describe , ##__VA_ARGS__ );\
	LogTo(level, category, sdsDescribe, fileName, fileLine);\
	}\
} while (0);


#define elog_error(category, describe, ...) elog(log_error, category, describe, ##__VA_ARGS__)
#define elog_warn(category, describe, ...) elog(log_warn, category, describe, ##__VA_ARGS__)
#define elog_stat(category, describe, ...) elog(log_stat, category, describe, ##__VA_ARGS__)
#define elog_fun(category, describe, ...) elog(log_fun, category, describe, ##__VA_ARGS__)
#define elog_details(category, describe, ...) elog(log_details, category, describe, ##__VA_ARGS__)

//null
#define e_error(describe, ...) elog(log_error, ctg_null, describe, ##__VA_ARGS__)
#define e_warn(describe, ...) elog(log_warn, ctg_null, describe, ##__VA_ARGS__)
#define e_stat(describe, ...) elog(log_stat, ctg_null, describe, ##__VA_ARGS__)
#define e_fun(describe, ...) elog(log_fun, ctg_null, describe, ##__VA_ARGS__)
#define e_details(describe, ...) elog(log_details, ctg_null, describe, ##__VA_ARGS__)

//ctg_node
#define n_error(describe, ...) elog(log_error, ctg_node, describe, ##__VA_ARGS__)
#define n_warn(describe, ...) elog(log_warn, ctg_node, describe, ##__VA_ARGS__)
#define n_stat(describe, ...) elog(log_stat, ctg_node, describe, ##__VA_ARGS__)
#define n_fun(describe, ...) elog(log_fun, ctg_node, describe, ##__VA_ARGS__)
#define n_details(describe, ...) elog(log_details, ctg_node, describe, ##__VA_ARGS__)

//ctg_script
#define s_error(describe, ...) elog(log_error, ctg_script, describe, ##__VA_ARGS__)
#define s_warn(describe, ...) elog(log_warn, ctg_script, describe, ##__VA_ARGS__)
#define s_stat(describe, ...) elog(log_stat, ctg_script, describe, ##__VA_ARGS__)
#define s_fun(describe, ...) elog(log_fun, ctg_script, describe, ##__VA_ARGS__)
#define s_details(describe, ...) elog(log_details, ctg_script, describe, ##__VA_ARGS__)

//ctg_user
#define u_error(describe, ...) elog(log_error, ctg_user, describe, ##__VA_ARGS__)
#define u_warn(describe, ...) elog(log_warn, ctg_user, describe, ##__VA_ARGS__)
#define u_stat(describe, ...) elog(log_stat, ctg_user, describe, ##__VA_ARGS__)
#define u_fun(describe, ...) elog(log_fun, ctg_user, describe, ##__VA_ARGS__)
#define u_details(describe, ...) elog(log_details, ctg_user, describe, ##__VA_ARGS__)
#endif


