#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include "winini.h"
#include "setuperr.h"
#include "filecopy.h"
#include "nocrt.h"

#define MAX_LINE 1024

typedef struct iniline
{
	struct iniline *next;
	BOOL issection;
	char data[1];
} iniline_t;

static char linebuf[MAX_LINE];
static iniline_t *ini = NULL;
static iniline_t *ini_last = NULL;

static void lineadd(size_t pos)
{
	while(pos > 0)
	{
		switch(linebuf[pos])
		{
			case ' ':
			case '\t':
			case '\r':
			case '\n':
			case '\0':
				pos--;
				continue;
		}
		break;
	}
	
	if(pos > 0)
	{
		iniline_t *ln = HeapAlloc(GetProcessHeap(), 0, sizeof(iniline_t) + pos);
		if(ln)
		{
			ln->next = NULL;
			
			if(ini_last == NULL)
			{
				ini = ln;
				ini_last = ln;
			}
			else
			{
				ini_last->next = ln;
				ini_last = ln;
			}
			
			memcpy(ln->data, linebuf, pos+1);
			ln->data[pos+1] = '\0';
			
			ln->issection = ln->data[0] == '[';
			
			
		}
	}
}

BOOL iniLoad(const char *filename)
{
	size_t pos = 0;
	char c;
	DWORD readed;
	
	HANDLE file = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	
	if(file == INVALID_HANDLE_VALUE)
	{
		return FALSE;
	}
	
	do
	{
		readed = 0;
		if(ReadFile(file, &c, 1, &readed, NULL))
		{
			if(readed == 1)
			{
				if(pos == 0)
				{
					switch(c)
					{
						case '\n':
						case '\r':
						case ' ':
						case '\t':
							continue; /* garbage on line begin */	
					}
				}
				
				if(c == '\n')
				{
					linebuf[pos] = '\0';
					lineadd(pos);
					pos = 0;
				}
				else
				{
					if(pos < MAX_LINE)
					{
						linebuf[pos++] = c;
					}
				}
					
				continue;
			} // readed 1
		}
		
		if(pos > 0)
		{
			linebuf[pos] = '\0';
			lineadd(pos);
		}
	} while(readed > 0);
	
	return (ini != NULL);
}

void iniFree()
{
	while(ini != NULL)
	{
		iniline_t *ptr = ini;
		ini = ini->next;
		HeapFree(GetProcessHeap(), 0, ptr);
	}
}

const char *iniValue(const char *section, const char *variable)
{
	iniline_t *ptr = ini;
	
	while(ptr != NULL)
	{
		if(strcmp(section, ptr->data) == 0)
		{
			break;
		}
		
		ptr = ptr->next;
	}
	
	size_t varlen = strlen(variable);
	
	if(ptr)
	{
		ptr = ptr->next;
		
		while(ptr != NULL)
		{
			if(ptr->issection)
			{
				break;
			}
			
			if(strncmp(ptr->data, variable, varlen) == 0)
			{
				char *sptr = ptr->data + varlen;
				while(*sptr != '\0')
				{
					switch(*sptr)
					{
						case ' ':
						case '\t':
						case '=':
							break;
						default:
							return sptr;
					}
					sptr++;
				}
			}
			
			ptr = ptr->next;
		}
	}
	
	return NULL;
}

const char *iniLine(const char *section, int line)
{
	iniline_t *ptr = ini;
	
	while(ptr != NULL)
	{
		if(strcmp(section, ptr->data) == 0)
		{
			break;
		}
		
		ptr = ptr->next;
	}
	
	if(ptr)
	{
		ptr = ptr->next;
		
		while(ptr != NULL)
		{
			if(ptr->issection)
			{
				break;
			}
			
			if(line == 0)
			{
				return ptr->data;
			}
			
			line--;
			
			ptr = ptr->next;
		}
	}
	
	return NULL;
}

const char *iniValueDef(const char *section, const char *variable, const char *defvalue)
{
	const char *val = iniValue(section, variable);
	if(val)
	{
		if(strlen(val) > 0)
		{
			return val;
		}
	}

	return defvalue;
}

BOOL liner(const char *src, const char *dst, linerRule_t *rules)
{
	static char linebuf[MAX_LINE];
	BOOL rc = 0;
	
	removeROFlag(src);
	removeROFlag(dst);
	
	HANDLE file = CreateFileA(src, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	
	if(file != INVALID_HANDLE_VALUE)
	{
		char c;
		DWORD readed;
		size_t linepos = 0;
		DWORD junk;
		
		HANDLE out = CreateFileA(dst, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		
		if(out != INVALID_HANDLE_VALUE)
		{
			do
			{
				readed = 0;
				if(ReadFile(file, &c, 1, &readed, NULL))
				{
					if(readed == 1)
					{
						switch(c)
						{
							case '\r':
							case '\0':
								break;
							case '\n':
							{
								BOOL ignore_line = FALSE;
								linebuf[linepos] = '\0';
								for(linerRule_t *r = rules; r->line_match != NULL; r++)
								{
									if(r->full_match)
									{
										if(strcmp(linebuf, r->line_match) == 0)
										{
											ignore_line = !r->action(linebuf, linepos);
											
											if(r->last_rule) break;
										}
									}
									else
									{
										if(strstr(linebuf, r->line_match) != NULL)
										{
											ignore_line = !r->action(linebuf, linepos);
											
											if(r->last_rule) break;
										}
									}
								}
								
								if(!ignore_line)
								{
									size_t line_len = strlen(linebuf);
									if(line_len >= MAX_LINE - 3)
									{
										linebuf[MAX_LINE - 3] = '\0';
										line_len = MAX_LINE - 3;
									}
								
									strcat(linebuf, "\r\n");
									line_len += 2;
								
									WriteFile(out, linebuf, line_len, &junk, NULL);
								}
								
								linepos = 0;
								break;
							}
							default:
								if(linepos < MAX_LINE-1)
								{
									linebuf[linepos++] = c;
								}
								break;
						}
					}
				}
			} while(readed > 0);
			
			CloseHandle(out);
			
			rc = TRUE;
		}
		else
		{
			REPORT("CreateFileA(%s, WRITE): %ld", dst, GetLastError());
		}
		
		CloseHandle(file);
	}
	else
	{
		REPORT("CreateFileA(%s, READ): %ld", src, GetLastError());
	}
	
	return rc;
}

BOOL addLine(const char *target, const char *line)
{
	removeROFlag(target);
	
	HANDLE file = CreateFileA(target, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if(file != INVALID_HANDLE_VALUE)
	{
		DWORD junk;
		
		SetFilePointer(file, 0, NULL, FILE_END);
		WriteFile(file, line, strlen(line), &junk, NULL);
		CloseHandle(file);
		return TRUE;
	}
	else
	{
		REPORT("CreateFileA: %ld", GetLastError());
	}
	
	return FALSE;
}
