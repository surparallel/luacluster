/* args.c - about args function
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
#include "sds.h"
#include "args.h"

#ifndef _WIN32
#include <readline/readline.h>
#include <readline/history.h>
#define plg_readline(b,p)	(((b)=readline(p)) != NULL)
#define plg_saveline(b) \
	if (strlen(b) > 0) \
	  add_history(b);
#define plg_freeline(b)	(free(b))
#else
#define plg_readline(b,p)	\
	(fputs(p, stdout), fflush(stdout),\
	fgets(b, 1024, stdin) != NULL)
#define plg_saveline(b)	{ (void)b; }
#define plg_freeline(b)	{ (void)b; }
#endif

static sds ReadArgFromStdin(void) {
	char buf[1024];
	char* p = buf;
	sds arg = sdsempty();
	size_t len;

	if (plg_readline(p, ">") == 0)
		return 0;

	len = strlen(p);
	if (p[len - 1] == '\n') {
		p[len - 1] = '\0';
		len -= 1;
	}

	arg = sdscatlen(arg, p, len);
	plg_freeline(p);
	return arg;
}

int ArgsInteractive(FUNIssueCommand pIssueCommand) {
	while (1) {
		sds ptr = ReadArgFromStdin();
		if (ptr == 0) {
			continue;
		}
		int vlen;
		sds* v = sdssplitlen(ptr, (int)sdslen(ptr), " ", 1, &vlen);
		sdsfree(ptr);
		int ret = pIssueCommand(vlen, v, 1);
		plg_saveline(v[0]);
		sdsfreesplitres(v, vlen);
		if (0 == ret) break;
	}
	return 1;
}

static int ArgsCheckArg(char* argv) {
	if (strcmp(argv, "--") == 0 || strcmp(argv, "-") == 0)
		return 0;
	else
		return 1;
}

