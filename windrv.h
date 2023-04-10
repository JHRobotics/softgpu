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

BOOL loadSETUAPI();
void freeSETUAPI();

#endif
