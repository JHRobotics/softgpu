/******************************************************************************
 * Copyright (c) 2023 Jaroslav Hensl                                          *
 *                                                                            *
 * Permission is hereby granted, free of charge, to any person                *
 * obtaining a copy of this software and associated documentation             *
 * files (the "Software"), to deal in the Software without                    *
 * restriction, including without limitation the rights to use,               *
 * copy, modify, merge, publish, distribute, sublicense, and/or sell          *
 * copies of the Software, and to permit persons to whom the                  *
 * Software is furnished to do so, subject to the following                   *
 * conditions:                                                                *
 *                                                                            *
 * The above copyright notice and this permission notice shall be             *
 * included in all copies or substantial portions of the Software.            *
 *                                                                            *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,            *
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES            *
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND                   *
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT                *
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,               *
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING               *
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR              *
 * OTHER DEALINGS IN THE SOFTWARE.                                            *
 *                                                                            *
*******************************************************************************/
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "nocrt.h"

#include "setuperr.h"

#define ERROR_MSG_MAX 1024
#define NUM_LOGS 4
static int actual_log = 0;

static int log_cnt = 0;

static char error_log[4][1024] = {"", "", "", ""};

void report(const char *file, int line, const char *msg, ...)
{
	size_t len;
	va_list args;
  va_start(args, msg);

	sprintf(error_log[actual_log], "[%s:%d]", file, line);
	len = strlen(error_log[actual_log]);

	vsprintf(&(error_log[actual_log][len]), msg, args);	

	va_end(args);	
	
//#ifdef DEBUG
//	printf("%s\n", error_log[actual_log]);
//#endif
	
	actual_log = (actual_log + 1) % NUM_LOGS;
	
	log_cnt++;
}

const char *get_report(int level)
{
	if(level >= 4)
	{
		return NULL;
	}
	
	int p = (actual_log + NUM_LOGS - level - 1) % NUM_LOGS;
	const char *s = error_log[p];
	
	if(strlen(s))
	{
		return s;
	}
	return NULL;
}

