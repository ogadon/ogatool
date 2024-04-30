/*
 *  sendevt : 他Windowを操作するテスト
 *
 *    2008/10/13 V0.10 by oga.
 */
#include <windows.h>
#include <stdio.h>

char *WinTitle = "素数べんち";
int  evf    = 0;            /* -ev flag      */
int  evid   = 0;            /* -ev event id  */

char class_name[20][1024];

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
	strcpy(class_name[level-1], classname);
	ctrltext[0] = '\0';
	ret = GetWindowText(hCntl, ctrltext, sizeof(ctrltext));
	if (ret == 0) {
	    ret = GetLastError();
	    if (ret != ERROR_SUCCESS) {
	        sprintf(ctrltext, "GetWindowText Error(%d)", ret);
	    }
	}
	printf("%sClass:%-20s  Text:[%s]\n", spc, classname, ctrltext);
	if (evf && !strcmp(class_name[0], "Notepad") && !strcmp(class_name[1], "Edit")) {
            //hWin = FindWindow(NULL, ctrltext);  /* hWin取り直し */
	    //SetForegroundWindow(hWin);
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
    strcpy(class_name[level-1], "");
    return 0;
}

int main(int a, char *b[])
{
    HWND hWin = NULL;
    int  i;

    memset(class_name, 0, sizeof(class_name));

    for (i = 1; i<a; i++) {
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
	    printf("usage: sendevt { <win_title> [-close | -key | -ev <evt_id>]}\n");
	    printf("       -close : send close msg to specific window (evt:%d)\n", WM_CLOSE);
	    printf("       -ev <evt_id> : send specific event\n");
	    exit(1);
	}
        WinTitle = b[1];
    }

    /* find all window */
    GetControlList(NULL, 0);

    return 0;
}
