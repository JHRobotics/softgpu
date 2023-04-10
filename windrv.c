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

/* for win 9X */
#define  USE_SP_DRVINFO_DATA_V1 1

#include <setupapi.h>
#include <initguid.h>
#include <devguid.h>

#include "windrv.h"
#include "winreg.h"
#include "setuperr.h"
#include "nocrt.h"

#define SETUPAPI_DYN

#ifdef SETUPAPI_DYN
# define SETUPAPI_FUNC(_t, _n, _a) \
	typedef _t (WINAPI * f##_n)_a; \
	static f##_n h##_n = NULL;
# include "setupapi_list.h"
# undef SETUPAPI_FUNC
# define DYN(_f) h##_f
#else
# define DYN(_f) _f
#endif


#ifndef CONFIGFLAG_DISABLED
#define CONFIGFLAG_DISABLED 0x00000001 // Set if disabled
#endif

typedef struct drivers_list
{
	const char *name;
	const char *hwid;
	const char *section;
} drivers_list_t;

static const drivers_list_t knowndrivers[]= 
{
	{"VBox VGA",    "PCI\\VEN_80EE&DEV_BEEF&SUBSYS_00000000&REV_00", "VBox"},
	{"VBox SVGA",   "PCI\\VEN_80EE&DEV_BEEF&SUBSYS_040515AD&REV_00", "VBoxSvga"},
	{"VMWare SVGA", "PCI\\VEN_15AD&DEV_0405&SUBSYS_040515AD&REV_00", "VMSvga"},
	{NULL,          NULL,                                            NULL}
};

typedef DWORD (WINAPI *GetVersionFunc)(void);

void version_parse(const char *version_str, version_t *out)
{
	int group = 0;
	const char *ptr;
	int junk = 0;
	
	memset(out, 0, sizeof(version_t));
	
	for(ptr = version_str; *ptr != '\0'; ptr++)
	{
		switch(*ptr)
		{
			case '0': case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8': case '9':
			{
				int *num = &junk;
				switch(group)
				{
					case 0:
						num = &(out->major);
						break;
					case 1:
						num = &(out->minor);
						break;
					case 2:
						num = &(out->patch);
						break;
					case 3:
						num = &(out->build);
						break;
				}
				*num = ((*num)*10) + ((*ptr) - '0');
				break;
			}
			case '.':
			case ',':
				group++;
				break;
		}
	}
}

/*
 * v1  < v2 => negative
 * v1 == v2 =>  0
 * v1  > v2 => positive
 */
int version_compare(version_t *v1, version_t *v2)
{
	int r = v1->major - v2->major;
	if(r == 0)
	{
		r = v1->minor - v2->minor;
		if(r == 0)
		{
			r = v1->patch - v2->patch;
			if(r == 0)
			{
				r = v1->build - v2->build;
			}
		}
	}
	
	return r;
}

void version_win(version_t *v)
{
	memset(v, 0, sizeof(version_t));
	
	HANDLE h = GetModuleHandleA("kernel32.dll");
	if(h)
	{
		GetVersionFunc GetVersionPtr = (GetVersionFunc)GetProcAddress(h, "GetVersion");
    DWORD dwVersion = 0;
		
		if(GetVersionPtr != NULL)
		{
			dwVersion = GetVersionPtr();
			v->major = (int)(LOBYTE(LOWORD(dwVersion)));
  		v->minor = (int)(HIBYTE(LOWORD(dwVersion)));

			if (dwVersion < 0x80000000)
			{
				v->build = (int)(HIWORD(dwVersion));
			}
			
			if(v->major == 4 && !version_is_nt())
			{
				version_t vreg;
				static char vbuf[32];
				if(registryRead("HKLM\\Software\\Microsoft\\Windows\\CurrentVersion\\VersionNumber", vbuf, sizeof(vbuf)))
				{
					version_parse(vbuf, &vreg);
					if(vreg.major == v->major
						&& vreg.minor == v->minor)
					{
						v->patch = vreg.patch;
					}
				}
			}
			
			return;
		}
	}
	
	/* windows >= 8.1 doesn't have this GetVersion */
	v->major = 8;
	v->minor = 1;
}

/*
*  NT function tip from: https://stackoverflow.com/a/57130 
 * but here is modification, 9x kernel has these function defined, but call them
 * fail with ERROR_CALL_NOT_IMPLEMENTED.
 */
typedef BOOL (WINAPI * GetThreadPriorityBoostF)(HANDLE hThread, PBOOL pDisablePriorityBoost);
BOOL version_is_nt()
{
	HANDLE h = GetModuleHandleA("kernel32.dll");
	if(h)
	{
		GetThreadPriorityBoostF GetThreadPriorityBoostH = (GetThreadPriorityBoostF)GetProcAddress(h, "GetThreadPriorityBoost");
		if(GetThreadPriorityBoostH != NULL)
		{
			BOOL junk;
			if(GetThreadPriorityBoostH(GetCurrentThread(), &junk))
			{
				return TRUE;	
			}
			
			if(GetLastError() != ERROR_CALL_NOT_IMPLEMENTED)
			{
				return TRUE;
			}
		}
	}
	
	return FALSE;
}

#ifdef SETUPAPI_DYN
static HMODULE hSETUPAPI = NULL;
#endif

BOOL loadSETUAPI()
{
#ifdef SETUPAPI_DYN
  /* shutup this cast warning */
  #pragma GCC diagnostic ignored "-Wcast-function-type"

	hSETUPAPI = LoadLibraryA("SETUPAPI.dll");
	if(hSETUPAPI != NULL)
	{
		#define SETUPAPI_FUNC(_t, _n, _a) \
			h##_n = (f##_n)GetProcAddress(hSETUPAPI, #_n); \
			if(h##_n == NULL){REPORT("GetProcAddress(SETUPAPI.dll, %s) = %ld", #_n, GetLastError()); return FALSE;}

		#include "setupapi_list.h"
		
		#undef SETUPAPI_FUNC
		return TRUE;
	}
	else
	{
		REPORT("LoadLibraryA(SETUPAPI.dll): %ld", GetLastError());
	}
	
	return FALSE;
#else
	return TRUE;
#endif
}

void freeSETUAPI()
{
#ifdef SETUPAPI_DYN
	if(hSETUPAPI != NULL)
	{
		FreeLibrary(hSETUPAPI);
	}
#endif
}

static void closeAndDestroy(HDEVINFO hDevInfo, HINF hInf)
{
	DYN(SetupCloseInfFile)(hInf);
	DYN(SetupDiDestroyDeviceInfoList)(hDevInfo);
}

const char *getVGAadapter(HDEVINFO DeviceInfoSet, SP_DEVINFO_DATA *did_out)
{
	static char hwid_str[1000];
	int DeviceIndex = 0;
	SP_DEVINFO_DATA DeviceInfoData;
	if(DeviceInfoSet)
	{
		ZeroMemory(&DeviceInfoData, sizeof(SP_DEVINFO_DATA));
		DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
		DeviceIndex = 0;
		
		while(DYN(SetupDiEnumDeviceInfo)(DeviceInfoSet, DeviceIndex, &DeviceInfoData))
		{
			if(DYN(SetupDiGetDeviceRegistryPropertyA)(DeviceInfoSet, &DeviceInfoData, SPDRP_HARDWAREID, NULL, (PBYTE)&(hwid_str[0]), sizeof(hwid_str), NULL))
			{
				for(const drivers_list_t *driver = &(knowndrivers[0]); driver->name != NULL; driver++)
				{
					if(stricmp(driver->hwid, hwid_str) == 0)
					{
						memcpy(did_out, &DeviceInfoData, sizeof(SP_DEVINFO_DATA));
						return driver->section;
					}
				}
				
			}
	    DeviceIndex++;
	  }
	}
	
	return NULL;
}

BOOL installVideoDriver(const char *szDriverDir, const char *infName)
{
  HDEVINFO hDevInfo;
  SP_DEVINSTALL_PARAMS_A DeviceInstallParams = {0};
  SP_DRVINFO_DATA_A drvInfoData = {0};
  SP_DRVINFO_DETAIL_DATA_A DriverInfoDetailData = {0};

  DWORD cbReqSize;

  HINF hInf;

  SP_DEVINFO_DATA deviceInfoData;
  DWORD configFlags;

  /* Enumerate all display adapters */
  hDevInfo = DYN(SetupDiGetClassDevsA)(&GUID_DEVCLASS_DISPLAY, NULL, NULL, DIGCF_PRESENT);

  if(hDevInfo == INVALID_HANDLE_VALUE)
  {
  	REPORT("SetupDiCreateDeviceInfoList: %ld", GetLastError());
    return FALSE;
  }

  memset(&DeviceInstallParams, 0, sizeof(SP_DEVINSTALL_PARAMS_A));
  DeviceInstallParams.cbSize = sizeof(SP_DEVINSTALL_PARAMS_A);

  if(!DYN(SetupDiGetDeviceInstallParamsA)(hDevInfo, NULL, &DeviceInstallParams))
  {
  	REPORT("SetupDiGetDeviceInstallParamsA: %ld", GetLastError());
  	return FALSE;
  }

  DeviceInstallParams.cbSize = sizeof(SP_DEVINSTALL_PARAMS_A);
  DeviceInstallParams.Flags |= DI_NOFILECOPY | // not copy here!
    DI_DONOTCALLCONFIGMG |
    DI_ENUMSINGLEINF; /* .DriverPath specifies an inf file */

  /* Path to inf file */
  strcpy(DeviceInstallParams.DriverPath, szDriverDir);
  strcat(DeviceInstallParams.DriverPath, "\\");
  strcat(DeviceInstallParams.DriverPath, infName);

  if(!DYN(SetupDiSetDeviceInstallParamsA)(hDevInfo, NULL, &DeviceInstallParams))
  {
  	REPORT("SetupDiSetDeviceInstallParamsA: %ld", GetLastError());
  	return FALSE;
  }

  /* Read the drivers from the inf file */
  if (!DYN(SetupDiBuildDriverInfoList)(hDevInfo, NULL, SPDIT_CLASSDRIVER))
	{
		REPORT("SetupDiBuildDriverInfoList: %ld", GetLastError());
		DYN(SetupDiDestroyDeviceInfoList)(hDevInfo);
		return FALSE;
	}

  int drvIndex = -1;
  const char *driverSection = getVGAadapter(hDevInfo, &deviceInfoData);
  
  if(driverSection == NULL)
  {
  	REPORT("No supported VGA adapter found! (It could happen if PCI bus isn't detected or is missdetect, for example Win9X in QEMU do this, more informations in README)");
  	return FALSE;
  }
  
  do
  {
  	drvIndex++;
  	
	  drvInfoData.cbSize = sizeof(SP_DRVINFO_DATA_A);
	  if(DYN(SetupDiEnumDriverInfoA)(hDevInfo, NULL, SPDIT_CLASSDRIVER, drvIndex, &drvInfoData) == FALSE)
	  {
	  	REPORT("SetupDiEnumDriverInfoA: %lu\n", GetLastError());
	    DYN(SetupDiDestroyDeviceInfoList)(hDevInfo);
	    return FALSE;
	  }

	  /* Get necessary driver details */
	  DriverInfoDetailData.cbSize = sizeof(SP_DRVINFO_DETAIL_DATA_A);
	  if (!(!DYN(SetupDiGetDriverInfoDetailA)(hDevInfo, NULL, &drvInfoData, &DriverInfoDetailData, DriverInfoDetailData.cbSize, &cbReqSize)
      	  && GetLastError() == ERROR_INSUFFICIENT_BUFFER))
		{
			REPORT("SetupDiGetDriverInfoDetailA: %ld", GetLastError());
			DYN(SetupDiDestroyDriverInfoList)(hDevInfo, NULL, SPDIT_CLASSDRIVER);
			DYN(SetupDiDestroyDeviceInfoList)(hDevInfo);
			return FALSE;
		}
		
		//printf("Enum index %d: %s\n", drvIndex, DriverInfoDetailData.SectionName);
	}
	while(strcmp(DriverInfoDetailData.SectionName, driverSection) != 0);

  hInf = DYN(SetupOpenInfFileA)(DriverInfoDetailData.InfFileName, NULL, INF_STYLE_WIN4, NULL);

  if (hInf == INVALID_HANDLE_VALUE)
	{
		REPORT("SetupOpenInfFileA: %ld", GetLastError());
		DYN(SetupDiDestroyDriverInfoList)(hDevInfo, NULL, SPDIT_CLASSDRIVER);
		DYN(SetupDiDestroyDeviceInfoList)(hDevInfo);
		return FALSE;
	}
	
  if(driverSection != NULL)
	{
		memset(&DeviceInstallParams, 0,sizeof(SP_DEVINSTALL_PARAMS_A));
		DeviceInstallParams.cbSize = sizeof(SP_DEVINSTALL_PARAMS_A);

		if(!DYN(SetupDiGetDeviceInstallParamsA)(hDevInfo, &deviceInfoData, &DeviceInstallParams))
		{
			REPORT("SetupDiGetDeviceInstallParamsA: %ld", GetLastError());
		}

		DeviceInstallParams.Flags |= //DI_NOFILECOPY | //We already copied the files
        //DI_DONOTCALLCONFIGMG |
        DI_NEEDRESTART |
        DI_ENUMSINGLEINF; //Use our INF file only

    /* Path to inf file */
    strcpy(DeviceInstallParams.DriverPath, szDriverDir);
    strcat(DeviceInstallParams.DriverPath, "\\");
    strcat(DeviceInstallParams.DriverPath, infName);

    if(!DYN(SetupDiSetDeviceInstallParamsA)(hDevInfo, &deviceInfoData, &DeviceInstallParams))
    {
    	REPORT("SetupDiSetDeviceInstallParams: %ld\n", GetLastError());
    }

    if(!DYN(SetupDiBuildDriverInfoList)(hDevInfo, &deviceInfoData, SPDIT_CLASSDRIVER))
		{
			REPORT("SetupDiBuildDriverInfoList: %ld\n", GetLastError());
			closeAndDestroy(hDevInfo, hInf);
			return FALSE;
		}

    drvInfoData.cbSize = sizeof(SP_DRVINFO_DATA_A);
    if (!DYN(SetupDiEnumDriverInfoA)(hDevInfo, &deviceInfoData, SPDIT_CLASSDRIVER, drvIndex, &drvInfoData))
		{
			REPORT("SetupDiEnumDriverInfoA: %ld\n", GetLastError());
			closeAndDestroy(hDevInfo, hInf);
			return FALSE;
		}

    if(!DYN(SetupDiSetSelectedDriverA)(hDevInfo, &deviceInfoData, &drvInfoData))
		{
			REPORT("SetupDiSetSelectedDriverA: %ld\n", GetLastError());
			closeAndDestroy(hDevInfo, hInf);
			return FALSE;
		}

    if(!DYN(SetupDiInstallDevice)(hDevInfo, &deviceInfoData))
		{
			REPORT("SetupDiInstallDevice: %ld\n", GetLastError());
			closeAndDestroy(hDevInfo, hInf);
			return FALSE;
		}
	}

  /* Make sure the device is enabled */
  if (DYN(SetupDiGetDeviceRegistryPropertyA)(hDevInfo, &deviceInfoData, SPDRP_CONFIGFLAGS,
                                       NULL, (LPBYTE) &configFlags, sizeof(DWORD), NULL)
                                       && (configFlags & CONFIGFLAG_DISABLED))
	{
		configFlags &= ~CONFIGFLAG_DISABLED;

		DYN(SetupDiSetDeviceRegistryPropertyA)(hDevInfo, &deviceInfoData, SPDRP_CONFIGFLAGS, (LPBYTE) &configFlags,sizeof(DWORD));
	}

  /* Cleanup */
  closeAndDestroy(hDevInfo, hInf);
  
  /* set screen color depth to some usable value */
  DWORD BitsPerPixel = 0;
  if(registryReadDWORD("HKCC\\Display\\Settings\\BitsPerPixel", &BitsPerPixel))
  {
  	if(BitsPerPixel <= 8)
  	{
  		registryWriteDWORD("HKCC\\Display\\Settings\\BitsPerPixel", 32);
  	}
  }
   

  return TRUE;
}

