#ifndef __WINDRV_H__INCLUDED__
#define __WINDRV_H__INCLUDED__

#include <stdint.h>

BOOL installVideoDriver(const char *szDriverDir, const char *infName);
const char *normalize_version(const char *version_str);

typedef struct _version_t
{
	int major;
	int minor;
	int patch;
	int build;
} version_t;

void version_parse(const char *version_str, version_t *out);
int version_compare(const version_t *v1, const version_t *v2);
void version_win(version_t *v);
BOOL version_is_nt();
DWORD checkInstallation();

BOOL loadSETUAPI();
void freeSETUAPI();

#define CHECK_DRV_OK          0
#define CHECK_DRV_FAIL        4

typedef struct vga_device
{
	int type;
	union
	{
		struct
		{
			uint16_t pnp;
		} isa;
		struct
		{
			uint16_t ven;
			uint16_t dev;
			uint32_t subsys;
			BOOL has_subsys;
		} pci;
	};
} vga_device_t;

#define VGA_DEVICE_ISA 1
#define VGA_DEVICE_PCI 2

vga_device_t *device_get(int n);
int device_count();
BOOL device_parse(const char *hw_ident, vga_device_t *dev);
void device_str(vga_device_t *dev, char *buf, BOOL shorter);
const char *device_ini_get(vga_device_t *dev, const char *property);

#endif
