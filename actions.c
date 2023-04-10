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
#include "winini.h"
#include "windrv.h"
#include "actions.h"
#include "filecopy.h"
#include "winreg.h"
#include "setuperr.h"
#include "nocrt.h"

#define MAX(_a, _b) (((_b) > (_a)) ? (_b) : (_a))

static install_action_t *actual_action = NULL;
static install_action_t *last_action = NULL;

static char install_path[MAX_PATH];
static const char *src_path = NULL;
static const char *dx_path = NULL;
static const char *ie_path = NULL;

static BOOL forced_shutdown = FALSE;

void setInstallPath(HWND input)
{
	GetWindowTextA(input, install_path, sizeof(install_path));
}

void setDXpath(const char *dx)
{
	dx_path = dx;
}

void setIEpath(const char *ie)
{
	ie_path = ie;
}

BOOL failure_continue(HWND hwnd)
{
	static char buf[4096] = "Action failure! Continue?";
	const char *log;
	int i = 0;
	
	while((log = get_report(i)) != NULL)
	{
		if(i == 0)
		{
			strcat(buf, "\n\nTrace:\n");
		}
		
		strcat(buf, log);
		strcat(buf, "\n");
		i++;
	}
		
	int t = MessageBoxA(hwnd, buf, "FAIL", MB_ICONINFORMATION | MB_YESNO | MB_DEFBUTTON2);
	
	if(t == IDNO)
	{
		return FALSE;
	}
	
	return TRUE;
}

void flush_actions()
{
	while(actual_action != NULL)
	{
		install_action_t *old = actual_action;
		actual_action = actual_action->next;
		
		if(last_action == old)
		{
			last_action = NULL;
		}
			
		HeapFree(GetProcessHeap(), 0, old);	
	}
}

void next_action()
{
	install_action_t *old = actual_action;
	
	actual_action = actual_action->next;
	
	if(last_action == old)
	{
		last_action = NULL;
	}
	
	HeapFree(GetProcessHeap(), 0, old);
}

static install_action_t *fetch_action = NULL;

void timer_proc(HWND hwnd)
{
	if(fetch_action != NULL)
	{
		if(fetch_action->process)
		{
			if(!fetch_action->process(hwnd))
			{
				/* something still needs to be done */
				goto timer_proc_ret;
			}
		}
		
		if(fetch_action->end == NULL)
		{
			next_action();
			fetch_action = NULL;
		}
		else
		{
			if(fetch_action->end(hwnd))
			{
				next_action();
				fetch_action = NULL;
			}
			else
			{
				if(forced_shutdown)
				{
					PostQuitMessage(0);
					return;
				}
				
				if(failure_continue(hwnd))
				{
					next_action();
					fetch_action = NULL;
				}
				else
				{
					flush_actions();
					fetch_action = NULL;
					softgpu_reset(hwnd);
				}
			}
		}
		
		/* all done */
		if(actual_action == NULL)
		{
			softgpu_done(hwnd);
		}
	}
	else if(actual_action)
	{
		fetch_action = actual_action;
		
		softgpu_info(hwnd, fetch_action->name);
		
		if(!fetch_action->start(hwnd))
		{
			if(forced_shutdown)
			{
				PostQuitMessage(0);
				return;
			}
			
			if(failure_continue(hwnd))
			{
				next_action();
				fetch_action = NULL;
			}
			else
			{
				flush_actions();
				fetch_action = NULL;
				softgpu_reset(hwnd);
			}
		}
	}
	
timer_proc_ret:
	SetTimer(hwnd, TIMER_ID, 100, NULL);
}

BOOL action_create(const char *name, action_start_f astart, action_process_f aprocess, action_end_f aend)
{
	install_action_t *na = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(install_action_t));
	if(na != NULL)
	{
		strncpy(na->name, name, ACTION_NAME_MAX);
		na->start   = astart;
		na->process = aprocess;
		na->end     = aend;
		na->next    = NULL;
		
		if(last_action != NULL)
		{
			last_action->next = na;
			last_action = na;
		}
		else
		{
			actual_action = na;
			last_action   = na;
		}
		
		return TRUE;
	}
	
	return FALSE;
}

const char *dirname(const char *full_path)
{
	static char buf[PATH_MAX];
	
	const char *p1 = strrchr(full_path, '\\');
	const char *p2 = strrchr(full_path, '/');
	const char *p = MAX(p1, p2);
	
	if(p == NULL)
	{
		return full_path;
	}

	buf[p - full_path] = '\0';
	memcpy(buf, full_path, p - full_path);
	
	return buf;
}

BOOL install_run(const char *setup_path, HANDLE *pi_proc, HANDLE *pi_thread, BOOL runAndExit)
{
	STARTUPINFOA si;
	PROCESS_INFORMATION pi;
	
	memset(&si, 0, sizeof(si));
	memset(&pi, 0, sizeof(pi));
	
	si.cb = sizeof(si);
	
	if(CreateProcessA(setup_path, NULL, NULL, NULL, FALSE, DETACHED_PROCESS, NULL, dirname(setup_path), &si, &pi))
	{
		*pi_proc = pi.hProcess;
    *pi_thread =  pi.hThread;
    
    if(runAndExit)
    {
    	forced_shutdown = TRUE;
    	CloseHandle(pi.hThread);
    	CloseHandle(pi.hProcess);
    	
			*pi_proc = INVALID_HANDLE_VALUE;
    	*pi_thread = INVALID_HANDLE_VALUE;
    	
    	return FALSE;
    }
    
    return TRUE;
	}
	
	return FALSE;
}

static HANDLE pi_proc   = INVALID_HANDLE_VALUE;
static HANDLE pi_thread = INVALID_HANDLE_VALUE;

BOOL proc_wait(HWND hwnd)
{
	(void)hwnd;
	
	if(pi_proc != INVALID_HANDLE_VALUE)
	{
		DWORD code = 0;
		
		if(GetExitCodeProcess(pi_proc, &code))
		{
			if(code == STILL_ACTIVE)
			{
				return FALSE;
			}
		}
		
		CloseHandle(pi_proc);
		CloseHandle(pi_thread);
	}
	
	pi_proc = INVALID_HANDLE_VALUE;
	pi_thread = INVALID_HANDLE_VALUE;
	
	return TRUE;
}

extern version_t sysver;
extern version_t WINVER98SE;

void install_infobox(HWND hwnd, const char *name)
{
	static char msg_buffer[512];

#if 0
	if(version_compare(&sysver, &WINVER98SE) < 0)
	{
		sprintf(msg_buffer,
			"Setup will install now %s\n\n"
			"Afrer instalation reboot is recomended.\n\n"
			"WARNING: your system have \"CPU speed bug\", update system component sometimes overwrite patched files!"
			"Make sure you have Patcher9x boot floppy, if the system fail boot afrer update.\n\n"
			"After reboot, please run SOFTGPU setup again!", name);
		MessageBoxA(hwnd, msg_buffer, name, MB_ICONWARNING);
	}
	else
#endif
	{
		sprintf(msg_buffer, "Setup will install now %s\n\nAfrer instalation reboot is recomended.\n\nAfter reboot, please run SOFTGPU setup again!", name);
		MessageBoxA(hwnd, msg_buffer, name, MB_ICONINFORMATION);
	}

}

BOOL mscv_start(HWND hwnd)
{
	//MessageBoxA(hwnd, "Setup will install now VC6 redistributable.\n\nAfrer instalation reboot is recomended.\n\nAfter reboot, please run SOFTGPU setup again!", "VC6 redistributable instalation", MB_OK | MB_ICONINFORMATION);
	install_infobox(hwnd, "VC6 redistributable");
	return install_run(iniValue("[softgpu]", "msvcrtpath"), &pi_proc, &pi_thread, TRUE);
}

BOOL dx_start(HWND hwnd)
{
	if(dx_path == NULL)
	{
		return FALSE;
	}
	
	install_infobox(hwnd, "MS DirectX");
	return install_run(dx_path, &pi_proc, &pi_thread, TRUE);
}

BOOL ie_start(HWND hwnd)
{
	install_infobox(hwnd, "Internet Explorer");
	return install_run(ie_path, &pi_proc, &pi_thread, TRUE);
}

BOOL setup_end(HWND hwnd)
{
	(void)hwnd;
	return TRUE;
}

BOOL driver_without_setupapi(HWND hwnd)
{
	static char mgsbuf[512];
	sprintf(mgsbuf, "Automatic instalation isn't possible, because it is unsupported on this system. You have to install driver manually!\n\nDriver files are located here: %s", install_path);
	
	MessageBoxA(hwnd, mgsbuf, "No automatic driver install", MB_ICONWARNING);
	
	return TRUE;
}

BOOL driver_install(HWND hwnd)
{
	(void)hwnd;
	
	return installVideoDriver(install_path, iniValue("[softgpu]", "drvfile"));
}

void setInstallSrc(const char *path)
{
	src_path = path;
}

static HANDLE filescopy_thread = 0;
static int copiedfiles = 0;

DWORD WINAPI filescopy_proc(LPVOID lpParameter)
{
	(void)lpParameter;
	
	copiedfiles = copydir(src_path, install_path);
	
	return 0;
}

BOOL filescopy_start(HWND hwnd)
{
	(void)hwnd;
	SECURITY_ATTRIBUTES sa;
	sa.nLength = sizeof(sa);
	sa.lpSecurityDescriptor = NULL;
	sa.bInheritHandle = TRUE;
	DWORD junk;
	
	filescopy_thread = CreateThread(&sa, 0, filescopy_proc, NULL, 0, &junk);
	if(filescopy_thread == INVALID_HANDLE_VALUE)
	{
		REPORT("CreateThread: %ld", GetLastError());
		return FALSE;
	}
	
	return TRUE;
}

BOOL filescopy_wait(HWND hwnd)
{
	(void)hwnd;
	
	DWORD code;
	
	if(filescopy_thread != INVALID_HANDLE_VALUE)
	{
		if(GetExitCodeThread(filescopy_thread, &code))
		{
			if(code == STILL_ACTIVE)
			{
				return FALSE;
			}
		}
		
		CloseHandle(filescopy_thread);
	}
	
	filescopy_thread = INVALID_HANDLE_VALUE;
	
	return TRUE;
}

BOOL filescopy_result(HWND hwnd)
{
	(void)hwnd;
	
	if(copiedfiles == 0)
	{
		REPORT("No files were copied! (%s => %s)", src_path, install_path);
		return FALSE;
	}
	
	return TRUE;
}

BOOL install_wine = FALSE;
BOOL install_glide = FALSE;

BOOL setLineSvga(char *buf, size_t bufs)
{
	(void)bufs;
	
	strcpy(buf, "CopyFiles=VMSvga.Copy");
	if(install_wine)
	{
		strcat(buf, ",Dx.Copy,DX.CopyBackup");
	}
	
	if(install_glide)
	{
		strcat(buf, ",Voodoo.Copy");
	}
	
	return TRUE;
}

BOOL setLineVbox(char *buf, size_t bufs)
{
	(void)bufs;
	
	strcpy(buf, "CopyFiles=VBox.Copy");
	if(install_wine)
	{
		strcat(buf, ",Dx.Copy,DX.CopyBackup");
	}
	
	if(install_glide)
	{
		strcat(buf, ",Voodoo.Copy");
	}
	
	return TRUE;
}

BOOL setLineSvgaReg(char *buf, size_t bufs)
{
	(void)bufs;
	
	strcpy(buf, "AddReg=VMSvga.AddReg,VM.AddReg");
	if(install_wine)
	{
		strcat(buf, ",DX.addReg");
	}
	
	return TRUE;
}

BOOL setLineVboxReg(char *buf, size_t bufs)
{
	(void)bufs;
	
	strcpy(buf, "AddReg=VBox.AddReg,VM.AddReg");
	if(install_wine)
	{
		strcat(buf, ",DX.addReg");
	}
	
	return TRUE;
}

linerRule_t infFixRules[] = {
	{"CopyFiles=VBox.Copy,Dx.Copy,DX.CopyBackup,Voodoo.Copy", TRUE, TRUE, setLineVbox},
	{"CopyFiles=VMSvga.Copy,Dx.Copy,DX.CopyBackup,Voodoo.Copy", TRUE, TRUE, setLineSvga},
	{"AddReg=VBox.AddReg,VM.AddReg,DX.addReg", TRUE, TRUE, setLineVboxReg},
	{"AddReg=VMSvga.AddReg,VM.AddReg,DX.addReg", TRUE, TRUE, setLineSvgaReg},
	{NULL, FALSE, FALSE, NULL}
};

BOOL infFixer(HWND hwnd)
{
	(void)hwnd;
	
	char srcfile[MAX_PATH];
	char dstfile[MAX_PATH];
	
	strcpy(srcfile, src_path);
	strcat(srcfile, "\\");
	strcat(srcfile, iniValue("[softgpu]", "drvfile"));
	
	strcpy(dstfile, install_path);
	strcat(dstfile, "\\");
	strcat(dstfile, iniValue("[softgpu]", "drvfile"));
	
	return liner(srcfile, dstfile, infFixRules);
}

BOOL ignore_line(char *buf, size_t bufs)
{
	(void)buf;
	(void)bufs;
	
	return FALSE;
}

linerRule_t simd95rules[] = {
	{"simd95.com", FALSE, TRUE, ignore_line},
	{NULL, FALSE, FALSE, NULL}
};

BOOL simd95(HWND hwnd)
{
	(void)hwnd;
	
	removeROFlag("C:\\simd95.com");
	removeROFlag("C:\\autoexec.bak");
	removeROFlag("C:\\autoexec.bat");
	
	if(CopyFileA(iniValue("[softgpu]", "simd95"), "C:\\simd95.com", FALSE))
	{
		if(CopyFileA("C:\\autoexec.bat", "C:\\autoexec.bak", FALSE))
		{
			//liner("C:\\autoexec.bak", "C:\\autoexec.new", simd95rules);
			
			addLine("C:\\autoexec.bat", "C:\\simd95.com\r\n");
		}
	}
	
	return TRUE;
}

BOOL apply_reg_fixes(HWND hwnd)
{
	const char *line = NULL;
	char buf[1024];
	int index = 0;
	(void)hwnd;
	
	for(; (line = iniLine("[fixes]", index)) != NULL; index++)
	{
		const char *p = strchr(line, ';');
		if(p)
		{
			memcpy(buf, line, p-line);
			buf[p-line] = '\0';
			
			if(strlen(buf) > 0 && strlen(p+1) > 0)
			{
				registryWrite(buf, p+1, WINREG_STR);
			}
		}
	}
	
	return TRUE;
}

BOOL gl95_start(HWND hwnd)
{
	(void)hwnd;
	static char path[PATH_MAX];
	
	if(GetSystemDirectoryA(path, PATH_MAX) == 0)
	{
		REPORT("GetSystemDirectoryA: %d", GetLastError());
		return FALSE;
	}
	strcat(path, "\\OPENGL32.DLL");
	
	if(!CopyFileA(iniValue("[softgpu]", "opengl95"), path, FALSE))
	{
		REPORT("CopyFileA: %d", GetLastError());
		return FALSE;
	}
	
	if(GetSystemDirectoryA(path, PATH_MAX) == 0)
	{
		REPORT("GetSystemDirectoryA: %d", GetLastError());
		return FALSE;
	}
	strcat(path, "\\GLU32.DLL");
	
	if(!CopyFileA(iniValue("[softgpu]", "glu95"), path, FALSE))
	{
		REPORT("CopyFileA: %d", GetLastError());
		return FALSE;
	}
	
	return TRUE;
}

