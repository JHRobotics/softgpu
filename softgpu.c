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
#include <Shlobj.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>

#include "softgpu.h"

#include "nocrt.h"

/*
 Some doc:
   Buttons:
   https://learn.microsoft.com/en-us/windows/win32/controls/button-types-and-styles
   
   Edit:
   https://learn.microsoft.com/cs-cz/windows/win32/controls/edit-control-styles
 
   Guidelines:
   https://learn.microsoft.com/en-us/windows/win32/uxguide/ctrl-check-boxes
   
   DX version:
   HKLM\SOFTWARE\Microsoft\DirectX\Version
*/

void about(HWND hwnd)
{
	static char txtbuf[2048];
	
	sprintf(txtbuf,
		"SoftGPU by Jaroslav Hensl <emulator@emulace.cz>\n"
		"Version: %s\n\n"
		"Components\n"
		"VMDisp9x: %s\n"
		"Mesa9x: %s\n"
		/*"Mesa9x (SSE): %s\n"*/
		"Wine9x: %s\n"
		"OpenGlide9x: %s\n"
		"SIMD95: %s\n\n"
		"more on: https://github.com/JHRobotics/softgpu\n",
		SOFTGPU_VERSION_STR,
		iniValue("[version]", "vmdisp9x"),
		iniValue("[version]", "mesa9x"),
		/*iniValue("[version]", "mesa9x_sse"),*/
		iniValue("[version]", "wine9x"),
		iniValue("[version]", "openglide9x"),
		iniValue("[version]", "simd95")
	);
	
	MessageBoxA(hwnd, txtbuf, "About SoftGPU", MB_OK);
}

void sysinfo(HWND hwnd)
{
	MessageBoxA(hwnd, sysinfomsg, "System informations", MB_OK);
}

void runprog(const char *exe_name)
{
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	char dirname_buf[MAX_PATH];
	char *dirname = NULL;
	char *basename = strrchr(exe_name, '\\');
	
	if(basename != NULL)
	{
		DWORD s2 = basename-exe_name;
		DWORD s = GetCurrentDirectory(MAX_PATH, dirname_buf);
		dirname_buf[s] = '\\';
		
		memcpy(&dirname_buf[s+1], exe_name, s2);
		dirname_buf[s+s2+1] = '\0';
	}

	memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);
  memset(&pi, 0, sizeof(pi));
	
	CreateProcessA(exe_name,
	NULL,
  NULL,
  NULL,
  FALSE,
  0,
  NULL,
  dirname,
  &si,
  &pi);
  
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
}

#define WND_SOFTGPU_CLASS_NAME "SoftGPUCLS"
#define WND_SOFTGPU_LOAD_CLASS_NAME "SoftGPULDCLS"

float rdpiX = 1.0;
float rdpiY = 1.0;

BOOL isNT    = FALSE;
version_t sysver = {0};
version_t dxver  = {0};
BOOL     hasCRT = FALSE;
BOOL     hasSETUPAPI = FALSE;
uint32_t hasSSE3 = 0;
uint32_t hasSSE42 = 0;
uint32_t hasAVX = 0;
BOOL     hasOpengl = FALSE;

BOOL     reinstall_dx = FALSE;

static DWORD    drvPCstatus = CHECK_DRV_OK;

char sysinfomsg[1024];


BOOL checkDXReinstall()
{
	char syspath[MAX_PATH];
	size_t syspath_len;
	
	if(GetSystemDirectoryA(syspath, MAX_PATH) == 0)
	{
		return FALSE;
	}
	syspath_len = strlen(syspath);
	
	strcat(syspath, "\\ddraw.dll");
	if(is_wrapper(syspath, FALSE))
	{
		strcpy(syspath+syspath_len, "\\ddsys.dll");
		if(is_wrapper(syspath, TRUE))
		{
			return TRUE;
		}
	}
	
	strcpy(syspath+syspath_len, "\\d3d8.dll");
	if(is_wrapper(syspath, FALSE))
	{
		strcpy(syspath+syspath_len, "\\msd3d8.dll");
		if(is_wrapper(syspath, TRUE))
		{
			return TRUE;
		}
	}
	
	if(version_compare(&sysver, &WINVER98) >= 0)
	{
		strcpy(syspath+syspath_len, "\\d3d9.dll");
		if(is_wrapper(syspath, FALSE))
		{
			strcpy(syspath+syspath_len, "\\msd3d9.dll");
			if(is_wrapper(syspath, TRUE))
			{
				return TRUE;
			}
		}
	}
	
	return FALSE;
}

void readCPUInfo()
{
	/* AVX avaibility by intel atricle:
	http://software.intel.com/en-us/blogs/2011/04/14/is-avx-enabled/ */
	__asm(
		"pusha\n"
		"xorl %%eax, %%eax\n"
		"cpuid\n"
		"cmpl $1, %%eax\n"             // does CPUID support eax = 1?
		"jb not_supported\n"
		"movl $1, %%eax\n"
		"cpuid\n"
		"andl $0x18000000, %%ecx\n"   // check 27 bit (OS uses XSAVE/XRSTOR)
		"cmpl $0x18000000, %%ecx\n"   // and 28 (AVX supported by CPU)
		"jne not_supported\n"
		"xorl %%ecx, %%ecx\n"          // XFEATURE_ENABLED_MASK/XCR0 register number = 0
		"xgetbv\n"                     // XFEATURE_ENABLED_MASK register is in edx:eax
		"andl $6, %%eax\n"
		"cmpl $6, %%eax\n"              // check the AVX registers restore at context switch
		"jne not_supported\n"
		"movl $1, %0\n"
		"jmp end\n"
		"not_supported:\n"
		"movl $0, %0\n"
		"end:\n"
		"popa" : "=m" (hasAVX));
  
	if(version_compare(&sysver, &WINVER98) >= 0) /* in 98 SSE supported if they are present */
	{
		__asm(
			"pusha\n"
			"movl $0, %0\n"
			"movl $0, %1\n"
			"xorl %%eax, %%eax\n"
			"cpuid\n"
			"cmpl $1, %%eax\n"             // does CPUID support eax = 1?
			"jb end_sse\n"
			"movl $1, %%eax\n"
			"cpuid\n"
			"andl $0x06000000, %%edx\n"   // check 25 and 26 - SSE and SSE2
			"cmpl $0x06000000, %%edx\n" 
			"jne end_sse\n"
			"mov %%ecx, %%eax\n"
			"andl $0x00000201, %%eax\n"   // check 0 and 9 - SSE3 and SSSE3
			"cmpl $0x00000201, %%eax\n"
			"jne end_sse\n" // SSE1, SSE2, SSE3, SSSE3
			"movl $1, %0\n"
			"andl $0x00180000, %%ecx\n"   // check 19, 20 - SSE3 and SSE4.1 SSE4.2
			"cmpl $0x00180000, %%ecx\n"
			"jne end_sse\n" // SSE4.1, SSE4.2
			"movl $1, %1\n"
			"jmp end_sse\n"
			"end_sse:\n"
			"popa" : "=m" (hasSSE3), "=m" (hasSSE42));
	}
	else
	{
		/*
		 * win 95, if XSAVE is enable, SSE present too, I need write better test
		 * (probably exec some sse instruction and catch exeption)
		 *
		 */
	  __asm(
			"pusha\n"
			"xorl %%eax, %%eax\n"
			"cpuid\n"
			"cmpl $1, %%eax\n"
			"jb not_supported_sse95\n"
			"mov $1, %%eax\n"
			"cpuid\n"
			"andl $0x08000000, %%ecx\n" // we simly check for enable xstore
			"cmpl $0x08000000, %%ecx\n" // i still haven't come up with better test
			"jne not_supported_sse95\n"
			"movl $1, %0\n"
			"jmp end_sse95\n"
			"not_supported_sse95:\n"
			"movl $0, %0\n"
			"end_sse95:\n"
			"popa" : "=m" (hasSSE3));
	}
  
}

void softgpu_sysinfo()
{
	version_win(&sysver);
	isNT = version_is_nt();
	
	static char buf[50];
	
	if(registryRead("HKLM\\SOFTWARE\\Microsoft\\DirectX\\Version", buf, 50))
	{
		version_parse(buf, &dxver);
	}
	
	HMODULE testLib = LoadLibraryA("msvcrt.dll");
	if(testLib != NULL)
	{
		hasCRT = TRUE;
		FreeLibrary(testLib);
	}
	
	testLib = LoadLibraryA("opengl32.dll");
	if(testLib != NULL)
	{
		hasOpengl = TRUE;
		FreeLibrary(testLib);
	}
	
	readCPUInfo();
	
	reinstall_dx = checkDXReinstall();
		
	sprintf(sysinfomsg,
		"System version: %d.%d.%d.%d\n"
		"Core type: %s\n"
		"DX version: %d.%d.%d.%d\n"
		"MSVCRT.dll: %s\n"
		"SETUPAPI.dll: %s\n"
		"opengl32.dll: %s\n"
		"SSE: %s\n"
		"AVX: %s\n",
		sysver.major, sysver.minor, sysver.patch, sysver.build,
		isNT ? "NT" : "9x",
		dxver.major, dxver.minor, dxver.patch, dxver.build,
		hasCRT ? "yes" : "no",
		hasSETUPAPI ? "yes" : "no",
		hasOpengl ? "yes" : "no",
		hasSSE42 ? "4.2" : (hasSSE3 ? "3" : "no"),
		hasAVX  ? "yes" : "no"
	);
}


HWND infobox = INVALID_HANDLE_VALUE;
HWND installbtn = INVALID_HANDLE_VALUE;
HWND pathinput = INVALID_HANDLE_VALUE;

static BOOL need_reboot = FALSE;

void softgpu_done(HWND hwnd)
{
	if(installbtn != INVALID_HANDLE_VALUE)
	{
		DWORD sty = GetWindowLongA(installbtn, GWL_STYLE) & (~WS_DISABLED);
		SetWindowLongA(installbtn, GWL_STYLE, sty);
	}
	
	SetWindowText(infobox, "SUCCESS!");
	
	int id = MessageBoxA(hwnd, "Installation complete! Reboot is required. Reboot now?", "Complete!", MB_YESNO | MB_ICONQUESTION);
	if(id == IDYES)
	{
		need_reboot = TRUE;
		PostQuitMessage(0);
	}
}

void softgpu_reset(HWND hwnd)
{
	(void)hwnd;
	
	if(installbtn != INVALID_HANDLE_VALUE)
	{
		DWORD sty = GetWindowLongA(installbtn, GWL_STYLE) & (~WS_DISABLED);
		SetWindowLongA(installbtn, GWL_STYLE, sty);
	}
	
	SetWindowText(infobox, "Installation failed!");
}

void softgpu_info(HWND hwnd, const char *part)
{
	(void)hwnd;
	char msgbuff[256];
	
	if(infobox != INVALID_HANDLE_VALUE)
	{
		strcpy(msgbuff, "Working: ");
		strcat(msgbuff, part);
		strcat(msgbuff, "...");
		
		SetWindowText(infobox, msgbuff);
	}
}

void softgpu_browse(HWND hwnd)
{
	static char bwpath[MAX_PATH] = "";
	
	BROWSEINFOA bi;
	memset(&bi, 0, sizeof(bi));
	bi.hwndOwner = hwnd;
	bi.pszDisplayName = bwpath;
	bi.lpszTitle = "Select install directory";
	bi.ulFlags = BIF_RETURNONLYFSDIRS;
	
	
	PIDLIST_ABSOLUTE pl = SHBrowseForFolderA(&bi);
	if(pl != NULL)
	{
		if(pathinput != INVALID_HANDLE_VALUE)
		{
			if(SHGetPathFromIDListA(pl, bwpath))
			{
				size_t bwpath_len = strlen(bwpath);
				if(bwpath_len)
				{
					if(bwpath[bwpath_len-1] != '\\')
					{
						strcat(bwpath, "\\");
					}
					strcat(bwpath, "softgpu");
					SetWindowTextA(pathinput, bwpath);
				}
			}
		}
	}
}

int GetInputInt(HWND hwnd, int id)
{
	HWND el = GetDlgItem(hwnd, id);
	if(el)
	{
		char inp[64];
		if(GetWindowTextA(el, inp, 64) != 0)
		{
			return strtol(inp, NULL, 0);
		}
	}
	
	return 0;
}

LRESULT CALLBACK softgpuWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg)
	{
		case WM_CREATE:
		{
			softgpu_window_create(hwnd, lParam);
			break;
		}
		case WM_COMMAND:
		{
			switch (wParam)
      {
				case BTN_INSTALL:
					if(installbtn != INVALID_HANDLE_VALUE)
					{
						DWORD sty = GetWindowLongA(installbtn, GWL_STYLE) | WS_DISABLED;
						SetWindowLongA(installbtn, GWL_STYLE, sty);
						
						writeSettings(hwnd);
					}
					
					setInstallPath(pathinput);
					
					/*
					if(IsDlgButtonChecked(hwnd, CHBX_IE))
					{
						action_create("IE installation", ie_start, proc_wait, setup_end);
					}*/
					
					if(IsDlgButtonChecked(hwnd, CHBX_GL95))
					{
						action_create("OpenGL95 install", gl95_start, NULL, NULL);
					}
					
					if(IsDlgButtonChecked(hwnd, CHBX_MSCRT))
					{
						action_create("MSVCRT installation", mscv_start, proc_wait, setup_end);
					}
					
					if(IsDlgButtonChecked(hwnd, CHBX_DX))
					{
						if(version_compare(&sysver, &WINVER98SE) >= 0)
						{
							setDXpath(iniValue("[softgpu]", "dx9path"));
						}
						else if(version_compare(&sysver, &WINVER98) >= 0)
						{
							setDXpath(iniValue("[softgpu]", "dx9path.fe"));
						}
						else
						{
							setDXpath(iniValue("[softgpu]", "dx8path"));
						}
						
						action_create("DX installation", dx_start, proc_wait, setup_end);
					}

#if 0
					if(IsDlgButtonChecked(hwnd, CHBX_IE))
					{
						if(version_compare(&sysver, &WINVER98) >= 0)
						{
							setIEpath(iniValue("[softgpu]", "ie98"));
						}
						else
						{
							setIEpath(iniValue("[softgpu]", "ie95"));
						}
						
						action_create("IE installation", ie_start, proc_wait, setup_end);
					}
#endif
					
					if(IsDlgButtonChecked(hwnd, RAD_SSE4))
					{
						setInstallSrc(iniValue("[softgpu]", "drvpath.sse4"));
					}
					else if(IsDlgButtonChecked(hwnd, RAD_SSE))
					{
						setInstallSrc(iniValue("[softgpu]", "drvpath.sse"));
					}
					else
					{
						setInstallSrc(iniValue("[softgpu]", "drvpath"));
					}
					
					install_settings.install_wine  = IsDlgButtonChecked(hwnd, CHBX_WINE);
					install_settings.install_glide = IsDlgButtonChecked(hwnd, CHBX_GLIDE);
					
					install_settings.install_res_qxga = IsDlgButtonChecked(hwnd, CHBX_QXGA);
					install_settings.install_res_1440 = IsDlgButtonChecked(hwnd, CHBX_1440);
					install_settings.install_res_4k   = IsDlgButtonChecked(hwnd, CHBX_4K);
					install_settings.install_res_5k   = IsDlgButtonChecked(hwnd, CHBX_5K);
					
					install_settings.bug_rgb565       = IsDlgButtonChecked(hwnd, CHBX_BUG_RGB565);
					install_settings.bug_prefer_fifo  = IsDlgButtonChecked(hwnd, CHBX_BUG_PREFER_FIFO);
					install_settings.bug_dx_flags     = IsDlgButtonChecked(hwnd, CHBX_BUG_DX_FLAGS);
					
					install_settings.dd_set_system    = IsDlgButtonChecked(hwnd, RAD_DD_HAL);
					install_settings.d8_set_nine      = IsDlgButtonChecked(hwnd, RAD_D8_NINE);
					install_settings.d9_set_nine      = IsDlgButtonChecked(hwnd, RAD_D9_NINE);
					install_settings.install_3dfx     = IsDlgButtonChecked(hwnd, CHBX_3DFX);
					
					install_settings.vram_limit = GetInputInt(hwnd, INP_VRAM_LIMIT);
					install_settings.gmr_limit  = GetInputInt(hwnd, INP_GMR_LIMIT);
					
					action_create("Files copy", filescopy_start, filescopy_wait, filescopy_result);
					
					if(install_settings.install_3dfx)
					{
						action_create("Voodoo files copy", voodoo_copy, NULL, NULL);
					}
					
					action_create("Modify inf", infFixer, NULL, NULL);
					
					if(hasSETUPAPI && version_compare(&sysver, &WINVER98) >= 0)
					{
						if(!IsDlgButtonChecked(hwnd, CHBX_NO_INSTALL))
						{
							action_create("Driver installation", driver_install, NULL, NULL);
						}
						else
						{
							action_create("Driver installation", driver_without_setupapi, NULL, NULL);
						}
					}
					else
					{
						action_create("Driver installation", driver_without_setupapi, NULL, NULL);
					}
					
					action_create("Configure wrappers", winenine, NULL, NULL);
					
					if(IsDlgButtonChecked(hwnd, CHBX_FIXES))
					{
						action_create("Compat. enhacements", apply_reg_fixes, NULL, NULL);
					}
					
					if(IsDlgButtonChecked(hwnd, CHBX_SIMD95))
					{
						action_create("SIMD95", simd95, NULL, NULL);
					}
					
					break;
				case BTN_EXIT:
					//writeSettings(hwnd);
					PostQuitMessage(0);
					break;
				case BTN_ABOUT:
					about(hwnd);
					break;
				case BTN_BROWSE:
					softgpu_browse(hwnd);
					break;
				case CHBX_SIMD95:
					if(IsDlgButtonChecked(hwnd, CHBX_SIMD95))
					{
						if(MessageBoxA(hwnd, "SIMD95 works well only in VirtualBox,\nin other cases, the system probably won't boot!\n\nAre you sure you want to this option set?", "WARNING!", MB_ICONWARNING|MB_YESNO) == IDNO)
						{
							CheckDlgButton(hwnd, CHBX_SIMD95, BST_UNCHECKED);
						}
					}
					break;
				case BTN_SYSINFO:
					sysinfo(hwnd);
					break;
				case BTN_WGLTEST:
					runprog("tools\\wgltest.exe");
					break;
				case BTN_GLCHECKER:
					runprog("tools\\glchecker.exe");
					break;
				case BTN_README:
				{
					char url[MAX_PATH] = "file:///";
					size_t len = strlen(url);
					size_t i;
					
					GetCurrentDirectory(MAX_PATH-len, url+len);
					len = strlen(url);
					for(i = 0; i < len; i++)
					{
						if(url[i] == '\\')
							url[i] = '/';
					}
					strcat(url, "/softgpu.htm");
					
					softgpu_browser(url);
					
					break;
				}
				case BTN_DEFAULTS:
					void settingsReset();
					softgpu_set(hwnd);
					break;
				
			} // switch(wParam)
			break;
		}
		case WM_TIMER:
		{
			if(wParam == TIMER_ID)
      {
      	KillTimer(hwnd, TIMER_ID);
				timer_proc(hwnd);
      }
			
			break;
		}
		case WM_DESTROY:
		{
			PostQuitMessage(0);
			break;
		}
  }
  
  return DefWindowProc(hwnd, msg, wParam, lParam);
}

/* Enumerating PCI buse take some time, so we show loading window */

LRESULT CALLBACK softgpuLoadingProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg)
	{
		case WM_CREATE:
		{
			softgpu_loading_create(hwnd, lParam);
			break;
		}
		case WM_DESTROY:
		{
			break;
		}
	}	
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

#ifdef EXTRA_INFO
#define WINDOW_TITLE "SoftGPU setup - " SOFTGPU_STR(EXTRA_INFO)
#else
#define WINDOW_TITLE "SoftGPU setup"
#endif

DWORD WINAPI softgpuLoadingThread(LPVOID lpParameter)
{
	MSG msg;
	(void)lpParameter;
	
	HWND win_loading = CreateWindowA(WND_SOFTGPU_LOAD_CLASS_NAME, WINDOW_TITLE, WS_TILED|WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, DPIX(400), DPIY(250), 0, 0, NULL, 0);
	
	while(GetMessage(&msg, NULL, 0, 0))
  {
  	if (IsDialogMessage(win_loading, &msg))
  	{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		
		if(msg.message == WM_DESTROY)
		{
			break;
		}
  }
  
  DestroyWindow(win_loading);
  
  return 0;
}

typedef BOOL (WINAPI * SetProcessDPIAware_f)();

void setHightDPI()
{
	SetProcessDPIAware_f SetProcessDPIAware_h = NULL;
	HMODULE hmod = GetModuleHandleA("user32.dll");
	if(hmod != NULL)
	{
		SetProcessDPIAware_h = (SetProcessDPIAware_f)GetProcAddress(hmod, "SetProcessDPIAware");
		
		if(SetProcessDPIAware_h != NULL)
		{
			SetProcessDPIAware_h();
		}
	}
}

int main(int argc, const char *argv[])
{
	MSG msg; 
	HINSTANCE hInst = GetModuleHandle(NULL);
  setHightDPI();
	
	(void)argc;
	(void)argv;
	
	WNDCLASS wc_loading;
	memset(&wc_loading, 0, sizeof(wc_loading));
	
	wc_loading.style         = CS_HREDRAW | CS_VREDRAW;
	wc_loading.lpfnWndProc   = softgpuLoadingProc;
	wc_loading.lpszClassName = WND_SOFTGPU_LOAD_CLASS_NAME;
	wc_loading.hbrBackground = GetSysColorBrush(COLOR_3DFACE);
	wc_loading.hCursor       = LoadCursor(0, IDC_WAIT);
	wc_loading.hIcon         = LoadIconA(hInst, MAKEINTRESOURCE(SOFTGPU_ICON1));
	wc_loading.hInstance     = hInst;
	RegisterClass(&wc_loading);
	
	DWORD threadId;
	HANDLE win_loading_th = CreateThread(NULL, 0, softgpuLoadingThread, NULL, 0, &threadId);
	
	readSettings();

  hasSETUPAPI = loadSETUAPI();
  if(hasSETUPAPI)
  {
  	drvPCstatus = checkInstallation();
  }
  
  softgpu_sysinfo();
    
  if(win_loading_th != INVALID_HANDLE_VALUE)
  {
		/* destroy loading window */
		PostThreadMessage(threadId, WM_DESTROY, 0, 0);
		
		/* wait loading win thread to close... */
		if(WaitForSingleObject(win_loading_th, 1000) == WAIT_TIMEOUT)
		{
			TerminateThread(win_loading_th, 0);
		}
		CloseHandle(win_loading_th);
	}

	// Register the window class.
	WNDCLASS wc;
	memset(&wc, 0, sizeof(wc));
	
	wc.style         = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc   = softgpuWndProc;
	wc.lpszClassName = WND_SOFTGPU_CLASS_NAME;
	wc.hbrBackground = GetSysColorBrush(COLOR_3DFACE);
	wc.hCursor       = LoadCursor(0, IDC_ARROW);
	wc.hIcon         = LoadIconA(hInst, MAKEINTRESOURCE(SOFTGPU_ICON1));
	wc.hInstance     = hInst;

	RegisterClass(&wc);
	
	if(!iniLoad("softgpu.ini"))
	{
		MessageBoxA(NULL, "Can't load configuration file softgpu.ini!", "Configuration error!", MB_ICONSTOP);
		return 1;
	}
	
	// Get DPI for non 96 dpi displays
	HDC hdc = GetDC(NULL);
	if (hdc)
	{
		rdpiX = GetDeviceCaps(hdc, LOGPIXELSX) / 96.0f;
		rdpiY = GetDeviceCaps(hdc, LOGPIXELSY) / 96.0f;
		ReleaseDC(NULL, hdc);
	}
	
	HWND win = CreateWindowA(WND_SOFTGPU_CLASS_NAME, WINDOW_TITLE, SOFTGPU_WIN_STYLE|WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, DPIX(600), DPIY(420), 0, 0, NULL, 0);
	
	if(isNT)
	{
		MessageBoxA(win, "Your system is NT-based (eg. NT, 2k, XP, ..., 11). But this driver package is only for Windows 9x = 95, 98 and Me.\n\nIt can't work on any modern system. Sorry.", "Only for 9x", MB_ICONSTOP);
	}
	else
	{
		if(version_compare(&sysver, &WINVER98) >= 0)
		{
			if(drvPCstatus != CHECK_DRV_OK)
			{
				if(drvPCstatus & CHECK_DRV_NOPCI)
				{
					MessageBoxA(win, "No working PCI bus found! Without PCI bus installer and driver can't work!\n\nIf you're seeing this on QEMU, check README to solve this problem:\nhttps://github.com/JHRobotics/softgpu#qemu", "No PCI bus", MB_OK | MB_ICONHAND);
				}
				else if(drvPCstatus & CHECK_DRV_LEGACY_VGA)
				{
					MessageBoxA(win, "Found non PCI VGA adapter! This is probably result of the late PCI bus driver installation.\n\nPlease, remove this adapter first!\nMore on README in QEMU section:\nhttps://github.com/JHRobotics/softgpu#qemu", "Legacy non-PCI VGA adapter", MB_OK | MB_ICONHAND);
				}
			}
		}
		else
		{
			MessageBoxA(win, "In Windows 95 isn't available automatic installation. This program copy and set driver and you must manually install it from Device manager.", "Windows 95", MB_OK);
		}
		
		timer_proc(win);
	}
	
	if(reinstall_dx)
	{
		MessageBoxA(win, "Some DirectX files were modified by previous SoftGPU installation or other wrapper. It is recommended to install DirectX redistributable again!", "DX redistributable", MB_OK);
	}
	
  while(GetMessage(&msg, NULL, 0, 0))
  {
  	/*if(IsDialogMessageA(win, &msg))
  	{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}*/
		IsDialogMessageA(win, &msg);
  }
  
  DestroyWindow(win);
  
  iniFree();
  
  freeSETUAPI();
  
  if(need_reboot)
  {
  	ExitWindowsEx(EWX_REBOOT | EWX_FORCE, SHTDN_REASON_MAJOR_HARDWARE | SHTDN_REASON_MINOR_INSTALLATION);
  }
  
  /* SHGetPathFromIDListA creates some co-threads, this kill them too */
  ExitProcess(0);
  return (int) msg.wParam;
}
