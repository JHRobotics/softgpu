/******************************************************************************
 * Copyright (c) 2024 Jaroslav Hensl                                          *
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

#define DRAW_START_X 16
#define DRAW_START_Y 10
#define LINE_HEIGHT 20
#define LINE_HL     10
#define LINE_QL      5
#define LINE_WIDTH  230
#define LINE_QWIDTH  80
#define LINE_HWIDTH  160

#define LINE_WIDTH2  350

#define DRAW_MOVE(_w, _h) if(draw_direction == 0) \
	{draw_y += _h;} \
	else \
	{draw_x += _w;}

#define CHECKBOX(id, w, h, text) CreateWindowA("BUTTON", text, WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, \
	DPIX(draw_x), DPIY(draw_y), DPIX(w), DPIY(h), \
	hwnd, (HMENU)id, ((LPCREATESTRUCT)lParam)->hInstance, NULL); \
	DRAW_MOVE(w, h)

#define RADIO(id, w, h, text, flags) CreateWindowA("BUTTON", text, (WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON) | (flags), \
	DPIX(draw_x), DPIY(draw_y), DPIX(w), DPIY(h), \
	hwnd, (HMENU)id, ((LPCREATESTRUCT)lParam)->hInstance, NULL); \
	DRAW_MOVE(w, h)

#define BUTTON(id, w, h, text, flags) CreateWindowA("BUTTON", text, (WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON) | (flags), \
	DPIX(draw_x), DPIY(draw_y), DPIX(w), DPIY(h), \
	hwnd, (HMENU)id, ((LPCREATESTRUCT)lParam)->hInstance, NULL); \
	DRAW_MOVE(w, h)

#define LABEL(id, w, h, text, flags) CreateWindowA("STATIC", text,	(WS_VISIBLE | WS_CHILD | SS_LEFT) | (flags), \
	DPIX(draw_x), DPIY(draw_y), DPIX(w), DPIY(h), \
	hwnd, (HMENU)id, ((LPCREATESTRUCT)lParam)->hInstance, NULL); \
	DRAW_MOVE(w, h)
	

#define INPUT(id, w, h, value, flags) CreateWindowA("EDIT", value,	(WS_VISIBLE | WS_CHILD | ES_LEFT | WS_BORDER | WS_GROUP) | (flags), \
	DPIX(draw_x), DPIY(draw_y), DPIX(w), DPIY(h), \
	hwnd, (HMENU)id, ((LPCREATESTRUCT)lParam)->hInstance, NULL); \
	DRAW_MOVE(w, h)

#define DROPDOWN(id, w, h, items_to_show, flags) CreateWindowA("COMBOBOX", "",	(WS_VISIBLE | WS_CHILD | WS_GROUP | CBS_DROPDOWNLIST | WS_OVERLAPPED) | (flags), \
	DPIX(draw_x), DPIY(draw_y), DPIX(w), DPIY(h*(items_to_show)), \
	hwnd, (HMENU)id, ((LPCREATESTRUCT)lParam)->hInstance, NULL); \
	DRAW_MOVE(w, h)

#define DRAW_DIRECTION_DOWN draw_direction = 0
#define DRAW_DIRECTION_RIGHT draw_direction = 1

#define VSPACE draw_y += LINE_QL

typedef UINT (WINAPI *GetDpiForWindowFunc)(HWND hwnd);

UINT GetDpiForWindowDynamic(HWND hwnd)
{
	GetDpiForWindowFunc GetDpiForWindowProc = NULL;
	HMODULE user32 = GetModuleHandleA("user32.dll");
	
	if(user32)
	{
		GetDpiForWindowProc = (GetDpiForWindowFunc)GetProcAddress(user32, "GetDpiForWindow");
	}
	
	if(GetDpiForWindowProc)
	{
		return GetDpiForWindowProc(hwnd);
	}
	
	return 0;
}

void softgpu_window_create(HWND hwnd, LPARAM lParam)
{
	static char dxname[250];
	
	int draw_x = 0;
	int draw_y = 0;
	int draw_direction = 0;
	int saved_y = 0;
	
	
	UINT dpi10 = GetDpiForWindowDynamic(hwnd);
	if(dpi10 != 0)
	{
		rdpiX = dpi10 / 96.0f;
		rdpiY = dpi10 / 96.0f;
	}
	
	if(version_compare(&sysver, &WINVER98) >= 0)
	{
		strcpy(dxname, "Install DirectX 9");
	}
	else
	{
		strcpy(dxname, "Install DirectX 8");
	}
	
	DRAW_DIRECTION_DOWN;
	
	draw_x = DRAW_START_X;
	draw_y = DRAW_START_Y;
	
	DRAW_DIRECTION_RIGHT;
	CHECKBOX(CHBX_GL95,         LINE_WIDTH/3, LINE_HEIGHT, "GL95");
	CHECKBOX(CHBX_DOTCOM,       LINE_WIDTH/3, LINE_HEIGHT, ".COM");
	CHECKBOX(CHBX_WS2,          LINE_WIDTH/3, LINE_HEIGHT, "WS2");
	
	draw_x = DRAW_START_X;
	draw_y += LINE_HEIGHT;
	DRAW_DIRECTION_DOWN;
	
	CHECKBOX(CHBX_MSCRT,        LINE_WIDTH, LINE_HEIGHT, "Install MSVCRT");
	CHECKBOX(CHBX_DX,           LINE_WIDTH, LINE_HEIGHT, dxname);
	
	VSPACE;
	
	RADIO(RAD_NORMAL,           LINE_WIDTH, LINE_HEIGHT, "Install MMX binaries",    WS_GROUP);
	RADIO(RAD_SSE,              LINE_WIDTH, LINE_HEIGHT, "Install SSE3 binaries",   0);
	RADIO(RAD_SSE4,             LINE_WIDTH, LINE_HEIGHT, "Install SSE4.2 binaries", 0);
	
	VSPACE;
	
	CHECKBOX(CHBX_WINE,         LINE_WIDTH, LINE_HEIGHT, "Install DirectX wrapper");
	CHECKBOX(CHBX_GLIDE,        LINE_WIDTH, LINE_HEIGHT, "Install Glide wrapper");
	CHECKBOX(CHBX_SIMD95,       LINE_WIDTH, LINE_HEIGHT, "Enable SSE/AVX hack");
	
	saved_y = draw_y;
	
	draw_x = DRAW_START_X+LINE_WIDTH;
	draw_y = DRAW_START_Y;
	
	RADIO(RAD_DD_HAL,          LINE_WIDTH+50, LINE_HEIGHT, "Native HAL for DX <= 7 (2D only, no 3D!)", WS_GROUP);
	RADIO(RAD_DD_WINE,         LINE_WIDTH, LINE_HEIGHT, "Wine for DX <= 7",    0);
	
	VSPACE;
	
	RADIO(RAD_D8_WINE,         LINE_WIDTH-30, LINE_HEIGHT, "Wine for DX 8",    WS_GROUP);
	RADIO(RAD_D8_NINE,         LINE_WIDTH-20, LINE_HEIGHT, "Nine for DX 8 (experimental)",    0);
	
	VSPACE;
	
	RADIO(RAD_D9_WINE,         LINE_WIDTH-30, LINE_HEIGHT, "Wine for DX 9",    WS_GROUP);
	RADIO(RAD_D9_NINE,         LINE_WIDTH-20, LINE_HEIGHT, "Nine for DX 9 (experimental)",    0);
	
	VSPACE;
	
	CHECKBOX(CHBX_3DFX,         LINE_WIDTH, LINE_HEIGHT, "Copy 3DFX files (vgl, splash)");
	CHECKBOX(CHBX_FIXES,        LINE_WIDTH, LINE_HEIGHT, "Apply compatibility fixes");

	draw_x = DRAW_START_X;
	draw_y += LINE_HEIGHT + LINE_QL;
	
	draw_x = DRAW_START_X+LINE_WIDTH*2;
	draw_y = DRAW_START_Y+LINE_HEIGHT*2;
	DRAW_DIRECTION_DOWN;
	
	// third column
	BUTTON(BTN_ABOUT,           LINE_QWIDTH, LINE_HEIGHT, "About",     0);
	VSPACE;
	BUTTON(BTN_README,          LINE_QWIDTH, LINE_HEIGHT, "Readme",    0);
	VSPACE;
	BUTTON(BTN_SYSINFO,         LINE_QWIDTH, LINE_HEIGHT, "Sys. info", 0);
	VSPACE;
	BUTTON(BTN_WGLTEST,         LINE_QWIDTH, LINE_HEIGHT, "WGL test",  0);
	VSPACE;
	BUTTON(BTN_GLCHECKER,       LINE_QWIDTH, LINE_HEIGHT, "GLChecker", 0);
	
	draw_x = DRAW_START_X;
	draw_y = saved_y;
		
	// Resolutions
	DRAW_DIRECTION_RIGHT;
	LABEL(0,                160, LINE_HEIGHT, "Resolutions > FullHD:", 0);
	CHECKBOX(CHBX_QXGA,     LINE_QWIDTH,   LINE_HEIGHT, "QXGA");
	CHECKBOX(CHBX_1440,     LINE_QWIDTH,   LINE_HEIGHT, "1440p");
	CHECKBOX(CHBX_4K,       LINE_QWIDTH,   LINE_HEIGHT, "4K");
	CHECKBOX(CHBX_5K,       LINE_QWIDTH,   LINE_HEIGHT, "5K");
	
	draw_x = DRAW_START_X;
	draw_y += LINE_HEIGHT;
	DRAW_DIRECTION_DOWN;
	
	// BUGS
	DRAW_DIRECTION_RIGHT;
	LABEL(0,                140, LINE_HEIGHT, "Hypervisor preset:", 0);
	
	// profiles
	DROPDOWN(LBX_PROFILE, LINE_WIDTH+LINE_QWIDTH, LINE_HEIGHT, 10, 0);
	
	HWND profile = GetDlgItem(hwnd, LBX_PROFILE);
	if(profile != NULL)
	{
		int n = iniSectionsCount("[profile]");
		for(int i = 0; i < n; i++)
	  {
			int pos = (int)SendMessageA(profile, CB_ADDSTRING, 0, (LPARAM)iniSectionsValueDef("[profile]", i, "name", "?"));
			SendMessageA(profile, CB_SETITEMDATA, pos, (LPARAM)i); 
		}

		SendMessage(profile, CB_SETCURSEL, (WPARAM)0, (LPARAM)0);
	}
	
	draw_x += 10;
	BUTTON(BTN_CUSTOM,     LINE_QWIDTH,   LINE_HEIGHT,  "Customize", 0);
	
	draw_x = DRAW_START_X;
	draw_y += LINE_HEIGHT + LINE_QL;
	
	LABEL(0,                120, LINE_HEIGHT, "Detected GPU:", 0);
	

	LABEL(0, LINE_WIDTH2, LINE_HEIGHT, msg_gpu_names, SS_NOPREFIX);
	draw_y += LINE_HEIGHT;
	draw_x = DRAW_START_X;
	
	LABEL(STATIC_GPUMSG, 560, LINE_HEIGHT*2, msg_gpu_status, SS_NOPREFIX);

	draw_x = DRAW_START_X;
	draw_y += LINE_QL;
	DRAW_DIRECTION_DOWN;

	// path and install
	draw_x = DRAW_START_X;
	draw_y += LINE_HEIGHT + LINE_HL;

	DRAW_DIRECTION_RIGHT;
	
	LABEL(0, 100, LINE_HEIGHT, "Install path:", 0);
	//draw_x += 20;
	pathinput = INPUT(INP_PATH, LINE_WIDTH*2-100-20, LINE_HEIGHT, iniValue("[softgpu]", "defpath"), WS_TABSTOP);
	draw_x += 20;
	BUTTON(BTN_BROWSE,     LINE_QWIDTH,   LINE_HEIGHT,  "Browse", WS_TABSTOP);
	
	draw_x = DRAW_START_X;
	draw_y += LINE_HEIGHT + LINE_QL;
	
	BUTTON(BTN_EXIT,     LINE_QWIDTH,   LINE_HEIGHT,  "Exit", WS_TABSTOP);
	draw_x += 20;
	CHECKBOX(CHBX_NO_INSTALL, LINE_WIDTH*2 - 2*20-LINE_QWIDTH, LINE_HEIGHT, "Don't install driver, only copy files");
	draw_x += 20;
	
	const char *int_button_text = "Install!";
	switch(installable_status())
	{
		case SETUP_COPY_ONLY:
			int_button_text = "Copy files!";
			break;
		case SETUP_INSTALL:
			int_button_text = "Start!";
			break;
	}
	
	installbtn = BUTTON(BTN_INSTALL, LINE_QWIDTH, LINE_HEIGHT*2+LINE_QL, int_button_text, WS_TABSTOP);
	SetFocus(installbtn);
	
	draw_x = DRAW_START_X;
	draw_y += LINE_HEIGHT + LINE_QL;
	
	BUTTON(BTN_DEFAULTS, LINE_QWIDTH, LINE_HEIGHT, "Defaults", 0);
	draw_x += 20;
	
	infobox = LABEL(0,  LINE_WIDTH*2 - 2*20 - LINE_QWIDTH, LINE_HEIGHT, "https://github.com/JHRobotics/softgpu", SS_CENTER);

	settingsApply(hwnd);
}

void softgpu_loading_create(HWND hwnd, LPARAM lParam)
{
	CreateWindowA("STATIC", "Inspecting system...\n\nPlease stand by!",
		WS_VISIBLE | WS_CHILD | SS_CENTER,
		DPIX(0), DPIY(80), DPIX(400), DPIY(LINE_HEIGHT*3),
		hwnd, (HMENU)0, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
}

void softgpu_browser(const char *url)
{
	ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOW);
}

#define CUST_WIDTH 250

void softgpu_cur_window_create(HWND hwnd, LPARAM lParam)
{
	int draw_x = 0;
	int draw_y = 0;
	int draw_direction = 0;
	
	DRAW_DIRECTION_RIGHT;
	draw_x = DRAW_START_X;
	draw_y = DRAW_START_Y;
	
	LABEL(0,             200, LINE_HEIGHT, "VRAM limit (MB): ", 0);
	INPUT(INP_VRAM_LIMIT, 50, LINE_HEIGHT, "", 0);
	
	draw_x = DRAW_START_X;
	draw_y += LINE_HEIGHT + LINE_HL;
	
	DRAW_DIRECTION_DOWN;
	
	LABEL(0, CUST_WIDTH, LINE_HEIGHT, "SVGA settings", 0);

	CHECKBOX(CHBX_DMA_TO_FB,           CUST_WIDTH,   LINE_HEIGHT, "DMA surface to framebuffer (VB)");
	CHECKBOX(CHBX_HWCURSOR,            CUST_WIDTH,   LINE_HEIGHT, "HW cursor (VMW, in VB bugged)");

	CHECKBOX(CHBX_BUG_DX_FLAGS,        CUST_WIDTH,   LINE_HEIGHT, "DX flags (VB <= 7.0.14)");
	CHECKBOX(CHBX_BUG_PREFER_FIFO,     CUST_WIDTH,   LINE_HEIGHT, "Prefer FIFO (VME)");
	CHECKBOX(CHBX_BUG_RGB565,          CUST_WIDTH,   LINE_HEIGHT, "RGB565 (VB < 7.0.10, < 6.1.46)");

	CHECKBOX(CHBX_MESA_DOWNGRADE,      CUST_WIDTH,   LINE_HEIGHT, "Downgrade Mesa3D (VMW vGPU9)");

	CHECKBOX(CHBX_NO_MULTISAMPLE,      CUST_WIDTH,   LINE_HEIGHT, "Disable multisample (VMW)");

	draw_y += LINE_HEIGHT;
	
	LABEL(0, CUST_WIDTH, LINE_HEIGHT, "Experimental settings", 0);
//	draw_y += LINE_HL;

	DRAW_DIRECTION_RIGHT;
	LABEL(0,             200, LINE_HEIGHT, "Async MOBs: ", 0);
	INPUT(INP_ASYNCMOBS,  50, LINE_HEIGHT, "", 0);
	draw_y += LINE_HEIGHT;
	draw_x = DRAW_START_X;
	DRAW_DIRECTION_DOWN;

	CHECKBOX(CHBX_GMR_CACHE,           CUST_WIDTH,   LINE_HEIGHT, "GMR cache (vGPU9, inefficient)");
	CHECKBOX(CHBX_SW_GAMMA,            CUST_WIDTH,   LINE_HEIGHT, "Enable gamma globaly (slow)");

	DRAW_DIRECTION_RIGHT;
	LABEL(0,             200, LINE_HEIGHT, "SVGA RAM limit (MB): ", 0);
	INPUT(IMP_SVGA_MEM_MAX,  50, LINE_HEIGHT, "", 0);
	draw_x = DRAW_START_X;
	draw_y += LINE_HEIGHT;
	DRAW_DIRECTION_DOWN;
	
	//LABEL(0,             200, LINE_HEIGHT, "VB = VirtualBox, VMW = WMware Workstation ", 0);

	settingsApply(hwnd);
}
