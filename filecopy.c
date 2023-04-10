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
#include <string.h>
#include <stdio.h>

#include "filecopy.h"
#include "setuperr.h"
#include "nocrt.h"

BOOL is_dir(const char *path)
{
	DWORD atrs = GetFileAttributesA(path);
	if(atrs != INVALID_FILE_ATTRIBUTES)
	{
		return (atrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
	}
	
	return FALSE;
}


BOOL mkdir_recrusive(const char *path)
{
	char atom[MAX_PATH];
	size_t pos = 0;
	size_t new_pos = 0;
	const char *ptr;
	
	while((ptr = strchr(path+pos, '\\')) != NULL)
	{
		new_pos = ptr - path;
		
		if((new_pos - pos) > 0)
		{
			memcpy(atom, path, new_pos);
			atom[new_pos] = '\0';
			
			if(!is_dir(atom))
			{
				//printf("Creating atom: %s\n", atom);
				if(!CreateDirectoryA(atom, NULL))
				{
					return FALSE;
				}
			}
		}
		pos = new_pos + 1;
	}
	
	if(!CreateDirectoryA(path, NULL))
	{
		return FALSE;
	}
	
	return TRUE;
}

void removeROFlag(const char *path)
{
	DWORD attrs = GetFileAttributesA(path);
	if(attrs != INVALID_FILE_ATTRIBUTES)
	{
		if(attrs & FILE_ATTRIBUTE_READONLY)
		{
			SetFileAttributes(path, attrs ^ FILE_ATTRIBUTE_READONLY);
		}
	}
}

int copydir(const char *from, const char *to)
{
	char file_re[MAX_PATH] = {0};
	char srcpath[MAX_PATH];
	char dstpath[MAX_PATH];
	WIN32_FIND_DATAA taa;
	int cnt = 0;
	HANDLE hdir;
	
	if(!is_dir(to))
	{
		if(!mkdir_recrusive(to))
		{
			REPORT("mkdir_recrusive(%s): %ld\n", to, GetLastError());
			return 0;
		}
	}

	strcpy(file_re, from);
	strcat(file_re, "\\*");
	
	memset(&taa, 0, sizeof(taa));
	
	hdir = FindFirstFileA(file_re, &taa);
	if(hdir != INVALID_HANDLE_VALUE)
	{
		do
		{
			if(strcmp(taa.cFileName, ".") != 0 && strcmp(taa.cFileName, "..") != 0)
			{
				strcpy(srcpath, from);
				strcat(srcpath, "\\");
				strcat(srcpath, taa.cFileName);
				
				strcpy(dstpath, to);
				strcat(dstpath, "\\");
				strcat(dstpath, taa.cFileName);
				
				if((taa.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
				{
					cnt += copydir(srcpath, dstpath);
				}
				else
				{
					removeROFlag(dstpath);
					if(CopyFileA(srcpath, dstpath, FALSE))
					{
						removeROFlag(dstpath);
						
						cnt++;
					}
				}
			}
				
		} while(FindNextFileA(hdir, &taa));
			
		CloseHandle(hdir);
	}
	else
	{
		REPORT("FindFirstFileA: %ld\n", GetLastError());
	}
	
	return cnt;
}
