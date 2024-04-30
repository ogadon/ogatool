/*
 *   uname.c
 *
 *       99/02/11 V1.00  by oga.
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int a, char *b[])
{
    int i;
    int af = 0;
    int vf = 0;
    char *strPlat = "unknown";
    OSVERSIONINFO ver;

    for (i=1; i<a; i++) {
        if (!strcmp(b[i],"-a")) {
	    af = 1;
	    continue;
	}
        if (!strcmp(b[i],"-v")) {
	    vf = 1;
	    continue;
	}
    }

    memset(&ver, 0, sizeof(OSVERSIONINFO));
    ver.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

    if (GetVersionEx(&ver)) {
    	if (ver.dwPlatformId == VER_PLATFORM_WIN32s) {
	    strPlat = "Windows 3.1";
	} else if (ver.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {
	    strPlat = "Windows 95";
	} else if (ver.dwPlatformId == VER_PLATFORM_WIN32_NT) {
	    strPlat = "Windows NT";
	}
        if (vf) {
            printf("Measure : %d\n",     ver.dwMajorVersion);
            printf("Minor   : %d\n",     ver.dwMinorVersion);
            printf("Build   : %d\n",     ver.dwBuildNumber);
            printf("Platform: %d (%s)\n",ver.dwPlatformId, strPlat);
            printf("CSDVer  : %s\n",     ver.szCSDVersion);
	}
	if (af) {
	    printf("%s Version %d.%d  build %d %s\n",strPlat, 
	    			ver.dwMajorVersion,
	    			ver.dwMinorVersion,
	    			ver.dwBuildNumber,
	    			ver.szCSDVersion );
	} else {
	    printf("%s Version %d.%d\n",strPlat,
	    			ver.dwMajorVersion,
	    			ver.dwMinorVersion);
	}
    } else {
        printf("GetVersionEx() error(%d)\n",GetLastError());
        return 1;
    }
    return 0;
}

/* vim:ts=8:sw=4:
 */

