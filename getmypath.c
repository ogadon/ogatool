/*
 *  Get my exe path sample
 *
 *  2001/01/18 by oga.
 *
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int a, char *b[])
{
    
    char path[1024];
    HMODULE hMod = NULL;
    char *pt = NULL;

    /* ˆÈ‰º‚ğŠÖ”‰»‚·‚é‚Æ‰½ŒÌ‚©ƒhƒ‰ƒCƒu–¼‚ª•Ô‚Á‚Ä‚«‚Ü‚·‚²’ˆÓ */
    hMod = GetModuleHandle(NULL);
    if (hMod) {
        GetModuleFileName(hMod, path, sizeof(path));
    } else {
        printf("GetModuleHandle() error.(%d)\n",GetLastError());
	return -1;
    }
    printf("      I am [%s]\n", path);

    if (pt = strrchr(path, '\\')) {
        *pt = '\0';
    }
    printf("My path is [%s]\n", path);

    return 0;
}
