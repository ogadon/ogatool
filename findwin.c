/*
 *  findwin : Windowタイトルからコントロールの値(text)を取り出す
 *
 *    2001/01/15 V0.10 by oga.
 *    2014/02/20 V0.11 fix warning for MinGW
 */
#include <windows.h>
#include <stdio.h>

char *WinTitle = "素数べんち";
int  af     = 0;            /* -a  flag      */
int  evf    = 0;            /* -ev flag      */
int  evid   = 0;            /* -ev event id  */

int SendMsg(HWND hWin, int evid)
{
    switch (evid) {
      case WM_KEYDOWN:
        SendMessage(hWin, evid,     'a', 0);
        SendMessage(hWin, WM_KEYUP, 'a', 0);
        SendMessage(hWin, WM_SETTEXT, 4, (long)"abcd");  // タイトルバーに設定されてしまう 
	break;

      default:
        SendMessage(hWin, evid, 0, 0);
	break;
    }
    return 0;
}

int GetControlList(HWND hWin, int level)
{
    HWND hCntl = NULL;
    char classname[4096];
    char ctrltext[4096];
    char spc[1024];
    int  ret;

    ++level;
    strcpy(spc, "                                                          ");
    spc[level-1] = '\0';
#if 0
    if (level > 1) {
        spc[level-1] = '+';
    }
#endif

    while (1) {
        hCntl = FindWindowEx(hWin, hCntl, NULL, NULL);
	if (!hCntl) {
	    break;
	}
	GetClassName(hCntl, classname, sizeof(classname));
	ctrltext[0] = '\0';
	ret = GetWindowText(hCntl, ctrltext, sizeof(ctrltext));
	if (ret == 0) {
	    ret = GetLastError();
	    if (ret != ERROR_SUCCESS) {
	        sprintf(ctrltext, "GetWindowText Error(%d)", ret);
	    }
	}
	printf("%sClass:%-20s  Text:[%s]\n", spc, classname, ctrltext);
	if (evf && strstr(ctrltext, WinTitle) && strncmp(ctrltext, "コマンド", 8)) {
            hWin = FindWindow(NULL, ctrltext);  /* hWin取り直し */
	    SetForegroundWindow(hWin);
            printf("## Send Message(%d) to %s.\n", evid, ctrltext);
	    SendMsg(hWin, evid);
	    exit(1);
	}
#if 0
        /* replace other window text */
	if (!strcmp(ctrltext, "0.391")) {
	    SetWindowText(hCntl, "0.222");
	    printf("Set 0.222!!\n");
	    /* Send Redraw Message to Message Queue */
	    SendMessage(hWin, WM_PAINT, 0, 0);
	}
#endif
	GetControlList(hCntl, level);
    }
    return 0;
}

int main(int a, char *b[])
{
    HWND hWin = NULL;
    int  i;

    for (i = 1; i<a; i++) {
        if (!strcmp(b[i], "-a")) {
	    af = 1;
	    continue;
	}
        if (i+1 < a && !strcmp(b[i], "-ev")) {
	    evf = 1;
	    evid = atoi(b[++i]);
	    continue;
	}
        if (!strcmp(b[i], "-close")) {
	    evf = 1;
	    evid = WM_CLOSE;
	    continue;
	}
        if (!strcmp(b[i], "-key")) {
	    evf = 1;
	    evid = WM_KEYDOWN;
	    continue;
	}
        if (!strcmp(b[i], "-h")) {
	    printf("usage: findwin { <win_title> [-close] | -a }\n");
	    printf("       -close : send close msg to specific window\n");
	    printf("       -a     : find all window\n");
	    //printf("       -ev <event_id> : send specific event\n");
	    exit(1);
	}
        WinTitle = b[1];
    }

    if (af) {
        /* find all window */
        GetControlList(NULL, 0);
    } else {
        /* find specific window */
        hWin = FindWindow(NULL, WinTitle);
        if (hWin) {
            printf("[%s] Window Handle = 0x%08x\n", WinTitle, hWin);
	    GetControlList(hWin, 0);
        } else {
            printf("Not found %s.\n", WinTitle);
        }
	if (evf) {
            printf("Send Message(%d) to %s.\n", evid, WinTitle);
	    SendMsg(hWin, evid);
	} 
    }
    return 0;
}
