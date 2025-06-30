#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <winreg.h>
#include "nocrt.h"

#include "winres.h"

#include "setuperr.h"

BOOL is_wrapper(const char *dll, BOOL need_exists)
{
	DWORD cbRes = 0;
	DWORD dwHandle = 0;
	void *res;
	char strbuf[255];
	int cnt = 0;
	BOOL rc = FALSE;
	
	cbRes = GetFileVersionInfoSizeA(dll, &dwHandle);
	if(cbRes > 0)
	{
		res = malloc(cbRes);
		if(res != NULL)
		{
			if(GetFileVersionInfoA(dll, dwHandle, cbRes, res))
			{
				struct LANGANDCODEPAGE {
				  WORD wLanguage;
				  WORD wCodePage;
				} *lpTranslate;
				UINT cbTranslate;
				UINT i;
					
				VerQueryValueA(res, "\\VarFileInfo\\Translation", (LPVOID *)&lpTranslate, &cbTranslate);
					
				for(i = 0; i < (cbTranslate/sizeof(struct LANGANDCODEPAGE)); i++)
				{
					void *desc;
					UINT descLen;
					
					sprintf(strbuf, "\\StringFileInfo\\%04x%04x\\ProductName", lpTranslate[i].wLanguage, lpTranslate[i].wCodePage);
					if(VerQueryValue(res, strbuf, &desc, &descLen))
					{
						if(descLen < sizeof(strbuf))
						{
							memcpy(strbuf, desc, descLen);
							strbuf[descLen] = '\0';
							//printf("ProductName (%u): %s\n", descLen, strbuf);
							if(strstr(strbuf, "WINE") != NULL)
							{
								rc = TRUE;
							}
							else if(strstr(strbuf, "wine") != NULL)
							{
								rc = TRUE;
							}
						}
						
						cnt++;
					}
				}
			}
			
			free(res);
			
			/* no info, this is probably wrapper... */
			if(cnt == 0)
			{
				rc = TRUE;
			}
			
		}
	}
	else if(need_exists)
	{
		return TRUE;
	}
	
	return rc;
}

