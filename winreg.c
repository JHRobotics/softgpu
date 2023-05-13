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
#include <string.h>
#include <winreg.h>
#include "nocrt.h"

#include "winreg.h"

#include "setuperr.h"

#define MIN(_a, _b) (((_b) < (_a)) ? (_b) : (_a))

typedef struct rootkey
{
	const char *name;
	HKEY key;
} rootkey_t;

static const rootkey_t rootkeys[] = 
{
	{"HKCR", HKEY_CLASSES_ROOT},
	{"HKCC", HKEY_CURRENT_CONFIG},
	{"HKCU", HKEY_CURRENT_USER},
	{"HKLM", HKEY_LOCAL_MACHINE},
	{"HKU",  HKEY_USERS},
	{"HKEY_CLASSES_ROOT", HKEY_CLASSES_ROOT},
	{"HKEY_CURRENT_CONFIG", HKEY_CURRENT_CONFIG},
	{"HKEY_CURRENT_USER", HKEY_CURRENT_USER},
	{"HKEY_LOCAL_MACHINE", HKEY_LOCAL_MACHINE},
	{"HKEY_USERS",  HKEY_USERS},
	{NULL,   0}
};

#define REG_PATH_MAX 1024

static char winreg_tmp[REG_PATH_MAX];

static BOOL registrySplitPath(const char *path, HKEY *rootkey, const char **key, const char **subkey)
{
	const char *p1 = strchr(path, '\\');
	const char *p2 = strrchr(path, '\\');
	
	size_t p1_len = p1 - path;
	size_t p2_len = 0;
	size_t p3_len = strlen(p2+1);
	
	if(p2 - p1 > 0)
	{
		p2_len = p2 - p1 - 1;
	}
	
	if(p1_len == 0)
	{
		return FALSE;
	}
	
	memcpy(winreg_tmp, path, p1_len);
	winreg_tmp[p1_len] = '\0';
	
	for(const rootkey_t *rk = &rootkeys[0]; rk->name != NULL; rk++)
	{
		if(stricmp(rk->name, winreg_tmp) == 0)
		{
			*rootkey = rk->key;
			if(p2_len > 0)
			{
				memcpy(winreg_tmp, p1+1, p2_len);
				winreg_tmp[p2_len] = '\0';
				*key = &(winreg_tmp[0]);
			}
			else
			{
				*key = NULL;
			}
			
			if(p3_len > 0)
			{
				*subkey = p2+1;
			}
			else
			{
				*subkey = NULL;
			}
			
			return TRUE;
		}
	}
	
	return FALSE;
}

BOOL registryRead(const char *path, char *buffer, size_t buffer_size)
{
	HKEY rootkey;
	const char *key;
	const char *subkey;
	BOOL rc = FALSE;
	
	if(registrySplitPath(path, &rootkey, &key, &subkey))
	{
		HKEY hKey;
		LSTATUS lResult;
		
		lResult = RegOpenKeyEx(rootkey, key, 0, KEY_READ, &hKey);
		if(lResult == ERROR_SUCCESS)
		{
			DWORD type;
			DWORD size = REG_PATH_MAX;
			
			//lResult = RegGetValueA(hKey, NULL, subkey, RRF_RT_DWORD | RRF_RT_REG_SZ, &type, winreg_tmp, &size);
			lResult = RegQueryValueExA(hKey, subkey, NULL, &type, (LPBYTE)winreg_tmp, &size);
	    if(lResult == ERROR_SUCCESS)
	    {
	    	switch(type)
	    	{
					case REG_SZ:
					case REG_MULTI_SZ:
					case REG_EXPAND_SZ:
					{
						size_t cpy_size = MIN(size-1, buffer_size-1);
						memcpy(buffer, winreg_tmp, cpy_size);
						buffer[cpy_size] = '\0';
						rc = TRUE;
						break;
					}
					case REG_DWORD:
					{
						//DWORD temp_dw = *((LPDWORD)winreg_tmp);
						DWORD temp_dw;
						memcpy(&temp_dw, winreg_tmp, sizeof(DWORD));
						sprintf(buffer, "%lu", temp_dw);
						rc = TRUE;
						break;
					}
				}
			}
			
			RegCloseKey(hKey);
		}
	}
	
	return rc;
}

#define DW_BUF 12

BOOL registryReadDWORD(const char *path, DWORD *out)
{
	static char buf[DW_BUF];
	
	if(registryRead(path, buf, DW_BUF))
	{
		*out = strtoul(buf, NULL, 10);
		return TRUE;
	}
	
	return FALSE;
}

BOOL registryWrite(const char *path, const char *str, int type)
{
	HKEY rootkey;
	const char *key;
	const char *subkey;
	BOOL rc = FALSE;
	
	if(registrySplitPath(path, &rootkey, &key, &subkey))
	{
		HKEY hKey;
		LSTATUS lResult = -1;
		
		lResult = RegCreateKeyA(rootkey, key, &hKey);
		//lResult = RegOpenKeyEx(rootkey, key, 0, KEY_READ, &hKey);
		if(lResult == ERROR_SUCCESS)
		{
			if(type == WINREG_DWORD)
			{
				DWORD tmp = strtoul(str, NULL, 10);
				lResult = RegSetValueExA(hKey, subkey, 0, REG_DWORD, (LPBYTE)&tmp, sizeof(DWORD));
			}
			else if(type == WINREG_STR)
			{
				lResult = RegSetValueExA(hKey, subkey, 0, REG_SZ, (LPBYTE)str, strlen(str)+1);
			}
			
	  	if(lResult == ERROR_SUCCESS)
	  	{
				rc = TRUE;
			}
			RegCloseKey(hKey);
		}
		else
		{
			REPORT("RegCreateKeyA: %ld", lResult);
		}
	}
	
	return rc;
}

BOOL registryWriteDWORD(const char *path, DWORD dw)
{
	static char buf[DW_BUF];
	
	sprintf(buf, "%lu", dw);
	return registryWrite(path, buf, WINREG_DWORD);
}
