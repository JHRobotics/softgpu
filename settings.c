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

#define ST_DEF 0
#define VRAM_DEF 128
#define DEFAULT_INST_PATH "C:\\drivers\\softgpu"

#define T_CHECKBOX  1
#define T_RADIO     2
#define T_INPUT_STR 3
#define T_INPUT_NUM 4
#define T_DROPDOWN  5

typedef struct _settings_item_t
{
	DWORD menu;
	DWORD type;
	DWORD pos;
	const char *name;
	DWORD dvalue;
	const char *svalue;
	char *svalue_freeptr;
	int group;
	int disabled;
} settings_item_t;

static const settings_item_t settings_def[] = 
{
	{CHBX_MSCRT,           T_CHECKBOX,   0, "mscrt",    0, NULL, NULL, 0, 0},
	{CHBX_DX,              T_CHECKBOX,   1, "dxredist", 1, NULL, NULL, 0, 0},
	{RAD_NORMAL,           T_RADIO,      2, "bin_mmx",  1, NULL, NULL, 1, 0},
	{RAD_SSE,              T_RADIO,      3, "bin_sse",  0, NULL, NULL, 1, 0},
	{RAD_SSE4,             T_RADIO,      4, "bin_sse4", 0, NULL, NULL, 1, 0},
	{CHBX_GL95,            T_CHECKBOX,   5, "gl95",     0, NULL, NULL, 0, 0},
	{CHBX_WINE,            T_CHECKBOX,   6, "wine",     1, NULL, NULL, 0, 0},
	{CHBX_GLIDE,           T_CHECKBOX,   7, "glide",    1, NULL, NULL, 0, 0},
	{CHBX_SIMD95,          T_CHECKBOX,   8, "simd95",   0, NULL, NULL, 0, 0},
	{CHBX_3DFX,            T_CHECKBOX,   9, "3dfx",     1, NULL, NULL, 0, 0},
	{CHBX_FIXES,           T_CHECKBOX,  10, "fixes",    1, NULL, NULL, 0, 0},
	{CHBX_QXGA,            T_CHECKBOX,  11, "res_qxga", 0, NULL, NULL, 0, 0},
	{CHBX_1440,            T_CHECKBOX,  12, "res_1440", 0, NULL, NULL, 0, 0},
	{CHBX_4K,              T_CHECKBOX,  13, "res_2160", 0, NULL, NULL, 0, 0},
	{CHBX_5K,              T_CHECKBOX,  14, "res_2880", 0, NULL, NULL, 0, 0},
	{RAD_DD_HAL,           T_RADIO,     15, "dd_hal",   0, NULL, NULL, 2, 0},
	{RAD_DD_WINE,          T_RADIO,     16, "dd_wine",  1, NULL, NULL, 2, 0},
	{RAD_D8_WINE,          T_RADIO,     17, "d8_wine",  1, NULL, NULL, 3, 0},
	{RAD_D8_NINE,          T_RADIO,     18, "d8_nine",  0, NULL, NULL, 3, 0},
	{RAD_D9_WINE,          T_RADIO,     19, "d9_wine",  1, NULL, NULL, 4, 0},
	{RAD_D9_NINE,          T_RADIO,     20, "d9_nine",  0, NULL, NULL, 4, 0},
	{CHBX_BUG_PREFER_FIFO, T_CHECKBOX,  21, "fifo",     1, NULL, NULL, 0, 0},
	{CHBX_BUG_RGB565,      T_CHECKBOX,  22, "rgb565",   1, NULL, NULL, 0, 0},
	{CHBX_BUG_DX_FLAGS,    T_CHECKBOX,  23, "dxflags",  0, NULL, NULL, 0, 0},
	{LBX_PROFILE,          T_DROPDOWN,  24, "profile",  0, NULL, NULL, 0, 0},
	{INP_PATH,             T_INPUT_STR, 25, "path",     0, DEFAULT_INST_PATH, NULL, 0, 0},
	{INP_VRAM_LIMIT,       T_INPUT_NUM, 26, "vram",     VRAM_DEF, NULL, NULL, 0, 0},
	{INP_SCREENTARGET,     T_INPUT_NUM, 27, "screentarget_mb",    ST_DEF, NULL, NULL, 0, 0},
	{CHBX_ST_16,           T_CHECKBOX,  28, "screentarget_16bpp", 0, NULL, NULL, 0, 0},
	{CHBX_ST_MOUSE,        T_CHECKBOX,  29, "screentarget_mouse", 0, NULL, NULL, 0, 0},
	{CHBX_ST_MOUSE_HIDE,   T_CHECKBOX,  30, "screentarget_mouse_hide", 0, NULL, NULL, 0, 0},
	{CHBX_DOTCOM,          T_CHECKBOX,  31, "dotcom",   0, NULL, NULL, 0, 0},
	{CHBX_WS2,             T_CHECKBOX,  32, "ws2",      0, NULL, NULL, 0, 0},
	{CHBX_BLIT_SURF,       T_CHECKBOX,  33, "blit_surf", 0, NULL, NULL, 0, 0},
	{CHBX_DMA_NEED_REREAD, T_CHECKBOX,  34, "dma_need_reread", 1, NULL, NULL, 0, 0},
	
	{0,                    0,            0, NULL,        0, NULL, NULL, 0, 0}
};

static settings_item_t settings_cur[sizeof(settings_def)/sizeof(settings_item_t)];

static BOOL settings_cur_init = FALSE;

static void settingClearGroup(int group)
{
	settings_item_t *item;
	
	if(group == 0)
		return;
	
	for(item = &settings_cur[0]; item->menu != 0; item++)
	{
		if(item->group == group)
		{
			item->dvalue = 0;
		}
	}
}

void settingsApply(HWND hwnd)
{
	settings_item_t *item;
	
	for(item = &settings_cur[0]; item->menu != 0; item++)
	{
		HWND hItem = GetDlgItem(hwnd, item->menu);
		if(hItem)
		{
			switch(item->type)
			{
				case T_RADIO:
				case T_CHECKBOX:
					if(item->disabled)
					{
						CheckDlgButton(hwnd, item->menu, BST_UNCHECKED);
						LONG sty = GetWindowLongA(hItem, GWL_STYLE) | WS_DISABLED;
						SetWindowLongA(hItem, GWL_STYLE, sty);
					}
					else
					{
						LONG sty = GetWindowLongA(hItem, GWL_STYLE) & (~WS_DISABLED);
						SetWindowLongA(hItem, GWL_STYLE, sty);
						
						if(item->dvalue)
							CheckDlgButton(hwnd, item->menu, BST_CHECKED);
						else
							CheckDlgButton(hwnd, item->menu, BST_UNCHECKED);
					}
					break;
				case T_INPUT_NUM:
				{
					char inp[64];
					sprintf(inp, "%u", item->dvalue);					
					SetWindowTextA(hItem, inp);
					break;
				}
				case T_INPUT_STR:
					SetWindowTextA(hItem, item->svalue);
					break;
				case T_DROPDOWN:
					SendMessage(hItem, (UINT)CB_SETCURSEL, (WPARAM)item->dvalue, (LPARAM)0);
					break;
			}
		}
	}
}

void settingsReadback(HWND hwnd)
{
	settings_item_t *item;
	
	for(item = &settings_cur[0]; item->menu != 0; item++)
	{
		DWORD value;
		HWND hItem = GetDlgItem(hwnd, item->menu);
		if(hItem)
		{
			switch(item->type)
			{
				case T_RADIO:
					value = IsDlgButtonChecked(hwnd, item->menu) ? 1 : 0;
					if(value > 0)
					{
						settingClearGroup(item->group);
						item->dvalue = value;
					}
					break;
				case T_CHECKBOX:
					value = IsDlgButtonChecked(hwnd, item->menu) ? 1 : 0;
					item->dvalue = value;
					break;
				case T_INPUT_NUM:
				{
					char inp[64];
					if(GetWindowTextA(hItem, inp, 64) != 0)
					{
						item->dvalue = strtol(inp, NULL, 0);
					}
					break;
				}
				case T_INPUT_STR:
				{
					if(item->svalue_freeptr != NULL)
					{
						free(item->svalue_freeptr);
						item->svalue_freeptr = NULL;
						item->svalue = NULL;
					}
					
					size_t s = GetWindowTextLengthA(hItem) + 1;
					item->svalue_freeptr = malloc(s);
					if(item->svalue_freeptr)
					{
						GetWindowTextA(hItem, item->svalue_freeptr, s);
						item->svalue = item->svalue_freeptr;
					}
					break;
				}
				case T_DROPDOWN:
					item->dvalue = SendMessage(hItem, (UINT)CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
					break;
			}
		}
	}
}

void settingsApplyProfile(int profile)
{
	settings_item_t *item;
	
	for(item = &settings_cur[0]; item->menu != 0; item++)
	{
		const char *v = iniSectionsValue("[profile]", profile, item->name);
		if(v == NULL)
		{
			v = iniValue("[defaults]", item->name);
		}
		
		if(v != NULL)
		{
			DWORD dw = strtoul(v, NULL, 0);
			settingsSet(item->menu, dw);
		}
	}
}

static BOOL have_3dfx_files()
{
	BOOL r = FALSE;
	const char *drv_voodoo = iniValue("[softgpu]", "voodoo2path");
	if(drv_voodoo != NULL)
	{
		if(is_dir(drv_voodoo))
		{
			char testpath[MAX_PATH];
			strcpy(testpath, drv_voodoo);
			strcat(testpath, "\\3dfxSpl2.dll");
			if(is_file(testpath))
			{
				strcpy(testpath, drv_voodoo);
				strcat(testpath, "\\3dfxSpl3.dll");
				if(is_file(testpath))
				{
					strcpy(testpath, drv_voodoo);
					strcat(testpath, "\\3dfxVGL.dll");
					if(is_file(testpath))
					{
						r = TRUE;
					}
				}
			}
		}
	}
	
	return r;
}

static void settingsCompute()
{
	if(version_compare(&sysver, &WINVER98) < 0)
	{
		if(!hasOpengl)
		{
			settingsSetByName("gl95", 1);
		}
		
		if(!hasOle32)
		{
			settingsSetByName("dotcom", 1);
		}
		
		if(!hasWS2)
		{
			settingsSetByName("ws2", 1);
		}
	}
	else
	{
		settingsDisableByName("gl95",   1);
		settingsDisableByName("dotcom", 1);
		settingsDisableByName("ws2",    1);
	}
	
	if(!isNT)
	{
		if(!hasCRT)
		{
			settingsSetByName("mscrt", 1);
		}
		
		if(version_compare(&dxver, &dxtarget) < 0 || reinstall_dx)
		{
			settingsSetByName("dxredist", 1);
		}
	}
	else
	{
		settingsDisableByName("mscrt", 1);
		settingsDisableByName("dxredist", 1);
	}
	
	const char *drv_sse4 = iniValue("[softgpu]", "drvpath.sse4");
	BOOL sse4_bins = drv_sse4 != NULL && is_dir(drv_sse4);
	
	if(sse4_bins)
	{
		if(version_compare(&sysver, &WINVER98) >= 0 && hasSSE42)
		{
			settingsSetByName("bin_sse4", 1);
		}
		else if(version_compare(&sysver, &WINVER98) >= 0 && hasSSE3)
		{
			settingsSetByName("bin_sse", 1);
		}
		else
		{
			settingsSetByName("bin_mmx", 1);
		}
	}
	else
	{
		settingsDisableByName("bin_sse4", 1);
		
		if(version_compare(&sysver, &WINVER98) >= 0 && hasSSE3)
		{
			settingsSetByName("bin_sse", 1);
		}
		else
		{
			settingsSetByName("bin_mmx", 1);
		}
	}
	
	if(version_compare(&sysver, &WINVERME) >= 0)
	{
		settingsDisableByName("simd95", 1);
	}
	
	if(!have_3dfx_files())
	{
		settingsDisableByName("3dfx", 1);
	}
}

void settingsSet(DWORD menu, DWORD value)
{
	settings_item_t *item;
	
	for(item = &settings_cur[0]; item->menu != 0; item++)
	{
		if(item->menu == menu)
		{
			switch(item->type)
			{
				case T_RADIO:
					if(value > 0)
					{
						settingClearGroup(item->group);
						item->dvalue = value;
					}
					break;
				case T_CHECKBOX:
				case T_INPUT_NUM:
				case T_DROPDOWN:
					item->dvalue = value;
					break;
			}
			break;
		}
	}
}

void settingsSetByName(const char *name, DWORD value)
{
	settings_item_t *item;
	
	for(item = &settings_cur[0]; item->menu != 0; item++)
	{
		if(strcmp(name, item->name) == 0)
		{
			switch(item->type)
			{
				case T_RADIO:
					if(value > 0)
					{
						settingClearGroup(item->group);
						item->dvalue = value;
					}
					break;
				case T_CHECKBOX:
				case T_INPUT_NUM:
				case T_DROPDOWN:
					item->dvalue = value;
					break;
			}
			break;
		}
	}
}

void settingsDisableByName(const char *name, int disabled)
{
	settings_item_t *item;
	
	for(item = &settings_cur[0]; item->menu != 0; item++)
	{
		if(strcmp(name, item->name) == 0)
		{
			item->disabled = disabled;
			break;
		}
	}
}

/* free allocated strings */
void settingsFree()
{
	settings_item_t *item;
	
	for(item = &settings_cur[0]; item->menu != 0; item++)
	{
		if(item->type == T_INPUT_STR)
		{
			if(item->svalue_freeptr != NULL)
			{
				free(item->svalue_freeptr);
				item->svalue_freeptr = NULL;
				item->svalue = NULL;
			}
		}
	}
}

/* from last save something can change */
static void settingsReEval()
{	
	if(hasOpengl)
	{
		settingsSetByName("gl95", 0);
	}
		
	if(hasOle32)
	{
		settingsSetByName("dotcom", 0);
	}
		
	if(hasWS2)
	{
		settingsSetByName("ws2", 0);
	}
	
	if(hasCRT)
	{
		settingsSetByName("mscrt", 0);
	}
		
	if(version_compare(&dxver, &dxtarget) >= 0 && reinstall_dx == FALSE)
	{
		settingsSetByName("dxredist", 0);
	}
}

/* load default settings */
void settingsReset()
{
	if(settings_cur_init)
	{
		settingsFree();
	}
	
	memcpy(&settings_cur[0], &settings_def[0], sizeof(settings_def));
	
	settingsCompute();
	
	const char *defProfile = iniValue("[softgpu]", "default_profile");
	if(defProfile != NULL)
	{
		DWORD p = strtoul(defProfile, NULL, 0);
		settingsSetByName("profile", p);
		settingsApplyProfile(p);
		//printf("profile: %u\n", p);
	}
	else
	{
		settingsApplyProfile(0);
	}
	
	const char *defPath = iniValue("[softgpu]", "default_path");
	if(defPath != NULL)
	{
		settingsSetStr(INP_PATH, defPath, FALSE);
	}
	
	/* read from registry first time */
	if(!settings_cur_init)
	{
		readSettings();
		settingsReEval();
	}
	
	settings_cur_init = TRUE;
}

BOOL isSettingSet(DWORD menu)
{
	settings_item_t *item;
	for(item = &settings_cur[0]; item->menu != 0; item++)
	{
		if(item->menu == menu)
		{
			return item->dvalue;
		}
	}
	
	return FALSE;
}

DWORD settingReadDW(DWORD menu)
{
	settings_item_t *item;
	for(item = &settings_cur[0]; item->menu != 0; item++)
	{
		if(item->menu == menu)
		{
			switch(item->type)
			{
				case T_CHECKBOX:
				case T_RADIO:
				case T_INPUT_NUM:
				case T_DROPDOWN:
					return item->dvalue;
					break;
				case T_INPUT_STR:
					return strtoul(item->svalue, NULL, 0);
					break;
			}
		}
	}
	
	return 0;
}

const char *settingsReadStr(DWORD menu)
{
	settings_item_t *item;
	for(item = &settings_cur[0]; item->menu != 0; item++)
	{
		if(item->menu == menu)
		{
			switch(item->type)
			{
				case T_INPUT_STR:
					return item->svalue;
			}
		}
	}
	
	return NULL;
}

void settingsSetStr(DWORD menu, const char *str, BOOL copy)
{
	settings_item_t *item;
	for(item = &settings_cur[0]; item->menu != 0; item++)
	{
		if(item->menu == menu)
		{
			switch(item->type)
			{
				case T_INPUT_STR:
					if(item->svalue_freeptr != NULL)
					{
						free(item->svalue_freeptr);
						item->svalue_freeptr = NULL;
						item->svalue = NULL;
					}
					
					if(!copy)
					{
						item->svalue = str;
					}
					else
					{
						size_t s = strlen(str) + 1;
						item->svalue_freeptr = malloc(s);
						if(item->svalue_freeptr)
						{
							memcpy(item->svalue_freeptr, str, s);
							item->svalue = item->svalue_freeptr;
						}
					}
					break;
			}
		}
	}
}

#define MAX_CONF_DW 4

#define KEYNAME_FMT "HKCU\\SOFTWARE\\SoftGPU\\setup-%d\\%s"

void writeSettings()
{
	char keyname[MAX_PATH];
	DWORD confdw[MAX_CONF_DW];
	
	memset(&confdw[0], 0, sizeof(confdw));
	
	settings_item_t *item;
	for(item = &settings_cur[0]; item->menu != 0; item++)
	{
		switch(item->type)
		{
			case T_CHECKBOX:
			case T_RADIO:
				DWORD p = item->pos / 32;
				DWORD i = item->pos % 32;
				if(item->dvalue > 0)
				{
					confdw[p] |= (1 << i);
				}
				break;
			case T_INPUT_NUM:
			case T_DROPDOWN:
				sprintf(keyname, KEYNAME_FMT, SOFTGPU_BUILD, item->name);
				registryWriteDWORD(keyname, item->dvalue);
				break;
			case T_INPUT_STR:
				sprintf(keyname, KEYNAME_FMT, SOFTGPU_BUILD, item->name);
				registryWrite(keyname, item->svalue, WINREG_STR);
				break;
		}
	}
	
	int j;
	for(j = 0; j < MAX_CONF_DW; j++)
	{
		sprintf(keyname, KEYNAME_FMT "%02d", SOFTGPU_BUILD, "cfg", j);
		registryWriteDWORD(keyname, confdw[j]);
	}
}

BOOL readSettings()
{
	char keyname[MAX_PATH];
	char value[MAX_PATH];
	DWORD confdw[MAX_CONF_DW];
	
	int j;
	for(j = 0; j < MAX_CONF_DW; j++)
	{
		sprintf(keyname, KEYNAME_FMT "%02d", SOFTGPU_BUILD, "cfg", j);
		if(!registryReadDWORD(keyname, &confdw[j]))
		{
			return FALSE;
		}
	}
	
	settings_item_t *item;
	for(item = &settings_cur[0]; item->menu != 0; item++)
	{
		switch(item->type)
		{
			case T_CHECKBOX:
			case T_RADIO:
				DWORD p = item->pos / 32;
				DWORD i = item->pos % 32;
				item->dvalue = (confdw[p] >> i) & 0x1;
				break;
			case T_INPUT_NUM:
			case T_DROPDOWN:
				sprintf(keyname, KEYNAME_FMT, SOFTGPU_BUILD, item->name);
				registryReadDWORD(keyname, &item->dvalue);
				break;
			case T_INPUT_STR:
				sprintf(keyname, KEYNAME_FMT, SOFTGPU_BUILD, item->name);
				if(registryRead(keyname, value, MAX_PATH))
				{
					if(item->svalue_freeptr != NULL)
					{
						free(item->svalue_freeptr);
						item->svalue_freeptr = NULL;
						item->svalue = NULL;
					}
					
					size_t s = strlen(value)+1;
					item->svalue_freeptr = malloc(s);
					if(item->svalue_freeptr)
					{
						memcpy(item->svalue_freeptr, value, s);
						item->svalue = item->svalue_freeptr;
					}
				}
				break;
		}
	}
	
	return TRUE;
}
