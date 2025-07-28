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
		"Mesa9x (alt.): %s\n"
		"Wine9x: %s\n"
		"OpenGlide9x: %s\n"
		"SIMD95: %s\n\n"
		"more on: https://github.com/JHRobotics/softgpu\n",
		SOFTGPU_VERSION_STR,
		iniValueDef("[version]", "vmdisp9x", "-"),
		iniValueDef("[version]", "mesa9x_main", "-"),
		iniValueDef("[version]", "mesa9x_alt", "-"),
		iniValueDef("[version]", "wine9x", "-"),
		iniValueDef("[version]", "openglide9x", "-"),
		iniValueDef("[version]", "simd95", "-")
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
#define WND_SOFTGPU_CUR_CLASS_NAME "SoftGPUCURCLS"
#define WND_SOFTGPU_LOAD_CLASS_NAME "SoftGPULDCLS"

float rdpiX = 1.0;
float rdpiY = 1.0;

BOOL isNT    = FALSE;
version_t sysver = {0};
version_t dxver  = {0};
version_t dxtarget = {0};
BOOL     hasCRT = FALSE;
BOOL     hasSETUPAPI = FALSE;
uint32_t hasSSE3 = 0;
uint32_t hasSSE42 = 0;
uint32_t hasAVX = 0;
uint32_t hasCPUID = 0;
uint32_t hasP2    = 0;
BOOL     hasOpengl = FALSE;
BOOL     hasOle32  = FALSE;
BOOL     hasWS2    = FALSE;

BOOL     reinstall_dx = FALSE;

static HFONT font = NULL;

HWND win_main = NULL;
HWND win_cust  = NULL;

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
		//strcpy(syspath+syspath_len, "\\ddsys.dll");
		//if(is_wrapper(syspath, TRUE))
		return TRUE;
	}
	
	strcpy(syspath+syspath_len, "\\d3d8.dll");
	if(is_wrapper(syspath, FALSE))
	{
		//strcpy(syspath+syspath_len, "\\msd3d8.dll");
		//if(is_wrapper(syspath, TRUE))
		return TRUE;
	}
	
	if(version_compare(&sysver, &WINVER98) >= 0)
	{
		strcpy(syspath+syspath_len, "\\d3d9.dll");
		if(is_wrapper(syspath, FALSE))
		{
			//strcpy(syspath+syspath_len, "\\msd3d9.dll");
			//if(is_wrapper(syspath, TRUE))
			return TRUE;
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
		"pushf\n"                   // push eflags on the stack
		"pop  %%eax\n"              // pop them into eax
		"movl %%eax, %%ebx\n"       // save to ebx for restoring afterwards
		"xorl $0x200000, %%eax\n"   // toggle bit 21
		"push %%eax\n"              // push the toggled eflags
		"popf\n"                    // pop them back into eflags
		"pushf\n"                   // push eflags
		"pop %%eax\n"               // pop them back into eax
		"cmpl %%ebx, %%eax\n"       // see if bit 21 was reset
		"jz not_supported\n"
		"movl $1, %1\n"
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
		"popa" : "=m" (hasAVX), "=m" (hasCPUID));

  if(hasCPUID)
  {
		__asm(
			"pusha\n"
			"xorl %%eax, %%eax\n"
			"cpuid\n"
			"cmpl $1, %%eax\n"             // does CPUID support eax = 1?
			"jb end_p2\n"
			"movl $1, %%eax\n"
			"cpuid\n"
			"andl $0x01800000, %%edx\n"   // check MMX and FXSR
			"cmpl $0x01800000, %%edx\n" 
			"jne end_p2\n"
			"mov $1, %0\n"
			"end_p2:\n"
			"popa" : "=m" (hasP2));

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
}

void softgpu_sysinfo()
{
	version_win(&sysver);
	isNT = version_is_nt();
	
	static char buf[50];
	
	readCPUInfo();
	
	if(registryRead("HKLM\\SOFTWARE\\Microsoft\\DirectX\\Version", buf, 50))
	{
		version_parse(buf, &dxver);
	}
	
	if(version_compare(&sysver, &WINVER98) >= 0 && hasP2)
	{
		version_parse(iniValue("[softgpu]", "dx9target"), &dxtarget);
	}
	else
	{
		version_parse(iniValue("[softgpu]", "dx8target"), &dxtarget);
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
	
	/* ole32 is usually present, but for example "comcat.dll" not */
	testLib = LoadLibraryA("comcat.dll");
	if(testLib != NULL)
	{
		hasOle32 = TRUE;
		FreeLibrary(testLib);
	}
	
	testLib = LoadLibraryA("ws2_32.dll");
	if(testLib != NULL)
	{
		hasWS2 = TRUE;
		FreeLibrary(testLib);
	}
	
	reinstall_dx = checkDXReinstall();
		
	sprintf(sysinfomsg,
		"System version: %d.%d.%d.%d\n"
		"Core type: %s\n"
		"DX version: %d.%d.%d.%d\n"
		"MSVCRT.dll: %s\n"
		"SETUPAPI.dll: %s\n"
		"opengl32.dll: %s\n"
		"CPUID: %s\n"
		"MMX+FXSR: %s\n"
		"SSE: %s\n"
		"AVX: %s\n",
		sysver.major, sysver.minor, sysver.patch, sysver.build,
		isNT ? "NT" : "9x",
		dxver.major, dxver.minor, dxver.patch, dxver.build,
		hasCRT ? "yes" : "no",
		hasSETUPAPI ? "yes" : "no",
		hasOpengl ? "yes" : "no",
		hasCPUID ? "yes" : "no",
		hasP2    ? "Pentium II+" : "no",
		hasSSE42 ? "4.2" : (hasSSE3 ? "3" : "no"),
		hasAVX  ? "yes" : "no"
	);
}


HWND infobox = INVALID_HANDLE_VALUE;
HWND installbtn = INVALID_HANDLE_VALUE;
HWND pathinput = INVALID_HANDLE_VALUE;

static BOOL need_reboot = FALSE;

void softgpu_done(HWND hwnd, BOOL reboot)
{
	if(installbtn != INVALID_HANDLE_VALUE)
	{
		DWORD sty = GetWindowLongA(installbtn, GWL_STYLE) & (~WS_DISABLED);
		SetWindowLongA(installbtn, GWL_STYLE, sty);
	}
	
	SetWindowText(infobox, "SUCCESS!");
	
	if(reboot)
	{
		int id = MessageBoxA(hwnd, "Installation complete! Reboot is required. Reboot now?", "Complete!", MB_YESNO | MB_ICONQUESTION);
		if(id == IDYES)
		{
			need_reboot = TRUE;
			PostQuitMessage(0);
		}
	}
	else
	{
		int id = MessageBoxA(hwnd, "Driver files copied successfully! Close installer now?", "Complete!", MB_YESNO | MB_ICONQUESTION);
		if(id == IDYES)
		{
			PostQuitMessage(0);
		}
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

BOOL CALLBACK SetFontToChild(HWND child, LPARAM lParam)
{
	SendMessageA(child, WM_SETFONT, (WPARAM)lParam, 1);
	return TRUE;
}

LRESULT CALLBACK softgpuWndCurProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg)
	{
		case WM_CREATE:
		{
			softgpu_cur_window_create(hwnd, lParam);
			break;
		}
		case WM_CLOSE:
		{
			//LONG sty = GetWindowLongA(hwnd, GWL_STYLE) & (~WS_VISIBLE);
			//SetWindowLongA(win_cust, GWL_STYLE, sty);
			ShowWindow(win_cust, SW_HIDE);
			return 0;
		}
		case WM_SETFONT:
		{
			/* broadcast to all subwindows */
			EnumChildWindows(hwnd, SetFontToChild, (LPARAM)wParam /* the font */);
		}
	}
	
  return DefWindowProc(hwnd, msg, wParam, lParam);
}

static void process_install()
{
	setInstallPath(pathinput);
	
	if(isSettingSet(CHBX_GL95))
	{
		action_create("OpenGL95 installation", gl95_start, NULL, NULL);
	}

	if(isSettingSet(CHBX_DOTCOM))
	{
		action_create(".COM installation", dotcom_start, NULL, NULL);
	}

	if(isSettingSet(CHBX_WS2))
	{
		action_create("WS2 installation", ws2_start, NULL, NULL);
	}

	if(isSettingSet(CHBX_MSCRT))
	{
		action_create("MSVCRT installation", mscv_start, proc_wait, setup_end);
	}

	setTrayPath(iniValue("[softgpu]", "drvpath.mmx"));

	action_create("stop Tray3D", kill_tray, proc_wait, setup_end);

	if(isSettingSet(CHBX_DX))
	{
		if(version_compare(&sysver, &WINVER98SE) >= 0 && hasP2)
		{
			setDXpath(iniValue("[softgpu]", "dx9path"));
		}
		else if(version_compare(&sysver, &WINVER98) >= 0 && hasP2)
		{
			setDXpath(iniValue("[softgpu]", "dx9path.fe"));
		}
		else
		{
			setDXpath(iniValue("[softgpu]", "dx8path"));
		}
						
		action_create("DX installation", dx_start, proc_wait, setup_end);
	}

	if(isSettingSet(RAD_SSE4))
	{
		setInstallSrc(iniValue("[softgpu]", "drvpath.sse4"));
	}
	else if(isSettingSet(RAD_SSE))
	{
		setInstallSrc(iniValue("[softgpu]", "drvpath.sse3"));
	}
	else
	{
		setInstallSrc(iniValue("[softgpu]", "drvpath.mmx"));
	}
					
	action_create("Files copy", filescopy_start, filescopy_wait, filescopy_result);
					
	if(isSettingSet(CHBX_3DFX))
	{
		action_create("Voodoo files copy", voodoo_copy, NULL, NULL);
	}

	action_create("Modify inf", infFixer, NULL, NULL);
	action_create("Configure wrappers", set_inf_regs, NULL, NULL);

	if(isSettingSet(CHBX_FIXES))
	{
		action_create("Compat. enhacements", apply_reg_fixes, NULL, NULL);
	}

	if(isSettingSet(CHBX_SIMD95))
	{
		action_create("SIMD95", simd95, NULL, NULL);
	}	

	if(installable_status() == SETUP_INSTALL_DRIVER && !isSettingSet(CHBX_NO_INSTALL))
	{
		action_create("Driver installation", driver_install, NULL, NULL);
	}
	else
	{
		action_create("Driver installation", driver_copy, NULL, NULL);
	}
}

LRESULT CALLBACK softgpuWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	//printf("event: %u\n", msg);
	
	switch(msg)
	{
		case WM_NCCREATE:
		{
			break;
		}
		case WM_CREATE:
		{
			softgpu_window_create(hwnd, lParam);
			break;
		}
		case WM_COMMAND:
		{
			switch(LOWORD(wParam))
      {
				case BTN_INSTALL:
					settingsReadback(win_main);
					settingsReadback(win_cust);
					
					if(installbtn != INVALID_HANDLE_VALUE)
					{
						DWORD sty = GetWindowLongA(installbtn, GWL_STYLE) | WS_DISABLED;
						SetWindowLongA(installbtn, GWL_STYLE, sty);
						
						writeSettings();
					}
					process_install();
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
					size_t len;
					size_t i;
					
					if(version_compare(&sysver, &WINVER98) < 0)
					{
						strcpy(url, "file:");
					}
					len = strlen(url);
					
					GetCurrentDirectory(MAX_PATH-len, url+len);
					len = strlen(url);
					for(i = 0; i < len; i++)
					{
						if(url[i] == '\\')
							url[i] = '/';
					}
					
					if(url[i-1] == '/' || url[i-1] == '\\')
					{
						url[i-1] = '\0';
					}
					
					strcat(url, "/softgpu.htm");
					
					softgpu_browser(url);
					
					break;
				}
				case BTN_DEFAULTS:
					settingsReset();
					settingsApply(win_main);
					settingsApply(win_cust);
					//softgpu_set(hwnd);
					break;
				case BTN_CUSTOM:
				{
					ShowWindow(win_cust, SW_NORMAL);
					SetForegroundWindow(win_cust);
					break;
				}
				case LBX_PROFILE:
					if(HIWORD(wParam) == CBN_SELCHANGE)
					{
            int ItemIndex = SendMessage((HWND) lParam, (UINT) CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
            printf("selected: %d\n", ItemIndex);
            settingsReadback(win_main);
            settingsReadback(win_cust);
            settingsApplyProfile(ItemIndex);
            settingsApply(win_main);
            settingsApply(win_cust);
            
            //MessageBox(hwnd, (LPCWSTR) ListItem, TEXT("Item Selected"), MB_OK); 
            return 0;
					}
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
		case WM_SETFONT:
		{
			/* broadcast to all subwindows */
			EnumChildWindows(hwnd, SetFontToChild, (LPARAM)wParam /* the font */);
		}
		break;
		case WM_CTLCOLORSTATIC:
		{
			if((HWND)lParam == GetDlgItem(hwnd, STATIC_GPUMSG)) 
			{
				switch(detection_status())
				{
					case DETECT_OK:
						SetTextColor((HDC)wParam, RGB(0x00,0x80,0x00));
     				SetBkMode((HDC)wParam, TRANSPARENT);
    				return GetClassLong(hwnd, GCL_HBRBACKGROUND);
					case DETECT_WARN:
						SetTextColor((HDC)wParam, RGB(255,128,64));
     				SetBkMode((HDC)wParam, TRANSPARENT);
    				return GetClassLong(hwnd, GCL_HBRBACKGROUND);
					case DETECT_ERR:
						SetTextColor((HDC)wParam, RGB(255, 0, 0));
     				SetBkMode((HDC)wParam, TRANSPARENT);
    				return GetClassLong(hwnd, GCL_HBRBACKGROUND);
				}
			}
		}
		break;
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
	
	if(!iniLoad("softgpu.ini"))
	{
		MessageBoxA(NULL, "Can't load configuration file softgpu.ini!", "Configuration error!", MB_ICONSTOP);
		return 1;
	}
	
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
	
  hasSETUPAPI = loadSETUAPI();
  if(hasSETUPAPI)
  {
  	checkInstallation();
  }
  
  /* load system informations */
  softgpu_sysinfo();
  check_SW_HW();
    
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
	WNDCLASS wc, wc2;
	memset(&wc,  0, sizeof(WNDCLASS));
	memset(&wc2, 0, sizeof(WNDCLASS));
	
	wc.style         = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc   = softgpuWndProc;
	wc.lpszClassName = WND_SOFTGPU_CLASS_NAME;
	wc.hbrBackground = GetSysColorBrush(COLOR_3DFACE);
	wc.hCursor       = LoadCursor(0, IDC_ARROW);
	wc.hIcon         = LoadIconA(hInst, MAKEINTRESOURCE(SOFTGPU_ICON1));
	wc.hInstance     = hInst;

	RegisterClass(&wc);
	
	wc2.style         = CS_HREDRAW | CS_VREDRAW;
	wc2.lpfnWndProc   = softgpuWndCurProc;
	wc2.lpszClassName = WND_SOFTGPU_CUR_CLASS_NAME;
	wc2.hbrBackground = GetSysColorBrush(COLOR_3DFACE);
	wc2.hCursor       = LoadCursor(0, IDC_ARROW);
	wc2.hIcon         = LoadIconA(hInst, MAKEINTRESOURCE(SOFTGPU_ICON1));
	wc2.hInstance     = hInst;

	RegisterClass(&wc2);
	
	// Get DPI for non 96 dpi displays
	HDC hdc = GetDC(NULL);
	if (hdc)
	{
		rdpiX = GetDeviceCaps(hdc, LOGPIXELSX) / 96.0f;
		rdpiY = GetDeviceCaps(hdc, LOGPIXELSY) / 96.0f;
		ReleaseDC(NULL, hdc);
	}
	
	/* init and calculate settings  */
	settingsReset();
	
	font = CreateFontA(DPIY(16), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, NULL);

	win_main = CreateWindowA(WND_SOFTGPU_CLASS_NAME, WINDOW_TITLE, SOFTGPU_WIN_STYLE|WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, DPIX(600), DPIY(420), 0, 0, NULL, 0);
	win_cust = CreateWindowA(WND_SOFTGPU_CUR_CLASS_NAME, WINDOW_TITLE, SOFTGPU_WIN_STYLE|WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, DPIX(300), DPIY(400), 0, 0, NULL, 0);

	if(font)
	{
		SendMessageA(win_main, WM_SETFONT, (WPARAM)font, 1);
		SendMessageA(win_cust, WM_SETFONT, (WPARAM)font, 1);
	}
	
	ShowWindow(win_cust, SW_HIDE);
	
	SetForegroundWindow(win_main);
	
	if(iniValue("[softgpu]", "warn") != NULL)
	{
		MessageBoxA(win_main, iniValue("[softgpu]", "warn"), "Warning", MB_ICONWARNING);
	}
	
	timer_proc(win_main);
	
	if(reinstall_dx)
	{
		MessageBoxA(win_main, "Some DirectX files were modified by previous SoftGPU installation or other wrapper. It is recommended to install DirectX redistributable again!", "DX redistributable", MB_OK);
	}
	
  while(GetMessage(&msg, NULL, 0, 0))
  {
  	if(!IsDialogMessageA(win_main, &msg))
  	{
  		if(!IsDialogMessageA(win_cust, &msg))
  		{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
  }
  
  DestroyWindow(win_cust);
  DestroyWindow(win_main);
  
  settingsFree();
  iniFree();
  freeSETUAPI();
  
  if(need_reboot)
  {
  	ExitWindowsEx(EWX_REBOOT, SHTDN_REASON_MAJOR_HARDWARE | SHTDN_REASON_MINOR_INSTALLATION);
  }
  
  /* SHGetPathFromIDListA creates some co-threads, this kill them too */
  ExitProcess(0);
  return (int) msg.wParam;
}
