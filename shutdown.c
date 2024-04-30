//
//   shutdown for win
//
//   2001/10/17 V1.00 by oga.
//   2007/04/19 V1.01 support suspend
//   2009/12/17 V1.02 fix ExitWindowsEx() error log
//   2014/02/22 V1.03 support MinGW
//
#include <windows.h>
#include <stdio.h>
#include <powrprof.h>

// globals
char *VER = "1.03";
int  logff = 0;          // -log option
char *logfile = NULL;    // log file name
FILE *fp = NULL;

void usage()
{
    printf("shutdown Ver %s\n", VER);
    printf("usage: shutdown {-h [-pwoff] | -r | -logoff | -s} [-f] [-log <logfile>] <time>\n");
    printf("  -h      : shutdown\n");
    printf("  -pwoff  : power off after shutdown\n");
    printf("  -r      : shutdown and reboot\n");
    printf("  -logoff : only logoff\n");
    printf("  -s      : suspend\n");                                      /* V1.01-A */
    printf("  -b      : hibernate\n");                                    /* V1.01-A */
    printf("  time    : now     ... shutdown immediately (+0)\n");
    printf("            +<sec>  ... shutdown after <sec> seconds\n");
    printf("            hh:mm   ... shutdown at hh:mm (24h format)\n");
    printf("  -f      : force process to terminate. don't send terminate message.\n");
    printf("  -log    : output shutdown log to <logfile>\n");
    exit(1);
}

int outlog(char *msg)
{
    SYSTEMTIME syst;
    if (logff) {
        if (fp == NULL) {
	    if ((fp = fopen(logfile, "a")) == NULL) {
	        perror("Error: fopen()");
		logff = 0;
	    }
	}
	GetLocalTime(&syst);
	if (fp) fprintf(fp, "%04d/%02d/%02d %02d:%02d:%02d %s",
	                     syst.wYear, syst.wMonth, syst.wDay,
			     syst.wHour, syst.wMinute, syst.wSecond,
	                     msg);
    }
    printf("%s", msg);
    fflush(stdout);

    return 0;
}

//  IS_NT()
//    OUT : ret : 1:NT series   0: Win95 series or unknown
//
BOOL IS_NT()
{
    char    str[4096];        // message buffer
    OSVERSIONINFO ver;

    memset(&ver, 0, sizeof(OSVERSIONINFO));
    ver.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    if (GetVersionEx(&ver)) {
        // get success
	if (ver.dwPlatformId == VER_PLATFORM_WIN32_NT) {
	    return TRUE;  // NT series
	}
    } else {
        sprintf(str,"GetVersionEx() Error. (%d)\n", GetLastError());
	outlog(str);
    }
    return FALSE; // non NT
}

BOOL EnablePrivilege( LPTSTR privilege, BOOL enable )
{
    char    str[4096];        // message buffer
    HANDLE  hToken;
    LUID    luid;
    TOKEN_PRIVILEGES    tokenPrivileges;

    if(!IS_NT()) {
        return TRUE;
    }

    if(!OpenProcessToken( GetCurrentProcess(),
                          TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
                          &hToken)) {
        sprintf(str,"OpenProcessToken() Error. (%d)\n", GetLastError());
	outlog(str);
        return FALSE;
    }

    if(!LookupPrivilegeValue(0, privilege, &luid)) {
        sprintf(str,"LookupPrivilegeValue() Error. (%d)\n", GetLastError());
	outlog(str);
        return FALSE;
    }

    tokenPrivileges.PrivilegeCount = 1;
    tokenPrivileges.Privileges[0].Luid = luid;
    tokenPrivileges.Privileges[0].Attributes = (enable?SE_PRIVILEGE_ENABLED:0);

    if(!AdjustTokenPrivileges(hToken, FALSE, &tokenPrivileges, 0, 0, 0) ) {
        sprintf(str,"AdjustTokenPrivileges() Error. (%d)\n", GetLastError());
	outlog(str);
        return FALSE;
    }

    return TRUE;
}

int main(int a, char *b[])
{
    int shutopt = 0;   // shutdown options  (-h|-r|-f)
    int waitsec = 0;   // +<sec> option 
    int vf      = 0;   // -v
    int ff      = 0;   // -f                             /* V1.01-A */
    int sf      = 0;   // -s                             /* V1.01-A */
    int bf      = 0;   // -b                             /* V1.01-A */
    int logoff  = 0;   // -logoff
    char *timestr = NULL;  // down time string

    int i;
    int hh      = -99;
    int mm      = -99;
    char *command = "shutdown";
    char str[4096];        // message buffer
    SYSTEMTIME syst;

    for (i=1; i<a; i++) {
        if (!strcmp(b[i], "-v")) {
            ++vf;
            continue;
        }
        if (!strcmp(b[i], "-h")) {
	    shutopt &= ~(EWX_LOGOFF | EWX_REBOOT);
	    shutopt |= EWX_SHUTDOWN;
            continue;
        }
        if (!strcmp(b[i], "-pwoff")) {
	    shutopt &= ~(EWX_LOGOFF | EWX_REBOOT);
	    shutopt |= EWX_POWEROFF;
            continue;
        }
        if (!strcmp(b[i], "-r")) {
	    shutopt &= ~(EWX_LOGOFF | EWX_SHUTDOWN | EWX_POWEROFF);
	    shutopt |= EWX_REBOOT;
	    continue;
	}
        if (!strcmp(b[i], "-f")) {
	    shutopt |= EWX_FORCE;
	    ff = 1;                             /* V1.01-A */
	    continue;
	}
	/* V1.01-A start */
        if (!strcmp(b[i], "-s")) {
	    sf = 1;   /* suspend   */
	    continue;
	}
        if (!strcmp(b[i], "-b")) {
	    bf = 1;   /* hibernate */
	    continue;
	}
	/* V1.01-A end   */
        if (!strcmp(b[i], "-log")) {
	    if (++i < a) {
	        logff = 1;
		logfile = b[i];
	        continue;
	    } else {
                printf("Error: log file not specified\n", timestr);
                usage();
	    }
	}
        if (!strcmp(b[i], "-logoff")) {
	    shutopt &= ~(EWX_SHUTDOWN | EWX_POWEROFF | EWX_REBOOT);
	    shutopt |= EWX_LOGOFF;
	    continue;
	}
	timestr = b[i];
    }
    if (timestr == NULL) {
        printf("Error: time not specified\n");
        usage();
    }

    // -h & -pwoff check
    if ((shutopt & EWX_POWEROFF) && ((shutopt & EWX_SHUTDOWN) == 0)) {
        usage();
    }

    // analyze time arg
    if (!strcmp(timestr, "now")) {
        // "now"
        waitsec = 0;
    } else if (timestr[0] == '+') {
        // "+<sec>"
	waitsec = atoi(&timestr[1]);
    } else if (timestr[2] == ':' && strlen(timestr) == 5) {
        // "hh:mm"
	hh = atoi(&timestr[0]);
	mm = atoi(&timestr[3]);
	if (hh < 0 || hh > 23 || mm < 0 || mm > 59) {
	    // invalid value
	    printf("Error: Invalid hh:mm value [%s]\n", timestr);
	    usage();
	}
    } else {
        printf("Error: Invalid time value [%s]\n", timestr);
        usage();
    }

    //
    // shutdown main
    //
    if (shutopt & EWX_LOGOFF) {
        command = "logoff";
	logoff = 1;
    }
    GetLocalTime(&syst);
    sprintf(str, "##### shutdown Ver %s  at %02d:%02d:%02d #####\n", 
                             VER,
			     syst.wHour, syst.wMinute, syst.wSecond);
    outlog(str);

    sprintf(str, "shutdown opt : %s%s%s%s%s%s\n",                   /* V1.01-C */
                      sf                       ? "SUSPEND "  : "",  /* V1.01-A */
                      bf                       ? "HIBERNATE ": "",  /* V1.01-A */
                      (shutopt & EWX_LOGOFF)   ? "LOGOFF "   : "",
                      (shutopt & EWX_SHUTDOWN) ? "SHUTDOWN " : "",
                      (shutopt & EWX_POWEROFF) ? "POWEROFF " : "",
                      (shutopt & EWX_REBOOT)   ? "REBOOT "   : "",
                      (shutopt & EWX_FORCE)    ? "FORCE "    : "");
    outlog(str);

    if (hh == -99) {
        // shutdown after <sec>
        sprintf(str,"%s after %d second%s.\n", 
	             command, waitsec, (waitsec > 1)?"s":"");
	outlog(str);
	Sleep(waitsec * 1000);
    } else {
        // shutdown at hh:mm
        sprintf(str,"%s at %02d:%02d \n", command, hh, mm);
	outlog(str);
	while (1) {
	    GetLocalTime(&syst);
	    if (syst.wHour == hh && syst.wMinute == mm) {
	        break;
	    }
	    Sleep(5000);
	}
    }

    // shutdown start message and beep
    sprintf(str, "%s start...\n", command);
    outlog(str);
    printf("");
    fflush(stdout);
    
    /* V1.01-A start */
    /* suspend/hibernate */
    if (sf || bf) {
		    	/* TRUE:hibernate   FALSE:suspend */
			/* TRUE:immediately FALSE:send PBT_APMQUERYSUSPEND */
	   		/* TRUE:disable wake events FALSE:wake event enable */
	if (FALSE == SetSuspendState(bf?TRUE:FALSE,
			ff?TRUE:FALSE,
			FALSE))
	{
       	    printf(str, "Error: SetSuspendState() failed. (%d)\n", GetLastError());
            exit(4);
	}
    }
    /* V1.01-A start */

    // if shutdown, Enable Privilege
    if (!logoff && (EnablePrivilege(SE_SHUTDOWN_NAME, TRUE) == FALSE)) {
        sprintf(str, "%s cancelled.\n", command);
        outlog(str);
        exit(2);
    }

    if (fp) fclose(fp);  // close logfile
    logff = 0;
    if (!ExitWindowsEx(shutopt, 0)) {
    	char *msgbuf;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
	              FORMAT_MESSAGE_FROM_SYSTEM,
	              NULL,
	              GetLastError(),
	              MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // デフォルト ユーザー言語 
	              (LPTSTR)&msgbuf,
	              0,
	              NULL);
        sprintf(str, "Error: ExitWindowEx() failed. (%d) %s\n", GetLastError(), msgbuf);  /* V1.02-C */
	LocalFree(msgbuf);
        outlog(str);                                                                      /* V1.02-A */
        exit(3);
    }
    return 0;
}

/* vim:ts=8:sw=4:
 */

