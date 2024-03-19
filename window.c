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

version_t dxtarget;

void softgpu_set(HWND hwnd)
{
	DWORD sty;
	
	/* opengl 95 */
	if(version_compare(&sysver, &WINVER98) < 0)
	{
		if(!hasOpengl && isSettingSet(CHBX_GL95))
		{
			CheckDlgButton(hwnd, CHBX_GL95, BST_CHECKED);
		}
		else
		{
			CheckDlgButton(hwnd, CHBX_GL95, BST_UNCHECKED);
		}
	}
	else
	{
		HWND btnGL = GetDlgItem(hwnd, CHBX_GL95);
		if(btnGL)
		{
			sty = GetWindowLongA(btnGL, GWL_STYLE) | WS_DISABLED;
			SetWindowLongA(btnGL, GWL_STYLE, sty);
		}
	}
	
	/* CRT */
	if(!hasCRT && isSettingSet(CHBX_MSCRT))
	{
		CheckDlgButton(hwnd, CHBX_MSCRT, BST_CHECKED);
	}
	else
	{
		CheckDlgButton(hwnd, CHBX_MSCRT, BST_UNCHECKED);
	}
	
	/* DX redistrib. */
	CheckDlgButton(hwnd, CHBX_DX, BST_UNCHECKED);
	if(version_compare(&dxver, &dxtarget) < 0 || reinstall_dx)
	{
		if(isSettingSet(CHBX_DX))
			CheckDlgButton(hwnd, CHBX_DX, BST_CHECKED);
	}
	
	/* reset items */
	CheckDlgButton(hwnd, RAD_NORMAL, BST_UNCHECKED);
	CheckDlgButton(hwnd, RAD_SSE,    BST_UNCHECKED);
	CheckDlgButton(hwnd, RAD_SSE4,   BST_UNCHECKED);
	
	/* MMX/SSE3/SSE4.2 */
	const char *drv_sse4 = iniValue("[softgpu]", "drvpath.sse4");
	int selected_sse = 0;
	if(drv_sse4 != NULL
		&& is_dir(drv_sse4)
		&& version_compare(&sysver, &WINVER98) >= 0
		&& hasSSE42)
	{
		selected_sse = 4;
	}
	else if(version_compare(&sysver, &WINVER98) >= 0 && hasSSE3)
	{
		selected_sse = 3;
	}
	
	if(drv_sse4 == NULL || !is_dir(drv_sse4))
	{
		HWND radioSSE4 = GetDlgItem(hwnd, RAD_SSE4);
		if(radioSSE4)
		{
			sty = GetWindowLongA(radioSSE4, GWL_STYLE) | WS_DISABLED;
			SetWindowLongA(radioSSE4, GWL_STYLE, sty);
		}
	}
	
	switch(selected_sse)
	{
		case 3:
			CheckDlgButton(hwnd, RAD_SSE, BST_CHECKED);
			break;
		case 4:
			CheckDlgButton(hwnd, RAD_SSE4, BST_CHECKED);
			break;
		default:
			CheckDlgButton(hwnd, RAD_NORMAL, BST_CHECKED);
			break;
	}
	
	/* Wine */
	if(isSettingSet(CHBX_WINE))
		CheckDlgButton(hwnd, CHBX_WINE, BST_CHECKED);
	else
		CheckDlgButton(hwnd, CHBX_WINE, BST_UNCHECKED);
	
	/* Glide */
	if(isSettingSet(CHBX_GLIDE))
		CheckDlgButton(hwnd, CHBX_GLIDE, BST_CHECKED);
	else
		CheckDlgButton(hwnd, CHBX_GLIDE, BST_UNCHECKED);
	
	/* SIMD95 */
	if(version_compare(&sysver, &WINVERME) < 0)
	{
		if(!hasAVX)
		{
			if(isSettingSet(CHBX_SIMD95))
			{
				CheckDlgButton(hwnd, CHBX_SIMD95, BST_CHECKED);
			}
			else
			{
				CheckDlgButton(hwnd, CHBX_SIMD95, BST_UNCHECKED);
			}
		}
	}
	else
	{
		HWND btnSIMD95 = GetDlgItem(hwnd, CHBX_SIMD95);
		if(btnSIMD95)
		{
			sty = GetWindowLongA(btnSIMD95, GWL_STYLE) | WS_DISABLED;
			SetWindowLongA(btnSIMD95, GWL_STYLE, sty);
		}
	}
	
	/* fixes */
	if(isSettingSet(CHBX_FIXES))
		CheckDlgButton(hwnd, CHBX_FIXES, BST_CHECKED);
	else
		CheckDlgButton(hwnd, CHBX_FIXES, BST_UNCHECKED);
	
	/* reset */
	CheckDlgButton(hwnd, RAD_DD_HAL,  BST_UNCHECKED);
	CheckDlgButton(hwnd, RAD_DD_WINE, BST_UNCHECKED);
	CheckDlgButton(hwnd, RAD_D8_WINE, BST_UNCHECKED);
	CheckDlgButton(hwnd, RAD_D8_NINE, BST_UNCHECKED);
	CheckDlgButton(hwnd, RAD_D9_NINE, BST_UNCHECKED);
	CheckDlgButton(hwnd, RAD_D9_WINE, BST_UNCHECKED);
	
	// wine vs nine
	if(isSettingSet(RAD_DD_HAL))
		CheckDlgButton(hwnd, RAD_DD_HAL, BST_CHECKED);
	else
		CheckDlgButton(hwnd, RAD_DD_WINE, BST_CHECKED);
	
	if(isSettingSet(RAD_D8_NINE))
		CheckDlgButton(hwnd, RAD_D8_NINE, BST_CHECKED);
	else
		CheckDlgButton(hwnd, RAD_D8_WINE, BST_CHECKED);
	
	if(isSettingSet(RAD_D9_NINE))
		CheckDlgButton(hwnd, RAD_D9_NINE, BST_CHECKED);
	else
		CheckDlgButton(hwnd, RAD_D9_WINE, BST_CHECKED);
	
	// BUGS
	if(isSettingSet(CHBX_BUG_PREFER_FIFO))
		CheckDlgButton(hwnd, CHBX_BUG_PREFER_FIFO, BST_CHECKED);
	else
		CheckDlgButton(hwnd, CHBX_BUG_PREFER_FIFO, BST_UNCHECKED);
	
	if(isSettingSet(CHBX_BUG_RGB565))
		CheckDlgButton(hwnd, CHBX_BUG_RGB565, BST_CHECKED);
	else
		CheckDlgButton(hwnd, CHBX_BUG_RGB565, BST_UNCHECKED);
	
	if(isSettingSet(CHBX_BUG_DX_FLAGS))
		CheckDlgButton(hwnd, CHBX_BUG_DX_FLAGS, BST_CHECKED);
	else
		CheckDlgButton(hwnd, CHBX_BUG_DX_FLAGS, BST_UNCHECKED);

	// extra resolutions
	if(isSettingSet(CHBX_QXGA))
		CheckDlgButton(hwnd, CHBX_QXGA, BST_CHECKED);
	else
		CheckDlgButton(hwnd, CHBX_QXGA, BST_UNCHECKED);
	
	if(isSettingSet(CHBX_1440))
		CheckDlgButton(hwnd, CHBX_1440, BST_CHECKED);
	else
		CheckDlgButton(hwnd, CHBX_1440, BST_UNCHECKED);

	if(isSettingSet(CHBX_4K))
		CheckDlgButton(hwnd, CHBX_4K, BST_CHECKED);
	else
		CheckDlgButton(hwnd, CHBX_4K, BST_UNCHECKED);
	
	if(isSettingSet(CHBX_5K))
		CheckDlgButton(hwnd, CHBX_5K, BST_CHECKED);
	else
		CheckDlgButton(hwnd, CHBX_5K, BST_UNCHECKED);
	
	if(version_compare(&sysver, &WINVER98) < 0)
	{
		HWND btnNoInstall = GetDlgItem(hwnd, CHBX_NO_INSTALL);
		if(btnNoInstall)
		{
			sty = GetWindowLongA(btnNoInstall, GWL_STYLE) | WS_DISABLED;
			SetWindowLongA(btnNoInstall, GWL_STYLE, sty);
		}
		CheckDlgButton(hwnd, CHBX_NO_INSTALL, BST_CHECKED);
	}
	
	char tmplabel[64];
	HWND inp_vram = GetDlgItem(hwnd, INP_VRAM_LIMIT);
	if(inp_vram)
	{
		sprintf(tmplabel, "%d", intSettings(SETTINGS_VRAM));
		SetWindowTextA(inp_vram, tmplabel);
	}
	
	HWND inp_gmr = GetDlgItem(hwnd, INP_GMR_LIMIT);
	if(inp_gmr)
	{
		sprintf(tmplabel, "%d", intSettings(SETTINGS_GMR));
		SetWindowTextA(inp_gmr, tmplabel);
	}
	
	
}

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

#define LABEL(id, w, h, text) CreateWindowA("STATIC", text,	WS_VISIBLE | WS_CHILD | SS_LEFT, \
	DPIX(draw_x), DPIY(draw_y), DPIX(w), DPIY(h), \
	hwnd, (HMENU)id, ((LPCREATESTRUCT)lParam)->hInstance, NULL); \
	DRAW_MOVE(w, h)
	
#define LABEL_CENTER(id, w, h, text) CreateWindowA("STATIC", text,	WS_VISIBLE | WS_CHILD | SS_CENTER, \
	DPIX(draw_x), DPIY(draw_y), DPIX(w), DPIY(h), \
	hwnd, (HMENU)id, ((LPCREATESTRUCT)lParam)->hInstance, NULL); \
	DRAW_MOVE(w, h)

#define INPUT(id, w, h, value, flags) CreateWindowA("EDIT", value,	(WS_VISIBLE | WS_CHILD | ES_LEFT | WS_BORDER | WS_GROUP) | (flags), \
	DPIX(draw_x), DPIY(draw_y), DPIX(w), DPIY(h), \
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
		version_parse(iniValue("[softgpu]", "dx9target"), &dxtarget);
		strcpy(dxname, "Install DirectX 9");
	}
	else
	{
		version_parse(iniValue("[softgpu]", "dx8target"), &dxtarget);
		strcpy(dxname, "Install DirectX 8");
	}
	
	DRAW_DIRECTION_DOWN;
	
	draw_x = DRAW_START_X;
	draw_y = DRAW_START_Y;
	
	CHECKBOX(CHBX_GL95,         LINE_WIDTH, LINE_HEIGHT, "Install OpenGL 95");
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
	CHECKBOX(CHBX_FIXES,        LINE_WIDTH, LINE_HEIGHT, "Apply compatibility fixes");
	
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
	
	DRAW_DIRECTION_RIGHT;
	LABEL(0,             120, LINE_HEIGHT, "VRAM limit (MB): ");
	INPUT(INP_VRAM_LIMIT, 50, LINE_HEIGHT, "", 0)
	
	draw_x = DRAW_START_X+LINE_WIDTH;
	draw_y += LINE_HEIGHT + LINE_HL;
	
	LABEL(0,            120, LINE_HEIGHT, "GMR limit (MB): ");
	INPUT(INP_GMR_LIMIT, 50, LINE_HEIGHT, "", 0)
	
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
	draw_y = saved_y+LINE_HL;
	
	// BUGS
	DRAW_DIRECTION_RIGHT;
	LABEL(0,                160, LINE_HEIGHT, "HV bugs override:");

	CHECKBOX(CHBX_BUG_DX_FLAGS,        LINE_HWIDTH+40,   LINE_HEIGHT, "DX flags (VBox <= 7.0.14)");
	CHECKBOX(CHBX_BUG_PREFER_FIFO,     LINE_HWIDTH+20,   LINE_HEIGHT, "Prefer FIFO (VMware)");
	
	draw_y += LINE_HEIGHT;
	draw_x = DRAW_START_X + 160;
	
	CHECKBOX(CHBX_BUG_RGB565,          LINE_WIDTH2,   LINE_HEIGHT, "RGB565 (VBox < 7.0.10, VBox < 6.1.46)");
	
	draw_x = DRAW_START_X;
	draw_y += LINE_HEIGHT + LINE_QL;
	DRAW_DIRECTION_DOWN;
	
	// Resolutions
	DRAW_DIRECTION_RIGHT;
	LABEL(0,                160, LINE_HEIGHT, "Resolutions > FullHD:");
	CHECKBOX(CHBX_QXGA,     LINE_QWIDTH,   LINE_HEIGHT, "QXGA");
	CHECKBOX(CHBX_1440,     LINE_QWIDTH,   LINE_HEIGHT, "1440p");
	CHECKBOX(CHBX_4K,       LINE_QWIDTH,   LINE_HEIGHT, "4K");
	CHECKBOX(CHBX_5K,       LINE_QWIDTH,   LINE_HEIGHT, "5K");
	
	// path and install
	draw_x = DRAW_START_X;
	draw_y += LINE_HEIGHT + LINE_HL;

	DRAW_DIRECTION_RIGHT;
	
	LABEL(0, 100, LINE_HEIGHT, "Install path:");
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
	
	if(!isNT)
	{
		installbtn = BUTTON(BTN_INSTALL, LINE_QWIDTH, LINE_HEIGHT*2+LINE_QL, "Install!", WS_TABSTOP);
		SetFocus(installbtn);
	}
	else
	{
		BUTTON(BTN_INSTALL, LINE_QWIDTH, LINE_HEIGHT*2+LINE_QL, "Install!", WS_DISABLED);
	}
	
	draw_x = DRAW_START_X;
	draw_y += LINE_HEIGHT + LINE_QL;
	
	BUTTON(BTN_DEFAULTS, LINE_QWIDTH, LINE_HEIGHT, "Defaults", 0);
	draw_x += 20;
	
	infobox = LABEL_CENTER(0,  LINE_WIDTH*2 - 2*20 - LINE_QWIDTH, LINE_HEIGHT, "https://github.com/JHRobotics/softgpu");
	
#if 0 // EXTRA_INFO
	CreateWindowA("STATIC", EXTRA_INFO,
		WS_VISIBLE | WS_CHILD | SS_RIGHT | WS_DISABLED,
		DPIX(DRAW_START_X+LINE_WIDTH+10), DPIY(DRAW_START_Y-5 + 0*LINE_HEIGHT), DPIX(200), DPIY(LINE_HEIGHT),
		hwnd, (HMENU)0, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
#endif

	softgpu_set(hwnd);
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
