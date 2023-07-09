#ifndef __WINDRV_H__INCLUDED__
#define __WINDRV_H__INCLUDED__

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
int version_compare(version_t *v1, version_t *v2);
void version_win(version_t *v);
BOOL version_is_nt();
DWORD checkInstallation();

BOOL loadSETUAPI();
void freeSETUAPI();

#define CHECK_DRV_OK          0
#define CHECK_DRV_NOPCI       1
#define CHECK_DRV_LEGACY_VGA  2
#define CHECK_DRV_FAIL        4

#endif
