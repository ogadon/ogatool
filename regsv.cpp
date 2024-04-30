/*
 *  regsv : Service Add/Delete Tool
 *
 *    2001/03/06 V1.00 by oga.
 *    2003/07/16 V1.01 support -checksv
 */
#include <windows.h>
#include <stdio.h>

int AddService(char *svname, char *exepath)
{
    SC_HANDLE hSCManager;
    SC_HANDLE hService;

    printf("### Add service %s (%s)\n", svname, exepath);

    hSCManager = OpenSCManager(
                        NULL,                   // machine (NULL == local)
                        NULL,                   // database (NULL == default)
                        SC_MANAGER_ALL_ACCESS   // access required
                        ) ;

    if (hSCManager == NULL) {
        printf("OpenSCM Error (%s) (%d)\n",svname, GetLastError());
        return 1;
    }

    /* XXX */
    hService = CreateService(
            hSCManager,                 // SCManager database
            svname,                     // name of service
            svname,                     // name to display
            SERVICE_ALL_ACCESS,         // desired access
            SERVICE_WIN32_SHARE_PROCESS,// service type
            //SERVICE_AUTO_START,       // start type (AUTO)
            SERVICE_DEMAND_START,       // start type (DEMAND)
            SERVICE_ERROR_NORMAL,       // error control type
            exepath,                    // service's binary
            NULL,                       // no load ordering group
            NULL,                       // no tag identifier
            NULL,                       // dependencies
            NULL,                       // LocalSystem account
            NULL) ;                     // no password
    /* XXX */

    if (hService == NULL) {
        printf("CreateService Error (%s) (%d)\n",svname, GetLastError());
        CloseServiceHandle(hSCManager);
        return 1;
    } else {
        printf("CreateServcie success(%s)\n",svname);
        CloseServiceHandle(hService);
    }
    CloseServiceHandle(hSCManager);

    return 0;
}

int DelService(char *svname)
{
    SC_HANDLE hSCManager;
    SC_HANDLE hService;

    printf("### Delete service %s\n",svname);

    hSCManager = OpenSCManager(
                        NULL,                   // machine (NULL == local)
                        NULL,                   // database (NULL == default)
                        SC_MANAGER_ALL_ACCESS   // access required
                        ) ;

    if (hSCManager == NULL) {
        printf("OpenSCM Error (%s) (%d)\n",svname, GetLastError());
        return 1;
    }

    hService = OpenService( hSCManager, svname, SERVICE_ALL_ACCESS) ;

    if (hService == NULL) {
        printf("OpenService Error (%s) (%d)\n",svname, GetLastError());
        return 1;
    }

    if(DeleteService(hService) == TRUE) {
         printf("DeleteServcie success(%s)\n",svname);
    } else {
         printf("DeleteService Error %d\n",GetLastError());
    }

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);
    return 0;
}

/*  サービスの状態をチェックする V1.01-A */
int CheckService(char *svname)
{
    SC_HANDLE      hSCManager;
    SC_HANDLE      hService;
    SERVICE_STATUS svstat;
    int            dispf = 0;
    int            i;

    DWORD vals[] = { SERVICE_CONTINUE_PENDING,
                     SERVICE_PAUSE_PENDING,
                     SERVICE_PAUSED,
                     SERVICE_RUNNING,
                     SERVICE_START_PENDING,
                     SERVICE_STOP_PENDING,
                     SERVICE_STOPPED };

    char *stat_name[] = { "SERVICE_CONTINUE_PENDING",
                          "SERVICE_PAUSE_PENDING",
                          "SERVICE_PAUSED",
                          "SERVICE_RUNNING",
                          "SERVICE_START_PENDING",
                          "SERVICE_STOP_PENDING",
                          "SERVICE_STOPPED" };


    printf("### Check service [%s] status\n",svname);

    hSCManager = OpenSCManager(
                        NULL,                   // machine (NULL == local)
                        NULL,                   // database (NULL == default)
                        GENERIC_READ            // access required
                        ) ;

    if (hSCManager == NULL) {
        printf("OpenSCM Error (%s) (%d)\n",svname, GetLastError());
        return 1;
    }

    hService = OpenService( hSCManager, svname, SERVICE_QUERY_STATUS) ;

    if (hService == NULL) {
        printf("OpenService Error (%s) (%d)\n",svname, GetLastError());
        return 1;
    }

    if(QueryServiceStatus(hService, &svstat) == TRUE) {
         /* printf("QueryServiceStatus success(%s)\n",svname); */
    } else {
         printf("QueryServiceStatus Error %d\n",GetLastError());
    }

    for (i = 0; i < sizeof(vals)/sizeof(DWORD); i++) {
        if (svstat.dwCurrentState == vals[i]) {
	    printf("[%s] status : %s\n", svname, stat_name[i]);
	    dispf = 1;
	    break;
	}
    }

    if (!dispf) {
        printf("unknown service status %d\n", svstat.dwCurrentState);
    }

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);
    return 0;
}

void usage()
{
    printf("usage: regsv -check <svname>\n");
    printf("usage: regsv -add <svname> <exepath>\n");
    printf("usage: regsv -del <svname>\n");
    printf("usage: regsv -deladd <svname> <exepath>\n");
}

int main(int a, char *b[])
{
    char svname[256];
    char *sid = "dummySID";
    char *sup = "dummySUP";
    char *typ = "dummyTYPE";
    int  mpn;

    if (a == 1 || !strcmp(b[1], "-h")) {
        usage();
	return 1;
    }
    if (!strcmp(b[1], "-add")) {
        // -add  (Add Spec Service)
	if (a < 3) {
	    usage();
	    return 1;
	}
	AddService(b[2], b[3]);
    } else if (!strcmp(b[1], "-deladd")) {
        // -deladd  (Delete & re Add Spec Service)
	if (a < 3) {
	    usage();
	    return 1;
	}
        DelService(b[2]);
	AddService(b[2], b[3]);
    } else if (!strcmp(b[1], "-del")) {
        // Delete Spec Service
        DelService(b[2]);
    } else if (!strcmp(b[1], "-check")) {
        // Check Spec Service
        CheckService(b[2]);
    } else {
        printf("Invalid Option (%s)\n", b[1]);
    }

    return 0;
}


