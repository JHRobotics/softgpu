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
#include <stdint.h>

#include "softgpu.h"

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

BOOL dotcom_start(HWND hwnd)
{
	install_infobox(hwnd, ".COM for Windows 95");
	return install_run(iniValue("[softgpu]", "dcom95path"), &pi_proc, &pi_thread, TRUE);
}

BOOL ws2_start(HWND hwnd)
{
	install_infobox(hwnd, "winsock 2 for Windows 95");
	return install_run(iniValue("[softgpu]", "ws2path"), &pi_proc, &pi_thread, TRUE);
}

void delete_bad_dx_files()
{
	char syspath[MAX_PATH];
	size_t syspath_len;
	
	if(GetSystemDirectoryA(syspath, MAX_PATH) == 0)
	{
		return;
	}
	syspath_len = strlen(syspath);
	
	strcpy(syspath+syspath_len, "\\ddraw.dll");
	if(is_wrapper(syspath, FALSE))
	{
		DeleteFileA(syspath);
	}
	
	strcpy(syspath+syspath_len, "\\d3d8.dll");
	if(is_wrapper(syspath, FALSE))
	{
		DeleteFileA(syspath);
	}
	
	if(version_compare(&sysver, &WINVER98) >= 0)
	{
		strcpy(syspath+syspath_len, "\\d3d9.dll");
		if(is_wrapper(syspath, FALSE))
		{
			removeROFlag(syspath);
			DeleteFileA(syspath);
		}
	}
}

BOOL dx_start(HWND hwnd)
{
	if(dx_path == NULL)
	{
		return FALSE;
	}
	
	install_infobox(hwnd, "MS DirectX");
	
	delete_bad_dx_files();
	
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

BOOL driver_copy(HWND hwnd)
{
	static char mgsbuf[512];
	if(version_compare(&sysver, &WINVER98) >= 0)
	{
		sprintf(mgsbuf, "Driver files are located here: %s", install_path);
		MessageBoxA(hwnd, mgsbuf, "No automatic driver install", MB_ICONWARNING);
	}
	else
	{
		sprintf(mgsbuf, "Automatic instalation isn't possible, because it is unsupported on this system. You have to install driver manually!\n\nDriver files are located here: %s", install_path);
		MessageBoxA(hwnd, mgsbuf, "No automatic driver install", MB_ICONWARNING);
	}
	
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

static void fix_ddraw(const char* file)
{
	const uint8_t search[] = {
		0x44, 0x44, 0x52, 0x41, 0x57, 0x31, 0x36, 0x2E, 0x44, 0x4C, 0x4C, 0x00, 0x44, 0x44, 0x52, 0x41, // ; DDRAW16.DLL.DDRA
		0x57, 0x2E, 0x44, 0x4C, 0x4C, 0x00                                                              // ; W.DLL.
	};
	const size_t replace_pos = 12;
	const char defname[] = "ddraw.dll";
	const char    dest[] = "ddsys.dll";
	int found = 0;
	
	FILE *fr = fopen(file, "rb");
	if(fr != NULL)
	{
		size_t fs = 0;
		uint8_t *mem = NULL;
		
		fseek(fr, 0, SEEK_END);
		fs = ftell(fr);
		fseek(fr, 0, SEEK_SET);
		
		mem = (uint8_t*)malloc(fs);
		if(mem != NULL)
		{
			if(fread(mem, 1, fs, fr) == fs)
			{
				fclose(fr);
				fr = NULL;

				size_t i, j;
				
				for(i = 0; i < fs - sizeof(search); i++)
				{
					if(memcmp(mem+i, search, sizeof(search)) == 0)
					{
						for(j = 0; j < sizeof(defname)-1; j++)
						{
							mem[i+replace_pos+j] = toupper(dest[j]);
						}
						found++;
					}
				}
				
				if(found > 0)
				{
					FILE *fw = fopen(file, "wb");
					if(fw)
					{
						fwrite(mem, 1, fs, fw);
						
						fclose(fw);
					}
				}
			}
			
			free(mem);
		}
		
		if(fr != NULL) fclose(fr);
	};
}

static HANDLE filescopy_thread = 0;
static int copiedfiles = 0;

static BOOL has_sys_ddraw = FALSE;
static BOOL has_sys_d3d8  = FALSE;
static BOOL has_sys_d3d9  = FALSE;

void backup_sysfiles()
{
	char syspath[MAX_PATH];
	size_t syspath_len;
	char inspath[MAX_PATH];
	size_t inspath_len;
	
	if(GetSystemDirectoryA(syspath, MAX_PATH) == 0)
	{
		return;
	}
	syspath_len = strlen(syspath);
	strcpy(inspath, install_path);
	inspath_len = strlen(inspath); 
	
	/* create dirs */
	strcpy(inspath+inspath_len, "\\backup");
	mkdir_recrusive(inspath);
	strcpy(inspath+inspath_len, "\\sys");
	mkdir_recrusive(inspath);
	
	/* ddraw.dll */
	strcpy(syspath+syspath_len, "\\ddraw.dll");
	if(!is_wrapper(syspath, TRUE))
	{
		strcpy(inspath+inspath_len, "\\backup\\ddraw.dll");
		CopyFileA(syspath, inspath, FALSE);
		
		strcpy(inspath+inspath_len, "\\sys\\ddsys.dll");
		if(CopyFileA(syspath, inspath, FALSE))
		{
			removeROFlag(inspath);
			fix_ddraw(inspath);
			has_sys_ddraw = TRUE;
		}
	}
	else
	{
		strcpy(syspath+syspath_len, "\\ddsys.dll");
		if(!is_wrapper(syspath, TRUE))
		{
			strcpy(inspath+inspath_len, "\\sys\\ddsys.dll");
			if(CopyFileA(syspath, inspath, FALSE))
			{
				removeROFlag(inspath);
				fix_ddraw(inspath);
				has_sys_ddraw = TRUE;
			}
		}
	}
	
	/* d3d8.dll */
	strcpy(syspath+syspath_len, "\\d3d8.dll");
	if(!is_wrapper(syspath, TRUE))
	{
		strcpy(inspath+inspath_len, "\\backup\\d3d8.dll");
		CopyFileA(syspath, inspath, FALSE);
		
		strcpy(inspath+inspath_len, "\\sys\\msd3d8.dll");
		if(CopyFileA(syspath, inspath, FALSE))
		{
			removeROFlag(inspath);
			has_sys_d3d8 = TRUE;
		}
	}
	else
	{
		strcpy(syspath+syspath_len, "\\msd3d8.dll");
		if(!is_wrapper(syspath, TRUE))
		{
			strcpy(inspath+inspath_len, "\\sys\\msd3d8.dll");
			if(CopyFileA(syspath, inspath, FALSE))
			{
				removeROFlag(inspath);
				has_sys_d3d8 = TRUE;
			}
		}
	}
	
	/* d3d9.dll */
	strcpy(syspath+syspath_len, "\\d3d9.dll");
	if(!is_wrapper(syspath, TRUE))
	{
		strcpy(inspath+inspath_len, "\\backup\\d3d9.dll");
		CopyFileA(syspath, inspath, FALSE);
		
		strcpy(inspath+inspath_len, "\\sys\\msd3d9.dll");
		if(CopyFileA(syspath, inspath, FALSE))
		{
			removeROFlag(inspath);
			has_sys_d3d9 = TRUE;
		}
	}
	else
	{
		strcpy(syspath+syspath_len, "\\msd3d9.dll");
		if(!is_wrapper(syspath, TRUE))
		{
			strcpy(inspath+inspath_len, "\\sys\\msd3d9.dll");
			if(CopyFileA(syspath, inspath, FALSE))
			{
				removeROFlag(inspath);
				has_sys_d3d9 = TRUE;
			}
		}
	}
}

BOOL voodoo_copy(HWND hwnd)
{
	(void)hwnd;
	
	char srcpath[MAX_PATH];
	char dstpath[MAX_PATH];
	const char *drv_voodoo = iniValue("[softgpu]", "voodoo2path");
	const char *f;
	int i;
	
	const char *voodoo_files[] = {
		"\\3dfxSpl2.dll",
		"\\3dfxSpl3.dll",
		"\\3dfxVGL.dll",
		NULL
	};
	
	if(drv_voodoo == NULL)
	{
		REPORT("Failed to get source path to Voodoo driver");
		return FALSE;
	}
	
	strcpy(dstpath, install_path);
	strcat(dstpath, "\\3dfx");
	mkdir_recrusive(dstpath);
	
	for(i = 0; (f = voodoo_files[i]) != NULL; i++)
	{
		strcpy(dstpath, install_path);
		strcat(dstpath, "\\3dfx");
		strcat(dstpath, f);
		
		strcpy(srcpath, drv_voodoo);
		strcat(srcpath, f);
		
		CopyFileA(srcpath, dstpath, FALSE);
	}
	
	return TRUE;
}

DWORD WINAPI filescopy_proc(LPVOID lpParameter)
{
	(void)lpParameter;
	
	copiedfiles = copydir(src_path, install_path);
	
	backup_sysfiles();
	
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

BOOL setLineSvga(char *buf, size_t bufs)
{
	(void)bufs;
	
	strcpy(buf, "CopyFiles=VMSvga.Copy");
	if(isSettingSet(CHBX_WINE))
	{
		strcat(buf, ",Dx.Copy,DX.CopyBackup");
	}
	
	if(isSettingSet(CHBX_GLIDE))
	{
		strcat(buf, ",Voodoo.Copy");
	}
	
	return TRUE;
}

BOOL setLineVbox(char *buf, size_t bufs)
{
	(void)bufs;
	
	strcpy(buf, "CopyFiles=VBox.Copy");
	if(isSettingSet(CHBX_WINE))
	{
		strcat(buf, ",Dx.Copy,DX.CopyBackup");
	}
	
	if(isSettingSet(CHBX_GLIDE))
	{
		strcat(buf, ",Voodoo.Copy");
	}
	
	return TRUE;
}

BOOL setLineQemu(char *buf, size_t bufs)
{
	(void)bufs;
	
	strcpy(buf, "CopyFiles=Qemu.Copy");
	if(isSettingSet(CHBX_WINE))
	{
		strcat(buf, ",Dx.Copy,DX.CopyBackup");
	}
	
	if(isSettingSet(CHBX_GLIDE))
	{
		strcat(buf, ",Voodoo.Copy");
	}
	
	return TRUE;
}

BOOL setLineSvgaReg(char *buf, size_t bufs)
{
	(void)bufs;
	
	strcpy(buf, "AddReg=VMSvga.AddReg,VM.AddReg");
	
	if(isSettingSet(CHBX_WINE) && version_compare(&sysver, &WINVERME) >= 0)
	{
		/* these entries is only relevant to system, that cannot replace ddraw.dll */
		strcat(buf, ",DX.addReg");
	}
	
	if(isSettingSet(CHBX_QXGA))
		strcat(buf, ",VM.QXGA");
	
	if(isSettingSet(CHBX_1440))
		strcat(buf, ",VM.WQHD");
	
	if(isSettingSet(CHBX_4K))
		strcat(buf, ",VM.UHD");
	
	if(isSettingSet(CHBX_5K))
		strcat(buf, ",VM.R5K");
	
	strcat(buf, ",VM.regextra");
	
	return TRUE;
}

BOOL setLineVboxReg(char *buf, size_t bufs)
{
	(void)bufs;
	
	strcpy(buf, "AddReg=VBox.AddReg,VM.AddReg");
	
	if(isSettingSet(CHBX_WINE) && version_compare(&sysver, &WINVERME) >= 0)
	{
		strcat(buf, ",DX.addReg");
	}
	
	if(isSettingSet(CHBX_QXGA))
		strcat(buf, ",VM.QXGA");
	
	if(isSettingSet(CHBX_1440))
		strcat(buf, ",VM.WQHD");
	
	if(isSettingSet(CHBX_4K))
		strcat(buf, ",VM.UHD");
	
	if(isSettingSet(CHBX_5K))
		strcat(buf, ",VM.R5K");
		
		
	strcat(buf, ",VM.regextra");
	
	return TRUE;
}

BOOL setLineQemuReg(char *buf, size_t bufs)
{
	(void)bufs;
	
	strcpy(buf, "AddReg=Qemu.AddReg,VM.AddReg");
	
	if(isSettingSet(CHBX_WINE) && version_compare(&sysver, &WINVERME) >= 0)
	{
		strcat(buf, ",DX.addReg");
	}
	
	if(isSettingSet(CHBX_QXGA))
		strcat(buf, ",VM.QXGA");
	
	if(isSettingSet(CHBX_1440))
		strcat(buf, ",VM.WQHD");
	
	if(isSettingSet(CHBX_4K))
		strcat(buf, ",VM.UHD");
	
	if(isSettingSet(CHBX_5K))
		strcat(buf, ",VM.R5K");
	
	strcat(buf, ",VM.regextra");
	
	return TRUE;
}

BOOL setLineMeFix(char *buf, size_t bufs)
{
	(void)bufs;
	
	if(version_compare(&sysver, &WINVERME) > 0 &&
		isSettingSet(CHBX_WINE))
	{
		size_t s_full = strlen(buf);
		size_t s_prefix = sizeof(";mefix:") - 1;
		size_t i;
		
		// memmove(buf, buf + s_prefix, s_full - s_prefix + 1);
		for(i = 0; i < s_full - s_prefix + 1; i++)
		{
			buf[i] = buf[i + s_prefix];
		}
	}
	
	return TRUE;
}

BOOL setBug565(char *buf, size_t bufs)
{
	(void)bufs;
	char *dst = strstr(buf, ",,");
	if(dst)
	{
		sprintf(dst+2, "%d", isSettingSet(CHBX_BUG_RGB565) ? 1 : 0);
		return TRUE;
	}
	return FALSE;
}

BOOL setBugPreferFifo(char *buf, size_t bufs)
{
	(void)bufs;
	char *dst = strstr(buf, ",,");
	if(dst)
	{
		sprintf(dst+2, "%d", isSettingSet(CHBX_BUG_PREFER_FIFO) ? 1 : 0);
		return TRUE;
	}
	return FALSE;
}

BOOL setBugDxFlags(char *buf, size_t bufs)
{
	(void)bufs;
	char *dst = strstr(buf, ",,");
	if(dst)
	{
		sprintf(dst+2, "%d", isSettingSet(CHBX_BUG_DX_FLAGS) ? 1 : 0);
		return TRUE;
	}
	return FALSE;
}

BOOL setLimitVRAM(char *buf, size_t bufs)
{
	(void)bufs;
	char *dst = strstr(buf, ",,");
	if(dst)
	{
		sprintf(dst+2, "%d", settingReadDW(INP_VRAM_LIMIT));
		return TRUE;
	}
	return FALSE;
}

BOOL setLineSyscopy(char *buf, size_t bufs)
{
	(void)bufs;
	BOOL fix_line = FALSE;
	
	if(strstr(buf, "ddsys.dll") != NULL)
	{
		if(has_sys_ddraw)
		{
			fix_line = TRUE;
		}
	}
	
	if(strstr(buf, "msd3d8.dll") != NULL)
	{
		if(has_sys_d3d8)
		{
			fix_line = TRUE;
		}
	}
	
	if(strstr(buf, "msd3d9.dll") != NULL)
	{
		if(has_sys_d3d9)
		{
			fix_line = TRUE;
		}
	}
	
	if(fix_line)
	{	
		int line_start = sizeof(";syscopy:")-1;
		int line_len  = strlen(buf);
		int i;
			
		for(i = 0; i < line_len - line_start + 1; i++)
		{
			buf[i] = buf[i + line_start];
		}
	}
	
	return TRUE;
}

BOOL setLine3DFX(char *buf, size_t bufs)
{
	(void)bufs;
	
	if(isSettingSet(CHBX_3DFX))
	{
		int line_start = sizeof(";3dfx:")-1;
		int line_len  = strlen(buf);
		int i;
			
		for(i = 0; i < line_len - line_start + 1; i++)
		{
			buf[i] = buf[i + line_start];
		}
	}
	
	return TRUE;
}

linerRule_t infFixRules[] = {
	{"CopyFiles=VBox.Copy,Dx.Copy,DX.CopyBackup,Voodoo.Copy",   TRUE, TRUE, setLineVbox},
	{"CopyFiles=VMSvga.Copy,Dx.Copy,DX.CopyBackup,Voodoo.Copy", TRUE, TRUE, setLineSvga},
	{"CopyFiles=Qemu.Copy,Dx.Copy,DX.CopyBackup,Voodoo.Copy",   TRUE, TRUE, setLineQemu},
	{"AddReg=VBox.AddReg,VM.AddReg,DX.addReg,VM.regextra",                  TRUE, TRUE, setLineVboxReg},
	{"AddReg=VMSvga.AddReg,VM.AddReg,DX.addReg,VM.regextra",                TRUE, TRUE, setLineSvgaReg},
	{"AddReg=Qemu.AddReg,VM.AddReg,DX.addReg,VM.regextra",                  TRUE, TRUE, setLineQemuReg},
	{"HKLM,Software\\VMWSVGA,RGB565bug,,0",                     TRUE, TRUE, setBug565},
	{"HKLM,Software\\VMWSVGA,PreferFIFO,,0",                    TRUE, TRUE, setBugPreferFifo},
	{"HKLM,Software\\Mesa3D\\global,SVGA_CLEAR_DX_FLAGS,,1",    TRUE, TRUE, setBugDxFlags},
	{"HKLM,Software\\VMWSVGA,VRAMLimit,,",                     FALSE, TRUE, setLimitVRAM},
	{";mefix:",                                                FALSE, TRUE, setLineMeFix},
	{";syscopy:",                                              FALSE, TRUE, setLineSyscopy},
	{";3dfx:",                                                 FALSE, TRUE, setLine3DFX},
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
			liner("C:\\autoexec.bak", "C:\\autoexec.bat", simd95rules);
		}
		
		addLine("C:\\autoexec.bat", "REM Added by SOFTGPU = simd95.com = enable SSE/AVX\r\n");
		addLine("C:\\autoexec.bat", "C:\\simd95.com\r\n");
	}
	
	return TRUE;
}

BOOL set_inf_regs(HWND hwnd)
{
	(void)hwnd;
	char dstfile[MAX_PATH];
	
	strcpy(dstfile, install_path);
	strcat(dstfile, "\\");
	strcat(dstfile, iniValue("[softgpu]", "drvfile"));
	
	if(isSettingSet(RAD_DD_HAL))
		registryWriteInf("HKLM\\Software\\DDSwitcher\\global", "system", WINREG_STR, dstfile);
	else
		registryWriteInf("HKLM\\Software\\DDSwitcher\\global", "wine", WINREG_STR, dstfile);
	
	if(isSettingSet(RAD_D8_NINE))
		registryWriteInf("HKLM\\Software\\D8Switcher\\global", "ninemore", WINREG_STR, dstfile);
	else
		registryWriteInf("HKLM\\Software\\D8Switcher\\global", "wine", WINREG_STR, dstfile);
		
	if(isSettingSet(RAD_D9_NINE))
		registryWriteInf("HKLM\\Software\\D9Switcher\\global", "nine", WINREG_STR, dstfile);
	else
		registryWriteInf("HKLM\\Software\\D9Switcher\\global", "wine", WINREG_STR, dstfile);
	
	DWORD stoptions = 0;
	if(isSettingSet(CHBX_ST_16))
	{
		stoptions |= 0x1;
	}
	
	if(isSettingSet(CHBX_ST_MOUSE))
	{
		stoptions |= 0x2;
	}
	
	if(isSettingSet(CHBX_ST_MOUSE_HIDE))
	{
		stoptions |= 0x4;
	}
	
	registryWriteInfDWORD("HKLM\\Software\\VMWSVGA\\STSize", settingReadDW(INP_SCREENTARGET), dstfile);
	registryWriteInfDWORD("HKLM\\Software\\VMWSVGA\\STOptions", stoptions, dstfile);
	
	
	return TRUE;
}

BOOL apply_reg_fixes(HWND hwnd)
{
	const char *line = NULL;
	char buf[1024];
	int index = 0;
	(void)hwnd;
	char dstfile[MAX_PATH];
	
	strcpy(dstfile, install_path);
	strcat(dstfile, "\\");
	strcat(dstfile, iniValue("[softgpu]", "drvfile"));
	
	if(version_compare(&sysver, &WINVERME) < 0)
	{
		registryDeleteInf("HKLM\\System\\CurrentControlSet\\Control\\SessionManager\\KnownDLLs\\DDRAW", dstfile);
	}
	
	for(; (line = iniLine("[fixes]", index)) != NULL; index++)
	{
		const char *p = strchr(line, ';');
		if(p)
		{
			if(strstr(p, ";DWORD:") == p)
			{
				memcpy(buf, line, p-line);
				buf[p-line] = '\0';
				
				p += sizeof(";DWORD:")-1;
				
				if(strlen(buf) > 0 && strlen(p) > 0)
				{
					registryWriteInf(buf, p, WINREG_DWORD, dstfile);
				}
			}
			else if(strstr(p, ";DELETE") == p)
			{
				memcpy(buf, line, p-line);
				buf[p-line] = '\0';
				registryDeleteInf(buf, dstfile);
			}
			else
			{
				memcpy(buf, line, p-line);
				buf[p-line] = '\0';
			
				if(strlen(buf) > 0 && strlen(p+1) > 0)
				{
					registryWriteInf(buf, p+1, WINREG_STR, dstfile);
				}
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

