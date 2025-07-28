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
#ifndef __SOFTGPU_H__INCLUDED__
#define __SOFTGPU_H__INCLUDED__

#include "winini.h"
#include "actions.h"

#include "windrv.h"
#include "winreg.h"
#include "filecopy.h"
#include "winres.h"

#include "actions.h"
#include "setuperr.h"
#include "softgpuver.h"

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
#define RAD_SSE4    20

#define RAD_DD_HAL  21
#define RAD_DD_WINE 22

#define RAD_D8_WINE 23
#define RAD_D8_NINE 24

#define RAD_D9_WINE 25
#define RAD_D9_NINE 26

#define CHBX_BUG_PREFER_FIFO 27
#define CHBX_BUG_RGB565 28
#define CHBX_BUG_DX_FLAGS 29

#define INP_VRAM_LIMIT 30
#define INP_GMR_LIMIT 31

#define BTN_README 32
#define BTN_SYSINFO 33
#define BTN_WGLTEST 34
#define BTN_GLCHECKER 35
#define BTN_DEFAULTS 36

#define CHBX_3DFX  37

#define LBX_PROFILE 38
#define STATIC_GPUMSG 39

#define BTN_CUSTOM 40

//#define INP_SCREENTARGET 41
//#define CHBX_ST_16 42
//#define CHBX_ST_MOUSE 43
//#define CHBX_ST_MOUSE_HIDE 44

#define CHBX_DOTCOM  45
#define CHBX_WS2     46

//#define CHBX_BLIT_SURF 47
//#define CHBX_DMA_NEED_REREAD 48

//#define CHBX_MESA_DOWNGRADE 49
#define CHBX_NO_MULTISAMPLE 50
#define INP_ASYNCMOBS 51

#define CHBX_GMR_CACHE   52
#define IMP_SVGA_MEM_MAX 53
#define CHBX_SW_GAMMA    54
#define CHBX_DMA_TO_FB   55
#define CHBX_HWCURSOR    56

#define RAD_D8_HAL 57
#define RAD_D9_HAL 58

#define CHBX_MESA_ALT 59

#define RAD_LOWDETAIL_0 60
#define RAD_LOWDETAIL_1 61
#define RAD_LOWDETAIL_2 62
#define RAD_LOWDETAIL_3 63

BOOL isSettingSet(DWORD menu);
void writeSettings();
int intSettings(int type);
void settingsReset();
void settingsSetStr(DWORD menu, const char *str, BOOL copy);
void settingsFree();

BOOL readSettings();

#define SETTINGS_VRAM 1
#define SETTINGS_GMR  2

extern float rdpiX;
extern float rdpiY;

extern BOOL      isNT;
extern version_t sysver;
extern version_t dxver;
extern version_t dxtarget;
extern BOOL      hasCRT;
extern BOOL      hasSETUPAPI;
extern uint32_t  hasSSE3;
extern uint32_t  hasSSE42;
extern uint32_t  hasAVX;
extern uint32_t  hasP2;
extern BOOL      hasOpengl;
extern BOOL      hasOle32;
extern BOOL      hasWS2;

#define DPIX(_spx) ((int)(ceil((_spx)*rdpiX)))
#define DPIY(_spx) ((int)(ceil((_spx)*rdpiY)))

static const version_t WINVER95 = {4,0,0,0};
static const version_t WINVER98 = {4,10,0,0};
static const version_t WINVER98SE = {4,10,2222,0};
static const version_t WINVERME = {4,90,0,0};
static const version_t WINVER2K = {5,0,0,0};

void softgpu_window_create(HWND hwnd, LPARAM lParam);
void softgpu_loading_create(HWND hwnd, LPARAM lParam);
void softgpu_cur_window_create(HWND hwnd, LPARAM lParam);

extern char sysinfomsg[];

extern HWND infobox;
extern HWND installbtn;
extern HWND pathinput;

extern BOOL reinstall_dx;

#define SOFTGPU_WIN_STYLE (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX)

void softgpu_browser(const char *url);
int GetInputInt(HWND hwnd, int id);
void softgpu_set(HWND hwnd);

void settingsSet(DWORD menu, DWORD value);
void settingsSetByName(const char *name, DWORD value);
void settingsDisableByName(const char *name, int disabled);
void settingsApply(HWND hwnd);
void settingsReadback(HWND hwnd);
void settingsApplyProfile(int profile);
DWORD settingReadDW(DWORD menu);

extern char msg_gpu_status[];
extern char msg_gpu_names[];

#define DETECT_OK 0
#define DETECT_WARN 1
#define DETECT_ERR  2

#define SETUP_COPY_ONLY      0
#define SETUP_INSTALL        1
#define SETUP_INSTALL_DRIVER 2

void check_SW_HW();
int detection_status();
int installable_status();
int get_gpu_profile();

#endif /* __SOFTGPU_H__INCLUDED__ */
