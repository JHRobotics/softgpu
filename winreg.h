#ifndef __WINREG_H__INCLUDED__
#define __WINREG_H__INCLUDED__

BOOL registryRead(const char *path, char *buffer, size_t buffer_size);
BOOL registryReadDWORD(const char *path, DWORD *out);
BOOL registryWrite(const char *path, const char *str, int type);
BOOL registryWriteDWORD(const char *path, DWORD dw);
BOOL registryDelete(const char *path);

BOOL registryWriteInfDWORD(const char *path, DWORD dw, const char *inf);
BOOL registryDeleteInf(const char *path, const char *inf);
BOOL registryWriteInf(const char *path, const char *str, int type, const char *inf);

#define WINREG_DWORD 1
#define WINREG_STR   2

#endif /*  __WINREG_H__INCLUDED__ */
