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
#include <cfgmgr32.h> /* CM_DEVCAP_Xxx constants */

#include "windrv.h"
#include "winreg.h"
#include "setuperr.h"
#include "winini.h"

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

#if 0
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
	{"QEMU STD VGA","PCI\\VEN_1234&DEV_1111&",                       "Qemu"},
	{NULL,          NULL,                                            NULL}
};

static const char *pci_ids[] = {
	"*PNP0A03",               /* PCI bus */
	"ACPI\\VEN_PNP&DEV_0A03", /* PCI bus (NT) */
	"*PNP0A08",               /* PCI Express root bus */
	"ACPI\\VEN_PNP&DEV_0A08", /* PCI Express root bus (NT) */
	"*PNP0C00",               /* PCI bus (qemu) */
	NULL
};

static const char non_pci_vga[] = "*PNP0900";

#endif


#define VGA_DEVICES_MAX 10
static vga_device_t devices[VGA_DEVICES_MAX];
static int devices_cnt = 0;

typedef DWORD (WINAPI *GetVersionFunc)(void);

vga_device_t *device_get(int n)
{
	if(n < devices_cnt)
	{
		return &devices[n];
	}
	
	return NULL;
}

int device_count()
{
	return devices_cnt;
}

BOOL device_parse(const char *hw_ident, vga_device_t *dev)
{
	char *ptr;
	
	if(strstr(hw_ident, "PCI\\") == hw_ident)
	{
		ptr = strstr(hw_ident, "VEN_");
		if(ptr)
		{
			dev->pci.ven = strtoul(ptr+4, NULL, 16);
			ptr = strstr(hw_ident, "DEV_");
			if(ptr)
			{
				dev->pci.dev = strtoul(ptr+4, NULL, 16);
				ptr = strstr(hw_ident, "SUBSYS_");
				if(ptr)
				{
					dev->pci.subsys = strtoul(ptr+7, NULL, 16);
					dev->pci.has_subsys = TRUE;
				}
				else
				{
					dev->pci.subsys = 0;
					dev->pci.has_subsys = FALSE;
				}
				
				//printf("parsed: %04X %04X %08X\n", dev->pci.ven, dev->pci.dev, dev->pci.subsys);
				
				dev->type = VGA_DEVICE_PCI;
				return TRUE;
			}
		}
	}
	else if(strstr(hw_ident, "*PNP") == hw_ident)
	{
		dev->type = VGA_DEVICE_ISA;
		dev->isa.pnp = strtoul(hw_ident+4, NULL, 16);
		return TRUE;
	}
	
	return FALSE;
}

void device_str(vga_device_t *dev, char *buf, BOOL shorter)
{
	switch(dev->type)
	{
		case VGA_DEVICE_ISA:
			sprintf(buf, "*PNP%4X", dev->isa.pnp);
			break;
		case VGA_DEVICE_PCI:
		{
			const char *prefix = "PCI\\";
			if(shorter)
				prefix = "";
			
			if(dev->pci.has_subsys)
			{
				sprintf(buf, "%sVEN_%04X&DEV_%04X&SUBSYS_%08X", prefix, dev->pci.ven, dev->pci.dev, dev->pci.subsys);
			}
			else
			{
				sprintf(buf, "%sVEN_%04X&DEV_%04X", prefix, dev->pci.ven, dev->pci.dev);
			}
			break;
		}
		default:
			buf[0] = '\0';
			break;
	}
}

const char *device_ini_get(vga_device_t *dev, const char *property)
{
	int n, i;
	switch(dev->type)
	{
		case VGA_DEVICE_ISA:
			n = iniSectionsCount("[isa]");
			for(i = 0; i < n; i++)
			{
				const char *s_pnp = iniSectionsValue("[isa]", i, "pnp");
				if(s_pnp)
				{
					uint16_t pnp = strtoul(s_pnp, NULL, 0);
					if(pnp == dev->isa.pnp)
					{
						return iniSectionsValue("[isa]", i, property);
					}
				}
			}
			break;
		case VGA_DEVICE_PCI:
			n = iniSectionsCount("[pci]");
			for(i = 0; i < n; i++)
			{
				const char *s_ven    = iniSectionsValue("[pci]", i, "ven");
				const char *s_dev    = iniSectionsValue("[pci]", i, "dev");
				const char *s_subsys = iniSectionsValue("[pci]", i, "subsys");
				
				if(s_ven && s_dev)
				{
					uint16_t ven_id = strtoul(s_ven, NULL, 0);
					uint16_t dev_id = strtoul(s_dev, NULL, 0);
					
					//printf("scan: %04X %04X\n", ven_id, dev_id);
					
					if(ven_id == dev->pci.ven && dev_id == dev->pci.dev)
					{
						BOOL found = FALSE;
						if(s_subsys == NULL)
						{
							found = TRUE;
						}
						else if(dev->pci.has_subsys && s_subsys != NULL)
						{
							uint32_t subsys_id = strtoul(s_subsys, NULL, 0);
							
							//printf(" subsys %08X\n", subsys_id);
							
							if(subsys_id == dev->pci.subsys)
							{
								found = TRUE;
							}
						}
					
						if(found)
						{
							return iniSectionsValue("[pci]", i, property);
						}
					}
				}
			}
			break;
	}
	
	return NULL;
}

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
int version_compare(const version_t *v1, const version_t *v2)
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
  /* shut up this cast warning */
# if __GNUC__ > 4
#  pragma GCC diagnostic ignored "-Wcast-function-type"
# endif

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
				vga_device_t dev;
				
				if(device_parse(hwid_str, &dev))
				{
					const char *section = device_ini_get(&dev, "inf");
					if(section != NULL)
					{
						memcpy(did_out, &DeviceInfoData, sizeof(SP_DEVINFO_DATA));
						return section;
					}
				}
			}
	    DeviceIndex++;
	  }
	}
	
	return NULL;
}

static DWORD checkInstallatioListLookup(HDEVINFO DeviceInfoSet)
{
	static char device_id[1024];
	int DeviceIndex = 0;
	SP_DEVINFO_DATA DeviceInfoData;
	DWORD ret = CHECK_DRV_OK;
	
	if(DeviceInfoSet)
	{
		ZeroMemory(&DeviceInfoData, sizeof(SP_DEVINFO_DATA));
		DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
		DeviceIndex = 0;
		
		while(DYN(SetupDiEnumDeviceInfo)(DeviceInfoSet, DeviceIndex, &DeviceInfoData))
		{
			if(DYN(SetupDiGetDeviceRegistryPropertyA)(DeviceInfoSet, &DeviceInfoData, SPDRP_HARDWAREID, NULL, (PBYTE)&(device_id[0]), sizeof(device_id), NULL))
			{
				if(device_parse(device_id, &devices[devices_cnt]))
				{
					devices_cnt++;
				}
			}
	    DeviceIndex++;
	  }
	}
	
	return ret;
}

DWORD checkInstallation()
{
  HDEVINFO hDevInfo;
  DWORD ret = 0;

  /* Enumerate all display adapters */
  hDevInfo = DYN(SetupDiGetClassDevsA)(&GUID_DEVCLASS_DISPLAY, NULL, NULL, DIGCF_PRESENT);

  if(hDevInfo == INVALID_HANDLE_VALUE)
  {
  	REPORT("SetupDiCreateDeviceInfoList: %ld", GetLastError());
    return CHECK_DRV_FAIL;
  }

  /* Read the drivers from the inf file */
  if (!DYN(SetupDiBuildDriverInfoList)(hDevInfo, NULL, SPDIT_CLASSDRIVER))
	{
		REPORT("SetupDiBuildDriverInfoList: %ld", GetLastError());
		DYN(SetupDiDestroyDeviceInfoList)(hDevInfo);
		return CHECK_DRV_FAIL;
	}
	
	ret = checkInstallatioListLookup(hDevInfo);

  /* Cleanup */
	DYN(SetupDiDestroyDeviceInfoList)(hDevInfo);

  return ret;
}

static void set32bpp()
{
	DEVMODEA mode;
	memset(&mode, 0, sizeof(DEVMODEA));
	mode.dmSize = sizeof(DEVMODEA);
	mode.dmSpecVersion = DM_SPECVERSION;
	mode.dmBitsPerPel = 32;
	mode.dmPelsWidth  = 640;
	mode.dmPelsHeight = 480;
	mode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
	
	ChangeDisplaySettingsA(&mode, CDS_UPDATEREGISTRY | CDS_NORESET);
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
  	REPORT("No supported VGA adapter found! (It could happen if PCI bus isn't detected or is misdetected, for example Win9X in QEMU do this, more informations in README)");
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
  set32bpp();

  return TRUE;
}
