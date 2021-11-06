/* elog.c - Log related, the annoying output format to the callback function
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

#include "uv.h"
#include "plateform.h"
#include "elog.h"
#include "sds.h"
#include "locks.h"
#include "timesys.h"
#include "adlist.h"
#include "filesys.h"
#include "conio.h"
#include "equeue.h"
#include "dict.h"
#include "dicthelp.h"
#include "cJSON.h"
#include "sdshelp.h"

#define FILEFORMDEFAULT "@dir/@ctg_@data.log"

enum logform {
	logform_simple = 0,
	logform_full,
	logform_dev
};

typedef struct _LogPacket
{
	enum PacketCmd cmd;
	int level;
	char isNet;//是否由网络转发而来，如果是网络转发而来的将不会再次转发
	char category;
	char* describe;
	sds fileName;
	size_t line;
}*PLogPacket, LogPacket;

typedef struct _LogRule
{
	char maxlevel;//后端过滤输出的最高等级
	char minlevel;//后端过滤输出的最低等级
	sds fileName;//后端匹配的文件名0为全部文件
	size_t line;//后端匹配行数0为全部行数
	char isBreak;//规则是放在前面还是后面，一旦被处理过的消息就不会在后面的规则处理。
	unsigned int process;//处理方式目前有文件，屏幕，转发。
	unsigned int form;//日志输出的格式。
}*PLogRule, LogRule;

typedef struct _LogFileHandle
{
	char _setMaxlevel;//前端过滤输出的最高等级
	char _setMinlevel;//前端过滤输出的最低等级
	sds fileName;//前端过滤指定文件名
	size_t	fileLine;//前端过滤指定行
	char	category;//前端过滤分类
	void* eQueue;
	uv_thread_t thread;
	dict* fileList;//缓存文件句柄
	dict* category_rule;//根据分类的规则器
	sds level_error;
	sds level_warn;
	sds level_stat;
	sds level_function;
	sds level_details;

	sds sctg_node;
	sds sctg_dock;
	sds sctg_script;
	
	int maxQueueSize;
	sds	 outDir;//输出目录
	unsigned long long setFileSec;//设定文件日志的间隔时间
	PLogRule rule_default;//当所有规则都无效时默认的规则从配置文件读取
	sds form;//日志文件输出格式
}*PLogFileHandle, LogFileHandle;

typedef struct _LogFile
{
	FILE* fileHandle;
	unsigned long long fileSec;
}*PLogFile, LogFile;


static void fileDestructor(void* privdata, void* obj) {
	DICT_NOTUSED(privdata);
	PLogFile pLogFile = (PLogFile)obj;
	fclose(pLogFile->fileHandle);
	free(pLogFile);
}

static dictType SdsFileDictType = {
	sdsHashCallback,
	NULL,
	NULL,
	sdsCompareCallback,
	NULL,
	fileDestructor
};

static void ruleDestructor(void* privdata, void* obj) {
	DICT_NOTUSED(privdata);

	list* plist = obj;
	//free rule
	listIter* iter = listGetIterator(plist, AL_START_HEAD);
	listNode* node;
	while ((node = listNext(iter)) != NULL) {
		PLogRule pLogRule = listNodeValue(node);
		sdsfree(pLogRule->fileName);
		sdsfree((sds)pLogRule);
	}
	listReleaseIterator(iter);
	listRelease(plist);
}

static dictType SdsRuleDictType = {
	sdsHashCallback,
	NULL,
	NULL,
	sdsCompareCallback,
	sdsDestructor,
	ruleDestructor
};

static PLogFileHandle _pLogFileHandle = NULL;//全局的日志句柄

void* LogGetInstance() {
	return _pLogFileHandle;
}

char* GetLevelName(char level) {
	if (level == log_error) {
		return _pLogFileHandle->level_error;
	} else if (level == log_warn) {
		return _pLogFileHandle->level_warn;
	} else if (level == log_stat) {
		return _pLogFileHandle->level_stat;
	} else if (level == log_fun) {
		return _pLogFileHandle->level_function;
	} else if (level == log_details) {
		return _pLogFileHandle->level_details;
	}
	return 0;
}

char* GetCategoryName(char Category) {
	if (Category == ctg_node) {
		return _pLogFileHandle->sctg_node;
	}
	else if (Category == ctg_dock) {
		return _pLogFileHandle->sctg_dock;
	}
	else if (Category == ctg_script) {
		return _pLogFileHandle->sctg_script;
	}
	return 0;
}

int double2str(double value, int decimals, char* str, size_t len)
{
	int l;
	if (decimals == 0)
		l = sprintf(str, "%g", value);
	else {
		char format[10];
		sprintf(format, "%%.%dlf", decimals);
		l = snprintf(str, len, format, value);
	}

	return l;
}
#define SDS_LLSTR_SIZE 21
char* LogFormatDescribe(char const *fmt, ...) {
	sds s = sdsempty();
	size_t initlen = sdslen(s);
	const char *f = fmt;
	size_t i;
	va_list ap;

	va_start(ap, fmt);
	f = fmt;    /* Next format specifier byte to process. */
	i = initlen; /* Position of the next byte to write to dest str. */
	while (*f) {
		char next, *str;
		size_t l;
		long long num;
		unsigned long long unum;
		double dnum;

		/* Make sure there is always space for at least 1 char. */
		if (sdsavail(s) == 0) {
			s = sdsMakeRoomFor(s, 1);
		}

		switch (*f) {
		case '%':
			next = *(f + 1);
			f++;
			switch (next) {
			case 's':
			case 'S':
				str = va_arg(ap, char*);
				l = (next == 's') ? strlen(str) : sdslen(str);
				if (sdsavail(s) < l) {
					s = sdsMakeRoomFor(s, l);
				}
				memcpy(s + i, str, l);
				sdsIncrLen(s, (int)l);
				i += l;
				break;
			case 'i':
			case 'I':
				if (next == 'i')
					num = va_arg(ap, int);
				else
					num = va_arg(ap, long long);
				{
					char buf[SDS_LLSTR_SIZE];
					l = sdsll2str(buf, num);
					if (sdsavail(s) < l) {
						s = sdsMakeRoomFor(s, l);
					}
					memcpy(s + i, buf, l);
					sdsIncrLen(s, (int)l);
					i += l;
				}
				break;
			case 'f':
			case 'F':
				dnum = va_arg(ap, double);
				{
					char buf[SDS_LLSTR_SIZE];
					l = double2str(dnum, 0, buf, SDS_LLSTR_SIZE);
					if (sdsavail(s) < l) {
						s = sdsMakeRoomFor(s, l);
					}
					memcpy(s + i, buf, l);
					sdsIncrLen(s, (int)l);
					i += l;
				}
				break;
			case 'u':
			case 'U':
				if (next == 'u')
					unum = va_arg(ap, unsigned int);
				else
					unum = va_arg(ap, unsigned long long);
				{
					char buf[SDS_LLSTR_SIZE];
					l = sdsull2str(buf, unum);
					if (sdsavail(s) < l) {
						s = sdsMakeRoomFor(s, l);
					}
					memcpy(s + i, buf, l);
					sdsIncrLen(s, (int)l);
					i += l;
				}
				break;
			default: /* Handle %% and generally %<unknown>. */
				s[i++] = next;
				sdsIncrLen(s, 1);
				break;
			}
			break;
		default:
			s[i++] = *f;
			sdsIncrLen(s, 1);
			break;
		}
		f++;
	}
	va_end(ap);

	/* Add null-term */
	s[i] = '\0';
	return s;
}

void LogFreeForm(void* s) {
	sdsfree(s);
}

void LogSetMaxLevel(PLogFileHandle pLogFileHandle, char level) {
	pLogFileHandle->_setMaxlevel = level;
}

short LogGetMaxLevel() {
	return _pLogFileHandle->_setMaxlevel;
}

void LogSetMinLevel(PLogFileHandle pLogFileHandle, char level) {
	pLogFileHandle->_setMinlevel = level;
}

short LogGetMinLevel() {
	return _pLogFileHandle->_setMinlevel;
}

char* LogGetFileName() {
	return _pLogFileHandle->fileName;
}

void LogSetFileName(PLogFileHandle pLogFileHandle, char* fileName) {

	if (pLogFileHandle->fileName != NULL) {
		sdsfree(pLogFileHandle->fileName);
	}
	pLogFileHandle->fileName = sdsnew(fileName);
	
}

size_t LogGetFileLine() {
	return _pLogFileHandle->fileLine;
}

void LogSetLine(PLogFileHandle pLogFileHandle, size_t fileLine) {
	pLogFileHandle->fileLine = fileLine;
}

void LogSetOutDir(PLogFileHandle pLogFileHandle, char* outDir) {
	pLogFileHandle->outDir = outDir;
}

static void LogErrFunPrintf(char level, char category, const char* describe, const char* fileName, size_t line, unsigned int form) {

	if (level == log_error) {
		Color(73);
	}

	sds time = GetTimForm();
	if (form == logform_simple) {
		printf("%s %s\n", time, describe);
	}
	else if (form == logform_full) {
		printf("%s %s %s %s\n", time, GetCategoryName(category), GetLevelName(level), describe);
	}
	else if (form == logform_dev) {
		printf("%s %s %s (%s-%d) %s\n", time, GetCategoryName(category), GetLevelName(level), fileName, (int)line, describe);
	}
	
	sdsfree(time);
	if (level == log_error) {
		ClearColor();
	}
}

static void CreateLogFile(PLogFile pLogFile, char category) {

	//文件按类别分类，每种类别的输出格式和输出目录，输出目的，可以配置
	
	sds fielPath;
	sds d = GetDayForm();
	if (_pLogFileHandle->form == NULL) {
		fielPath = sdsnew(FILEFORMDEFAULT);
	}
	else {
		fielPath = _pLogFileHandle->form;
	}

	//替换分类
	sds newFielPath = sdscatsrlen(sdsempty(), fielPath, sdslen(fielPath), "@ctg", 4, GetCategoryName(category), sdslen(GetCategoryName(category)));
	sdsfree(fielPath);
	fielPath = newFielPath;

	//替换日期
	newFielPath = sdscatsrlen(sdsempty(), fielPath, sdslen(fielPath), "@data", 5, d, sdslen(d));
	sdsfree(fielPath);
	fielPath = newFielPath;

	//替换输出目录
	newFielPath = sdscatsrlen(sdsempty(), fielPath, sdslen(fielPath), "@dir", 4, _pLogFileHandle->outDir, sdslen(_pLogFileHandle->outDir));
	sdsfree(fielPath);
	fielPath = newFielPath;
	
	MkDirs(_pLogFileHandle->outDir);
	pLogFile->fileHandle = fopen_t(fielPath, "ab");
	pLogFile->fileSec = GetCurrentSec();
	sdsfree(d);
	sdsfree(fielPath);
}


static void LogErrFunFile(char level, char category, const char* describe, const char* fileName, size_t line, unsigned int form) {

	PLogFile pLogFile;
	dictEntry* fileEntry = dictFind(_pLogFileHandle->fileList, GetCategoryName(category));
	if (fileEntry != 0) {
		pLogFile = dictGetVal(fileEntry);
	}
	else {
		pLogFile = calloc(1, sizeof(LogFile));
		dictAdd(_pLogFileHandle->fileList, GetCategoryName(category), pLogFile);
		CreateLogFile(pLogFile, category);
	}

	
	unsigned long long csec = GetCurrentSec();
	if (csec - pLogFile->fileSec > _pLogFileHandle->setFileSec) {
		CreateLogFile(pLogFile, category);
	}

	sds time = GetTimForm();
	fseek_t(pLogFile->fileHandle, 0, SEEK_END);

	if (form == logform_simple) {
		fprintf(pLogFile->fileHandle, "%s %s\n", time, describe);
	}
	else if (form == logform_full) {
		fprintf(pLogFile->fileHandle, "%s %s %s\n", time, GetLevelName(level), describe);
	}
	else if (form == logform_dev) {
		fprintf(pLogFile->fileHandle, "%s %s (%s-%d) %s\n", time, GetLevelName(level), fileName, (int)line, describe);
	}

	fflush(pLogFile->fileHandle);
	sdsfree(time);
}

void LogTo(char level, char category, char* describe, const char* fileName, size_t line) {

	PLogPacket pLogPacket = calloc(1, sizeof(LogPacket));
	pLogPacket->cmd = cmd_null;
	pLogPacket->level = level;
	pLogPacket->category = category;
	pLogPacket->line = line;
	pLogPacket->fileName = sdsnew(fileName);
	pLogPacket->describe = describe;

	EqPush(_pLogFileHandle->eQueue, pLogPacket);
}

static void PacketFree(void* ptr) {

	PLogPacket pLogPacket = (PLogPacket)ptr;
	sdsfree(pLogPacket->describe);
	sdsfree(pLogPacket->fileName);
	free(pLogPacket);
}

void LogRun(void* pVoid) {
	PLogFileHandle pLogFileHandle = pVoid;
	do {

		EqWait(pLogFileHandle->eQueue);
		size_t eventQueueLength = 0;
		do {
			//如果长度超过限制要释放掉队列	
			PLogPacket pLogPacket = (PLogPacket)EqPopWithLen(pLogFileHandle->eQueue, &eventQueueLength);

			if (pLogPacket != 0) {
				//退出线程
				if (pLogPacket->cmd == cmd_cancel) {
					return;
				}
				else if (pLogPacket->cmd == cmd_rule) {
					dictEntry* pdictEntry = dictFind(pLogFileHandle->category_rule, GetLevelName(pLogPacket->category));
					list* plist;
					if (pdictEntry == 0) {
						plist = listCreate();
						dictAdd(pLogFileHandle->category_rule, GetLevelName(pLogPacket->category), plist);
					}
					else {
						plist = dictGetVal(pdictEntry);
					}
				
					PLogRule pLogRule = (PLogRule)pLogPacket->describe;
					if (pLogRule->isBreak) {
						listAddNodeHead(plist, pLogRule);
					}
					else {
						listAddNodeTail(plist, pLogRule);
					}
					continue;
				}

				dictEntry* pdictEntry = dictFind(pLogFileHandle->category_rule, GetLevelName(pLogPacket->category));
				if (pdictEntry != 0)
				{
					listIter* iter = listGetIterator(dictGetVal(pdictEntry) , AL_START_HEAD);
					listNode* node;
					while ((node = listNext(iter)) != NULL) {
						PLogRule pLogRule = listNodeValue(node);

						if (pLogPacket->level < pLogRule->minlevel || pLogPacket->level > pLogRule->maxlevel) continue;
						else if (pLogRule->fileName != 0 && sdscmp(pLogRule->fileName, pLogPacket->fileName) != 0) continue;
						else if (pLogRule->line != 0 && pLogRule->line != pLogPacket->line) continue;
						else {
							if (pLogRule->process != prc_null)
							{
								if (pLogRule->process & prc_print || pLogPacket->level == log_error) {
									LogErrFunPrintf(pLogPacket->level, pLogPacket->category, pLogPacket->describe, pLogPacket->fileName, pLogPacket->line, pLogRule->form);
								}
							
								if (pLogRule->process & prc_file) {
									LogErrFunFile(pLogPacket->level, pLogPacket->category, pLogPacket->describe, pLogPacket->fileName, pLogPacket->line, pLogRule->form);
								}
							
								if (pLogRule->process & prc_net_file) {

									if (pLogPacket->isNet) {
										//是网络传输的
										LogErrFunFile(pLogPacket->level, pLogPacket->category, pLogPacket->describe, pLogPacket->fileName, pLogPacket->line, pLogRule->form);
										break;
									}
									else {
										//发送到网络目的地
									}
								}
								break;
							}
						
						}
					}
					listReleaseIterator(iter);
					PacketFree(pLogPacket);
				}
				else if(pLogFileHandle->rule_default){

					if (pLogPacket->level < pLogFileHandle->rule_default->minlevel || pLogPacket->level > pLogFileHandle->rule_default->maxlevel) continue;
					else if (pLogFileHandle->rule_default->fileName != 0 && sdscmp(pLogFileHandle->rule_default->fileName, pLogPacket->fileName) != 0) continue;
					else if (pLogFileHandle->rule_default->line != 0 && pLogFileHandle->rule_default->line != pLogPacket->line) continue;
					else {
						if (pLogFileHandle->rule_default->process != prc_null) {

					
							if (pLogFileHandle->rule_default->process & prc_print || pLogPacket->level == log_error) {
								LogErrFunPrintf(pLogPacket->level, pLogPacket->category, pLogPacket->describe, pLogPacket->fileName, pLogPacket->line, pLogFileHandle->rule_default->form);
							}
						
							if (pLogFileHandle->rule_default->process & prc_file) {
								LogErrFunFile(pLogPacket->level, pLogPacket->category, pLogPacket->describe, pLogPacket->fileName, pLogPacket->line, pLogFileHandle->rule_default->form);
							}
						
							if (pLogFileHandle->rule_default->process & prc_net_file) {

								if (pLogPacket->isNet) {
									//是网络传输的
									LogErrFunFile(pLogPacket->level, pLogPacket->category, pLogPacket->describe, pLogPacket->fileName, pLogPacket->line, pLogFileHandle->rule_default->form);
								}
								else {
									//发送到网络目的地
								}
							}
						}
					}
					PacketFree(pLogPacket);
				}

				if (eventQueueLength > pLogFileHandle->maxQueueSize) {
					EqEmpty(pLogFileHandle->eQueue, PacketFree);
				}
			}
		} while (eventQueueLength);
	} while (1);
	return;
}

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

	f = fopen_t(config, "rb");
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


	cJSON* logJson = cJSON_GetObjectItem(json, "log_config");
	if (logJson) {

		cJSON* item = logJson->child;
		while ((item != NULL) && (item->string != NULL))
		{
			if (strcmp(item->string, "maxQueueSize") == 0) {
				_pLogFileHandle->maxQueueSize = (int)cJSON_GetNumberValue(item);
			}
			else if (strcmp(item->string, "outDir") == 0) {
				_pLogFileHandle->outDir = sdsnew(cJSON_GetStringValue(item));
			}
			else if (strcmp(item->string, "setFileSec") == 0) {
				_pLogFileHandle->setFileSec = (unsigned long long)cJSON_GetNumberValue(item);
			}
			else if (strcmp(item->string, "form") == 0) {
				_pLogFileHandle->form = sdsnew(cJSON_GetStringValue(item));
			}
			else if (strcmp(item->string, "ruleDefault") == 0) {

				_pLogFileHandle->rule_default = malloc(sizeof(LogRule));
				_pLogFileHandle->rule_default->maxlevel = log_all;
				_pLogFileHandle->rule_default->minlevel = log_null;
				_pLogFileHandle->rule_default->fileName = 0;
				_pLogFileHandle->rule_default->line = 0;
				_pLogFileHandle->rule_default->isBreak = 0;
				_pLogFileHandle->rule_default->process = prc_file;
				_pLogFileHandle->rule_default->form = logform_full;

				cJSON* runleItem = logJson->child;
				while ((runleItem != NULL) && (runleItem->string != NULL))
				{
					if (strcmp(runleItem->string, "maxlevel") == 0) {
						int outPutLevel = (int)cJSON_GetNumberValue(runleItem);
						if (log_null <= outPutLevel <= log_all) {
							_pLogFileHandle->rule_default->maxlevel = outPutLevel;
						}
					}
					else if (strcmp(runleItem->string, "minlevel") == 0) {
						int outPutLevel = (int)cJSON_GetNumberValue(runleItem);
						if (log_null <= outPutLevel <= log_all) {
							_pLogFileHandle->rule_default->maxlevel = outPutLevel;
						}
					}
					else if (strcmp(runleItem->string, "fileName") == 0) {
						_pLogFileHandle->rule_default->fileName = sdsnew(cJSON_GetStringValue(runleItem));
					}
					else if (strcmp(runleItem->string, "line") == 0) {
						_pLogFileHandle->rule_default->line = (size_t)cJSON_GetNumberValue(runleItem);
					}
					else if (strcmp(runleItem->string, "isBreak") == 0) {
						_pLogFileHandle->rule_default->isBreak = (char)cJSON_GetNumberValue(runleItem);
					}
					else if (strcmp(runleItem->string, "process") == 0) {
						_pLogFileHandle->rule_default->process = (unsigned int)cJSON_GetNumberValue(runleItem);
					}
					else if (strcmp(runleItem->string, "form") == 0) {
						_pLogFileHandle->rule_default->form = (unsigned int) cJSON_GetNumberValue(runleItem);
					}
					runleItem = runleItem->next;
				}
			}

			item = item->next;
		}
	}

	cJSON_Delete(json);
	free(data);
}

void LogInit(char* config) {
	if (_pLogFileHandle != NULL)
		return;

	_pLogFileHandle = calloc(1, sizeof(LogFileHandle));
	_pLogFileHandle->level_error = sdsnew("error");
	_pLogFileHandle->level_warn = sdsnew("warn");
	_pLogFileHandle->level_stat = sdsnew("stat");
	_pLogFileHandle->level_function = sdsnew("function");
	_pLogFileHandle->level_details = sdsnew("details");

	_pLogFileHandle->sctg_node = sdsnew("node");
	_pLogFileHandle->sctg_dock = sdsnew("dock");
	_pLogFileHandle->sctg_script = sdsnew("script");

	_pLogFileHandle->eQueue = EqCreate();
	_pLogFileHandle->category_rule = dictCreate(&SdsRuleDictType, NULL);
	_pLogFileHandle->fileList = dictCreate(&SdsFileDictType, NULL);


	_pLogFileHandle->maxQueueSize = 2000;
	_pLogFileHandle->outDir = sdsnew("./logs");
	_pLogFileHandle->setFileSec = 60 * 60 * 24;
	_pLogFileHandle->_setMaxlevel = log_all;
	_pLogFileHandle->_setMinlevel = log_null;
	_pLogFileHandle->rule_default = NULL;

	doJsonParseFile(config);

	uv_thread_create(&_pLogFileHandle->thread, LogRun, _pLogFileHandle);
}

void LogCancel() {
	PLogPacket pLogPacket = malloc(sizeof(LogPacket));
	pLogPacket->cmd = cmd_cancel;

	EqPush(_pLogFileHandle->eQueue, pLogPacket);
	uv_thread_join(&_pLogFileHandle->thread);
}

void LogDestroy() {

	LogCancel();
	EqDestory(_pLogFileHandle->eQueue, PacketFree);

	dictRelease(_pLogFileHandle->category_rule);
	dictRelease(_pLogFileHandle->fileList);
	
	sdsfree(_pLogFileHandle->sctg_node);
	sdsfree(_pLogFileHandle->sctg_dock);
	sdsfree(_pLogFileHandle->sctg_script);

	sdsfree(_pLogFileHandle->level_error);
	sdsfree(_pLogFileHandle->level_warn);
	sdsfree(_pLogFileHandle->level_stat);
	sdsfree(_pLogFileHandle->level_function);
	sdsfree(_pLogFileHandle->level_details);

	if (_pLogFileHandle->rule_default) {
		sdsfree(_pLogFileHandle->rule_default->fileName);
		free(_pLogFileHandle->rule_default);
	}

	free(_pLogFileHandle);
}

void LogRuleTo(char category, char maxlevel, char minlevel, char* fileName, size_t line, char isBreak, unsigned int process, char* form) {
	PLogPacket pLogPacket = calloc(1, sizeof(LogPacket));
	pLogPacket->cmd = cmd_rule;
	pLogPacket->category = category;

	PLogRule pLogRule = calloc(1, sizeof(LogRule));
	pLogRule->maxlevel = maxlevel;
	pLogRule->minlevel = minlevel;
	pLogRule->fileName = sdsnew(fileName);
	pLogRule->line = line;
	pLogRule->isBreak = isBreak;
	pLogRule->process = process;
	pLogRule->form = logform_full;
	pLogPacket->describe = sdsnewlen(pLogRule, sizeof(LogRule));
	
	EqPush(_pLogFileHandle->eQueue, pLogPacket);
}
