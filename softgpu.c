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

#include "winini.h"
#include "actions.h"

#include "windrv.h"
#include "winreg.h"

#include "actions.h"
#include "setuperr.h"
#include "softgpuver.h"

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

#define WND_SOFTGPU_CLASS_NAME "SoftGPUCLS"
#define WND_SOFTGPU_LOAD_CLASS_NAME "SoftGPULDCLS"

#define CHBX_MSCRT  1
#define CHBX_DX     2
#define INP_PATH    3
#define BTN_BROWSE  4
#define RAD_NORMAL  5
#define RAD_SSE     6
#define CHBX_WINE   7
#define CHBX_GLIDE  8
#define CHBX_SIMD95 9
#define CHBX_FIXES  10
//#define CHBX_IE     11

#define BTN_EXIT    11
#define BTN_INSTALL 12
#define CHBX_GL95   13
#define BTN_ABOUT   14

#define CHBX_QXGA   15
#define CHBX_1440   16
#define CHBX_4K     17
#define CHBX_5K     18

#define CHBX_NO_INSTALL 19

static float rdpiX = 1.0;
static float rdpiY = 1.0;

typedef struct _settings_item_t
{
	DWORD menu;
	DWORD pos;
	BOOL  negate;
} settings_item_t;

const settings_item_t settings[] = 
{
	{CHBX_MSCRT,   0, FALSE},
	{CHBX_DX,      1, FALSE},
	{RAD_NORMAL,   2, FALSE},
	{CHBX_WINE,    3, FALSE},
	{CHBX_GLIDE,   4, FALSE},
	{CHBX_SIMD95,  5, FALSE},
	{CHBX_FIXES,   6, FALSE},
	{BTN_ABOUT,    7, FALSE},
	{CHBX_GL95,    8, FALSE},
	{CHBX_QXGA,    9, TRUE},
	{CHBX_1440,   10, TRUE},
	{CHBX_4K,     11, TRUE},
	{CHBX_5K,     12, TRUE},
	{0, 0, FALSE}
};

DWORD settings_data = 0;

#define DPIX(_spx) ((int)(ceil((_spx)*rdpiX)))
#define DPIY(_spx) ((int)(ceil((_spx)*rdpiY)))

#define DEFAULT_INST_PATH "C:\\drivers\\softgpu"

#define DRAW_START_X 16
#define DRAW_START_Y 10
#define LINE_HEIGHT 20
#define LINE_HL     10
#define LINE_WIDTH  250
#define LINE_QWIDTH  80

#define LINE_WIDTH2  350

version_t WINVER95 = {4,0,0,0};
version_t WINVER98 = {4,10,0,0};
version_t WINVER98SE = {4,10,2222,0};
version_t WINVERME = {4,90,0,0};
version_t WINVER2K = {5,0,0,0};

static BOOL isNT    = FALSE;
version_t sysver = {0};
static version_t dxver  = {0};
static BOOL     hasCRT = FALSE;
static BOOL     hasSETUPAPI = FALSE;
static uint32_t hasSSE = 0;
static uint32_t hasAVX = 0;
static BOOL     hasOpengl = FALSE;
static DWORD    drvPCstatus = CHECK_DRV_OK;

static char sysinfomsg[1024];

static BOOL isSettingSet(DWORD menu)
{
	const settings_item_t *s;
	for(s = &settings[0]; s->menu != 0; s++)
	{
		if(s->menu == menu)
		{
			DWORD p = ((settings_data) >> s->pos) & 0x1;
			if(s->negate)
			{
				p = (~p) & 0x1;
			}
				
			if(p == 0)
			{
				return TRUE;
			}
			else
			{
				return FALSE;
			}
		}
	}
	
	return FALSE;
}

static void writeSettings(HWND hwnd)
{
	const settings_item_t *s;
	
	DWORD new_data = 0;
	
	for(s = &settings[0]; s->menu != 0; s++)
	{
		BOOL b = IsDlgButtonChecked(hwnd, s->menu);
		if(s->negate) b = !b;
		
		if(!b)
		{
			new_data |= 1 << s->pos;
		}
	}
	
	registryWriteDWORD("HKCU\\SOFTWARE\\SoftGPU\\setup", new_data);
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
			"xorl %%eax, %%eax\n"
			"cpuid\n"
			"cmpl $1, %%eax\n"             // does CPUID support eax = 1?
			"jb not_supported_sse\n"
			"movl $1, %%eax\n"
			"cpuid\n"
			"andl $0x06000000, %%edx\n"   // check 25 and 26 - SSE and SSE2
			"cmpl $0x06000000, %%edx\n" 
			"jne not_supported_sse\n"
			"andl $0x00180001, %%ecx\n"   // check 0, 19, 20 - SSE3 and SSE4.1 SSE4.2
			"cmpl $0x00180001, %%ecx\n"
			"jne not_supported_sse\n"
			"movl $1, %0\n"
			"jmp end_sse\n"
			"not_supported_sse:\n"
			"movl $0, %0\n"
			"end_sse:\n"
			"popa" : "=m" (hasSSE));
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
			"popa" : "=m" (hasSSE));
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
	
	//hasSSE2 = __builtin_cpu_supports("sse2") > 0;
	//hasAVX = __builtin_cpu_supports("avx") > 0;
	
	sprintf(sysinfomsg, "System informations:\n"
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
		hasSSE ? "yes" : "no",
		hasAVX  ? "yes" : "no"
	);
}


static HWND infobox = INVALID_HANDLE_VALUE;
static HWND installbtn = INVALID_HANDLE_VALUE;
static HWND pathinput = INVALID_HANDLE_VALUE;

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

LRESULT CALLBACK softgpuWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static char dxname[250];
	
	switch(msg)
	{
		case WM_CREATE:
		{
			version_t dxtarget;
			DWORD sty;
			
			if(version_compare(&sysver, &WINVER98) >= 0)
			{
				version_parse(iniValue("[softgpu]", "dx9target"), &dxtarget);
				strcpy(dxname, "Install DirectX 9");
			}
			else
			{
				version_parse(iniValue("[softgpu]", "dx8target"), &dxtarget);
				strcpy(dxname, "Install DirectX 8");
			}
			
			/*CreateWindowA("BUTTON", iename,
				WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
				DPIX(DRAW_START_X), DPIY(DRAW_START_Y + 0*LINE_HEIGHT), DPIX(LINE_WIDTH), DPIY(LINE_HEIGHT),
				hwnd, (HMENU)CHBX_IE, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
			if(!hasSETUPAPI)
			{
				CheckDlgButton(hwnd, CHBX_IE, BST_CHECKED);
			}*/
			
			HWND btnGL = CreateWindowA("BUTTON", "Install OpenGL 95",
				WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
				DPIX(DRAW_START_X), DPIY(DRAW_START_Y + 0*LINE_HEIGHT), DPIX(LINE_WIDTH), DPIY(LINE_HEIGHT),
				hwnd, (HMENU)CHBX_GL95, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
			if(version_compare(&sysver, &WINVER98) < 0)
			{
				if(!hasOpengl)
				{
					if(isSettingSet(CHBX_GL95))
					{
						CheckDlgButton(hwnd, CHBX_GL95, BST_CHECKED);
					}
				}
			}
			else
			{
				sty = GetWindowLongA(btnGL, GWL_STYLE) | WS_DISABLED;
				SetWindowLongA(btnGL, GWL_STYLE, sty);
			}
			
			CreateWindowA("BUTTON", "Install MSVCRT",
				WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX | WS_GROUP | WS_TABSTOP,
				DPIX(DRAW_START_X), DPIY(DRAW_START_Y + 1*LINE_HEIGHT), DPIX(LINE_WIDTH), DPIY(LINE_HEIGHT),
				hwnd, (HMENU)CHBX_MSCRT, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
			if(!hasCRT)
			{
				if(isSettingSet(CHBX_MSCRT))
				{
					CheckDlgButton(hwnd, CHBX_MSCRT, BST_CHECKED);
				}
			}
			
			/* directx checkbox */			
			CreateWindowA("BUTTON", dxname,
				WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
				DPIX(DRAW_START_X), DPIY(DRAW_START_Y + 2*LINE_HEIGHT), DPIX(LINE_WIDTH), DPIY(LINE_HEIGHT),
				hwnd, (HMENU)CHBX_DX, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
			if(version_compare(&dxver, &dxtarget) < 0)
			{
				if(isSettingSet(CHBX_DX))
				{
					CheckDlgButton(hwnd, CHBX_DX, BST_CHECKED);
				}
			}
			
			/* SSE/non SSE instalation type */
			CreateWindowA("BUTTON", "Install classical binaries",
				WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON | WS_GROUP,
				DPIX(DRAW_START_X), DPIY(DRAW_START_Y + LINE_HL + 3*LINE_HEIGHT), DPIX(LINE_WIDTH), DPIY(LINE_HEIGHT),
				hwnd, (HMENU)RAD_NORMAL, ((LPCREATESTRUCT)lParam)->hInstance, NULL);

			CreateWindowA("BUTTON", "Install SSE instrumented binaries",
				WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON,
				DPIX(DRAW_START_X), DPIY(DRAW_START_Y + LINE_HL + 4*LINE_HEIGHT), DPIX(LINE_WIDTH), DPIY(LINE_HEIGHT),
				hwnd, (HMENU)RAD_SSE, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
				
			if(version_compare(&sysver, &WINVER98) >= 0 && hasSSE)
			{
				CheckDlgButton(hwnd, RAD_SSE, BST_CHECKED);
			}
			else
			{
				CheckDlgButton(hwnd, RAD_NORMAL, BST_CHECKED);
			}
			
			CreateWindowA("BUTTON", "Install DirectX wrapper (Wine9x)",
				WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX | WS_GROUP | WS_TABSTOP,
				DPIX(DRAW_START_X), DPIY(DRAW_START_Y + LINE_HL + 5*LINE_HEIGHT), DPIX(LINE_WIDTH), DPIY(LINE_HEIGHT),
				hwnd, (HMENU)CHBX_WINE, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
			if(isSettingSet(CHBX_WINE))
			{
				CheckDlgButton(hwnd, CHBX_WINE, BST_CHECKED);
			}
			
			CreateWindowA("BUTTON", "Install Glide wrapper (OpenGlide9x)",
				WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
				DPIX(DRAW_START_X), DPIY(DRAW_START_Y + LINE_HL + 6*LINE_HEIGHT), DPIX(LINE_WIDTH), DPIY(LINE_HEIGHT),
				hwnd, (HMENU)CHBX_GLIDE, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
			if(isSettingSet(CHBX_GLIDE))
			{
				CheckDlgButton(hwnd, CHBX_GLIDE, BST_CHECKED);
			}
			
			HWND btnSIMD95 = CreateWindowA("BUTTON", "Enable SSE/AVX hack",
				WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
				DPIX(DRAW_START_X), DPIY(DRAW_START_Y + LINE_HL + 7*LINE_HEIGHT), DPIX(LINE_WIDTH), DPIY(LINE_HEIGHT),
				hwnd, (HMENU)CHBX_SIMD95, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
			if(version_compare(&sysver, &WINVERME) < 0)
			{
#if 0
				if(!hasAVX)
				{
					if(isSettingSet(CHBX_SIMD95))
					{
						CheckDlgButton(hwnd, CHBX_SIMD95, BST_CHECKED);
					}
				}
#endif
			}
			else
			{
				sty = GetWindowLongA(btnSIMD95, GWL_STYLE) | WS_DISABLED;
				SetWindowLongA(btnSIMD95, GWL_STYLE, sty);
			}
			
			CreateWindowA("BUTTON", "Apply compatibility enhacements",
				WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
				DPIX(DRAW_START_X), DPIY(DRAW_START_Y + LINE_HL + 8*LINE_HEIGHT), DPIX(LINE_WIDTH), DPIY(LINE_HEIGHT),
				hwnd, (HMENU)CHBX_FIXES, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
			if(isSettingSet(CHBX_FIXES))
			{
				CheckDlgButton(hwnd, CHBX_FIXES, BST_CHECKED);
			}
			
			CreateWindowA("STATIC", "Resolutions > FullHD:",
				WS_VISIBLE | WS_CHILD | SS_LEFT,
				DPIX(DRAW_START_X), DPIY(DRAW_START_Y + 10*LINE_HEIGHT), DPIX(LINE_QWIDTH*2), DPIY(LINE_HEIGHT),
				hwnd, (HMENU)0, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
			
			CreateWindowA("BUTTON", "QXGA",
				WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX | WS_GROUP | WS_TABSTOP,
				DPIX(DRAW_START_X+2*LINE_QWIDTH), DPIY(DRAW_START_Y + 10*LINE_HEIGHT), DPIX(LINE_QWIDTH), DPIY(LINE_HEIGHT),
				hwnd, (HMENU)CHBX_QXGA, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
			if(isSettingSet(CHBX_QXGA))
			{
				CheckDlgButton(hwnd, CHBX_QXGA, BST_CHECKED);
			}
			
			CreateWindowA("BUTTON", "1440p",
				WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
				DPIX(DRAW_START_X+3*LINE_QWIDTH), DPIY(DRAW_START_Y + 10*LINE_HEIGHT), DPIX(LINE_QWIDTH), DPIY(LINE_HEIGHT),
				hwnd, (HMENU)CHBX_1440, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
			if(isSettingSet(CHBX_1440))
			{
				CheckDlgButton(hwnd, CHBX_1440, BST_CHECKED);
			}
			
			CreateWindowA("BUTTON", "4K",
				WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
				DPIX(DRAW_START_X+4*LINE_QWIDTH), DPIY(DRAW_START_Y + 10*LINE_HEIGHT), DPIX(LINE_QWIDTH), DPIY(LINE_HEIGHT),
				hwnd, (HMENU)CHBX_4K, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
			if(isSettingSet(CHBX_4K))
			{
				CheckDlgButton(hwnd, CHBX_4K, BST_CHECKED);
			}
				
			CreateWindowA("BUTTON", "5K",
				WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
				DPIX(DRAW_START_X+5*LINE_QWIDTH), DPIY(DRAW_START_Y + 10*LINE_HEIGHT), DPIX(LINE_QWIDTH), DPIY(LINE_HEIGHT),
				hwnd, (HMENU)CHBX_5K, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
			if(isSettingSet(CHBX_5K))
			{
				CheckDlgButton(hwnd, CHBX_5K, BST_CHECKED);
			}
			
			CreateWindowA("STATIC", "Install path:",
				WS_VISIBLE | WS_CHILD | SS_LEFT | WS_GROUP,
				DPIX(DRAW_START_X), DPIY(DRAW_START_Y + 12*LINE_HEIGHT), DPIX(LINE_WIDTH), DPIY(LINE_HEIGHT),
				hwnd, (HMENU)0, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
			
			pathinput = CreateWindowA("EDIT", iniValue("[softgpu]", "defpath"),
				WS_VISIBLE | WS_CHILD | ES_LEFT | WS_BORDER | WS_GROUP | WS_TABSTOP,
				DPIX(DRAW_START_X), DPIY(DRAW_START_Y + 13*LINE_HEIGHT), DPIX(LINE_WIDTH2), DPIY(LINE_HEIGHT),
				hwnd, (HMENU)INP_PATH, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
			
			CreateWindowA("BUTTON", "Browse",
				WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | WS_GROUP,
				DPIX(DRAW_START_X+LINE_WIDTH2+10), DPIY(DRAW_START_Y + 13*LINE_HEIGHT), DPIX(80), DPIY(LINE_HEIGHT),
				hwnd, (HMENU)BTN_BROWSE, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
			
			CreateWindowA("BUTTON", "Exit",
				WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | WS_GROUP | WS_TABSTOP,
				DPIX(DRAW_START_X), DPIY(DRAW_START_Y + LINE_HL + 14*LINE_HEIGHT), DPIX(80), DPIY(LINE_HEIGHT),
				hwnd, (HMENU)BTN_EXIT, ((LPCREATESTRUCT)lParam)->hInstance, NULL);

			if(!isNT)
			{
				installbtn = CreateWindowA("BUTTON", "Install!",
					WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON | WS_GROUP | WS_TABSTOP,
					DPIX(DRAW_START_X+LINE_WIDTH2+10), DPIY(DRAW_START_Y + LINE_HL + 14*LINE_HEIGHT), DPIX(80), DPIY(LINE_HEIGHT),
					hwnd, (HMENU)BTN_INSTALL, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
				SetFocus(installbtn);
			}
			else
			{
				CreateWindowA("BUTTON", "Install!",
					WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON | WS_DISABLED,
					DPIX(DRAW_START_X+LINE_WIDTH2+10), DPIY(DRAW_START_Y + LINE_HL + 14*LINE_HEIGHT), DPIX(80), DPIY(LINE_HEIGHT),
					hwnd, (HMENU)0, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
			}
			
#ifdef EXTRA_INFO
			CreateWindowA("STATIC", EXTRA_INFO,
				WS_VISIBLE | WS_CHILD | SS_RIGHT | WS_DISABLED,
				DPIX(DRAW_START_X+LINE_WIDTH+10), DPIY(DRAW_START_Y-5 + 0*LINE_HEIGHT), DPIX(200), DPIY(LINE_HEIGHT),
				hwnd, (HMENU)0, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
#endif
			
			CreateWindowA("STATIC", sysinfomsg,
				WS_VISIBLE | WS_CHILD | SS_LEFT | WS_BORDER,
				DPIX(DRAW_START_X+LINE_WIDTH+10), DPIY(DRAW_START_Y + 1*LINE_HEIGHT), DPIX(200), DPIY(8*LINE_HEIGHT),
				hwnd, (HMENU)0, ((LPCREATESTRUCT)lParam)->hInstance, NULL);

			CreateWindowA("BUTTON", "About",
				WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
				DPIX(DRAW_START_X+LINE_WIDTH2+10), DPIY(DRAW_START_Y + LINE_HL + 11*LINE_HEIGHT), DPIX(80), DPIY(LINE_HEIGHT),
				hwnd, (HMENU)BTN_ABOUT, ((LPCREATESTRUCT)lParam)->hInstance, NULL);

			infobox = CreateWindowA("STATIC", "https://github.com/JHRobotics/softgpu",
				WS_VISIBLE | WS_CHILD | SS_CENTER,
				DPIX(DRAW_START_X+80+10), DPIY(DRAW_START_Y + LINE_HL +14*LINE_HEIGHT), DPIX(LINE_WIDTH2-80-10), DPIY(LINE_HEIGHT),
				hwnd, (HMENU)0, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
			
			if(version_compare(&sysver, &WINVER98) >= 0)
			{
				CreateWindowA("BUTTON", "Don't install driver, only copy files",
					WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
					DPIX(DRAW_START_X+80+10), DPIY(DRAW_START_Y + 16*LINE_HEIGHT), DPIX(LINE_WIDTH2-80-10), DPIY(LINE_HEIGHT),
					hwnd, (HMENU)CHBX_NO_INSTALL, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
			}
			else
			{
				CreateWindowA("BUTTON", "Don't install driver, only copy files",
					WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX | WS_DISABLED,
					DPIX(DRAW_START_X+80+10), DPIY(DRAW_START_Y + 16*LINE_HEIGHT), DPIX(LINE_WIDTH2-80-10), DPIY(LINE_HEIGHT),
					hwnd, (HMENU)CHBX_NO_INSTALL, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
				CheckDlgButton(hwnd, CHBX_NO_INSTALL, BST_CHECKED);
			}
			
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
					
					if(IsDlgButtonChecked(hwnd, RAD_SSE))
					{
						setInstallSrc(iniValue("[softgpu]", "drvpath.sse"));
					}
					else
					{
						setInstallSrc(iniValue("[softgpu]", "drvpath"));
					}
					
					install_wine  = IsDlgButtonChecked(hwnd, CHBX_WINE);
					install_glide = IsDlgButtonChecked(hwnd, CHBX_GLIDE);
					
					install_res_qxga = IsDlgButtonChecked(hwnd, CHBX_QXGA);
					install_res_1440 = IsDlgButtonChecked(hwnd, CHBX_1440);
					install_res_4k   = IsDlgButtonChecked(hwnd, CHBX_4K);
					install_res_5k   = IsDlgButtonChecked(hwnd, CHBX_5K);
					
					action_create("Files copy", filescopy_start, filescopy_wait, filescopy_result);
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
			CreateWindowA("STATIC", "Inspecting system,\nplease, stand by!",
				WS_VISIBLE | WS_CHILD | SS_CENTER,
				DPIX(0), DPIY(80), DPIX(400), DPIY(LINE_HEIGHT*2),
				hwnd, (HMENU)0, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
			break;
		}
		case WM_DESTROY:
		{
			break;
		}
	}	
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

DWORD WINAPI softgpuLoadingThread(LPVOID lpParameter)
{
	MSG msg;
	(void)lpParameter;
	
	HWND win_loading = CreateWindowA(WND_SOFTGPU_LOAD_CLASS_NAME, "SoftGPU setup", WS_TILED|WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, DPIX(400), DPIY(250), 0, 0, NULL, 0);
	
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
	
	RegisterClass(&wc_loading);
	
	DWORD threadId;
	HANDLE win_loading_th = CreateThread(NULL, 0, softgpuLoadingThread, NULL, 0, &threadId);
		
	registryReadDWORD("HKCU\\SOFTWARE\\SoftGPU\\setup", &settings_data);

  hasSETUPAPI = loadSETUAPI();
  if(hasSETUPAPI)
  {
  	drvPCstatus = checkInstallation();
  }
  
  softgpu_sysinfo();

	/* destroy loading window */
	PostThreadMessage(threadId, WM_DESTROY, 0, 0);
	
	/* wait loading win thread to close... */
	WaitForSingleObject(win_loading_th, INFINITE);
	CloseHandle(win_loading_th);

	// Register the window class.
	WNDCLASS wc;
	memset(&wc, 0, sizeof(wc));
	
	wc.style         = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc   = softgpuWndProc;
	wc.lpszClassName = WND_SOFTGPU_CLASS_NAME;
	wc.hbrBackground = GetSysColorBrush(COLOR_3DFACE);
	wc.hCursor       = LoadCursor(0, IDC_ARROW);
	wc.hIcon         = LoadIconA(hInst, MAKEINTRESOURCE(SOFTGPU_ICON1));

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
	
	HWND win = CreateWindowA(WND_SOFTGPU_CLASS_NAME, "SoftGPU setup", WS_OVERLAPPEDWINDOW|WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, DPIX(512), DPIY(390), 0, 0, NULL, 0);
	
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
					MessageBoxA(win, "No working PCI bus found! Without PCI bus installer and driver can't work!\n\nIf you're seeing this on QEMU, check REAME to solve this problem:\nhttps://github.com/JHRobotics/softgpu#qemu", "No PCI bus", MB_OK | MB_ICONHAND);
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
	
  while(GetMessage(&msg, NULL, 0, 0))
  {
  	if (IsDialogMessage(win, &msg))
  	{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
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
