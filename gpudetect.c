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
#include <stdio.h>

#include "softgpu.h"
#include "nocrt.h"


char msg_gpu_status[1024] = {'\0'};
char msg_gpu_names[1024]  = {'\0'};

static int detection   = DETECT_ERR;
static int installable = SETUP_COPY_ONLY;

const char msg_nt[] = 
	"Your system is NT! "
	"This driver is only for Windows 9x = 95, 98 and Me! "
	"Setup can preset the driver on this system but you've to copy "
	"these files and install on 9x system manually.";

const char msg_95[] = 
	"In Windows 95 isn't available automatic installation. "
	"This program copy and set driver but you've to manually install it from Device manager.";

const char msg_modegpu[] = 
	"There're more GPUs in this system. Please remove unwanted GPUs from Device Manager before continue! "
	"Multi-GPU isn't supported yet.";

const char msg_unknown[] = 
	"There isn't any known GPU for this driver. Installer only create GPU driver, but you've to "
	"install the driver manualy if you sure, that match to your device.";

const char msg_nogpu[] = 
	"No video detected, please make sure, that at last Microsoft generic driver (640x480, 4bpp) is installed!";

const char msg_win95[] = 
	"Automatic installation isn't possible on Windows 95. Installer preset driver, but you have to install it manualy via Device Manager. "
	"See README for more info.";

void check_SW_HW()
{
	static char hwid_buf[64];
	int errors = 0;
	int supported = 0;
	
	for(int i = 0; i < device_count(); i++)
	{
		vga_device_t *dev;
		dev = device_get(i);
		
		if(device_ini_get(dev, "inf") != NULL)
		{
			supported++;
		}
		
		if(i > 0)
		{
			strcat(msg_gpu_names, ", ");
		}
		
		const char *name = device_ini_get(dev, "name");
		if(name)
		{
			strcat(msg_gpu_names, name);
		}
		else
		{
			device_str(dev, hwid_buf, TRUE);
			strcat(msg_gpu_names, hwid_buf);
		}
		
		const char *desc = device_ini_get(dev, "desc");
		if(desc)
		{
			if(strlen(msg_gpu_status))
			{
				strcat(msg_gpu_status, ", ");
			}
			
			strcat(msg_gpu_status, desc);
		}
		
		const char *err_s = device_ini_get(dev, "error");
		if(err_s)
		{
			int e = strtol(err_s, NULL, 0);
			if(e)
			{
				errors++;
			}
		}
	}
		
	if(isNT)
	{
		strcpy(msg_gpu_status, msg_nt);
		detection = DETECT_ERR;
	}
	else
	{
		if(version_compare(&sysver, &WINVER98) < 0)
		{
			strcpy(msg_gpu_names, "no automatic detection on Windows 95");
			strcpy(msg_gpu_status, msg_win95);
			
			detection = DETECT_ERR;
			installable = SETUP_INSTALL;
		}
		else
		{
			if(supported == 0 && strlen(msg_gpu_status) == 0 && strlen(msg_gpu_names) == 0)
			{
				strcpy(msg_gpu_names, "None!");
				strcpy(msg_gpu_status, msg_nogpu);
				
			}
			else if(supported == 0 && strlen(msg_gpu_status) == 0)
			{
				strcpy(msg_gpu_status, msg_unknown);
			}
			else if(supported == 0)
			{
				detection = DETECT_ERR;
				installable = SETUP_INSTALL;
			}
			else if(errors > 0)
			{
				detection = DETECT_WARN;
				installable = SETUP_INSTALL_DRIVER;
			}
			else
			{
				detection = DETECT_OK;
				installable = SETUP_INSTALL_DRIVER;
			}
		}
	}
}

int detection_status()
{
	return detection;
}

int installable_status()
{
	return installable;
}


