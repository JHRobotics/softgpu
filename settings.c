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

#define VRAM_DEF 128
#define GMR_DEF  0

#define DEFAULT_INST_PATH "C:\\drivers\\softgpu"

static DWORD settings_data = 0;
static DWORD settings_vram = VRAM_DEF;
static DWORD settings_gmr  = GMR_DEF;

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
	{CHBX_SIMD95,  5, TRUE},
	{CHBX_FIXES,   6, FALSE},
	{BTN_ABOUT,    7, FALSE},
	{CHBX_GL95,    8, FALSE},
	{CHBX_QXGA,    9, TRUE},
	{CHBX_1440,   10, TRUE},
	{CHBX_4K,     11, TRUE},
	{CHBX_5K,     12, TRUE},
	{RAD_DD_HAL,  13, TRUE},
	{RAD_D8_NINE, 14, TRUE},
	{RAD_D9_NINE, 15, TRUE},
	{CHBX_BUG_PREFER_FIFO, 16, FALSE},
	{CHBX_BUG_RGB565,      17, FALSE},
	{CHBX_BUG_DX_FLAGS,    18, FALSE},
	{CHBX_3DFX,            19, FALSE},
	{0, 0, FALSE}
};

void settingsReset()
{
	settings_data = 0;
	settings_vram = VRAM_DEF;
	settings_gmr  = GMR_DEF;
}

BOOL isSettingSet(DWORD menu)
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

void writeSettings(HWND hwnd)
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
	
	registryWriteDWORD("HKCU\\SOFTWARE\\SoftGPU\\setup_" SOFTGPU_STR(SOFTGPU_BUILD), new_data);
	
	registryWriteDWORD("HKCU\\SOFTWARE\\SoftGPU\\setup_vram_" SOFTGPU_STR(SOFTGPU_BUILD), GetInputInt(hwnd, INP_VRAM_LIMIT));
	registryWriteDWORD("HKCU\\SOFTWARE\\SoftGPU\\setup_gmr_" SOFTGPU_STR(SOFTGPU_BUILD),  GetInputInt(hwnd, INP_GMR_LIMIT));
	
}

void readSettings()
{
	registryReadDWORD("HKCU\\SOFTWARE\\SoftGPU\\setup_" SOFTGPU_STR(SOFTGPU_BUILD), &settings_data);
	
	DWORD t = 0;
	registryReadDWORD("HKCU\\SOFTWARE\\SoftGPU\\setup_vram_" SOFTGPU_STR(SOFTGPU_BUILD), &t);
	if(t != 0)
		t = settings_vram;
	
	t = 0;
	registryReadDWORD("HKCU\\SOFTWARE\\SoftGPU\\setup_gmr_" SOFTGPU_STR(SOFTGPU_BUILD), &settings_gmr);
	if(t != 0)
		t = settings_gmr;
}

int intSettings(int type)
{
	switch(type)
	{
		case SETTINGS_VRAM:
			return settings_vram;
			break;
		case SETTINGS_GMR:
				if(settings_gmr == 0)
				{
					MEMORYSTATUS status;
					status.dwLength = sizeof(status);
					GlobalMemoryStatus(&status);
					
					if(status.dwTotalPhys > 700 * 1024 * 1024)
						settings_gmr = 256;
					else
						settings_gmr = 160;
				}
				
			return settings_gmr;
			break;
	}
	
	return 0;
}
